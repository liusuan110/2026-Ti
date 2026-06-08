from pathlib import Path
import csv

import numpy as np
import matplotlib.pyplot as plt


OUT_DIR = Path("outputs/lesson_20")
SCALE = 82_000


def compute_features(i_value, q_value):
    abs_i = abs(int(i_value))
    abs_q = abs(int(q_value))
    max_abs = max(abs_i, abs_q)
    min_abs = min(abs_i, abs_q)

    exact = float(np.sqrt(abs_i * abs_i + abs_q * abs_q))
    abs_sum = abs_i + abs_q
    max_half_min = max_abs + (min_abs >> 1)
    mag_sq = abs_i * abs_i + abs_q * abs_q

    return {
        "abs_i": abs_i,
        "abs_q": abs_q,
        "exact_mag": exact,
        "abs_sum": abs_sum,
        "max_half_min": max_half_min,
        "mag_sq": mag_sq,
    }


def angle_sweep_rows():
    rows = []
    for deg in range(0, 360):
        rad = np.deg2rad(deg)
        i_value = int(round(SCALE * np.cos(rad)))
        q_value = int(round(SCALE * np.sin(rad)))
        features = compute_features(i_value, q_value)
        rows.append({
            "phase_deg": deg,
            "i": i_value,
            "q": q_value,
            **features,
            "abs_sum_ratio": features["abs_sum"] / features["exact_mag"],
            "max_half_min_ratio": features["max_half_min"] / features["exact_mag"],
            "mag_sq_ratio": features["mag_sq"] / (SCALE * SCALE),
        })
    return rows


def am_rows():
    phases = [0, 30, 75, 120]
    envelopes = np.linspace(0.4, 1.6, 13)
    rows = []
    for phase_deg in phases:
        rad = np.deg2rad(phase_deg)
        for envelope in envelopes:
            amp = SCALE * envelope
            i_value = int(round(amp * np.cos(rad)))
            q_value = int(round(amp * np.sin(rad)))
            features = compute_features(i_value, q_value)
            rows.append({
                "phase_deg": phase_deg,
                "envelope": envelope,
                "i": i_value,
                "q": q_value,
                **features,
                "exact_norm": features["exact_mag"] / SCALE,
                "approx_norm": features["max_half_min"] / SCALE,
                "mag_sq_norm": features["mag_sq"] / (SCALE * SCALE),
            })
    return rows


def write_csv(path, rows):
    with path.open("w", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)


def plot_angle_sweep(rows):
    phase = np.array([row["phase_deg"] for row in rows])
    abs_sum_ratio = np.array([row["abs_sum_ratio"] for row in rows])
    max_half_min_ratio = np.array([row["max_half_min_ratio"] for row in rows])
    mag_sq_ratio = np.array([row["mag_sq_ratio"] for row in rows])

    fig, axes = plt.subplots(2, 1, figsize=(12, 7), sharex=True)
    axes[0].plot(phase, abs_sum_ratio, label="(|I|+|Q|) / exact")
    axes[0].plot(phase, max_half_min_ratio, label="(max+min/2) / exact")
    axes[0].axhline(1.0, color="black", linewidth=0.8)
    axes[0].set_title("Magnitude approximations vs I/Q phase angle")
    axes[0].set_ylabel("ratio to exact magnitude")
    axes[0].grid(True, alpha=0.3)
    axes[0].legend(loc="upper right")

    axes[1].plot(phase, mag_sq_ratio, color="#2ca02c", label="(I^2+Q^2) / SCALE^2")
    axes[1].axhline(1.0, color="black", linewidth=0.8)
    axes[1].set_title("Magnitude squared is angle-independent for a fixed radius")
    axes[1].set_xlabel("phase angle (deg)")
    axes[1].set_ylabel("normalized mag_sq")
    axes[1].grid(True, alpha=0.3)
    axes[1].legend(loc="upper right")

    fig.tight_layout()
    png_path = OUT_DIR / "magnitude_angle_error.png"
    fig.savefig(png_path, dpi=150)
    return png_path


def plot_am_rows(rows):
    fig, axes = plt.subplots(1, 2, figsize=(12, 5))

    for phase_deg in sorted(set(row["phase_deg"] for row in rows)):
        selected = [row for row in rows if row["phase_deg"] == phase_deg]
        envelope = np.array([row["envelope"] for row in selected])
        exact_norm = np.array([row["exact_norm"] for row in selected])
        approx_norm = np.array([row["approx_norm"] for row in selected])
        mag_sq_norm = np.array([row["mag_sq_norm"] for row in selected])

        axes[0].plot(envelope, approx_norm, marker="o", label=f"phase={phase_deg} deg")
        axes[1].plot(envelope, mag_sq_norm, marker="o", label=f"phase={phase_deg} deg")

    axes[0].plot([0.4, 1.6], [0.4, 1.6], color="black", linewidth=0.8, linestyle="--")
    axes[0].set_title("max+min/2 tracks AM envelope with angle-dependent gain")
    axes[0].set_xlabel("true envelope")
    axes[0].set_ylabel("approx magnitude / SCALE")
    axes[0].grid(True, alpha=0.3)
    axes[0].legend(loc="upper left", fontsize=8)

    axes[1].set_title("I^2+Q^2 tracks envelope squared")
    axes[1].set_xlabel("true envelope")
    axes[1].set_ylabel("mag_sq / SCALE^2")
    axes[1].grid(True, alpha=0.3)
    axes[1].legend(loc="upper left", fontsize=8)

    fig.tight_layout()
    png_path = OUT_DIR / "magnitude_am_tracking.png"
    fig.savefig(png_path, dpi=150)
    return png_path


def main():
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    sweep = angle_sweep_rows()
    am = am_rows()

    sweep_csv = OUT_DIR / "magnitude_angle_sweep.csv"
    am_csv = OUT_DIR / "magnitude_am_tracking.csv"
    write_csv(sweep_csv, sweep)
    write_csv(am_csv, am)

    angle_png = plot_angle_sweep(sweep)
    am_png = plot_am_rows(am)

    max_abs_sum_err = max(abs(row["abs_sum_ratio"] - 1.0) for row in sweep)
    max_approx_err = max(abs(row["max_half_min_ratio"] - 1.0) for row in sweep)
    mag_sq_span = max(row["mag_sq_ratio"] for row in sweep) - min(row["mag_sq_ratio"] for row in sweep)

    print("Magnitude feature demo")
    print(f"max |abs_sum/exact - 1| = {max_abs_sum_err:.4f}")
    print(f"max |max_half_min/exact - 1| = {max_approx_err:.4f}")
    print(f"mag_sq normalized span over angle = {mag_sq_span:.8f}")
    print()
    print("Example points:")
    print("phase_deg,I,Q,exact_mag,abs_sum,max_half_min,mag_sq")
    for deg in [0, 30, 45, 90, 135]:
        row = sweep[deg]
        print(
            f"{row['phase_deg']},{row['i']},{row['q']},{row['exact_mag']:.2f},"
            f"{row['abs_sum']},{row['max_half_min']},{row['mag_sq']}"
        )
    print(f"CSV written to: {sweep_csv.resolve()}")
    print(f"CSV written to: {am_csv.resolve()}")
    print(f"PNG written to: {angle_png.resolve()}")
    print(f"PNG written to: {am_png.resolve()}")


if __name__ == "__main__":
    main()
