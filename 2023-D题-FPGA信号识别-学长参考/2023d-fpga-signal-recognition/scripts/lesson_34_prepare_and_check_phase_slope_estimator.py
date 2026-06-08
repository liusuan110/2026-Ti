from pathlib import Path
import csv

import numpy as np
import matplotlib.pyplot as plt


ROOT = Path(__file__).resolve().parents[1]
SIM_DIR = ROOT / "sim"
OUT_DIR = ROOT / "outputs" / "lesson_34"
LESSON33_CSV = ROOT / "outputs" / "lesson_33" / "ddc_phase_unwrap_core_expected.csv"

PHASE_BITS = 40
ANGLE_FRAC_BITS = 20
ANGLE_SCALE = 1 << ANGLE_FRAC_BITS
FS = 20_000_000
WINDOW_SHIFT = 13
WINDOW_SAMPLES = (1 << WINDOW_SHIFT) + 1


def signed_to_twos_complement(value, bits):
    if value < 0:
        value = (1 << bits) + value
    return value


def read_phase_rows():
    rows = []
    with LESSON33_CSV.open("r", encoding="utf-8", newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            rows.append({
                "phase_sample_index": int(row["phase_sample_index"]),
                "phase_unwrapped_q4_20": int(row["phase_unwrapped_q4_20"]),
            })
    return rows


def write_phase_mem(path, rows):
    with path.open("w", encoding="utf-8") as f:
        for row in rows:
            code = signed_to_twos_complement(row["phase_unwrapped_q4_20"], PHASE_BITS)
            f.write(f"{code:0{(PHASE_BITS + 3) // 4}X}\n")


def make_expected(rows):
    expected = []
    block_index = 0
    start = 0
    while start + WINDOW_SAMPLES <= len(rows):
        stop = start + WINDOW_SAMPLES - 1
        first_phase = rows[start]["phase_unwrapped_q4_20"]
        last_phase = rows[stop]["phase_unwrapped_q4_20"]
        delta = last_phase - first_phase
        slope = delta >> WINDOW_SHIFT
        estimated_offset_hz = slope / ANGLE_SCALE * FS / (2 * np.pi)
        exact_offset_hz = (delta / (WINDOW_SAMPLES - 1)) / ANGLE_SCALE * FS / (2 * np.pi)
        expected.append({
            "block_index": block_index,
            "window_sample_count": WINDOW_SAMPLES,
            "first_sample_index": rows[start]["phase_sample_index"],
            "last_sample_index": rows[stop]["phase_sample_index"],
            "first_phase_q4_20": first_phase,
            "last_phase_q4_20": last_phase,
            "delta_phase_q4_20": delta,
            "slope_per_sample_q4_20": slope,
            "estimated_offset_hz_from_shift": estimated_offset_hz,
            "estimated_offset_hz_exact_div": exact_offset_hz,
        })
        block_index += 1
        start += WINDOW_SAMPLES
    return expected


def write_csv(path, rows):
    with path.open("w", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)


def read_sim_csv(path):
    rows = []
    with path.open("r", encoding="utf-8", newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            rows.append({
                "block_index": int(row["block_index"]),
                "window_sample_count": int(row["window_sample_count"]),
                "first_phase_q4_20": int(row["first_phase_q4_20"]),
                "last_phase_q4_20": int(row["last_phase_q4_20"]),
                "delta_phase_q4_20": int(row["delta_phase_q4_20"]),
                "slope_per_sample_q4_20": int(row["slope_per_sample_q4_20"]),
            })
    return rows


def compare_sim_with_expected(expected):
    sim_path = OUT_DIR / "phase_slope_estimator_sim.csv"
    if not sim_path.exists():
        print("No Vivado/XSim CSV found yet.")
        print(f"Expected simulation output path: {sim_path}")
        return

    expected_by_block = {row["block_index"]: row for row in expected}
    sim_rows = read_sim_csv(sim_path)
    mismatches = []

    for actual in sim_rows:
        expected_row = expected_by_block[actual["block_index"]]
        for key in [
            "window_sample_count",
            "first_phase_q4_20",
            "last_phase_q4_20",
            "delta_phase_q4_20",
            "slope_per_sample_q4_20",
        ]:
            if actual[key] != expected_row[key]:
                mismatches.append((actual["block_index"], key, expected_row[key], actual[key]))

    if mismatches:
        print(f"Found {len(mismatches)} mismatches.")
        for item in mismatches[:10]:
            print(f"block={item[0]} {item[1]} expected={item[2]} actual={item[3]}")
    else:
        print(f"Vivado/XSim output matches expected phase slope estimates for {len(sim_rows)} blocks.")


def plot_rows(phase_rows, expected):
    index = np.array([row["phase_sample_index"] for row in phase_rows])
    phase = np.array([row["phase_unwrapped_q4_20"] for row in phase_rows]) / ANGLE_SCALE

    fig, axes = plt.subplots(2, 1, figsize=(12, 7), sharex=False)
    axes[0].plot(index, phase, linewidth=0.9)
    axes[0].set_title("Input unwrapped phase from lesson 33")
    axes[0].set_xlabel("phase sample index")
    axes[0].set_ylabel("phase (rad)")
    axes[0].grid(True, alpha=0.3)

    block = np.array([row["block_index"] for row in expected])
    hz_shift = np.array([row["estimated_offset_hz_from_shift"] for row in expected])
    hz_exact = np.array([row["estimated_offset_hz_exact_div"] for row in expected])
    axes[1].plot(block, hz_shift, marker="o", label="hardware shift estimate")
    axes[1].plot(block, hz_exact, marker="x", label="exact endpoint division")
    axes[1].axhline(5000.0, color="black", linewidth=0.8, linestyle="--", label="injected offset")
    axes[1].set_title("Endpoint slope estimate per block")
    axes[1].set_xlabel("block index")
    axes[1].set_ylabel("estimated offset (Hz)")
    axes[1].grid(True, alpha=0.3)
    axes[1].legend(loc="upper right")

    fig.tight_layout()
    png_path = OUT_DIR / "phase_slope_estimator_expected.png"
    fig.savefig(png_path, dpi=150)
    return png_path


def main():
    SIM_DIR.mkdir(parents=True, exist_ok=True)
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    phase_rows = read_phase_rows()
    write_phase_mem(SIM_DIR / "phase_slope_input_19985.mem", phase_rows)

    expected = make_expected(phase_rows)
    expected_csv = OUT_DIR / "phase_slope_estimator_expected.csv"
    write_csv(expected_csv, expected)
    plot_path = plot_rows(phase_rows, expected)

    print("Lesson 34 phase slope estimator assets prepared")
    print(f"input phase samples = {len(phase_rows)}")
    print(f"window samples = {WINDOW_SAMPLES}")
    print(f"expected slope blocks = {len(expected)}")
    print(f"phase mem: {(SIM_DIR / 'phase_slope_input_19985.mem').resolve()}")
    print(f"expected csv: {expected_csv.resolve()}")
    print(f"expected plot: {plot_path.resolve()}")
    print()
    print("Expected slope blocks:")
    print("block,first_index,last_index,delta_q4_20,slope_q4_20,estimated_hz")
    for row in expected:
        print(
            f"{row['block_index']},{row['first_sample_index']},{row['last_sample_index']},"
            f"{row['delta_phase_q4_20']},{row['slope_per_sample_q4_20']},"
            f"{row['estimated_offset_hz_from_shift']:.3f}"
        )
    print()
    compare_sim_with_expected(expected)


if __name__ == "__main__":
    main()
