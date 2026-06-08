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
PRODUCT_BITS = 21
WINDOW_LEN = 16
SHIFT_BITS = 4
NUM_SAMPLES = 80


ROOT = Path(__file__).resolve().parents[1]
RTL_DIR = ROOT / "rtl"
SIM_DIR = ROOT / "sim"
OUT_DIR = ROOT / "outputs" / "lesson_18"


def signed_to_twos_complement(value, bits):
    if value < 0:
        value = (1 << bits) + value
    return value


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


def write_mem(path, values, bits):
    with path.open("w", encoding="utf-8") as f:
        for value in values:
            code = signed_to_twos_complement(int(value), bits)
            f.write(f"{code:0{(bits + 3) // 4}X}\n")


def ensure_dds_roms():
    write_mem(RTL_DIR / "cos_1024x12.mem", build_lut("cos"), AMP_BITS)
    write_mem(RTL_DIR / "sin_1024x12.mem", build_lut("sin"), AMP_BITS)


def dds_sequence():
    phase_modulus = 1 << PHASE_BITS
    phase_step = round(FC / FS * phase_modulus)
    cos_lut = build_lut("cos")
    sin_lut = build_lut("sin")

    phase = 0
    cos_values = []
    sin_values = []
    for _ in range(NUM_SAMPLES):
        lut_addr = phase >> (PHASE_BITS - LUT_ADDR_BITS)
        cos_values.append(int(cos_lut[lut_addr]))
        sin_values.append(int(sin_lut[lut_addr]))
        phase = (phase + phase_step) % phase_modulus

    return np.array(cos_values), np.array(sin_values)


def adc_sample_sequence():
    t = np.arange(NUM_SAMPLES) / FS
    sample = np.round(80 * np.cos(2 * np.pi * FC * t)).astype(int)
    return np.clip(sample, -(1 << (SAMPLE_BITS - 1)), (1 << (SAMPLE_BITS - 1)) - 1)


def moving_average_hw_style(x):
    delay = [0] * WINDOW_LEN
    running_sum = 0
    rows = []

    for n, value in enumerate(x):
        oldest = delay[-1]
        delay = [int(value)] + delay[:-1]
        running_sum = running_sum + int(value) - int(oldest)
        avg = running_sum >> SHIFT_BITS
        rows.append({
            "n": n,
            "sum": running_sum,
            "avg": avg,
            "valid": n >= WINDOW_LEN - 1,
        })

    return rows


def write_expected_csv(rows):
    csv_path = OUT_DIR / "ddc_core_expected.csv"
    with csv_path.open("w", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)
    return csv_path


def read_sim_csv(path):
    rows = []
    with path.open("r", encoding="utf-8", newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            rows.append({
                "n": int(row["n"]),
                "i_baseband": int(row["i_baseband"]),
                "q_baseband": int(row["q_baseband"]),
            })
    return rows


def compare_sim_with_expected(expected_rows):
    sim_path = OUT_DIR / "ddc_core_sim.csv"
    if not sim_path.exists():
        print("No Verilog simulation CSV found yet.")
        print(f"Expected Verilog simulation output path: {sim_path}")
        return

    expected_by_n = {row["n"]: row for row in expected_rows}
    sim_rows = read_sim_csv(sim_path)
    mismatches = []
    for actual in sim_rows:
        expected = expected_by_n[actual["n"]]
        for key in ["i_baseband", "q_baseband"]:
            if expected[key] != actual[key]:
                mismatches.append((actual["n"], key, expected[key], actual[key]))

    if mismatches:
        print(f"Found {len(mismatches)} mismatches.")
        for item in mismatches[:10]:
            print(f"n={item[0]} {item[1]} expected={item[2]} actual={item[3]}")
    else:
        print(f"Verilog simulation matches expected DDC core output for {len(sim_rows)} valid samples.")


def plot_rows(rows, i_raw, q_raw):
    n_valid = np.array([row["n"] for row in rows])
    i_baseband = np.array([row["i_baseband"] for row in rows])
    q_baseband = np.array([row["q_baseband"] for row in rows])
    n = np.arange(len(i_raw))

    fig, axes = plt.subplots(2, 1, figsize=(12, 7), sharex=True)
    axes[0].plot(n, i_raw, marker="o", linewidth=1.0, label="I_raw")
    axes[0].plot(n_valid, i_baseband, marker="o", linewidth=1.0, label="I_baseband")
    axes[0].set_title("DDC core I path")
    axes[0].set_ylabel("code")
    axes[0].grid(True, alpha=0.3)
    axes[0].legend(loc="upper right")

    axes[1].plot(n, q_raw, marker="o", linewidth=1.0, label="Q_raw")
    axes[1].plot(n_valid, q_baseband, marker="o", linewidth=1.0, label="Q_baseband")
    axes[1].set_title("DDC core Q path")
    axes[1].set_xlabel("sample index")
    axes[1].set_ylabel("code")
    axes[1].grid(True, alpha=0.3)
    axes[1].legend(loc="upper right")

    fig.tight_layout()
    png_path = OUT_DIR / "ddc_core_expected.png"
    fig.savefig(png_path, dpi=150)
    return png_path


def main():
    RTL_DIR.mkdir(parents=True, exist_ok=True)
    SIM_DIR.mkdir(parents=True, exist_ok=True)
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    ensure_dds_roms()

    sample = adc_sample_sequence()
    cos_in, sin_in = dds_sequence()
    i_raw = sample * cos_in
    q_raw = sample * (-sin_in)
    i_lpf = moving_average_hw_style(i_raw)
    q_lpf = moving_average_hw_style(q_raw)

    write_mem(SIM_DIR / "ddc_core_sample_80.mem", sample, SAMPLE_BITS)

    rows = []
    for n in range(NUM_SAMPLES):
        if i_lpf[n]["valid"] and q_lpf[n]["valid"]:
            rows.append({
                "n": n,
                "sample_in": int(sample[n]),
                "i_raw": int(i_raw[n]),
                "q_raw": int(q_raw[n]),
                "i_baseband": int(i_lpf[n]["avg"]),
                "q_baseband": int(q_lpf[n]["avg"]),
            })

    expected_csv = write_expected_csv(rows)
    png_path = plot_rows(rows, i_raw, q_raw)

    print("Lesson 18 DDC core assets prepared")
    print(f"sample mem: {(SIM_DIR / 'ddc_core_sample_80.mem').resolve()}")
    print(f"expected csv: {expected_csv.resolve()}")
    print(f"expected plot: {png_path.resolve()}")
    print()
    print("First 10 expected DDC core outputs:")
    print("n,sample_in,i_raw,q_raw,i_baseband,q_baseband")
    for row in rows[:10]:
        print(
            f"{row['n']},{row['sample_in']},{row['i_raw']},{row['q_raw']},"
            f"{row['i_baseband']},{row['q_baseband']}"
        )
    print()
    print(f"Mean I_baseband = {np.mean([row['i_baseband'] for row in rows]):.2f}")
    print(f"Mean Q_baseband = {np.mean([row['q_baseband'] for row in rows]):.2f}")
    print()
    compare_sim_with_expected(rows)


if __name__ == "__main__":
    main()
