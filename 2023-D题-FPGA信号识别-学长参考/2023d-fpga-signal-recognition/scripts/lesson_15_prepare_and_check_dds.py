from pathlib import Path
import csv

import numpy as np


FS = 20_000_000
FC = 2_000_000
PHASE_BITS = 32
LUT_ADDR_BITS = 10
AMP_BITS = 12
NUM_SAMPLES = 80


ROOT = Path(__file__).resolve().parents[1]
RTL_DIR = ROOT / "rtl"
OUT_DIR = ROOT / "outputs" / "lesson_15"


def signed_to_twos_complement(value, bits):
    if value < 0:
        value = (1 << bits) + value
    return value


def twos_complement_to_signed(value, bits):
    sign_bit = 1 << (bits - 1)
    if value & sign_bit:
        return value - (1 << bits)
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


def write_mem(path, values):
    with path.open("w", encoding="utf-8") as f:
        for value in values:
            code = signed_to_twos_complement(int(value), AMP_BITS)
            f.write(f"{code:0{(AMP_BITS + 3) // 4}X}\n")


def emulate_dds():
    phase_modulus = 1 << PHASE_BITS
    phase_step = round(FC / FS * phase_modulus)
    cos_lut = build_lut("cos")
    sin_lut = build_lut("sin")

    phase = 0
    rows = []
    for n in range(NUM_SAMPLES):
        lut_addr = phase >> (PHASE_BITS - LUT_ADDR_BITS)
        rows.append({
            "n": n,
            "phase": phase,
            "lut_addr": lut_addr,
            "cos_code": int(cos_lut[lut_addr]),
            "sin_code": int(sin_lut[lut_addr]),
        })
        phase = (phase + phase_step) % phase_modulus

    return phase_step, rows


def write_expected_csv(rows):
    csv_path = OUT_DIR / "dds_nco_expected.csv"
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
                "phase": int(row["phase"]),
                "cos_code": int(row["cos_code"]),
                "sin_code": int(row["sin_code"]),
            })
    return rows


def compare_sim_with_expected(expected_rows):
    sim_path = OUT_DIR / "dds_nco_sim.csv"
    if not sim_path.exists():
        print("No Verilog simulation CSV found yet.")
        print(f"Expected Verilog simulation output path: {sim_path}")
        return

    sim_rows = read_sim_csv(sim_path)
    mismatches = []
    for expected, actual in zip(expected_rows, sim_rows):
        for key in ["phase", "cos_code", "sin_code"]:
            if expected[key] != actual[key]:
                mismatches.append((expected["n"], key, expected[key], actual[key]))

    if mismatches:
        print(f"Found {len(mismatches)} mismatches.")
        for item in mismatches[:10]:
            print(f"n={item[0]} {item[1]} expected={item[2]} actual={item[3]}")
    else:
        print(f"Verilog simulation matches expected DDS output for {len(sim_rows)} samples.")


def main():
    RTL_DIR.mkdir(parents=True, exist_ok=True)
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    cos_lut = build_lut("cos")
    sin_lut = build_lut("sin")
    write_mem(RTL_DIR / "cos_1024x12.mem", cos_lut)
    write_mem(RTL_DIR / "sin_1024x12.mem", sin_lut)

    phase_step, expected_rows = emulate_dds()
    expected_csv = write_expected_csv(expected_rows)
    actual_freq = phase_step / (1 << PHASE_BITS) * FS

    print("Lesson 15 DDS assets prepared")
    print(f"phase_step = {phase_step}")
    print(f"actual_freq = {actual_freq:.6f} Hz")
    print(f"cos mem: {(RTL_DIR / 'cos_1024x12.mem').resolve()}")
    print(f"sin mem: {(RTL_DIR / 'sin_1024x12.mem').resolve()}")
    print(f"expected csv: {expected_csv.resolve()}")
    print()
    print("First 10 expected samples:")
    print("n,phase,lut_addr,cos_code,sin_code")
    for row in expected_rows[:10]:
        print(f"{row['n']},{row['phase']},{row['lut_addr']},{row['cos_code']},{row['sin_code']}")
    print()
    compare_sim_with_expected(expected_rows)


if __name__ == "__main__":
    main()
