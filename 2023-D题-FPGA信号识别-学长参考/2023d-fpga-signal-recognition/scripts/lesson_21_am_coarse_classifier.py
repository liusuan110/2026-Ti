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
THRESHOLD_PERCENT = 25
DURATION = 0.006
ADC_AMP = 50


OUT_DIR = Path("outputs/lesson_21")


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


def make_signal(t, kind):
    if kind == "AM weak":
        envelope = 1.0 + 0.3 * np.sin(2 * np.pi * 1_000 * t)
        return ADC_AMP * envelope * np.cos(2 * np.pi * FC * t)
    if kind == "AM strong":
        envelope = 1.0 + 0.7 * np.sin(2 * np.pi * 3_000 * t)
        return ADC_AMP * envelope * np.cos(2 * np.pi * FC * t)
    if kind == "FM":
        phase_mod = 3.0 * np.sin(2 * np.pi * 5_000 * t)
        return 80 * np.cos(2 * np.pi * FC * t + phase_mod)
    if kind == "CW":
        return 80 * np.cos(2 * np.pi * FC * t)
    raise ValueError(kind)


def ddc_mag_sq(adc_samples, cos_lo, sin_lo):
    i_raw = adc_samples * cos_lo
    q_raw = adc_samples * (-sin_lo)
    i_baseband, valid_i = moving_average_hw_style(i_raw)
    q_baseband, valid_q = moving_average_hw_style(q_raw)
    valid = valid_i & valid_q
    mag_sq = i_baseband * i_baseband + q_baseband * q_baseband
    return i_baseband, q_baseband, mag_sq, valid


def classify_blocks(mag_sq, valid, kind):
    valid_indices = np.flatnonzero(valid)
    start = int(valid_indices[0]) if len(valid_indices) else 0
    rows = []
    block_index = 0

    while start + BLOCK_LEN <= len(mag_sq):
        block = mag_sq[start:start + BLOCK_LEN]
        block_min = int(np.min(block))
        block_max = int(np.max(block))
        value_range = block_max - block_min
        value_sum = block_max + block_min
        score_percent = 100.0 * value_range / value_sum if value_sum else 0.0
        is_am = value_range * 100 >= value_sum * THRESHOLD_PERCENT

        rows.append({
            "kind": kind,
            "block": block_index,
            "start_n": start,
            "end_n": start + BLOCK_LEN - 1,
            "block_min": block_min,
            "block_max": block_max,
            "range": value_range,
            "sum": value_sum,
            "score_percent": score_percent,
            "threshold_percent": THRESHOLD_PERCENT,
            "decision": "AM" if is_am else "non-AM",
        })

        start += BLOCK_LEN
        block_index += 1

    return rows


def write_csv(path, rows):
    with path.open("w", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)


def plot_cases(case_data, rows):
    fig, axes = plt.subplots(2, 1, figsize=(12, 8), sharex=True)

    for kind, data in case_data.items():
        t_ms = data["t"] * 1e3
        mag_norm = data["mag_sq"] / np.nanmean(data["mag_sq"][data["valid"]])
        axes[0].plot(t_ms, mag_norm, linewidth=0.9, label=kind)

    axes[0].set_title("Normalized mag_sq over time")
    axes[0].set_ylabel("mag_sq / mean")
    axes[0].grid(True, alpha=0.3)
    axes[0].legend(loc="upper right")

    kinds = []
    scores = []
    decisions = []
    for row in rows:
        label = f"{row['kind']} b{row['block']}"
        kinds.append(label)
        scores.append(row["score_percent"])
        decisions.append(row["decision"])

    colors = ["#d62728" if decision == "AM" else "#1f77b4" for decision in decisions]
    axes[1].bar(kinds, scores, color=colors)
    axes[1].axhline(THRESHOLD_PERCENT, color="black", linewidth=1.0, linestyle="--", label="threshold")
    axes[1].set_title("AM coarse score: 100*(max-min)/(max+min)")
    axes[1].set_ylabel("score percent")
    axes[1].set_xlabel("case block")
    axes[1].tick_params(axis="x", rotation=35)
    axes[1].grid(True, axis="y", alpha=0.3)
    axes[1].legend(loc="upper right")

    fig.tight_layout()
    png_path = OUT_DIR / "am_coarse_classifier.png"
    fig.savefig(png_path, dpi=150)
    return png_path


def main():
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    num_samples = int(FS * DURATION)
    t = np.arange(num_samples) / FS
    cos_lo, sin_lo = dds_sequence(num_samples)

    case_data = {}
    all_rows = []
    for kind in ["AM weak", "AM strong", "FM", "CW"]:
        rf = make_signal(t, kind)
        adc = quantize_adc(rf)
        i_baseband, q_baseband, mag_sq, valid = ddc_mag_sq(adc, cos_lo, sin_lo)
        rows = classify_blocks(mag_sq, valid, kind)
        all_rows.extend(rows)
        case_data[kind] = {
            "t": t,
            "adc": adc,
            "i_baseband": i_baseband,
            "q_baseband": q_baseband,
            "mag_sq": mag_sq.astype(float),
            "valid": valid,
        }

    csv_path = OUT_DIR / "am_coarse_classifier_results.csv"
    write_csv(csv_path, all_rows)
    png_path = plot_cases(case_data, all_rows)

    print("AM coarse classifier demo")
    print("kind,block,score_percent,decision")
    for row in all_rows:
        print(f"{row['kind']},{row['block']},{row['score_percent']:.2f},{row['decision']}")
    print(f"CSV written to: {csv_path.resolve()}")
    print(f"PNG written to: {png_path.resolve()}")


if __name__ == "__main__":
    main()
