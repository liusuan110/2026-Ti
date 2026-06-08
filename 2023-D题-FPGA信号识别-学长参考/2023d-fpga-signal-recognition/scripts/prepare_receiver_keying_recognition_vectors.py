from pathlib import Path
import csv
import numpy as np


FS = 20_000_000
FC = 2_000_000
DECIM = 80
FB = FS // DECIM
BLOCK_LEN = 1024
DDC_SAMPLES_PER_CASE = BLOCK_LEN * 2 + 1
RAW_SAMPLES_PER_CASE = DECIM * DDC_SAMPLES_PER_CASE
RATES = [6_000, 8_000, 10_000]
CASE_SPECS = [
    ("2ASK", 0, 0.0),
    ("2ASK", 1, 0.0),
    ("2ASK", 2, 0.0),
    ("2PSK", 0, 0.0),
    ("2PSK", 1, 0.0),
    ("2PSK", 2, 0.0),
    ("2FSK", 0, 2.0),
    ("2FSK", 1, 3.5),
    ("2FSK", 2, 5.0),
]
CASE_COUNT = len(CASE_SPECS)

SAMPLE_BITS = 8
LO_BITS = 12
PHASE_BITS = 24
BIN_BITS = 4
BIN_COUNT = 1 << BIN_BITS
SCORE_BITS = 10
STEPS = [402_653, 536_871, 671_089]
COOLDOWN = 8
ANGLE_FRAC_BITS = 20
ANGLE_SCALE = 1 << ANGLE_FRAC_BITS
ITERATIONS = 20
DOT_SHIFT = 32
CORDIC_DATA_BITS = 25
TWO_PI_Q4_20 = 6_588_398
KIND_CODES = {
    "2ASK": 4,
    "2PSK": 5,
    "2FSK": 6,
}


def twos_hex(value, bits):
    value = int(value)
    if value < 0:
        value = (1 << bits) + value
    return f"{value & ((1 << bits) - 1):0{(bits + 3) // 4}x}"


def signed_clip(value, bits):
    lo = -(1 << (bits - 1))
    hi = (1 << (bits - 1)) - 1
    return int(np.clip(np.round(value), lo, hi))


def arshift(value, shift):
    return int(value) >> shift


def wrap_pi_q(angle_q):
    pi_q = int(round(np.pi * ANGLE_SCALE))
    two_pi_q = 2 * pi_q
    while angle_q > pi_q:
        angle_q -= two_pi_q
    while angle_q < -pi_q:
        angle_q += two_pi_q
    return angle_q


def atan_table_q():
    return [int(round(np.arctan(2.0 ** -i) * ANGLE_SCALE)) for i in range(ITERATIONS)]


ATAN_TABLE = atan_table_q()


def pipeline_cordic_model(i_value, q_value):
    pi_q = int(round(np.pi * ANGLE_SCALE))
    x = int(i_value)
    y = int(q_value)
    z = 0

    if x < 0:
        x = -x
        y = -y
        z = pi_q if q_value >= 0 else -pi_q

    for step, angle_q in enumerate(ATAN_TABLE):
        if y == 0:
            continue
        x_shift = x >> step
        y_shift = y >> step
        if y > 0:
            x, y, z = x + y_shift, y - x_shift, z + angle_q
        else:
            x, y, z = x - y_shift, y + x_shift, z - angle_q
    return wrap_pi_q(z)


def limited_run_bits(count, rng, max_run=3):
    bits = []
    last = int(rng.integers(0, 2))
    run = 0
    for _ in range(count):
        if run >= max_run:
            bit = 1 - last
        else:
            bit = int(rng.integers(0, 2))
        if bit == last:
            run += 1
        else:
            last = bit
            run = 1
        bits.append(bit)
    return np.array(bits, dtype=np.int8)


def make_carrier_lo():
    n = np.arange(RAW_SAMPLES_PER_CASE)
    t = n / FS
    cosv = np.array(
        [signed_clip(2047 * np.cos(2 * np.pi * FC * ti), LO_BITS) for ti in t],
        dtype=np.int64,
    )
    sinv = np.array(
        [signed_clip(2047 * np.sin(2 * np.pi * FC * ti), LO_BITS) for ti in t],
        dtype=np.int64,
    )
    return cosv, sinv


def symbol_bits_for_rate(rate, rng):
    total_time = RAW_SAMPLES_PER_CASE / FS
    offset = rng.uniform(0.0, 1.0 / rate)
    symbol_count = int(np.ceil((total_time + offset) * rate)) + 8
    return offset, limited_run_bits(symbol_count, rng)


def symbol_stream(rate, offset, bits):
    n = np.arange(RAW_SAMPLES_PER_CASE)
    t = n / FS
    bit_index = np.floor((t + offset) * rate).astype(np.int64)
    bit_index = np.clip(bit_index, 0, len(bits) - 1)
    return t, bits[bit_index]


def make_ask_signal(rate, case_idx, rng):
    offset, bits = symbol_bits_for_rate(rate, rng)
    t, b = symbol_stream(rate, offset, bits)
    amp = np.where(b > 0, 74.0, 5.5)
    carrier_phase = 0.18 + 0.19 * case_idx
    x = amp * np.cos(2 * np.pi * FC * t + carrier_phase)
    return np.array([signed_clip(v, SAMPLE_BITS) for v in x], dtype=np.int64)


def make_psk_signal(rate, case_idx, rng):
    offset, bits = symbol_bits_for_rate(rate, rng)
    t, b = symbol_stream(rate, offset, bits)
    carrier_phase = 0.05 + 0.11 * case_idx
    phase = np.where(b > 0, np.pi, 0.0)
    x = 76.0 * np.cos(2 * np.pi * FC * t + carrier_phase + phase)
    return np.array([signed_clip(v, SAMPLE_BITS) for v in x], dtype=np.int64)


def make_fsk_signal(rate, h, case_idx, rng):
    offset, bits = symbol_bits_for_rate(rate, rng)
    t, b = symbol_stream(rate, offset, bits)
    sep = h * rate
    inst_freq = FC + np.where(b > 0, sep / 2.0, -sep / 2.0)
    phase = 0.11 + 0.09 * case_idx + 2 * np.pi * np.cumsum(inst_freq) / FS
    x = 76.0 * np.cos(phase)
    return np.array([signed_clip(v, SAMPLE_BITS) for v in x], dtype=np.int64)


def ddc_reference(sample, carrier_cos, carrier_sin):
    i_prod = sample * carrier_cos
    q_prod = sample * (-carrier_sin)
    i_sum = i_prod.reshape(-1, DECIM).sum(axis=1)
    q_sum = q_prod.reshape(-1, DECIM).sum(axis=1)
    return i_sum.astype(np.int64), q_sum.astype(np.int64)


def phase_delta_stream(i_sum, q_sum):
    out = []
    for idx in range(1, len(i_sum)):
        dot = int(i_sum[idx] * i_sum[idx - 1] + q_sum[idx] * q_sum[idx - 1])
        cross = int(q_sum[idx] * i_sum[idx - 1] - i_sum[idx] * q_sum[idx - 1])
        dot_scaled = signed_clip(arshift(dot, DOT_SHIFT), CORDIC_DATA_BITS)
        cross_scaled = signed_clip(arshift(cross, DOT_SHIFT), CORDIC_DATA_BITS)
        out.append(pipeline_cordic_model(dot_scaled, cross_scaled))
    return np.array(out, dtype=np.int64)


def transition_rate_model(stream):
    phases = [0, 0, 0]
    bins = [[0 for _ in range(BIN_COUNT)] for _ in range(3)]
    prev_valid = False
    prev = 0
    transition_count = 0

    for bit in stream:
        next_phases = [(phases[i] + STEPS[i]) & ((1 << PHASE_BITS) - 1) for i in range(3)]
        transition = prev_valid and (int(bit) != prev)
        if transition:
            transition_count += 1
            for i in range(3):
                bin_idx = next_phases[i] >> (PHASE_BITS - BIN_BITS)
                bins[i][bin_idx] += 1
        phases = next_phases
        prev_valid = True
        prev = int(bit)

    scores = [max(row) for row in bins]
    best = max(range(3), key=lambda i: (scores[i], i))
    return best, scores, transition_count


def fsk_parameter_reference(sample, carrier_cos, carrier_sin):
    i_sum, q_sum = ddc_reference(sample, carrier_cos, carrier_sin)
    phase_delta = phase_delta_stream(i_sum, q_sum)
    train = phase_delta[:BLOCK_LEN]
    phase_min = int(np.min(train))
    phase_max = int(np.max(train))
    threshold = (phase_min + phase_max) >> 1
    sep_phase = phase_max - phase_min
    freq_sep_hz = (sep_phase * FB + (TWO_PI_Q4_20 // 2)) // TWO_PI_Q4_20
    bits = (phase_delta[BLOCK_LEN:2 * BLOCK_LEN] > threshold).astype(np.int8)
    best, scores, transition_count = transition_rate_model(bits)
    rc_hz = RATES[best]
    h_tenths = (freq_sep_hz * 10 + rc_hz // 2) // rc_hz
    return int(freq_sep_hz), int(h_tenths), best, scores, transition_count


def main():
    rng = np.random.default_rng(20230519)
    sim_dir = Path("sim")
    out_dir = Path("outputs/receiver_keying_recognition")
    out_dir.mkdir(parents=True, exist_ok=True)

    carrier_cos_one, carrier_sin_one = make_carrier_lo()
    all_sample = []
    all_carrier_cos = []
    all_carrier_sin = []
    rows = []

    for case_idx, (kind, rate_idx, h_value) in enumerate(CASE_SPECS):
        rate = RATES[rate_idx]
        if kind == "2ASK":
            sample = make_ask_signal(rate, case_idx, rng)
            freq_sep_hz = 0
            h_tenths = 0
        elif kind == "2PSK":
            sample = make_psk_signal(rate, case_idx, rng)
            freq_sep_hz = 0
            h_tenths = 0
        else:
            sample = make_fsk_signal(rate, h_value, case_idx, rng)
            freq_sep_hz, h_tenths, _, _, _ = fsk_parameter_reference(
                sample,
                carrier_cos_one,
                carrier_sin_one,
            )

        all_sample.extend(sample.tolist())
        all_carrier_cos.extend(carrier_cos_one.tolist())
        all_carrier_sin.extend(carrier_sin_one.tolist())
        rows.append({
            "case": case_idx,
            "kind": kind,
            "expected_kind_code": KIND_CODES[kind],
            "expected_rc_index": rate_idx,
            "expected_rc_hz": rate,
            "expected_freq_sep_hz": freq_sep_hz,
            "expected_h_tenths": h_tenths,
            "true_h": h_value,
        })

    (sim_dir / "receiver_keying_recognition_sample.mem").write_text(
        "\n".join(twos_hex(v, SAMPLE_BITS) for v in all_sample) + "\n",
        encoding="ascii",
    )
    (sim_dir / "receiver_keying_recognition_carrier_cos.mem").write_text(
        "\n".join(twos_hex(v, LO_BITS) for v in all_carrier_cos) + "\n",
        encoding="ascii",
    )
    (sim_dir / "receiver_keying_recognition_carrier_sin.mem").write_text(
        "\n".join(twos_hex(v, LO_BITS) for v in all_carrier_sin) + "\n",
        encoding="ascii",
    )

    for key, bits in [
        ("expected_kind_code", 3),
        ("expected_rc_index", 2),
        ("expected_rc_hz", 16),
        ("expected_freq_sep_hz", 16),
        ("expected_h_tenths", 8),
    ]:
        (sim_dir / f"receiver_keying_recognition_{key}.mem").write_text(
            "\n".join(twos_hex(row[key], bits) for row in rows) + "\n",
            encoding="ascii",
        )

    csv_path = out_dir / "receiver_keying_recognition_expected.csv"
    with csv_path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)

    print(csv_path.resolve())


if __name__ == "__main__":
    main()
