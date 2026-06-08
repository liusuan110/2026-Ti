from pathlib import Path
import csv

import numpy as np
import matplotlib.pyplot as plt


FS = 20_000_000
FC = 2_000_000
PHASE_BITS = 32
LUT_ADDR_BITS = 10
AMP_BITS = 12
SAMPLE_BITS = 8
WINDOW_LEN = 16
SHIFT_BITS = 4
DURATION = 0.006
ADC_AMP = 80
BLOCK_START_S = 0.001
BLOCK_LEN = 40_000
CANDIDATE_FREQS = np.array([1_000, 2_000, 3_000, 4_000, 5_000])


OUT_DIR = Path("outputs/lesson_24")


def build_lut(kind):
    amp_max = (1 << (AMP_BITS - 1)) - 1
    addr = np.arange(1 << LUT_ADDR_BITS)
    theta = 2 * np.pi * addr / (1 << LUT_ADDR_BITS)
    if kind == "cos":
        value = np.cos(theta)
    elif kind == "sin":
        value = np.sin(theta)
    else:
        raise ValueError(kind)
    return np.round(value * amp_max).astype(int)


def dds_sequence(num_samples):
    phase_modulus = 1 << PHASE_BITS
    phase_step = round(FC / FS * phase_modulus)
    cos_lut = build_lut("cos")
    sin_lut = build_lut("sin")

    phase = 0
    cos_values = np.zeros(num_samples, dtype=np.int64)
    sin_values = np.zeros(num_samples, dtype=np.int64)
    for n in range(num_samples):
        lut_addr = phase >> (PHASE_BITS - LUT_ADDR_BITS)
        cos_values[n] = int(cos_lut[lut_addr])
        sin_values[n] = int(sin_lut[lut_addr])
        phase = (phase + phase_step) % phase_modulus

    return cos_values, sin_values


def moving_average_hw_style(x):
    delay = [0] * WINDOW_LEN
    running_sum = 0
    avg = np.zeros(len(x), dtype=np.int64)
    valid = np.zeros(len(x), dtype=bool)

    for n, value in enumerate(x):
        oldest = delay[-1]
        delay = [int(value)] + delay[:-1]
        running_sum = running_sum + int(value) - int(oldest)
        avg[n] = running_sum >> SHIFT_BITS
        valid[n] = n >= WINDOW_LEN - 1

    return avg, valid


def quantize_adc(x):
    adc_max = (1 << (SAMPLE_BITS - 1)) - 1
    adc_min = -(1 << (SAMPLE_BITS - 1))
    return np.clip(np.round(x), adc_min, adc_max).astype(np.int64)


def fm_signal(t, mf, fm, carrier_offset_hz=0):
    phase_mod = mf * np.sin(2 * np.pi * fm * t)
    return ADC_AMP * np.cos(2 * np.pi * (FC + carrier_offset_hz) * t + phase_mod)


def cw_signal(t, carrier_offset_hz=0):
    return ADC_AMP * np.cos(2 * np.pi * (FC + carrier_offset_hz) * t)


def ddc_iq(adc_samples, cos_lo, sin_lo):
    i_raw = adc_samples * cos_lo
    q_raw = adc_samples * (-sin_lo)
    i_baseband, valid_i = moving_average_hw_style(i_raw)
    q_baseband, valid_q = moving_average_hw_style(q_raw)
    valid = valid_i & valid_q
    return i_baseband, q_baseband, valid


def fit_phase_line_by_endpoints(t_all, phase_all, start, stop):
    slope = (phase_all[stop] - phase_all[start]) / (t_all[stop] - t_all[start])
    intercept = phase_all[start] - slope * t_all[start]
    line = slope * t_all[start:stop] + intercept
    freq_offset_hz = slope / (2 * np.pi)
    return line, freq_offset_hz


def candidate_scores(t, feature):
    feature_ac = feature - np.mean(feature)
    rows = []
    powers = []

    for freq in CANDIDATE_FREQS:
        ref_cos = np.cos(2 * np.pi * freq * t)
        ref_sin = np.sin(2 * np.pi * freq * t)
        i_score = float(np.sum(feature_ac * ref_cos))
        q_score = float(np.sum(feature_ac * ref_sin))
        power = i_score * i_score + q_score * q_score
        rows.append({
            "candidate_hz": int(freq),
            "i_score": i_score,
            "q_score": q_score,
            "power": power,
        })
        powers.append(power)

    powers = np.array(powers)
    max_power = float(np.max(powers)) if len(powers) else 0.0
    for row in rows:
        row["score_norm"] = row["power"] / max_power if max_power else 0.0

    best = rows[int(np.argmax(powers))]
    return rows, int(best["candidate_hz"])


def sinusoid_amplitude(t, feature, freq):
    feature_ac = feature - np.mean(feature)
    ref_cos = np.cos(2 * np.pi * freq * t)
    ref_sin = np.sin(2 * np.pi * freq * t)
    i_score = np.sum(feature_ac * ref_cos)
    q_score = np.sum(feature_ac * ref_sin)
    return float(2.0 * np.sqrt(i_score * i_score + q_score * q_score) / len(feature_ac))


def phase_span(feature):
    feature_ac = feature - np.mean(feature)
    return float((np.max(feature_ac) - np.min(feature_ac)) / 2.0)


def analyze_signal(name, rf, t, cos_lo, sin_lo, truth):
    adc = quantize_adc(rf)
    i_baseband, q_baseband, valid = ddc_iq(adc, cos_lo, sin_lo)
    phase = np.unwrap(np.arctan2(q_baseband, i_baseband))

    start = int(BLOCK_START_S * FS)
    if not valid[start]:
        start = int(np.flatnonzero(valid)[0])
    stop = start + BLOCK_LEN

    t_block = t[start:stop]
    phase_block = phase[start:stop]
    line, carrier_offset_est_hz = fit_phase_line_by_endpoints(t, phase, start, stop)
    phase_detrended = phase_block - line
    phase_detrended = phase_detrended - np.mean(phase_detrended)

    score_rows, freq_est = candidate_scores(t_block, phase_detrended)
    mf_corr = sinusoid_amplitude(t_block, phase_detrended, freq_est)
    mf_span = phase_span(phase_detrended)
    delta_f_hz = mf_corr * freq_est

    result = {
        "name": name,
        "true_kind": truth["kind"],
        "true_fm_hz": truth.get("fm_hz"),
        "true_mf": truth.get("mf"),
        "true_delta_f_hz": truth.get("delta_f_hz"),
        "estimated_fm_hz": freq_est,
        "estimated_mf_corr": mf_corr,
        "estimated_mf_span": mf_span,
        "estimated_delta_f_hz": delta_f_hz,
        "estimated_carrier_offset_hz": carrier_offset_est_hz,
        "phase_span": mf_span,
        "ok_f": truth.get("fm_hz") == freq_est if truth.get("fm_hz") is not None else None,
        "ok_mf": abs(mf_corr - truth["mf"]) <= 0.3 if truth.get("mf") is not None else None,
        "ok_delta_f": abs(delta_f_hz - truth["delta_f_hz"]) <= 300 if truth.get("delta_f_hz") is not None else None,
        "t_block": t_block,
        "phase_raw": phase_block,
        "phase_detrended": phase_detrended,
        "score_rows": score_rows,
    }
    return result


def write_summary_csv(path, results):
    fields = [
        "name",
        "true_kind",
        "true_fm_hz",
        "estimated_fm_hz",
        "true_mf",
        "estimated_mf_corr",
        "estimated_mf_span",
        "true_delta_f_hz",
        "estimated_delta_f_hz",
        "estimated_carrier_offset_hz",
        "ok_f",
        "ok_mf",
        "ok_delta_f",
    ]
    with path.open("w", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()
        for result in results:
            writer.writerow({field: result.get(field) for field in fields})


def write_score_csv(path, results):
    rows = []
    for result in results:
        for score in result["score_rows"]:
            row = {
                "name": result["name"],
                "true_fm_hz": result["true_fm_hz"],
                "estimated_fm_hz": result["estimated_fm_hz"],
                **score,
            }
            rows.append(row)

    with path.open("w", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)


def plot_results(results):
    fig, axes = plt.subplots(2, 1, figsize=(12, 8))

    for result in results:
        if result["true_kind"] != "FM":
            continue
        t_ms = (result["t_block"] - result["t_block"][0]) * 1e3
        axes[0].plot(t_ms, result["phase_detrended"], linewidth=1.0, label=result["name"])
    axes[0].set_title("Detrended FM phase: mf is the sinusoid amplitude")
    axes[0].set_xlabel("time in block (ms)")
    axes[0].set_ylabel("phase (rad)")
    axes[0].grid(True, alpha=0.3)
    axes[0].legend(loc="upper right")

    selected = [result for result in results if result["name"] == "FM mf=3 F=5k offset=200Hz"][0]
    candidates = [row["candidate_hz"] for row in selected["score_rows"]]
    scores = [row["score_norm"] for row in selected["score_rows"]]
    axes[1].bar([str(freq) for freq in candidates], scores, color="#1f77b4")
    axes[1].set_title("Candidate frequency scores for FM mf=3, F=5k")
    axes[1].set_xlabel("candidate frequency (Hz)")
    axes[1].set_ylabel("normalized score")
    axes[1].grid(True, axis="y", alpha=0.3)

    fig.tight_layout()
    png_path = OUT_DIR / "fm_phase_estimator.png"
    fig.savefig(png_path, dpi=150)
    return png_path


def main():
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    num_samples = int(FS * DURATION)
    t = np.arange(num_samples) / FS
    cos_lo, sin_lo = dds_sequence(num_samples)

    cases = [
        (
            "FM mf=1 F=1k",
            fm_signal(t, mf=1.0, fm=1_000),
            {"kind": "FM", "fm_hz": 1_000, "mf": 1.0, "delta_f_hz": 1_000},
        ),
        (
            "FM mf=3 F=5k offset=200Hz",
            fm_signal(t, mf=3.0, fm=5_000, carrier_offset_hz=200),
            {"kind": "FM", "fm_hz": 5_000, "mf": 3.0, "delta_f_hz": 15_000},
        ),
        (
            "FM mf=5 F=2k",
            fm_signal(t, mf=5.0, fm=2_000),
            {"kind": "FM", "fm_hz": 2_000, "mf": 5.0, "delta_f_hz": 10_000},
        ),
        (
            "CW offset=200Hz",
            cw_signal(t, carrier_offset_hz=200),
            {"kind": "CW"},
        ),
    ]

    results = [analyze_signal(name, rf, t, cos_lo, sin_lo, truth) for name, rf, truth in cases]

    summary_csv = OUT_DIR / "fm_phase_estimator_summary.csv"
    score_csv = OUT_DIR / "fm_phase_estimator_scores.csv"
    write_summary_csv(summary_csv, results)
    write_score_csv(score_csv, results)
    png_path = plot_results(results)

    print("FM phase estimator demo")
    print("name,true_F,est_F,true_mf,est_mf,true_df,est_df,offset_est,ok")
    for result in results:
        ok = result["ok_f"] and result["ok_mf"] and result["ok_delta_f"] if result["true_kind"] == "FM" else "-"
        print(
            f"{result['name']},{result['true_fm_hz']},{result['estimated_fm_hz']},"
            f"{result['true_mf']},{result['estimated_mf_corr']:.4f},"
            f"{result['true_delta_f_hz']},{result['estimated_delta_f_hz']:.1f},"
            f"{result['estimated_carrier_offset_hz']:.2f},{ok}"
        )
    print(f"CSV written to: {summary_csv.resolve()}")
    print(f"CSV written to: {score_csv.resolve()}")
    print(f"PNG written to: {png_path.resolve()}")


if __name__ == "__main__":
    main()
