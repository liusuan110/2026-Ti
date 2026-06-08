from pathlib import Path
import csv

import numpy as np
import matplotlib.pyplot as plt


ROOT = Path(__file__).resolve().parents[1]
SIM_DIR = ROOT / "sim"
OUT_DIR = ROOT / "outputs" / "lesson_32"
LESSON31_CSV = ROOT / "outputs" / "lesson_31" / "ddc_phase_core_pipeline_expected.csv"

ANGLE_BITS = 24
UNWRAPPED_BITS = 40
ANGLE_FRAC_BITS = 20
ANGLE_SCALE = 1 << ANGLE_FRAC_BITS
PI_Q = int(round(np.pi * ANGLE_SCALE))
TWO_PI_Q = 2 * PI_Q

SYNTHETIC_SAMPLES = 256
SYNTHETIC_FS = 20_000
SYNTHETIC_OFFSET_HZ = 1_500
SYNTHETIC_TONE_HZ = 1_000
SYNTHETIC_MF = 0.8

LESSON31_FS = 20_000_000


def signed_to_twos_complement(value, bits):
    if value < 0:
        value = (1 << bits) + value
    return value


def wrap_pi_float(x):
    return (x + np.pi) % (2 * np.pi) - np.pi


def wrap_pi_q(value):
    while value > PI_Q:
        value -= TWO_PI_Q
    while value < -PI_Q:
        value += TWO_PI_Q
    return value


def make_synthetic_phase_rows():
    n = np.arange(SYNTHETIC_SAMPLES)
    t = n / SYNTHETIC_FS
    true_phase = (
        0.4
        + 2 * np.pi * SYNTHETIC_OFFSET_HZ * t
        + SYNTHETIC_MF * np.sin(2 * np.pi * SYNTHETIC_TONE_HZ * t)
    )
    wrapped_phase = wrap_pi_float(true_phase)
    wrapped_q = np.round(wrapped_phase * ANGLE_SCALE).astype(int)

    offset = 0
    prev = 0
    have_prev = False
    rows = []

    for index, phase_q in enumerate(wrapped_q):
        phase_q = wrap_pi_q(int(phase_q))
        if have_prev:
            delta = phase_q - prev
            if delta < -PI_Q:
                offset += TWO_PI_Q
            elif delta > PI_Q:
                offset -= TWO_PI_Q

        unwrapped_q = phase_q + offset
        rows.append({
            "n": index,
            "phase_wrapped_q4_20": phase_q,
            "phase_unwrapped_q4_20": unwrapped_q,
            "wrap_offset_q4_20": offset,
            "phase_wrapped_rad": phase_q / ANGLE_SCALE,
            "phase_unwrapped_rad": unwrapped_q / ANGLE_SCALE,
        })
        prev = phase_q
        have_prev = True

    return rows


def write_mem(path, rows):
    with path.open("w", encoding="utf-8") as f:
        for row in rows:
            code = signed_to_twos_complement(row["phase_wrapped_q4_20"], ANGLE_BITS)
            f.write(f"{code:0{(ANGLE_BITS + 3) // 4}X}\n")


def write_csv(path, rows, fieldnames=None):
    if fieldnames is None:
        fieldnames = list(rows[0].keys())
    with path.open("w", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def read_sim_csv(path):
    rows = []
    with path.open("r", encoding="utf-8", newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            rows.append({
                "n": int(row["n"]),
                "phase_wrapped_q4_20": int(row["phase_wrapped_q4_20"]),
                "phase_unwrapped_q4_20": int(row["phase_unwrapped_q4_20"]),
                "wrap_offset_q4_20": int(row["wrap_offset_q4_20"]),
            })
    return rows


def compare_sim_with_expected(expected_rows):
    sim_path = OUT_DIR / "phase_unwrap_sim.csv"
    if not sim_path.exists():
        print("No Vivado/XSim CSV found yet.")
        print(f"Expected simulation output path: {sim_path}")
        return

    expected_by_n = {row["n"]: row for row in expected_rows}
    sim_rows = read_sim_csv(sim_path)
    mismatches = []

    for actual in sim_rows:
        expected = expected_by_n[actual["n"]]
        for key in ["phase_wrapped_q4_20", "phase_unwrapped_q4_20", "wrap_offset_q4_20"]:
            if actual[key] != expected[key]:
                mismatches.append((actual["n"], key, expected[key], actual[key]))

    if mismatches:
        print(f"Found {len(mismatches)} phase_unwrap mismatches.")
        for item in mismatches[:10]:
            print(f"n={item[0]} {item[1]} expected={item[2]} actual={item[3]}")
    else:
        print(f"Vivado/XSim output matches expected phase_unwrap output for {len(sim_rows)} samples.")


def read_lesson31_phase():
    rows = []
    with LESSON31_CSV.open("r", encoding="utf-8", newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            rows.append({
                "phase_sample_index": int(row["phase_sample_index"]),
                "phase_q4_20": int(row["cordic_phase_q4_20"]),
            })
    return rows


def analyze_lesson31_phase():
    rows = read_lesson31_phase()
    index = np.array([row["phase_sample_index"] for row in rows], dtype=float)
    raw_phase = np.array([row["phase_q4_20"] / ANGLE_SCALE for row in rows])
    unwrapped = np.unwrap(raw_phase)
    t = index / LESSON31_FS

    slope, intercept = np.polyfit(t, unwrapped, 1)
    fitted = slope * t + intercept
    detrended = unwrapped - fitted
    estimated_offset_hz = slope / (2 * np.pi)

    out_rows = []
    for n in range(len(rows)):
        out_rows.append({
            "phase_sample_index": int(index[n]),
            "raw_phase_rad": raw_phase[n],
            "unwrapped_phase_rad": unwrapped[n],
            "fitted_line_rad": fitted[n],
            "detrended_phase_rad": detrended[n],
        })

    return {
        "rows": out_rows,
        "estimated_offset_hz": estimated_offset_hz,
        "raw_span_rad": float(np.max(unwrapped) - np.min(unwrapped)),
        "detrended_span_rad": float(np.max(detrended) - np.min(detrended)),
        "detrended_std_rad": float(np.std(detrended)),
        "index": index,
        "raw_phase": raw_phase,
        "unwrapped": unwrapped,
        "fitted": fitted,
        "detrended": detrended,
    }


def plot_results(synthetic_rows, lesson31):
    n = np.array([row["n"] for row in synthetic_rows])
    wrapped = np.array([row["phase_wrapped_rad"] for row in synthetic_rows])
    unwrapped = np.array([row["phase_unwrapped_rad"] for row in synthetic_rows])

    fig, axes = plt.subplots(3, 1, figsize=(12, 9))

    axes[0].plot(n, wrapped, label="wrapped phase")
    axes[0].plot(n, unwrapped, label="unwrapped phase")
    axes[0].set_title("Synthetic phase unwrap test")
    axes[0].set_ylabel("phase (rad)")
    axes[0].grid(True, alpha=0.3)
    axes[0].legend(loc="upper left")

    axes[1].plot(lesson31["index"], lesson31["unwrapped"], linewidth=1.0, label="unwrapped phase")
    axes[1].plot(lesson31["index"], lesson31["fitted"], linewidth=1.0, label="fitted line")
    axes[1].set_title("Lesson 31 phase slope fit")
    axes[1].set_ylabel("phase (rad)")
    axes[1].grid(True, alpha=0.3)
    axes[1].legend(loc="upper left")

    axes[2].plot(lesson31["index"], lesson31["detrended"], linewidth=1.0)
    axes[2].set_title("Lesson 31 phase after detrend")
    axes[2].set_xlabel("DDC valid-sample index")
    axes[2].set_ylabel("phase residual (rad)")
    axes[2].grid(True, alpha=0.3)

    fig.tight_layout()
    png_path = OUT_DIR / "phase_unwrap_detrend.png"
    fig.savefig(png_path, dpi=150)
    return png_path


def main():
    SIM_DIR.mkdir(parents=True, exist_ok=True)
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    synthetic_rows = make_synthetic_phase_rows()
    write_mem(SIM_DIR / "phase_unwrap_input_256.mem", synthetic_rows)
    expected_csv = OUT_DIR / "phase_unwrap_expected.csv"
    write_csv(expected_csv, synthetic_rows)

    lesson31 = analyze_lesson31_phase()
    lesson31_csv = OUT_DIR / "lesson31_phase_detrend.csv"
    write_csv(lesson31_csv, lesson31["rows"])

    plot_path = plot_results(synthetic_rows, lesson31)

    print("Lesson 32 phase unwrap and detrend assets prepared")
    print(f"synthetic phase samples = {len(synthetic_rows)}")
    print(f"synthetic offset = {SYNTHETIC_OFFSET_HZ} Hz")
    print(f"synthetic tone = {SYNTHETIC_TONE_HZ} Hz")
    print(f"phase unwrap mem: {(SIM_DIR / 'phase_unwrap_input_256.mem').resolve()}")
    print(f"phase unwrap expected csv: {expected_csv.resolve()}")
    print(f"lesson31 detrend csv: {lesson31_csv.resolve()}")
    print(f"plot: {plot_path.resolve()}")
    print()
    print("Lesson 31 phase detrend result:")
    print(f"estimated_carrier_offset_hz = {lesson31['estimated_offset_hz']:.3f}")
    print(f"unwrapped_phase_span_rad = {lesson31['raw_span_rad']:.6f}")
    print(f"detrended_phase_span_rad = {lesson31['detrended_span_rad']:.6f}")
    print(f"detrended_phase_std_rad = {lesson31['detrended_std_rad']:.6f}")
    print()
    print("First 10 synthetic unwrap expected samples:")
    print("n,wrapped_q4_20,unwrapped_q4_20,offset_q4_20")
    for row in synthetic_rows[:10]:
        print(
            f"{row['n']},{row['phase_wrapped_q4_20']},"
            f"{row['phase_unwrapped_q4_20']},{row['wrap_offset_q4_20']}"
        )
    print()
    compare_sim_with_expected(synthetic_rows)


if __name__ == "__main__":
    main()
