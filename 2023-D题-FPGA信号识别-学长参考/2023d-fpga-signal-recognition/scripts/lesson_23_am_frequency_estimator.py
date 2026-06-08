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
BLOCK_LEN = 40_000
DURATION = 0.006
ADC_AMP = 45
CANDIDATE_FREQS = np.array([1_000, 2_000, 3_000, 4_000, 5_000])


OUT_DIR = Path("outputs/lesson_23")


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


def am_signal(t, ma, fm):
    envelope = 1.0 + ma * np.sin(2 * np.pi * fm * t)
    return ADC_AMP * envelope * np.cos(2 * np.pi * FC * t)


def ddc_mag_sq(adc_samples, cos_lo, sin_lo):
    i_raw = adc_samples * cos_lo
    q_raw = adc_samples * (-sin_lo)
    i_baseband, valid_i = moving_average_hw_style(i_raw)
    q_baseband, valid_q = moving_average_hw_style(q_raw)
    valid = valid_i & valid_q
    mag_sq = i_baseband * i_baseband + q_baseband * q_baseband
    return mag_sq, valid


def candidate_scores(feature, t):
    feature_ac = feature - np.mean(feature)
    rows = []
    scores = []

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
        scores.append(power)

    scores = np.array(scores)
    for row in rows:
        row["score_norm"] = row["power"] / np.max(scores) if np.max(scores) else 0.0

    best = rows[int(np.argmax(scores))]
    return rows, int(best["candidate_hz"])


def analyze_case(ma, fm, t, cos_lo, sin_lo):
    adc = quantize_adc(am_signal(t, ma, fm))
    mag_sq, valid = ddc_mag_sq(adc, cos_lo, sin_lo)

    valid_indices = np.flatnonzero(valid)
    start = int(valid_indices[0])
    stop = start + BLOCK_LEN
    feature = mag_sq[start:stop].astype(float)
    t_block = t[start:stop]

    score_rows, best_freq = candidate_scores(feature, t_block)
    for row in score_rows:
        row.update({
            "true_ma": ma,
            "true_fm_hz": fm,
            "estimated_fm_hz": best_freq,
            "ok": best_freq == fm,
        })

    return score_rows, {
        "true_ma": ma,
        "true_fm_hz": fm,
        "estimated_fm_hz": best_freq,
        "ok": best_freq == fm,
        "feature": feature,
        "t_block": t_block,
    }


def write_csv(path, rows):
    with path.open("w", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)


def plot_scores(score_rows):
    fig, axes = plt.subplots(1, 2, figsize=(12, 5))

    ma_values = sorted(set(row["true_ma"] for row in score_rows))
    width = 0.25
    x = np.arange(len(CANDIDATE_FREQS))

    for offset, ma in enumerate(ma_values):
        selected = [row for row in score_rows if row["true_ma"] == ma and row["true_fm_hz"] == 3_000]
        selected = sorted(selected, key=lambda row: row["candidate_hz"])
        axes[0].bar(
            x + (offset - 1) * width,
            [row["score_norm"] for row in selected],
            width=width,
            label=f"ma={ma}",
        )
    axes[0].set_xticks(x)
    axes[0].set_xticklabels([str(freq) for freq in CANDIDATE_FREQS])
    axes[0].set_title("Candidate scores when true F = 3 kHz")
    axes[0].set_xlabel("candidate frequency (Hz)")
    axes[0].set_ylabel("normalized score")
    axes[0].grid(True, axis="y", alpha=0.3)
    axes[0].legend(loc="upper right")

    heat = np.zeros((len(CANDIDATE_FREQS), len(CANDIDATE_FREQS)))
    for true_index, true_freq in enumerate(CANDIDATE_FREQS):
        selected = [
            row for row in score_rows
            if row["true_ma"] == 0.6 and row["true_fm_hz"] == int(true_freq)
        ]
        selected = sorted(selected, key=lambda row: row["candidate_hz"])
        heat[true_index, :] = [row["score_norm"] for row in selected]

    im = axes[1].imshow(heat, aspect="auto", vmin=0, vmax=1, cmap="viridis")
    axes[1].set_xticks(np.arange(len(CANDIDATE_FREQS)))
    axes[1].set_xticklabels([str(freq) for freq in CANDIDATE_FREQS])
    axes[1].set_yticks(np.arange(len(CANDIDATE_FREQS)))
    axes[1].set_yticklabels([str(freq) for freq in CANDIDATE_FREQS])
    axes[1].set_title("Score heatmap for ma=0.6")
    axes[1].set_xlabel("candidate frequency (Hz)")
    axes[1].set_ylabel("true frequency (Hz)")
    fig.colorbar(im, ax=axes[1], label="normalized score")

    fig.tight_layout()
    png_path = OUT_DIR / "am_frequency_scores.png"
    fig.savefig(png_path, dpi=150)
    return png_path


def plot_feature(example):
    t_ms = (example["t_block"] - example["t_block"][0]) * 1e3
    feature = example["feature"]
    feature_norm = (feature - np.mean(feature)) / np.max(np.abs(feature - np.mean(feature)))

    fig, ax = plt.subplots(figsize=(12, 4))
    ax.plot(t_ms, feature_norm, linewidth=1.0)
    ax.set_title(
        f"AC part of mag_sq, true F={example['true_fm_hz']} Hz, "
        f"estimated F={example['estimated_fm_hz']} Hz"
    )
    ax.set_xlabel("time in block (ms)")
    ax.set_ylabel("normalized feature")
    ax.grid(True, alpha=0.3)
    fig.tight_layout()
    png_path = OUT_DIR / "am_frequency_feature_example.png"
    fig.savefig(png_path, dpi=150)
    return png_path


def main():
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    num_samples = int(FS * DURATION)
    t = np.arange(num_samples) / FS
    cos_lo, sin_lo = dds_sequence(num_samples)

    score_rows = []
    summary_rows = []
    example = None
    for ma in [0.3, 0.6, 0.9]:
        for fm in CANDIDATE_FREQS:
            rows, summary = analyze_case(ma, int(fm), t, cos_lo, sin_lo)
            score_rows.extend(rows)
            summary_rows.append({
                "true_ma": ma,
                "true_fm_hz": int(fm),
                "estimated_fm_hz": summary["estimated_fm_hz"],
                "ok": summary["ok"],
            })
            if ma == 0.6 and fm == 3_000:
                example = summary

    score_csv = OUT_DIR / "am_frequency_scores.csv"
    summary_csv = OUT_DIR / "am_frequency_summary.csv"
    write_csv(score_csv, score_rows)
    write_csv(summary_csv, summary_rows)

    scores_png = plot_scores(score_rows)
    feature_png = plot_feature(example)

    print("AM frequency estimator demo")
    print("true_ma,true_fm_hz,estimated_fm_hz,ok")
    for row in summary_rows:
        print(f"{row['true_ma']},{row['true_fm_hz']},{row['estimated_fm_hz']},{row['ok']}")
    ok_count = sum(row["ok"] for row in summary_rows)
    print(f"passed {ok_count}/{len(summary_rows)} cases")
    print(f"CSV written to: {score_csv.resolve()}")
    print(f"CSV written to: {summary_csv.resolve()}")
    print(f"PNG written to: {scores_png.resolve()}")
    print(f"PNG written to: {feature_png.resolve()}")


if __name__ == "__main__":
    main()
