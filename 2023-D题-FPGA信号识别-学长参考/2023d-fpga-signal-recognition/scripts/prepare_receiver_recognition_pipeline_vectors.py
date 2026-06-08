from pathlib import Path
import csv
import numpy as np


FS = 20_000_000
FC = 2_000_000
DECIM = 80
FB = FS // DECIM
TONE_BLOCK_LEN = 250
DDC_SAMPLES_PER_CASE = TONE_BLOCK_LEN + 1
RAW_SAMPLES_PER_CASE = DECIM * DDC_SAMPLES_PER_CASE
CANDIDATES = [1_000, 2_000, 3_000, 4_000, 5_000]

SAMPLE_BITS = 8
LO_BITS = 12
REF_BITS = 12
DATA_BITS = 32
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
ASK_MINMAX_PERCENT = 8
PHASE_ACTIVITY_PERCENT = 8
NEG_DOT_THRESHOLD = 1

MODE_CW = 0
MODE_AM = 1
MODE_FM = 2

KIND_CW = 0
KIND_AM_LIKE = 1
KIND_FM_LIKE = 2
KIND_PSK_LIKE = 3
FM_SCORE_RATIO_SHIFT = 4


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


def make_signal(mode, freq, case_idx):
    n = np.arange(RAW_SAMPLES_PER_CASE)
    t = n / FS
    carrier_phase = 0.17 + 0.23 * case_idx
    mod_phase = 0.31 + 0.11 * case_idx

    if mode == MODE_CW:
        x = 70.0 * np.cos(2 * np.pi * FC * t + carrier_phase)
    elif mode == MODE_AM:
        ma = 0.55
        env = 1.0 + ma * np.sin(2 * np.pi * freq * t + mod_phase)
        x = 50.0 * env * np.cos(2 * np.pi * FC * t + carrier_phase)
    elif mode == MODE_FM:
        mf = 1.7
        phase = 2 * np.pi * FC * t + carrier_phase
        phase = phase + mf * np.sin(2 * np.pi * freq * t + mod_phase)
        x = 76.0 * np.cos(phase)
    else:
        raise ValueError(mode)

    return np.array([signed_clip(v, SAMPLE_BITS) for v in x], dtype=np.int64)


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
    am_best, am_powers = score_feature(
        am_feature,
        [r[:TONE_BLOCK_LEN] for r in refs_cos],
        [r[:TONE_BLOCK_LEN] for r in refs_sin],
    )

    fm_feature = cross[1:TONE_BLOCK_LEN + 1] >> CROSS_SHIFT
    fm_best, fm_powers = score_feature(
        fm_feature,
        [r[1:TONE_BLOCK_LEN + 1] for r in refs_cos],
        [r[1:TONE_BLOCK_LEN + 1] for r in refs_sin],
    )

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
    ask_like = int(bool(mag_active) and mag_min * 100 <= mag_max * ASK_MINMAX_PERCENT)
    phase_active = int(abs_cross_sum * 100 >= mag_sum * PHASE_ACTIVITY_PERCENT)
    phase_flip_active = int(negative_dot_count >= NEG_DOT_THRESHOLD)
    coarse_flags = (phase_flip_active << 2) | (phase_active << 1) | mag_active

    if phase_flip_active:
        kind = KIND_PSK_LIKE
    elif phase_active:
        kind = KIND_FM_LIKE
    elif mag_active:
        kind = KIND_AM_LIKE
    else:
        kind = KIND_CW

    return {
        "kind": kind,
        "mag_active": mag_active,
        "ask_like": ask_like,
        "phase_active": phase_active,
        "phase_flip_active": phase_flip_active,
        "coarse_flags": coarse_flags,
    }


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
    block_min = int(np.min(mag_sq[:TONE_BLOCK_LEN]))
    block_max = int(np.max(mag_sq[:TONE_BLOCK_LEN]))
    return ma_lut(block_min, block_max)


def fm_delta_expected(i_sum, q_sum, freq_index):
    phases = []
    for idx in range(1, TONE_BLOCK_LEN + 1):
        dot = int(i_sum[idx] * i_sum[idx - 1] + q_sum[idx] * q_sum[idx - 1])
        cross = int(q_sum[idx] * i_sum[idx - 1] - i_sum[idx] * q_sum[idx - 1])
        dot_scaled = signed_clip(arshift(dot, DOT_SHIFT), CORDIC_DATA_BITS)
        cross_scaled = signed_clip(arshift(cross, DOT_SHIFT), CORDIC_DATA_BITS)
        phases.append(pipeline_cordic_model(dot_scaled, cross_scaled))

    max_abs_phase_delta = max(abs(v) for v in phases)
    delta_f_hz = (max_abs_phase_delta * FB + (TWO_PI_Q4_20 // 2)) // TWO_PI_Q4_20
    mod_freq = CANDIDATES[freq_index]
    mf_tenths = (delta_f_hz * 10 + mod_freq // 2) // mod_freq
    return int(delta_f_hz), int(mf_tenths)


def main():
    sim_dir = Path("sim")
    out_dir = Path("outputs/receiver_recognition")
    out_dir.mkdir(parents=True, exist_ok=True)

    carrier_cos_one, carrier_sin_one = make_carrier_lo()
    refs_raw_cos_one, refs_raw_sin_one = make_refs_raw()

    all_sample = []
    all_carrier_cos = []
    all_carrier_sin = []
    all_ref_cos = [[] for _ in CANDIDATES]
    all_ref_sin = [[] for _ in CANDIDATES]
    rows = []

    cases = [(MODE_CW, 1_000)] + [(MODE_AM, freq) for freq in CANDIDATES] + [(MODE_FM, freq) for freq in CANDIDATES]
    for case_idx, (mode, freq) in enumerate(cases):
        sample = make_signal(mode, freq, case_idx)
        i_sum, q_sum = ddc_reference(sample, carrier_cos_one, carrier_sin_one)
        am_best, fm_best, am_best_power, fm_best_power = tone_expected(i_sum, q_sum, refs_raw_cos_one, refs_raw_sin_one)
        coarse = coarse_expected(i_sum, q_sum)
        am_ma_permille = am_ma_expected(i_sum, q_sum)
        fm_tone_active = int(fm_best_power > (am_best_power << FM_SCORE_RATIO_SHIFT))

        if coarse["phase_flip_active"]:
            kind = KIND_PSK_LIKE
        elif coarse["phase_active"] or fm_tone_active:
            kind = KIND_FM_LIKE
        elif coarse["mag_active"]:
            kind = KIND_AM_LIKE
        else:
            kind = KIND_CW

        if kind == KIND_FM_LIKE:
            selected = fm_best
        elif kind == KIND_AM_LIKE:
            selected = am_best
        else:
            selected = 0

        fm_delta_f_hz, fm_mf_tenths = fm_delta_expected(i_sum, q_sum, fm_best)

        all_sample.extend(sample.tolist())
        all_carrier_cos.extend(carrier_cos_one.tolist())
        all_carrier_sin.extend(carrier_sin_one.tolist())
        for cand_idx in range(len(CANDIDATES)):
            all_ref_cos[cand_idx].extend(refs_raw_cos_one[cand_idx].tolist())
            all_ref_sin[cand_idx].extend(refs_raw_sin_one[cand_idx].tolist())

        rows.append({
            "case": case_idx,
            "mode": mode,
            "true_freq": freq if mode != MODE_CW else None,
            "kind": kind,
            "selected_freq_index": selected,
            "am_best_index": am_best,
            "fm_best_index": fm_best,
            "am_ma_valid": int(kind == KIND_AM_LIKE),
            "am_ma_permille": am_ma_permille,
            "fm_param_valid": int(kind == KIND_FM_LIKE),
            "fm_delta_f_hz": fm_delta_f_hz,
            "fm_mf_tenths": fm_mf_tenths,
            "mag_active": coarse["mag_active"],
            "ask_like": coarse["ask_like"],
            "phase_active": coarse["phase_active"],
            "phase_flip_active": coarse["phase_flip_active"],
            "coarse_flags": coarse["coarse_flags"],
        })

    (sim_dir / "receiver_recognition_sample.mem").write_text(
        "\n".join(twos_hex(v, SAMPLE_BITS) for v in all_sample) + "\n",
        encoding="ascii",
    )
    (sim_dir / "receiver_recognition_carrier_cos.mem").write_text(
        "\n".join(twos_hex(v, LO_BITS) for v in all_carrier_cos) + "\n",
        encoding="ascii",
    )
    (sim_dir / "receiver_recognition_carrier_sin.mem").write_text(
        "\n".join(twos_hex(v, LO_BITS) for v in all_carrier_sin) + "\n",
        encoding="ascii",
    )
    for idx in range(len(CANDIDATES)):
        (sim_dir / f"receiver_recognition_ref{idx}_cos.mem").write_text(
            "\n".join(twos_hex(v, REF_BITS) for v in all_ref_cos[idx]) + "\n",
            encoding="ascii",
        )
        (sim_dir / f"receiver_recognition_ref{idx}_sin.mem").write_text(
            "\n".join(twos_hex(v, REF_BITS) for v in all_ref_sin[idx]) + "\n",
            encoding="ascii",
        )

    for key, bits in [
        ("mode", 2),
        ("kind", 3),
        ("selected_freq_index", 3),
        ("am_best_index", 3),
        ("fm_best_index", 3),
        ("am_ma_valid", 1),
        ("am_ma_permille", 10),
        ("fm_param_valid", 1),
        ("fm_delta_f_hz", 16),
        ("fm_mf_tenths", 8),
        ("mag_active", 1),
        ("ask_like", 1),
        ("phase_active", 1),
        ("phase_flip_active", 1),
        ("coarse_flags", 3),
    ]:
        (sim_dir / f"receiver_recognition_exp_{key}.mem").write_text(
            "\n".join(twos_hex(row[key], bits) for row in rows) + "\n",
            encoding="ascii",
        )

    csv_path = out_dir / "receiver_recognition_expected.csv"
    with csv_path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)

    print(csv_path.resolve())


if __name__ == "__main__":
    main()
