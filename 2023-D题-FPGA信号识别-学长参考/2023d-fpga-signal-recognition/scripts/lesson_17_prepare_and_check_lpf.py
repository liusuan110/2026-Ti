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
SIM_DIR = ROOT / "sim"
OUT_DIR = ROOT / "outputs" / "lesson_17"


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


def write_mem(path, values, bits):
    with path.open("w", encoding="utf-8") as f:
        for value in values:
            code = signed_to_twos_complement(int(value), bits)
            f.write(f"{code:0{(bits + 3) // 4}X}\n")


def write_expected_csv(rows):
    csv_path = OUT_DIR / "moving_average_expected.csv"
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
                "i_raw": int(row["i_raw"]),
                "q_raw": int(row["q_raw"]),
                "i_sum": int(row["i_sum"]),
                "q_sum": int(row["q_sum"]),
                "i_avg": int(row["i_avg"]),
                "q_avg": int(row["q_avg"]),
            })
    return rows


def compare_sim_with_expected(expected_rows):
    sim_path = OUT_DIR / "moving_average_sim.csv"
    if not sim_path.exists():
        print("No Verilog simulation CSV found yet.")
        print(f"Expected Verilog simulation output path: {sim_path}")
        return

    expected_by_n = {row["n"]: row for row in expected_rows if row["valid"]}
    sim_rows = read_sim_csv(sim_path)
    mismatches = []
    for actual in sim_rows:
        expected = expected_by_n[actual["n"]]
        for key in ["i_raw", "q_raw", "i_sum", "q_sum", "i_avg", "q_avg"]:
            if expected[key] != actual[key]:
                mismatches.append((actual["n"], key, expected[key], actual[key]))

    if mismatches:
        print(f"Found {len(mismatches)} mismatches.")
        for item in mismatches[:10]:
            print(f"n={item[0]} {item[1]} expected={item[2]} actual={item[3]}")
    else:
        print(f"Verilog simulation matches expected moving-average output for {len(sim_rows)} valid samples.")


def plot_rows(rows):
    n = np.array([row["n"] for row in rows])
    i_raw = np.array([row["i_raw"] for row in rows])
    q_raw = np.array([row["q_raw"] for row in rows])
    i_avg = np.array([row["i_avg"] if row["valid"] else np.nan for row in rows], dtype=float)
    q_avg = np.array([row["q_avg"] if row["valid"] else np.nan for row in rows], dtype=float)

    fig, axes = plt.subplots(2, 1, figsize=(12, 7), sharex=True)
    axes[0].plot(n, i_raw, marker="o", linewidth=1.0, label="I_raw")
    axes[0].plot(n, i_avg, marker="o", linewidth=1.0, label="I_avg after 16-sample LPF")
    axes[0].set_title("I path: moving average keeps the low-frequency component")
    axes[0].set_ylabel("code")
    axes[0].grid(True, alpha=0.3)
    axes[0].legend(loc="upper right")

    axes[1].plot(n, q_raw, marker="o", linewidth=1.0, label="Q_raw")
    axes[1].plot(n, q_avg, marker="o", linewidth=1.0, label="Q_avg after 16-sample LPF")
    axes[1].set_title("Q path: same-phase test signal should average near zero")
    axes[1].set_xlabel("sample index")
    axes[1].set_ylabel("code")
    axes[1].grid(True, alpha=0.3)
    axes[1].legend(loc="upper right")

    fig.tight_layout()
    png_path = OUT_DIR / "moving_average_expected.png"
    fig.savefig(png_path, dpi=150)
    return png_path


def main():
    SIM_DIR.mkdir(parents=True, exist_ok=True)
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    sample = adc_sample_sequence()
    cos_in, sin_in = dds_sequence()
    i_raw = sample * cos_in
    q_raw = sample * (-sin_in)

    i_lpf = moving_average_hw_style(i_raw)
    q_lpf = moving_average_hw_style(q_raw)

    write_mem(SIM_DIR / "lpf_i_raw_80.mem", i_raw, PRODUCT_BITS)
    write_mem(SIM_DIR / "lpf_q_raw_80.mem", q_raw, PRODUCT_BITS)

    rows = []
    for n in range(NUM_SAMPLES):
        rows.append({
            "n": n,
            "i_raw": int(i_raw[n]),
            "q_raw": int(q_raw[n]),
            "i_sum": int(i_lpf[n]["sum"]),
            "q_sum": int(q_lpf[n]["sum"]),
            "i_avg": int(i_lpf[n]["avg"]),
            "q_avg": int(q_lpf[n]["avg"]),
            "valid": bool(i_lpf[n]["valid"] and q_lpf[n]["valid"]),
        })

    expected_csv = write_expected_csv(rows)
    png_path = plot_rows(rows)

    valid_rows = [row for row in rows if row["valid"]]
    print("Lesson 17 moving-average assets prepared")
    print(f"WINDOW_LEN = {WINDOW_LEN}")
    print(f"SHIFT_BITS = {SHIFT_BITS}")
    print(f"i raw mem: {(SIM_DIR / 'lpf_i_raw_80.mem').resolve()}")
    print(f"q raw mem: {(SIM_DIR / 'lpf_q_raw_80.mem').resolve()}")
    print(f"expected csv: {expected_csv.resolve()}")
    print(f"expected plot: {png_path.resolve()}")
    print()
    print("First 10 valid expected samples:")
    print("n,i_raw,q_raw,i_sum,q_sum,i_avg,q_avg")
    for row in valid_rows[:10]:
        print(
            f"{row['n']},{row['i_raw']},{row['q_raw']},{row['i_sum']},{row['q_sum']},"
            f"{row['i_avg']},{row['q_avg']}"
        )
    print()
    print(f"Mean valid I_avg = {np.mean([row['i_avg'] for row in valid_rows]):.2f}")
    print(f"Mean valid Q_avg = {np.mean([row['q_avg'] for row in valid_rows]):.2f}")
    print()
    compare_sim_with_expected(rows)


if __name__ == "__main__":
    main()
