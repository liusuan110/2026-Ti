from pathlib import Path
import csv

import numpy as np
import matplotlib.pyplot as plt


ROOT = Path(__file__).resolve().parents[1]
RTL_DIR = ROOT / "rtl"
SIM_DIR = ROOT / "sim"
OUT_DIR = ROOT / "outputs" / "lesson_33"

FS = 20_000_000
FC = 2_000_000
NUM_SAMPLES = 20_000
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
CARRIER_OFFSET_HZ = 5_000

PI_Q = int(round(np.pi * ANGLE_SCALE))
TWO_PI_Q = 2 * PI_Q


def signed_to_twos_complement(value, bits):
    if value < 0:
        value = (1 << bits) + value
    return value


def wrap_pi_q(angle_q):
    while angle_q > PI_Q:
        angle_q -= TWO_PI_Q
    while angle_q < -PI_Q:
        angle_q += TWO_PI_Q
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

    x = int(i_value)
    y = int(q_value)
    z = 0

    if x < 0:
        x = -x
        y = -y
        z = PI_Q if q_value >= 0 else -PI_Q

    for step, angle_q in enumerate(table):
        if y == 0:
            continue

        x_shift = x >> step
        y_shift = y >> step

        if y > 0:
            x, y, z = x + y_shift, y - x_shift, z + angle_q
        else:
            x, y, z = x - y_shift, y + x_shift, z - angle_q

    return wrap_pi_q(z)


def expected_rows(i_baseband, q_baseband, valid):
    rows = []
    valid_row_index = 0
    have_prev = False
    prev_phase = 0
    wrap_offset = 0

    for raw_index in np.flatnonzero(valid):
        i_value = int(i_baseband[raw_index])
        q_value = int(q_baseband[raw_index])
        wrapped_phase_q = cordic_model(i_value, q_value)

        if have_prev:
            delta = wrapped_phase_q - prev_phase
            if delta < -PI_Q:
                wrap_offset += TWO_PI_Q
            elif delta > PI_Q:
                wrap_offset -= TWO_PI_Q

        unwrapped_phase_q = wrapped_phase_q + wrap_offset
        rows.append({
            "phase_sample_index": valid_row_index,
            "raw_sample_index": int(raw_index),
            "i_baseband": i_value,
            "q_baseband": q_value,
            "phase_wrapped_q4_20": wrapped_phase_q,
            "phase_unwrapped_q4_20": unwrapped_phase_q,
            "wrap_offset_q4_20": wrap_offset,
            "phase_wrapped_rad": wrapped_phase_q / ANGLE_SCALE,
            "phase_unwrapped_rad": unwrapped_phase_q / ANGLE_SCALE,
        })

        prev_phase = wrapped_phase_q
        have_prev = True
        valid_row_index += 1

    return rows


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
                "phase_sample_index": int(row["phase_sample_index"]),
                "i_baseband": int(row["i_baseband"]),
                "q_baseband": int(row["q_baseband"]),
                "phase_wrapped_q4_20": int(row["phase_wrapped_q4_20"]),
                "phase_unwrapped_q4_20": int(row["phase_unwrapped_q4_20"]),
                "wrap_offset_q4_20": int(row["wrap_offset_q4_20"]),
            })
    return rows


def compare_sim_with_expected(expected):
    sim_path = OUT_DIR / "ddc_phase_unwrap_core_sim.csv"
    if not sim_path.exists():
        print("No Vivado/XSim CSV found yet.")
        print(f"Expected simulation output path: {sim_path}")
        return

    expected_by_index = {row["phase_sample_index"]: row for row in expected}
    sim_rows = read_sim_csv(sim_path)
    mismatches = []

    for actual in sim_rows:
        expected_row = expected_by_index[actual["phase_sample_index"]]
        for key in [
            "i_baseband",
            "q_baseband",
            "phase_wrapped_q4_20",
            "phase_unwrapped_q4_20",
            "wrap_offset_q4_20",
        ]:
            if actual[key] != expected_row[key]:
                mismatches.append((actual["phase_sample_index"], key, expected_row[key], actual[key]))

    if mismatches:
        print(f"Found {len(mismatches)} mismatches.")
        for item in mismatches[:12]:
            print(f"phase_sample_index={item[0]} {item[1]} expected={item[2]} actual={item[3]}")
    else:
        print(f"Vivado/XSim output matches expected DDC phase unwrap core output for {len(sim_rows)} samples.")


def analyze_unwrapped(rows):
    index = np.array([row["phase_sample_index"] for row in rows], dtype=float)
    phase = np.array([row["phase_unwrapped_rad"] for row in rows])
    t = index / FS
    slope, intercept = np.polyfit(t, phase, 1)
    fitted = slope * t + intercept
    detrended = phase - fitted
    return {
        "index": index,
        "wrapped": np.array([row["phase_wrapped_rad"] for row in rows]),
        "unwrapped": phase,
        "fitted": fitted,
        "detrended": detrended,
        "estimated_offset_hz": slope / (2 * np.pi),
        "detrended_span_rad": float(np.max(detrended) - np.min(detrended)),
    }


def plot_rows(rows, analysis):
    fig, axes = plt.subplots(3, 1, figsize=(12, 9), sharex=True)

    axes[0].plot(analysis["index"], analysis["wrapped"], linewidth=0.9)
    axes[0].set_title("DDC + CORDIC wrapped phase")
    axes[0].set_ylabel("phase (rad)")
    axes[0].grid(True, alpha=0.3)

    axes[1].plot(analysis["index"], analysis["unwrapped"], linewidth=0.9, label="unwrapped")
    axes[1].plot(analysis["index"], analysis["fitted"], linewidth=0.9, label="linear fit")
    axes[1].set_title("DDC + CORDIC + unwrap")
    axes[1].set_ylabel("phase (rad)")
    axes[1].grid(True, alpha=0.3)
    axes[1].legend(loc="upper left")

    axes[2].plot(analysis["index"], analysis["detrended"], linewidth=0.9)
    axes[2].set_title("Detrended residual")
    axes[2].set_xlabel("phase sample index")
    axes[2].set_ylabel("phase (rad)")
    axes[2].grid(True, alpha=0.3)

    fig.tight_layout()
    png_path = OUT_DIR / "ddc_phase_unwrap_core_expected.png"
    fig.savefig(png_path, dpi=150)
    return png_path


def main():
    RTL_DIR.mkdir(parents=True, exist_ok=True)
    SIM_DIR.mkdir(parents=True, exist_ok=True)
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    ensure_dds_roms()

    adc_samples = make_adc_samples()
    write_mem(SIM_DIR / "phase_unwrap_core_sample_20000.mem", adc_samples, SAMPLE_BITS)

    i_baseband, q_baseband, valid = ddc_iq(adc_samples)
    rows = expected_rows(i_baseband, q_baseband, valid)
    expected_csv = OUT_DIR / "ddc_phase_unwrap_core_expected.csv"
    write_csv(expected_csv, rows)

    analysis = analyze_unwrapped(rows)
    plot_path = plot_rows(rows, analysis)

    print("Lesson 33 DDC phase unwrap core assets prepared")
    print(f"input waveform: CW phase={INITIAL_PHASE_DEG} deg, carrier offset={CARRIER_OFFSET_HZ} Hz")
    print(f"adc samples = {len(adc_samples)}")
    print(f"expected unwrapped phase samples = {len(rows)}")
    print(f"estimated_carrier_offset_hz = {analysis['estimated_offset_hz']:.3f}")
    print(f"detrended_phase_span_rad = {analysis['detrended_span_rad']:.6f}")
    print(f"sample mem: {(SIM_DIR / 'phase_unwrap_core_sample_20000.mem').resolve()}")
    print(f"expected csv: {expected_csv.resolve()}")
    print(f"expected plot: {plot_path.resolve()}")
    print()
    print("First 10 expected samples:")
    print("phase_sample_index,raw_sample_index,I,Q,wrapped_q4_20,unwrapped_q4_20,offset_q4_20")
    for row in rows[:10]:
        print(
            f"{row['phase_sample_index']},{row['raw_sample_index']},"
            f"{row['i_baseband']},{row['q_baseband']},"
            f"{row['phase_wrapped_q4_20']},{row['phase_unwrapped_q4_20']},"
            f"{row['wrap_offset_q4_20']}"
        )
    print()
    compare_sim_with_expected(rows)


if __name__ == "__main__":
    main()
