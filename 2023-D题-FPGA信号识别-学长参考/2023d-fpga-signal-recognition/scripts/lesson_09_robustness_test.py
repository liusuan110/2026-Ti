from pathlib import Path
import csv

import numpy as np
import matplotlib.pyplot as plt


FS = 20_000_000
FC = 2_000_000
DURATION = 0.008
CANDIDATE_FREQS = np.array([1_000, 2_000, 3_000, 4_000, 5_000])


def am_signal(t, ma, fm, fc):
    envelope = 1.0 + ma * np.sin(2 * np.pi * fm * t)
    return envelope * np.cos(2 * np.pi * fc * t)


def fm_signal(t, mf, fm, fc):
    phase_mod = mf * np.sin(2 * np.pi * fm * t)
    return np.cos(2 * np.pi * fc * t + phase_mod)


def cw_signal(t, fc):
    return np.cos(2 * np.pi * fc * t)


def moving_average(x, window_len):
    window = np.ones(window_len) / window_len
    return np.convolve(x, window, mode="same")


def quantize_8bit(x, full_scale=2.0):
    adc_max = 127
    adc_min = -128
    codes = np.round(x / full_scale * adc_max)
    codes = np.clip(codes, adc_min, adc_max)
    return codes / adc_max * full_scale


def impair(x, amplitude=1.0, dc_offset=0.0, noise_rms=0.0, rng=None):
    if rng is None:
        rng = np.random.default_rng(0)
    y = amplitude * x + dc_offset
    if noise_rms > 0:
        y = y + rng.normal(0.0, noise_rms, size=len(x))
    return y


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
    max_score = np.max(scores)
    if max_score == 0:
        return scores
    return scores / max_score


def analyze_unknown_signal(x, t):
    start = int(FS * 0.001)
    stop = int(FS * 0.006)

    # Real ADC streams often have a DC bias. Remove it before DDC.
    x = x - np.mean(x[start:stop])

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
        result.update({
            "kind": "AM",
            "F_hz": freq_est,
            "ma": mag_depth,
        })
    elif phase_span > 0.45:
        scores = score_candidate_frequencies(t_u, phase_ac, CANDIDATE_FREQS)
        freq_est = int(CANDIDATE_FREQS[np.argmax(scores)])
        result.update({
            "kind": "FM",
            "F_hz": freq_est,
            "mf": phase_span,
            "delta_f_hz": phase_span * freq_est,
        })

    return result


def make_case_signal(t, kind, fc):
    if kind == "AM":
        return am_signal(t, ma=0.6, fm=3_000, fc=fc), {"F_hz": 3_000, "ma": 0.6}
    if kind == "FM":
        return fm_signal(t, mf=3.0, fm=5_000, fc=fc), {"F_hz": 5_000, "mf": 3.0, "delta_f_hz": 15_000}
    return cw_signal(t, fc=fc), {}


def evaluate_scenario(name, amplitude, dc_offset, noise_rms, carrier_offset_hz, rng):
    t = np.arange(int(FS * DURATION)) / FS
    rows = []

    for kind in ["AM", "FM", "CW"]:
        raw, truth = make_case_signal(t, kind, FC + carrier_offset_hz)
        impaired = impair(raw, amplitude=amplitude, dc_offset=dc_offset, noise_rms=noise_rms, rng=rng)
        adc_signal = quantize_8bit(impaired)
        result = analyze_unknown_signal(adc_signal, t)

        rows.append({
            "scenario": name,
            "true_kind": kind,
            "decision": result["kind"],
            "true_F_hz": truth.get("F_hz"),
            "estimated_F_hz": result["F_hz"],
            "true_ma": truth.get("ma"),
            "estimated_ma": result["ma"],
            "true_mf": truth.get("mf"),
            "estimated_mf": result["mf"],
            "true_delta_f_hz": truth.get("delta_f_hz"),
            "estimated_delta_f_hz": result["delta_f_hz"],
            "mag_depth": result["mag_depth"],
            "phase_span": result["phase_span"],
            "amplitude": amplitude,
            "dc_offset": dc_offset,
            "noise_rms": noise_rms,
            "carrier_offset_hz": carrier_offset_hz,
        })

    return rows


def ok_flag(row):
    if row["decision"] != row["true_kind"]:
        return False
    if row["true_F_hz"] is not None and row["estimated_F_hz"] != row["true_F_hz"]:
        return False
    if row["true_ma"] is not None and abs(row["estimated_ma"] - row["true_ma"]) > 0.1:
        return False
    if row["true_mf"] is not None and abs(row["estimated_mf"] - row["true_mf"]) > 0.3:
        return False
    if row["true_delta_f_hz"] is not None and abs(row["estimated_delta_f_hz"] - row["true_delta_f_hz"]) > 300:
        return False
    return True


def fmt(value, digits=4):
    if value is None:
        return "-"
    if isinstance(value, str):
        return value
    if isinstance(value, (int, np.integer)):
        return str(value)
    return f"{value:.{digits}f}"


def main():
    out_dir = Path("outputs/lesson_09")
    out_dir.mkdir(parents=True, exist_ok=True)
    rng = np.random.default_rng(2023)

    scenarios = [
        {"name": "clean", "amplitude": 1.00, "dc_offset": 0.00, "noise_rms": 0.00, "carrier_offset_hz": 0},
        {"name": "low_amp", "amplitude": 0.35, "dc_offset": 0.00, "noise_rms": 0.00, "carrier_offset_hz": 0},
        {"name": "dc_offset", "amplitude": 1.00, "dc_offset": 0.25, "noise_rms": 0.00, "carrier_offset_hz": 0},
        {"name": "noise_5pct", "amplitude": 1.00, "dc_offset": 0.00, "noise_rms": 0.05, "carrier_offset_hz": 0},
        {"name": "noise_12pct", "amplitude": 1.00, "dc_offset": 0.00, "noise_rms": 0.12, "carrier_offset_hz": 0},
        {"name": "carrier_plus_200hz", "amplitude": 1.00, "dc_offset": 0.00, "noise_rms": 0.00, "carrier_offset_hz": 200},
        {"name": "carrier_plus_1000hz", "amplitude": 1.00, "dc_offset": 0.00, "noise_rms": 0.00, "carrier_offset_hz": 1000},
        {"name": "mixed_mild", "amplitude": 0.70, "dc_offset": 0.15, "noise_rms": 0.05, "carrier_offset_hz": 200},
    ]

    rows = []
    for scenario in scenarios:
        rows.extend(evaluate_scenario(rng=rng, **scenario))

    csv_path = out_dir / "robustness_results.csv"
    with csv_path.open("w", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()) + ["ok"])
        writer.writeheader()
        for row in rows:
            out = dict(row)
            out["ok"] = ok_flag(row)
            writer.writerow(out)

    print("scenario,true,decision,ok,est_F,est_ma,est_mf,est_df,mag_depth,phase_span")
    for row in rows:
        print(
            f"{row['scenario']},{row['true_kind']},{row['decision']},{ok_flag(row)},"
            f"{fmt(row['estimated_F_hz'], 0)},{fmt(row['estimated_ma'])},"
            f"{fmt(row['estimated_mf'])},{fmt(row['estimated_delta_f_hz'], 1)},"
            f"{row['mag_depth']:.4f},{row['phase_span']:.4f}"
        )

    scenario_names = [s["name"] for s in scenarios]
    ok_rates = []
    for name in scenario_names:
        selected = [row for row in rows if row["scenario"] == name]
        ok_rates.append(sum(ok_flag(row) for row in selected) / len(selected))

    fig, ax = plt.subplots(figsize=(12, 4))
    ax.bar(scenario_names, ok_rates, color="#1f77b4")
    ax.set_ylim(0, 1.1)
    ax.set_ylabel("pass rate")
    ax.set_title("Basic pipeline robustness scenarios")
    ax.grid(True, axis="y", alpha=0.3)
    ax.tick_params(axis="x", rotation=25)
    fig.tight_layout()
    png_path = out_dir / "robustness_summary.png"
    fig.savefig(png_path, dpi=150)

    print(f"CSV written to: {csv_path.resolve()}")
    print(f"PNG written to: {png_path.resolve()}")


if __name__ == "__main__":
    main()
