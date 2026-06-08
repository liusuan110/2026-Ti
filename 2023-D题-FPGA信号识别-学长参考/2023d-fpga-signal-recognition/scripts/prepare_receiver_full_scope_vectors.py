from pathlib import Path
import csv
import numpy as np


FS = 20_000_000
FC = 2_000_000
DECIM = 80
FB = FS // DECIM
TONE_BLOCK_LEN = 250
KEY_BLOCK_LEN = 1024
DDC_SAMPLES_PER_CASE = KEY_BLOCK_LEN * 2 + 1
RAW_SAMPLES_PER_CASE = DECIM * DDC_SAMPLES_PER_CASE
CANDIDATES = [1_000, 2_000, 3_000, 4_000, 5_000]
RATES = [6_000, 8_000, 10_000]
CASE_SPECS = [
    ("CW", 0, 0.0),
    ("AM", 2, 0.0),
    ("FM", 3, 0.0),
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

SAMPLE_BITS = 8
LO_BITS = 12
REF_BITS = 12
MAG_SHIFT = 24
CROSS_SHIFT = 22
DOT_SHIFT = 32
CORDIC_DATA_BITS = 25
ANGLE_FRAC_BITS = 20
ANGLE_SCALE = 1 << ANGLE_FRAC_BITS
ITERATIONS = 20
TWO_PI_Q4_20 = 6_588_398

COARSE_BLOCK_LEN = 64
MAG_RANGE_PERCENT = 25
PHASE_ACTIVITY_PERCENT = 8
NEG_DOT_THRESHOLD = 1
FM_SCORE_RATIO_SHIFT = 4

PHASE_BITS = 24
BIN_BITS = 4
BIN_COUNT = 1 << BIN_BITS
STEPS = [402_653, 536_871, 671_089]

KIND_CODES = {
    "CW": 0,
    "AM": 1,
    "FM": 2,
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
    cosv = np.array([signed_clip(2047 * np.cos(2 * np.pi * FC * ti), LO_BITS) for ti in t], dtype=np.int64)
    sinv = np.array([signed_clip(2047 * np.sin(2 * np.pi * FC * ti), LO_BITS) for ti in t], dtype=np.int64)
    return cosv, sinv


def make_refs_raw():
    n = np.arange(RAW_SAMPLES_PER_CASE)
    bb_index = n // DECIM
    tb = bb_index / FB
    ref_cos = []
    ref_sin = []
    for freq in CANDIDATES:
        ref_cos.append(np.array([signed_clip(2047 * np.cos(2 * np.pi * freq * ti), REF_BITS) for ti in tb], dtype=np.int64))
        ref_sin.append(np.array([signed_clip(2047 * np.sin(2 * np.pi * freq * ti), REF_BITS) for ti in tb], dtype=np.int64))
    return ref_cos, ref_sin


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


def make_signal(kind, value, h_value, case_idx, rng):
    n = np.arange(RAW_SAMPLES_PER_CASE)
    t = n / FS
    carrier_phase = 0.17 + 0.09 * case_idx
    mod_phase = 0.31 + 0.07 * case_idx

    if kind == "CW":
        x = 70.0 * np.cos(2 * np.pi * FC * t + carrier_phase)
    elif kind == "AM":
        freq = CANDIDATES[value]
        ma = 0.55
        env = 1.0 + ma * np.sin(2 * np.pi * freq * t + mod_phase)
        x = 50.0 * env * np.cos(2 * np.pi * FC * t + carrier_phase)
    elif kind == "FM":
        freq = CANDIDATES[value]
        mf = 1.7
        phase = 2 * np.pi * FC * t + carrier_phase
        phase = phase + mf * np.sin(2 * np.pi * freq * t + mod_phase)
        x = 76.0 * np.cos(phase)
    elif kind == "2ASK":
        rate = RATES[value]
        offset, bits = symbol_bits_for_rate(rate, rng)
        t, b = symbol_stream(rate, offset, bits)
        amp = np.where(b > 0, 74.0, 5.5)
        x = amp * np.cos(2 * np.pi * FC * t + carrier_phase)
    elif kind == "2PSK":
        rate = RATES[value]
        offset, bits = symbol_bits_for_rate(rate, rng)
        t, b = symbol_stream(rate, offset, bits)
        phase = np.where(b > 0, np.pi, 0.0)
        x = 76.0 * np.cos(2 * np.pi * FC * t + carrier_phase + phase)
    elif kind == "2FSK":
        rate = RATES[value]
        offset, bits = symbol_bits_for_rate(rate, rng)
        _, b = symbol_stream(rate, offset, bits)
        sep = h_value * rate
        inst_freq = FC + np.where(b > 0, sep / 2.0, -sep / 2.0)
        phase = carrier_phase + 2 * np.pi * np.cumsum(inst_freq) / FS
        x = 76.0 * np.cos(phase)
    else:
        raise ValueError(kind)

    return np.array([signed_clip(v, SAMPLE_BITS) for v in x], dtype=np.int64)


def ddc_reference(sample, carrier_cos, carrier_sin):
    i_prod = sample * carrier_cos
    q_prod = sample * (-carrier_sin)
    i_sum = i_prod.reshape(-1, DECIM).sum(axis=1)
    q_sum = q_prod.reshape(-1, DECIM).sum(axis=1)
    return i_sum.astype(np.int64), q_sum.astype(np.int64)


def score_feature(feature, refs_cos, refs_sin):
    powers = []
    for c, s in zip(refs_cos, refs_sin):
        i_score = int(np.sum(feature * c))
        q_score = int(np.sum(feature * s))
        powers.append(int(i_score * i_score + q_score * q_score))
    best_index = int(np.argmax(powers))
    return best_index, powers


def tone_expected(i_sum, q_sum, refs_raw_cos, refs_raw_sin):
    mag_sq = i_sum * i_sum + q_sum * q_sum
    cross = np.zeros_like(i_sum)
    for idx in range(1, len(i_sum)):
        cross[idx] = q_sum[idx] * i_sum[idx - 1] - i_sum[idx] * q_sum[idx - 1]

    refs_cos = [r[DECIM - 1::DECIM] for r in refs_raw_cos]
    refs_sin = [r[DECIM - 1::DECIM] for r in refs_raw_sin]
    am_feature = mag_sq[:TONE_BLOCK_LEN] >> MAG_SHIFT
    am_best, am_powers = score_feature(am_feature, [r[:TONE_BLOCK_LEN] for r in refs_cos], [r[:TONE_BLOCK_LEN] for r in refs_sin])
    fm_feature = cross[1:TONE_BLOCK_LEN + 1] >> CROSS_SHIFT
    fm_best, fm_powers = score_feature(fm_feature, [r[1:TONE_BLOCK_LEN + 1] for r in refs_cos], [r[1:TONE_BLOCK_LEN + 1] for r in refs_sin])
    return am_best, fm_best, max(am_powers), max(fm_powers)


def coarse_expected(i_sum, q_sum):
    i = i_sum[:COARSE_BLOCK_LEN]
    q = q_sum[:COARSE_BLOCK_LEN]
    mag_sq = i * i + q * q
    dot = np.zeros(COARSE_BLOCK_LEN, dtype=np.int64)
    cross = np.zeros(COARSE_BLOCK_LEN, dtype=np.int64)
    for idx in range(1, COARSE_BLOCK_LEN):
        dot[idx] = i[idx] * i[idx - 1] + q[idx] * q[idx - 1]
        cross[idx] = q[idx] * i[idx - 1] - i[idx] * q[idx - 1]

    mag_min = int(np.min(mag_sq))
    mag_max = int(np.max(mag_sq))
    mag_sum = int(np.sum(mag_sq))
    abs_cross_sum = int(np.sum(np.abs(cross[1:])))
    negative_dot_count = int(np.sum(dot[1:] < 0))
    mag_range = mag_max - mag_min
    mag_active = int(mag_range * COARSE_BLOCK_LEN * 100 >= mag_sum * MAG_RANGE_PERCENT)
    phase_active = int(abs_cross_sum * 100 >= mag_sum * PHASE_ACTIVITY_PERCENT)
    phase_flip_active = int(negative_dot_count >= NEG_DOT_THRESHOLD)
    return mag_active, phase_active, phase_flip_active


def ma_lut(block_min, block_max):
    range_w = int(block_max) - int(block_min)
    sum_w = int(block_max) + int(block_min)
    lhs = range_w * 1000
    if lhs < sum_w * 623:
        return 300
    if lhs < sum_w * 748:
        return 400
    if lhs < sum_w * 846:
        return 500
    if lhs < sum_w * 914:
        return 600
    if lhs < sum_w * 960:
        return 700
    if lhs < sum_w * 986:
        return 800
    return 900


def am_ma_expected(i_sum, q_sum):
    mag_sq = i_sum * i_sum + q_sum * q_sum
    return ma_lut(int(np.min(mag_sq[:TONE_BLOCK_LEN])), int(np.max(mag_sq[:TONE_BLOCK_LEN])))


def phase_delta_stream(i_sum, q_sum):
    out = []
    for idx in range(1, len(i_sum)):
        dot = int(i_sum[idx] * i_sum[idx - 1] + q_sum[idx] * q_sum[idx - 1])
        cross = int(q_sum[idx] * i_sum[idx - 1] - i_sum[idx] * q_sum[idx - 1])
        dot_scaled = signed_clip(arshift(dot, DOT_SHIFT), CORDIC_DATA_BITS)
        cross_scaled = signed_clip(arshift(cross, DOT_SHIFT), CORDIC_DATA_BITS)
        out.append(pipeline_cordic_model(dot_scaled, cross_scaled))
    return np.array(out, dtype=np.int64)


def fm_delta_expected(i_sum, q_sum, freq_index):
    phases = phase_delta_stream(i_sum[:TONE_BLOCK_LEN + 1], q_sum[:TONE_BLOCK_LEN + 1])
    max_abs_phase_delta = max(abs(v) for v in phases)
    delta_f_hz = (max_abs_phase_delta * FB + (TWO_PI_Q4_20 // 2)) // TWO_PI_Q4_20
    mod_freq = CANDIDATES[freq_index]
    mf_tenths = (delta_f_hz * 10 + mod_freq // 2) // mod_freq
    return int(delta_f_hz), int(mf_tenths)


def fsk_parameter_reference(sample, carrier_cos, carrier_sin):
    i_sum, q_sum = ddc_reference(sample, carrier_cos, carrier_sin)
    phase_delta = phase_delta_stream(i_sum, q_sum)
    train = phase_delta[:KEY_BLOCK_LEN]
    phase_min = int(np.min(train))
    phase_max = int(np.max(train))
    threshold = (phase_min + phase_max) >> 1
    sep_phase = phase_max - phase_min
    freq_sep_hz = (sep_phase * FB + (TWO_PI_Q4_20 // 2)) // TWO_PI_Q4_20
    bits = (phase_delta[KEY_BLOCK_LEN:2 * KEY_BLOCK_LEN] > threshold).astype(np.int8)
    phases = [0, 0, 0]
    bins = [[0 for _ in range(BIN_COUNT)] for _ in range(3)]
    prev_valid = False
    prev = 0
    for bit in bits:
        next_phases = [(phases[i] + STEPS[i]) & ((1 << PHASE_BITS) - 1) for i in range(3)]
        if prev_valid and int(bit) != prev:
            for i in range(3):
                bins[i][next_phases[i] >> (PHASE_BITS - BIN_BITS)] += 1
        phases = next_phases
        prev_valid = True
        prev = int(bit)
    scores = [max(row) for row in bins]
    best = max(range(3), key=lambda i: (scores[i], i))
    rc_hz = RATES[best]
    h_tenths = (freq_sep_hz * 10 + rc_hz // 2) // rc_hz
    return int(freq_sep_hz), int(h_tenths)


def expected_for_case(kind, value, h_value, sample, carrier_cos, carrier_sin, refs_cos, refs_sin):
    i_sum, q_sum = ddc_reference(sample, carrier_cos, carrier_sin)
    am_best, fm_best, am_best_power, fm_best_power = tone_expected(i_sum, q_sum, refs_cos, refs_sin)
    mag_active, phase_active, phase_flip_active = coarse_expected(i_sum, q_sum)
    fm_tone_active = int(fm_best_power > (am_best_power << FM_SCORE_RATIO_SHIFT))
    am_ma = am_ma_expected(i_sum, q_sum)
    fm_delta_f, fm_mf = fm_delta_expected(i_sum, q_sum, fm_best)

    if kind == "AM":
        mod_index = am_best
    elif kind == "FM":
        mod_index = fm_best
    else:
        mod_index = 0

    freq_sep_hz = 0
    h_tenths = 0
    if kind == "2FSK":
        freq_sep_hz, h_tenths = fsk_parameter_reference(sample, carrier_cos, carrier_sin)

    return {
        "expected_kind_code": KIND_CODES[kind],
        "expected_mod_freq_valid": int(kind in ("AM", "FM")),
        "expected_mod_freq_hz": CANDIDATES[mod_index] if kind in ("AM", "FM") else 0,
        "expected_am_ma_valid": int(kind == "AM"),
        "expected_am_ma_permille": am_ma if kind == "AM" else 0,
        "expected_fm_param_valid": int(kind == "FM"),
        "expected_fm_delta_f_hz": fm_delta_f if kind == "FM" else 0,
        "expected_fm_mf_tenths": fm_mf if kind == "FM" else 0,
        "expected_keying_param_valid": int(kind in ("2ASK", "2PSK", "2FSK")),
        "expected_rc_hz": RATES[value] if kind in ("2ASK", "2PSK", "2FSK") else 0,
        "expected_fsk_param_valid": int(kind == "2FSK"),
        "expected_fsk_freq_sep_hz": freq_sep_hz,
        "expected_fsk_h_tenths": h_tenths,
        "analog_reference_kind": 3 if phase_flip_active else (2 if (phase_active or fm_tone_active) else (1 if mag_active else 0)),
        "am_best": am_best,
        "fm_best": fm_best,
        "true_h": h_value,
    }


def main():
    rng = np.random.default_rng(20230520)
    sim_dir = Path("sim")
    out_dir = Path("outputs/receiver_full_scope")
    out_dir.mkdir(parents=True, exist_ok=True)

    carrier_cos_one, carrier_sin_one = make_carrier_lo()
    refs_cos_one, refs_sin_one = make_refs_raw()
    all_sample = []
    all_carrier_cos = []
    all_carrier_sin = []
    all_ref_cos = [[] for _ in CANDIDATES]
    all_ref_sin = [[] for _ in CANDIDATES]
    rows = []

    for case_idx, (kind, value, h_value) in enumerate(CASE_SPECS):
        sample = make_signal(kind, value, h_value, case_idx, rng)
        row = {
            "case": case_idx,
            "kind": kind,
            "value": value,
        }
        row.update(expected_for_case(kind, value, h_value, sample, carrier_cos_one, carrier_sin_one, refs_cos_one, refs_sin_one))
        rows.append(row)

        all_sample.extend(sample.tolist())
        all_carrier_cos.extend(carrier_cos_one.tolist())
        all_carrier_sin.extend(carrier_sin_one.tolist())
        for cand_idx in range(len(CANDIDATES)):
            all_ref_cos[cand_idx].extend(refs_cos_one[cand_idx].tolist())
            all_ref_sin[cand_idx].extend(refs_sin_one[cand_idx].tolist())

    (sim_dir / "receiver_full_scope_sample.mem").write_text(
        "\n".join(twos_hex(v, SAMPLE_BITS) for v in all_sample) + "\n",
        encoding="ascii",
    )
    (sim_dir / "receiver_full_scope_carrier_cos.mem").write_text(
        "\n".join(twos_hex(v, LO_BITS) for v in all_carrier_cos) + "\n",
        encoding="ascii",
    )
    (sim_dir / "receiver_full_scope_carrier_sin.mem").write_text(
        "\n".join(twos_hex(v, LO_BITS) for v in all_carrier_sin) + "\n",
        encoding="ascii",
    )
    for idx in range(len(CANDIDATES)):
        (sim_dir / f"receiver_full_scope_ref{idx}_cos.mem").write_text(
            "\n".join(twos_hex(v, REF_BITS) for v in all_ref_cos[idx]) + "\n",
            encoding="ascii",
        )
        (sim_dir / f"receiver_full_scope_ref{idx}_sin.mem").write_text(
            "\n".join(twos_hex(v, REF_BITS) for v in all_ref_sin[idx]) + "\n",
            encoding="ascii",
        )

    for key, bits in [
        ("expected_kind_code", 3),
        ("expected_mod_freq_valid", 1),
        ("expected_mod_freq_hz", 16),
        ("expected_am_ma_valid", 1),
        ("expected_am_ma_permille", 10),
        ("expected_fm_param_valid", 1),
        ("expected_fm_delta_f_hz", 16),
        ("expected_fm_mf_tenths", 8),
        ("expected_keying_param_valid", 1),
        ("expected_rc_hz", 16),
        ("expected_fsk_param_valid", 1),
        ("expected_fsk_freq_sep_hz", 16),
        ("expected_fsk_h_tenths", 8),
    ]:
        (sim_dir / f"receiver_full_scope_{key}.mem").write_text(
            "\n".join(twos_hex(row[key], bits) for row in rows) + "\n",
            encoding="ascii",
        )

    csv_path = out_dir / "receiver_full_scope_expected.csv"
    with csv_path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)

    print(csv_path.resolve())


if __name__ == "__main__":
    main()
