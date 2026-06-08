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
NUM_SAMPLES = 80


ROOT = Path(__file__).resolve().parents[1]
SIM_DIR = ROOT / "sim"
OUT_DIR = ROOT / "outputs" / "lesson_16"


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


def write_mem(path, values, bits):
    with path.open("w", encoding="utf-8") as f:
        for value in values:
            code = signed_to_twos_complement(int(value), bits)
            f.write(f"{code:0{(bits + 3) // 4}X}\n")


def write_expected_csv(rows):
    csv_path = OUT_DIR / "ddc_mixer_expected.csv"
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
                "sample_in": int(row["sample_in"]),
                "cos_in": int(row["cos_in"]),
                "sin_in": int(row["sin_in"]),
                "i_raw": int(row["i_raw"]),
                "q_raw": int(row["q_raw"]),
            })
    return rows


def compare_sim_with_expected(expected_rows):
    sim_path = OUT_DIR / "ddc_mixer_sim.csv"
    if not sim_path.exists():
        print("No Verilog simulation CSV found yet.")
        print(f"Expected Verilog simulation output path: {sim_path}")
        return

    sim_rows = read_sim_csv(sim_path)
    mismatches = []
    for expected, actual in zip(expected_rows, sim_rows):
        for key in ["sample_in", "cos_in", "sin_in", "i_raw", "q_raw"]:
            if expected[key] != actual[key]:
                mismatches.append((expected["n"], key, expected[key], actual[key]))

    if mismatches:
        print(f"Found {len(mismatches)} mismatches.")
        for item in mismatches[:10]:
            print(f"n={item[0]} {item[1]} expected={item[2]} actual={item[3]}")
    else:
        print(f"Verilog simulation matches expected mixer output for {len(sim_rows)} samples.")


def plot_rows(rows):
    n = np.array([row["n"] for row in rows])
    sample = np.array([row["sample_in"] for row in rows])
    cos_in = np.array([row["cos_in"] for row in rows])
    sin_in = np.array([row["sin_in"] for row in rows])
    i_raw = np.array([row["i_raw"] for row in rows])
    q_raw = np.array([row["q_raw"] for row in rows])

    fig, axes = plt.subplots(3, 1, figsize=(12, 8), sharex=True)
    axes[0].stem(n[:40], sample[:40], basefmt=" ")
    axes[0].set_title("ADC sample input x[n]")
    axes[0].set_ylabel("8-bit code")
    axes[0].grid(True, alpha=0.3)

    axes[1].plot(n[:40], cos_in[:40], marker="o", label="cos[n]")
    axes[1].plot(n[:40], sin_in[:40], marker="o", label="sin[n]", alpha=0.8)
    axes[1].set_title("DDS local oscillator samples")
    axes[1].set_ylabel("12-bit code")
    axes[1].grid(True, alpha=0.3)
    axes[1].legend(loc="upper right")

    axes[2].plot(n[:40], i_raw[:40], marker="o", label="I_raw = x*cos")
    axes[2].plot(n[:40], q_raw[:40], marker="o", label="Q_raw = x*(-sin)", alpha=0.8)
    axes[2].set_title("Mixer products before low-pass filtering")
    axes[2].set_xlabel("sample index")
    axes[2].set_ylabel("product code")
    axes[2].grid(True, alpha=0.3)
    axes[2].legend(loc="upper right")

    fig.tight_layout()
    png_path = OUT_DIR / "ddc_mixer_expected.png"
    fig.savefig(png_path, dpi=150)
    return png_path


def main():
    SIM_DIR.mkdir(parents=True, exist_ok=True)
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    sample = adc_sample_sequence()
    cos_in, sin_in = dds_sequence()
    i_raw = sample * cos_in
    q_raw = sample * (-sin_in)

    write_mem(SIM_DIR / "mixer_sample_80.mem", sample, SAMPLE_BITS)
    write_mem(SIM_DIR / "mixer_cos_80.mem", cos_in, AMP_BITS)
    write_mem(SIM_DIR / "mixer_sin_80.mem", sin_in, AMP_BITS)

    rows = []
    for n in range(NUM_SAMPLES):
        rows.append({
            "n": n,
            "sample_in": int(sample[n]),
            "cos_in": int(cos_in[n]),
            "sin_in": int(sin_in[n]),
            "i_raw": int(i_raw[n]),
            "q_raw": int(q_raw[n]),
        })

    expected_csv = write_expected_csv(rows)
    png_path = plot_rows(rows)

    print("Lesson 16 mixer assets prepared")
    print(f"sample mem: {(SIM_DIR / 'mixer_sample_80.mem').resolve()}")
    print(f"cos mem: {(SIM_DIR / 'mixer_cos_80.mem').resolve()}")
    print(f"sin mem: {(SIM_DIR / 'mixer_sin_80.mem').resolve()}")
    print(f"expected csv: {expected_csv.resolve()}")
    print(f"expected plot: {png_path.resolve()}")
    print()
    print("First 10 expected samples:")
    print("n,sample_in,cos_in,sin_in,i_raw,q_raw")
    for row in rows[:10]:
        print(
            f"{row['n']},{row['sample_in']},{row['cos_in']},{row['sin_in']},"
            f"{row['i_raw']},{row['q_raw']}"
        )
    print()
    compare_sim_with_expected(rows)


if __name__ == "__main__":
    main()
