from pathlib import Path
import csv

import numpy as np
import matplotlib.pyplot as plt


ROOT = Path(__file__).resolve().parents[1]
RTL_DIR = ROOT / "rtl"
SIM_DIR = ROOT / "sim"
OUT_DIR = ROOT / "outputs" / "lesson_29"

FS = 20_000_000
FC = 2_000_000
NUM_SAMPLES = 8000
PHASE_BITS = 32
LUT_ADDR_BITS = 10
AMP_BITS = 12
SAMPLE_BITS = 8
PRODUCT_BITS = 21
WINDOW_LEN = 16
SHIFT_BITS = 4
ANGLE_FRAC_BITS = 20
ANGLE_SCALE = 1 << ANGLE_FRAC_BITS
ITERATIONS = 20
ADC_AMP = 80
INITIAL_PHASE_DEG = 30
CARRIER_OFFSET_HZ = 200
CORDIC_ACCEPT_INTERVAL = 21


def signed_to_twos_complement(value, bits):
    if value < 0:
        value = (1 << bits) + value
    return value


def wrap_pi_q(angle_q):
    pi_q = int(round(np.pi * ANGLE_SCALE))
    two_pi_q = 2 * pi_q
    while angle_q > pi_q:
        angle_q -= two_pi_q
    while angle_q < -pi_q:
        angle_q += two_pi_q
    return angle_q


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


def make_adc_samples():
    t = np.arange(NUM_SAMPLES) / FS
    phase0 = np.deg2rad(INITIAL_PHASE_DEG)
    rf = ADC_AMP * np.cos(2 * np.pi * (FC + CARRIER_OFFSET_HZ) * t + phase0)
    return quantize_adc(rf)


def ddc_iq(adc_samples):
    cos_lo, sin_lo = dds_sequence(len(adc_samples))
    i_raw = adc_samples * cos_lo
    q_raw = adc_samples * (-sin_lo)
    i_baseband, valid_i = moving_average_hw_style(i_raw)
    q_baseband, valid_q = moving_average_hw_style(q_raw)
    valid = valid_i & valid_q
    return i_baseband, q_baseband, valid


def atan_table_q():
    return [int(round(np.arctan(2.0 ** -i) * ANGLE_SCALE)) for i in range(ITERATIONS)]


def cordic_model(i_value, q_value):
    table = atan_table_q()
    pi_q = int(round(np.pi * ANGLE_SCALE))

    x = int(i_value)
    y = int(q_value)
    z = 0

    if x < 0:
        x = -x
        y = -y
        z = pi_q if q_value >= 0 else -pi_q

    for step, angle_q in enumerate(table):
        if y == 0:
            break

        x_shift = x >> step
        y_shift = y >> step

        if y > 0:
            x, y, z = x + y_shift, y - x_shift, z + angle_q
        else:
            x, y, z = x - y_shift, y + x_shift, z - angle_q

    return wrap_pi_q(z)


def expected_phase_rows(i_baseband, q_baseband, valid):
    valid_indices = np.flatnonzero(valid)
    rows = []
    valid_row_index = 0

    for n in valid_indices:
        if valid_row_index % CORDIC_ACCEPT_INTERVAL == 0:
            i_value = int(i_baseband[n])
            q_value = int(q_baseband[n])
            phase_q = cordic_model(i_value, q_value)
            exact_q = int(round(np.arctan2(q_value, i_value) * ANGLE_SCALE))
            rows.append({
                "phase_sample_index": valid_row_index,
                "raw_sample_index": int(n),
                "i_baseband": i_value,
                "q_baseband": q_value,
                "exact_phase_q4_20": exact_q,
                "cordic_phase_q4_20": phase_q,
                "cordic_error_q4_20": wrap_pi_q(phase_q - exact_q),
                "cordic_phase_rad": phase_q / ANGLE_SCALE,
            })
        valid_row_index += 1

    return rows


def write_expected_csv(path, rows):
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
                "phase_sample_index": int(row["phase_sample_index"]),
                "i_baseband": int(row["i_baseband"]),
                "q_baseband": int(row["q_baseband"]),
                "phase_q4_20": int(row["phase_q4_20"]),
            })
    return rows


def compare_sim_with_expected(expected_rows):
    sim_path = OUT_DIR / "ddc_phase_core_sim.csv"
    if not sim_path.exists():
        print("No Verilog simulation CSV found yet.")
        print(f"Expected Verilog simulation output path: {sim_path}")
        return

    expected_by_index = {row["phase_sample_index"]: row for row in expected_rows}
    sim_rows = read_sim_csv(sim_path)
    mismatches = []
    for actual in sim_rows:
        expected = expected_by_index[actual["phase_sample_index"]]
        for key, expected_key in [
            ("i_baseband", "i_baseband"),
            ("q_baseband", "q_baseband"),
            ("phase_q4_20", "cordic_phase_q4_20"),
        ]:
            if actual[key] != expected[expected_key]:
                mismatches.append((actual["phase_sample_index"], key, expected[expected_key], actual[key]))

    if mismatches:
        print(f"Found {len(mismatches)} mismatches.")
        for item in mismatches[:10]:
            print(f"phase_sample_index={item[0]} {item[1]} expected={item[2]} actual={item[3]}")
    else:
        print(f"Verilog simulation matches expected phase core output for {len(sim_rows)} phase samples.")


def plot_rows(rows):
    index = np.array([row["phase_sample_index"] for row in rows])
    phase = np.unwrap(np.array([row["cordic_phase_rad"] for row in rows]))
    i_values = np.array([row["i_baseband"] for row in rows])
    q_values = np.array([row["q_baseband"] for row in rows])

    fig, axes = plt.subplots(2, 1, figsize=(12, 7), sharex=True)
    axes[0].plot(index, phase, marker="o", linewidth=1.0)
    axes[0].set_title("DDC Phase Core expected phase output")
    axes[0].set_ylabel("unwrapped phase (rad)")
    axes[0].grid(True, alpha=0.3)

    axes[1].plot(index, i_values, marker="o", linewidth=1.0, label="I accepted by CORDIC")
    axes[1].plot(index, q_values, marker="o", linewidth=1.0, label="Q accepted by CORDIC")
    axes[1].set_title("I/Q samples accepted when CORDIC is ready")
    axes[1].set_xlabel("DDC valid-sample index")
    axes[1].set_ylabel("code")
    axes[1].grid(True, alpha=0.3)
    axes[1].legend(loc="upper right")

    fig.tight_layout()
    png_path = OUT_DIR / "ddc_phase_core_expected.png"
    fig.savefig(png_path, dpi=150)
    return png_path


def main():
    RTL_DIR.mkdir(parents=True, exist_ok=True)
    SIM_DIR.mkdir(parents=True, exist_ok=True)
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    ensure_dds_roms()

    adc_samples = make_adc_samples()
    write_mem(SIM_DIR / "phase_core_sample_8000.mem", adc_samples, SAMPLE_BITS)

    i_baseband, q_baseband, valid = ddc_iq(adc_samples)
    rows = expected_phase_rows(i_baseband, q_baseband, valid)

    expected_csv = OUT_DIR / "ddc_phase_core_expected.csv"
    write_expected_csv(expected_csv, rows)
    png_path = plot_rows(rows)

    errors_deg = [
        row["cordic_error_q4_20"] / ANGLE_SCALE * 180.0 / np.pi
        for row in rows
    ]
    print("Lesson 29 DDC phase core assets prepared")
    print(f"input waveform: CW phase={INITIAL_PHASE_DEG} deg, carrier offset={CARRIER_OFFSET_HZ} Hz")
    print(f"adc samples = {len(adc_samples)}")
    print(f"expected phase samples = {len(rows)}")
    print(f"CORDIC accept interval model = {CORDIC_ACCEPT_INTERVAL} DDC valid samples")
    print(f"max_abs_cordic_error_deg = {max(abs(v) for v in errors_deg):.6f}")
    print(f"sample mem: {(SIM_DIR / 'phase_core_sample_8000.mem').resolve()}")
    print(f"expected csv: {expected_csv.resolve()}")
    print(f"expected plot: {png_path.resolve()}")
    print()
    print("First 10 expected phase samples:")
    print("phase_sample_index,raw_sample_index,I,Q,phase_q4_20,phase_rad")
    for row in rows[:10]:
        print(
            f"{row['phase_sample_index']},{row['raw_sample_index']},"
            f"{row['i_baseband']},{row['q_baseband']},"
            f"{row['cordic_phase_q4_20']},{row['cordic_phase_rad']:.6f}"
        )
    print()
    compare_sim_with_expected(rows)


if __name__ == "__main__":
    main()
