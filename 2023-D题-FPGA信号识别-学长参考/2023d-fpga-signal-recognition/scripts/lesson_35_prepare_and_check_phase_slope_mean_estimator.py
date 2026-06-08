from pathlib import Path
import csv

import numpy as np
import matplotlib.pyplot as plt


ROOT = Path(__file__).resolve().parents[1]
SIM_DIR = ROOT / "sim"
OUT_DIR = ROOT / "outputs" / "lesson_35"
LESSON33_CSV = ROOT / "outputs" / "lesson_33" / "ddc_phase_unwrap_core_expected.csv"

PHASE_BITS = 40
ANGLE_FRAC_BITS = 20
ANGLE_SCALE = 1 << ANGLE_FRAC_BITS
FS = 20_000_000
WINDOW_SHIFT = 13
AVG_SHIFT = 4
DISTANCE = 1 << WINDOW_SHIFT
AVG_COUNT = 1 << AVG_SHIFT
BLOCK_SAMPLES = DISTANCE + AVG_COUNT


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
    while start + BLOCK_SAMPLES <= len(rows):
        start_sum = sum(row["phase_unwrapped_q4_20"] for row in rows[start:start + AVG_COUNT])
        end_start = start + DISTANCE
        end_sum = sum(row["phase_unwrapped_q4_20"] for row in rows[end_start:end_start + AVG_COUNT])
        delta_sum = end_sum - start_sum
        slope = delta_sum >> (WINDOW_SHIFT + AVG_SHIFT)
        estimated_offset_hz = slope / ANGLE_SCALE * FS / (2 * np.pi)
        exact_offset_hz = (delta_sum / (DISTANCE * AVG_COUNT)) / ANGLE_SCALE * FS / (2 * np.pi)
        expected.append({
            "block_index": block_index,
            "block_sample_count": BLOCK_SAMPLES,
            "start_first_index": rows[start]["phase_sample_index"],
            "start_last_index": rows[start + AVG_COUNT - 1]["phase_sample_index"],
            "end_first_index": rows[end_start]["phase_sample_index"],
            "end_last_index": rows[end_start + AVG_COUNT - 1]["phase_sample_index"],
            "start_sum_q4_20": start_sum,
            "end_sum_q4_20": end_sum,
            "delta_sum_q4_20": delta_sum,
            "slope_per_sample_q4_20": slope,
            "estimated_offset_hz_from_shift": estimated_offset_hz,
            "estimated_offset_hz_exact_div": exact_offset_hz,
        })
        block_index += 1
        start += BLOCK_SAMPLES
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
                "block_sample_count": int(row["block_sample_count"]),
                "start_sum_q4_20": int(row["start_sum_q4_20"]),
                "end_sum_q4_20": int(row["end_sum_q4_20"]),
                "delta_sum_q4_20": int(row["delta_sum_q4_20"]),
                "slope_per_sample_q4_20": int(row["slope_per_sample_q4_20"]),
            })
    return rows


def compare_sim_with_expected(expected):
    sim_path = OUT_DIR / "phase_slope_mean_estimator_sim.csv"
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
            "block_sample_count",
            "start_sum_q4_20",
            "end_sum_q4_20",
            "delta_sum_q4_20",
            "slope_per_sample_q4_20",
        ]:
            if actual[key] != expected_row[key]:
                mismatches.append((actual["block_index"], key, expected_row[key], actual[key]))

    if mismatches:
        print(f"Found {len(mismatches)} mismatches.")
        for item in mismatches[:10]:
            print(f"block={item[0]} {item[1]} expected={item[2]} actual={item[3]}")
    else:
        print(f"Vivado/XSim output matches expected mean slope estimates for {len(sim_rows)} blocks.")


def read_lesson34_expected():
    path = ROOT / "outputs" / "lesson_34" / "phase_slope_estimator_expected.csv"
    if not path.exists():
        return []
    rows = []
    with path.open("r", encoding="utf-8", newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            rows.append({
                "block_index": int(row["block_index"]),
                "estimated_offset_hz_from_shift": float(row["estimated_offset_hz_from_shift"]),
            })
    return rows


def plot_rows(phase_rows, expected, endpoint_rows):
    index = np.array([row["phase_sample_index"] for row in phase_rows])
    phase = np.array([row["phase_unwrapped_q4_20"] for row in phase_rows]) / ANGLE_SCALE

    fig, axes = plt.subplots(2, 1, figsize=(12, 7), sharex=False)
    axes[0].plot(index, phase, linewidth=0.9)
    axes[0].set_title("Input unwrapped phase from lesson 33")
    axes[0].set_xlabel("phase sample index")
    axes[0].set_ylabel("phase (rad)")
    axes[0].grid(True, alpha=0.3)

    block = np.array([row["block_index"] for row in expected])
    hz_mean = np.array([row["estimated_offset_hz_from_shift"] for row in expected])
    hz_mean_exact = np.array([row["estimated_offset_hz_exact_div"] for row in expected])
    axes[1].plot(block, hz_mean, marker="o", label="mean endpoint shift estimate")
    axes[1].plot(block, hz_mean_exact, marker="x", label="mean endpoint exact division")
    if endpoint_rows:
        endpoint_block = np.array([row["block_index"] for row in endpoint_rows])
        endpoint_hz = np.array([row["estimated_offset_hz_from_shift"] for row in endpoint_rows])
        axes[1].plot(endpoint_block, endpoint_hz, marker="s", linestyle="--", label="lesson 34 single endpoint")
    axes[1].axhline(5000.0, color="black", linewidth=0.8, linestyle="--", label="injected offset")
    axes[1].set_title("Mean endpoint slope estimate per block")
    axes[1].set_xlabel("block index")
    axes[1].set_ylabel("estimated offset (Hz)")
    axes[1].grid(True, alpha=0.3)
    axes[1].legend(loc="upper right")

    fig.tight_layout()
    png_path = OUT_DIR / "phase_slope_mean_estimator_expected.png"
    fig.savefig(png_path, dpi=150)
    return png_path


def main():
    SIM_DIR.mkdir(parents=True, exist_ok=True)
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    phase_rows = read_phase_rows()
    write_phase_mem(SIM_DIR / "phase_slope_mean_input_19985.mem", phase_rows)

    expected = make_expected(phase_rows)
    expected_csv = OUT_DIR / "phase_slope_mean_estimator_expected.csv"
    write_csv(expected_csv, expected)
    endpoint_rows = read_lesson34_expected()
    plot_path = plot_rows(phase_rows, expected, endpoint_rows)

    print("Lesson 35 mean endpoint phase slope estimator assets prepared")
    print(f"input phase samples = {len(phase_rows)}")
    print(f"distance samples = {DISTANCE}")
    print(f"average samples = {AVG_COUNT}")
    print(f"block samples = {BLOCK_SAMPLES}")
    print(f"expected slope blocks = {len(expected)}")
    print(f"phase mem: {(SIM_DIR / 'phase_slope_mean_input_19985.mem').resolve()}")
    print(f"expected csv: {expected_csv.resolve()}")
    print(f"expected plot: {plot_path.resolve()}")
    print()
    print("Expected mean slope blocks:")
    print("block,start_range,end_range,delta_sum_q4_20,slope_q4_20,estimated_hz")
    for row in expected:
        print(
            f"{row['block_index']},"
            f"{row['start_first_index']}..{row['start_last_index']},"
            f"{row['end_first_index']}..{row['end_last_index']},"
            f"{row['delta_sum_q4_20']},{row['slope_per_sample_q4_20']},"
            f"{row['estimated_offset_hz_from_shift']:.3f}"
        )
    print()
    compare_sim_with_expected(expected)


if __name__ == "__main__":
    main()
