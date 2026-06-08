from pathlib import Path
import csv

import numpy as np


FS = 20_000_000
FC = 2_000_000
DURATION = 0.008
CANDIDATE_FREQS = np.array([1_000, 2_000, 3_000, 4_000, 5_000])


def am_signal(t, ma, fm):
    envelope = 1.0 + ma * np.sin(2 * np.pi * fm * t)
    return envelope * np.cos(2 * np.pi * FC * t)


def fm_signal(t, mf, fm):
    phase_mod = mf * np.sin(2 * np.pi * fm * t)
    return np.cos(2 * np.pi * FC * t + phase_mod)


def cw_signal(t):
    return np.cos(2 * np.pi * FC * t)


def moving_average(x, window_len):
    window = np.ones(window_len) / window_len
    return np.convolve(x, window, mode="same")


def quantize_8bit(x, full_scale=2.0):
    adc_max = 127
    adc_min = -128
    codes = np.round(x / full_scale * adc_max)
    codes = np.clip(codes, adc_min, adc_max)
    return codes / adc_max * full_scale


def ddc_iq(x, t, window_len=201):
    lo_i = 2.0 * np.cos(2 * np.pi * FC * t)
    lo_q = -2.0 * np.sin(2 * np.pi * FC * t)
    i_baseband = moving_average(x * lo_i, window_len)
    q_baseband = moving_average(x * lo_q, window_len)
    return i_baseband, q_baseband


def score_candidate_frequencies(t, feature, candidates):
    feature_ac = feature - np.mean(feature)
    scores = []

    for freq in candidates:
        ref_cos = np.cos(2 * np.pi * freq * t)
        ref_sin = np.sin(2 * np.pi * freq * t)
        i_score = np.sum(feature_ac * ref_cos)
        q_score = np.sum(feature_ac * ref_sin)
        scores.append(np.sqrt(i_score * i_score + q_score * q_score))

    scores = np.array(scores)
    return scores / np.max(scores)


def analyze_unknown_signal(x, t):
    start = int(FS * 0.001)
    stop = int(FS * 0.006)

    i_baseband, q_baseband = ddc_iq(x, t)
    magnitude = np.sqrt(i_baseband * i_baseband + q_baseband * q_baseband)
    phase = np.unwrap(np.arctan2(q_baseband, i_baseband))

    mag_u = magnitude[start:stop]
    phase_u = phase[start:stop]
    phase_ac = phase_u - np.mean(phase_u)
    t_u = t[start:stop]

    mag_depth = (np.max(mag_u) - np.min(mag_u)) / (np.max(mag_u) + np.min(mag_u))
    phase_span = (np.max(phase_ac) - np.min(phase_ac)) / 2.0

    result = {
        "mag_depth": mag_depth,
        "phase_span": phase_span,
        "kind": "CW",
        "F_hz": None,
        "ma": None,
        "mf": None,
        "delta_f_hz": None,
    }

    if mag_depth > 0.18:
        scores = score_candidate_frequencies(t_u, mag_u, CANDIDATE_FREQS)
        freq_est = int(CANDIDATE_FREQS[np.argmax(scores)])
        ma_est = mag_depth
        result.update({
            "kind": "AM",
            "F_hz": freq_est,
            "ma": ma_est,
        })
    elif phase_span > 0.45:
        scores = score_candidate_frequencies(t_u, phase_ac, CANDIDATE_FREQS)
        freq_est = int(CANDIDATE_FREQS[np.argmax(scores)])
        mf_est = phase_span
        result.update({
            "kind": "FM",
            "F_hz": freq_est,
            "mf": mf_est,
            "delta_f_hz": mf_est * freq_est,
        })

    return result


def format_optional(value, digits=4):
    if value is None:
        return "-"
    if isinstance(value, int):
        return str(value)
    return f"{value:.{digits}f}"


def main():
    out_dir = Path("outputs/lesson_08")
    out_dir.mkdir(parents=True, exist_ok=True)

    t = np.arange(int(FS * DURATION)) / FS
    cases = [
        {"name": "am_1k_ma04", "true_kind": "AM", "F_hz": 1_000, "ma": 0.4, "signal": am_signal(t, 0.4, 1_000)},
        {"name": "am_5k_ma08", "true_kind": "AM", "F_hz": 5_000, "ma": 0.8, "signal": am_signal(t, 0.8, 5_000)},
        {"name": "fm_2k_mf20", "true_kind": "FM", "F_hz": 2_000, "mf": 2.0, "delta_f_hz": 4_000, "signal": fm_signal(t, 2.0, 2_000)},
        {"name": "fm_5k_mf50", "true_kind": "FM", "F_hz": 5_000, "mf": 5.0, "delta_f_hz": 25_000, "signal": fm_signal(t, 5.0, 5_000)},
        {"name": "cw", "true_kind": "CW", "signal": cw_signal(t)},
    ]

    rows = []
    print("2023D basic pipeline simulation")
    print("case,true,decision,true_F,est_F,true_ma,est_ma,true_mf,est_mf,true_df,est_df,mag_depth,phase_span")

    for case in cases:
        adc_signal = quantize_8bit(case["signal"])
        result = analyze_unknown_signal(adc_signal, t)

        row = {
            "case": case["name"],
            "true_kind": case["true_kind"],
            "decision": result["kind"],
            "true_F_hz": case.get("F_hz"),
            "estimated_F_hz": result["F_hz"],
            "true_ma": case.get("ma"),
            "estimated_ma": result["ma"],
            "true_mf": case.get("mf"),
            "estimated_mf": result["mf"],
            "true_delta_f_hz": case.get("delta_f_hz"),
            "estimated_delta_f_hz": result["delta_f_hz"],
            "mag_depth": result["mag_depth"],
            "phase_span": result["phase_span"],
        }
        rows.append(row)

        print(
            f"{row['case']},{row['true_kind']},{row['decision']},"
            f"{format_optional(row['true_F_hz'], 0)},{format_optional(row['estimated_F_hz'], 0)},"
            f"{format_optional(row['true_ma'])},{format_optional(row['estimated_ma'])},"
            f"{format_optional(row['true_mf'])},{format_optional(row['estimated_mf'])},"
            f"{format_optional(row['true_delta_f_hz'], 1)},{format_optional(row['estimated_delta_f_hz'], 1)},"
            f"{row['mag_depth']:.4f},{row['phase_span']:.4f}"
        )

    csv_path = out_dir / "basic_pipeline_results.csv"
    with csv_path.open("w", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)

    print(f"CSV written to: {csv_path.resolve()}")


if __name__ == "__main__":
    main()
