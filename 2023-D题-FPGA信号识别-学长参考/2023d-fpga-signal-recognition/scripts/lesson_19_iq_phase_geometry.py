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
NUM_SAMPLES = 200


OUT_DIR = Path("outputs/lesson_19")


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
    cos_values = []
    sin_values = []
    for _ in range(num_samples):
        lut_addr = phase >> (PHASE_BITS - LUT_ADDR_BITS)
        cos_values.append(int(cos_lut[lut_addr]))
        sin_values.append(int(sin_lut[lut_addr]))
        phase = (phase + phase_step) % phase_modulus

    return np.array(cos_values), np.array(sin_values)


def adc_sample_sequence(phase_deg):
    t = np.arange(NUM_SAMPLES) / FS
    phase_rad = np.deg2rad(phase_deg)
    sample = np.round(80 * np.cos(2 * np.pi * FC * t + phase_rad)).astype(int)
    return np.clip(sample, -(1 << (SAMPLE_BITS - 1)), (1 << (SAMPLE_BITS - 1)) - 1)


def moving_average_hw_style(x):
    delay = [0] * WINDOW_LEN
    running_sum = 0
    avg = []
    valid = []

    for n, value in enumerate(x):
        oldest = delay[-1]
        delay = [int(value)] + delay[:-1]
        running_sum = running_sum + int(value) - int(oldest)
        avg.append(running_sum >> SHIFT_BITS)
        valid.append(n >= WINDOW_LEN - 1)

    return np.array(avg), np.array(valid)


def analyze_phase_case(phase_deg):
    sample = adc_sample_sequence(phase_deg)
    cos_in, sin_in = dds_sequence(NUM_SAMPLES)

    i_raw = sample * cos_in
    q_raw = sample * (-sin_in)
    i_avg, valid_i = moving_average_hw_style(i_raw)
    q_avg, valid_q = moving_average_hw_style(q_raw)
    valid = valid_i & valid_q

    # Skip a little more than the fill time so the simple moving average edge
    # transient does not dominate the teaching numbers.
    stable = valid.copy()
    stable[:40] = False

    mean_i = float(np.mean(i_avg[stable]))
    mean_q = float(np.mean(q_avg[stable]))
    mag = float(np.sqrt(mean_i * mean_i + mean_q * mean_q))
    phase_est_deg = float(np.rad2deg(np.arctan2(mean_q, mean_i)))

    return {
        "phase_deg": phase_deg,
        "sample": sample,
        "i_raw": i_raw,
        "q_raw": q_raw,
        "i_avg": i_avg,
        "q_avg": q_avg,
        "valid": valid,
        "stable": stable,
        "mean_i": mean_i,
        "mean_q": mean_q,
        "mag": mag,
        "phase_est_deg": phase_est_deg,
    }


def write_summary_csv(results):
    csv_path = OUT_DIR / "iq_phase_summary.csv"
    with csv_path.open("w", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(
            f,
            fieldnames=["phase_deg", "mean_i", "mean_q", "magnitude", "phase_est_deg"],
        )
        writer.writeheader()
        for item in results:
            writer.writerow({
                "phase_deg": item["phase_deg"],
                "mean_i": item["mean_i"],
                "mean_q": item["mean_q"],
                "magnitude": item["mag"],
                "phase_est_deg": item["phase_est_deg"],
            })
    return csv_path


def plot_results(results):
    fig, axes = plt.subplots(1, 2, figsize=(12, 5))

    n = np.arange(NUM_SAMPLES)
    for item in results:
        axes[0].plot(
            n,
            item["i_avg"],
            linewidth=1.0,
            label=f"I, phase={item['phase_deg']} deg",
        )
        axes[0].plot(
            n,
            item["q_avg"],
            linewidth=1.0,
            linestyle="--",
            label=f"Q, phase={item['phase_deg']} deg",
        )
    axes[0].axvline(40, color="black", linewidth=1.0, alpha=0.4)
    axes[0].set_title("DDC low-pass I/Q for different input phases")
    axes[0].set_xlabel("sample index")
    axes[0].set_ylabel("baseband code")
    axes[0].grid(True, alpha=0.3)
    axes[0].legend(loc="upper right", fontsize=8)

    for item in results:
        axes[1].scatter(item["mean_i"], item["mean_q"], s=70, label=f"{item['phase_deg']} deg")
        axes[1].plot([0, item["mean_i"]], [0, item["mean_q"]], linewidth=1.0)
    axes[1].axhline(0, color="black", linewidth=0.8)
    axes[1].axvline(0, color="black", linewidth=0.8)
    axes[1].set_aspect("equal", adjustable="box")
    axes[1].set_title("I/Q point: magnitude and phase as coordinates")
    axes[1].set_xlabel("I")
    axes[1].set_ylabel("Q")
    axes[1].grid(True, alpha=0.3)
    axes[1].legend(loc="upper right")

    fig.tight_layout()
    png_path = OUT_DIR / "iq_phase_geometry.png"
    fig.savefig(png_path, dpi=150)
    return png_path


def main():
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    phase_cases = [0, 30, 90, -60, 150]
    results = [analyze_phase_case(phase) for phase in phase_cases]
    csv_path = write_summary_csv(results)
    png_path = plot_results(results)

    print("I/Q phase geometry demo")
    print("phase_deg,mean_i,mean_q,magnitude,phase_est_deg")
    for item in results:
        print(
            f"{item['phase_deg']},{item['mean_i']:.2f},{item['mean_q']:.2f},"
            f"{item['mag']:.2f},{item['phase_est_deg']:.2f}"
        )
    print(f"CSV written to: {csv_path.resolve()}")
    print(f"PNG written to: {png_path.resolve()}")


if __name__ == "__main__":
    main()
