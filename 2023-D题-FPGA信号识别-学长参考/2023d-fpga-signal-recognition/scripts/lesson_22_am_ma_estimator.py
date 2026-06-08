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
BLOCK_LEN = 32_768
DURATION = 0.008
ADC_AMP = 45


OUT_DIR = Path("outputs/lesson_22")


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


def estimate_ma_from_block(block):
    block_min = int(np.min(block))
    block_max = int(np.max(block))
    score = (block_max - block_min) / (block_max + block_min)

    sqrt_max = np.sqrt(block_max)
    sqrt_min = np.sqrt(block_min)
    ma_sqrt = (sqrt_max - sqrt_min) / (sqrt_max + sqrt_min)

    if score <= 0:
        ma_score = 0.0
    else:
        # score = 2*ma/(1+ma^2), choose the root in 0..1.
        ma_score = (1.0 - np.sqrt(max(0.0, 1.0 - score * score))) / score

    ma_lut = estimate_ma_lut(score)

    return {
        "block_min": block_min,
        "block_max": block_max,
        "score": score,
        "ma_naive": score,
        "ma_sqrt": ma_sqrt,
        "ma_score_formula": ma_score,
        "ma_lut": ma_lut,
    }


def estimate_ma_lut(score):
    score_permille = score * 1000.0
    if score_permille < 623:
        return 0.3
    if score_permille < 748:
        return 0.4
    if score_permille < 846:
        return 0.5
    if score_permille < 914:
        return 0.6
    if score_permille < 960:
        return 0.7
    if score_permille < 986:
        return 0.8
    return 0.9


def analyze_case(ma, fm, t, cos_lo, sin_lo):
    adc = quantize_adc(am_signal(t, ma, fm))
    mag_sq, valid = ddc_mag_sq(adc, cos_lo, sin_lo)
    valid_indices = np.flatnonzero(valid)
    start = int(valid_indices[0])
    rows = []
    block_index = 0

    while start + BLOCK_LEN <= len(mag_sq):
        block = mag_sq[start:start + BLOCK_LEN]
        est = estimate_ma_from_block(block)
        rows.append({
            "true_ma": ma,
            "fm_hz": fm,
            "block": block_index,
            "start_n": start,
            "end_n": start + BLOCK_LEN - 1,
            **est,
            "err_naive": est["ma_naive"] - ma,
            "err_sqrt": est["ma_sqrt"] - ma,
            "err_score_formula": est["ma_score_formula"] - ma,
            "err_lut": est["ma_lut"] - ma,
        })
        start += BLOCK_LEN
        block_index += 1

    return rows


def write_csv(path, rows):
    with path.open("w", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)


def plot_rows(rows):
    fig, axes = plt.subplots(1, 2, figsize=(12, 5))

    grouped = {}
    for row in rows:
        grouped.setdefault(row["true_ma"], []).append(row)

    true_values = []
    naive = []
    sqrt_est = []
    score_est = []
    lut_est = []

    for ma in sorted(grouped):
        selected = grouped[ma]
        true_values.append(ma)
        naive.append(np.mean([row["ma_naive"] for row in selected]))
        sqrt_est.append(np.mean([row["ma_sqrt"] for row in selected]))
        score_est.append(np.mean([row["ma_score_formula"] for row in selected]))
        lut_est.append(np.mean([row["ma_lut"] for row in selected]))

    axes[0].plot(true_values, true_values, color="black", linestyle="--", label="ideal")
    axes[0].plot(true_values, naive, marker="o", label="naive score")
    axes[0].plot(true_values, sqrt_est, marker="o", label="sqrt max/min")
    axes[0].plot(true_values, score_est, marker="o", label="score formula")
    axes[0].plot(true_values, lut_est, marker="o", label="LUT nearest 0.1")
    axes[0].set_title("AM ma estimation from mag_sq max/min")
    axes[0].set_xlabel("true ma")
    axes[0].set_ylabel("estimated ma")
    axes[0].grid(True, alpha=0.3)
    axes[0].legend(loc="upper left", fontsize=8)

    axes[1].plot(true_values, np.array(naive) - np.array(true_values), marker="o", label="naive score")
    axes[1].plot(true_values, np.array(sqrt_est) - np.array(true_values), marker="o", label="sqrt max/min")
    axes[1].plot(true_values, np.array(score_est) - np.array(true_values), marker="o", label="score formula")
    axes[1].plot(true_values, np.array(lut_est) - np.array(true_values), marker="o", label="LUT nearest 0.1")
    axes[1].axhline(0.1, color="black", linewidth=0.8, linestyle="--")
    axes[1].axhline(-0.1, color="black", linewidth=0.8, linestyle="--")
    axes[1].set_title("Estimation error")
    axes[1].set_xlabel("true ma")
    axes[1].set_ylabel("estimated - true")
    axes[1].grid(True, alpha=0.3)
    axes[1].legend(loc="upper left", fontsize=8)

    fig.tight_layout()
    png_path = OUT_DIR / "am_ma_estimator.png"
    fig.savefig(png_path, dpi=150)
    return png_path


def main():
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    num_samples = int(FS * DURATION)
    t = np.arange(num_samples) / FS
    cos_lo, sin_lo = dds_sequence(num_samples)

    rows = []
    for ma in [0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9]:
        rows.extend(analyze_case(ma, 1_000, t, cos_lo, sin_lo))
        rows.extend(analyze_case(ma, 3_000, t, cos_lo, sin_lo))

    csv_path = OUT_DIR / "am_ma_estimator_results.csv"
    write_csv(csv_path, rows)
    png_path = plot_rows(rows)

    print("AM ma estimator demo")
    print("true_ma,mean_naive,mean_sqrt,mean_score_formula,mean_lut,max_abs_lut_err")
    for ma in sorted(set(row["true_ma"] for row in rows)):
        selected = [row for row in rows if row["true_ma"] == ma]
        print(
            f"{ma:.1f},"
            f"{np.mean([row['ma_naive'] for row in selected]):.4f},"
            f"{np.mean([row['ma_sqrt'] for row in selected]):.4f},"
            f"{np.mean([row['ma_score_formula'] for row in selected]):.4f},"
            f"{np.mean([row['ma_lut'] for row in selected]):.4f},"
            f"{max(abs(row['err_lut']) for row in selected):.4f}"
        )
    print(f"CSV written to: {csv_path.resolve()}")
    print(f"PNG written to: {png_path.resolve()}")


if __name__ == "__main__":
    main()
