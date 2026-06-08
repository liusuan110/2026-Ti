from pathlib import Path
import csv
import numpy as np


FS = 20_000_000
FC = 2_000_000
DECIM = 80
FB = FS // DECIM
BLOCK_LEN = 250
DDC_SAMPLES_PER_CASE = BLOCK_LEN + 1
RAW_SAMPLES_PER_CASE = DECIM * DDC_SAMPLES_PER_CASE
CANDIDATES = [1_000, 2_000, 3_000, 4_000, 5_000]

SAMPLE_BITS = 8
LO_BITS = 12
IQ_BITS = SAMPLE_BITS + LO_BITS + 1 + 7
DOT_BITS = 2 * IQ_BITS + 1
DOT_SHIFT = 32
CORDIC_DATA_BITS = 25
ANGLE_BITS = 24
ANGLE_FRAC_BITS = 20
ANGLE_SCALE = 1 << ANGLE_FRAC_BITS
ITERATIONS = 20
TWO_PI_Q4_20 = 6_588_398

CASES = [
    (1_000, 1.2),
    (2_000, 1.8),
    (3_000, 2.4),
    (4_000, 3.0),
    (5_000, 3.6),
    (1_000, 4.2),
    (2_000, 4.8),
    (3_000, 3.5),
    (4_000, 2.7),
    (5_000, 4.5),
]


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


def make_fm_signal(mod_freq, mf, case_idx):
    n = np.arange(RAW_SAMPLES_PER_CASE)
    t = n / FS
    carrier_phase = 0.21 + 0.17 * case_idx
    mod_phase = 0.35 + 0.13 * case_idx
    phase = 2 * np.pi * FC * t + carrier_phase
    phase = phase + mf * np.sin(2 * np.pi * mod_freq * t + mod_phase)
    x = 76.0 * np.cos(phase)
    return np.array([signed_clip(v, SAMPLE_BITS) for v in x], dtype=np.int64)


def ddc_reference(sample, carrier_cos, carrier_sin):
    i_prod = sample * carrier_cos
    q_prod = sample * (-carrier_sin)
    i_sum = i_prod.reshape(-1, DECIM).sum(axis=1)
    q_sum = q_prod.reshape(-1, DECIM).sum(axis=1)
    return i_sum.astype(np.int64), q_sum.astype(np.int64)


def expected_for_case(sample, carrier_cos, carrier_sin, freq_index):
    i_sum, q_sum = ddc_reference(sample, carrier_cos, carrier_sin)
    phases = []
    for idx in range(1, BLOCK_LEN + 1):
        dot = int(i_sum[idx] * i_sum[idx - 1] + q_sum[idx] * q_sum[idx - 1])
        cross = int(q_sum[idx] * i_sum[idx - 1] - i_sum[idx] * q_sum[idx - 1])
        dot_scaled = signed_clip(arshift(dot, DOT_SHIFT), CORDIC_DATA_BITS)
        cross_scaled = signed_clip(arshift(cross, DOT_SHIFT), CORDIC_DATA_BITS)
        phases.append(pipeline_cordic_model(dot_scaled, cross_scaled))

    max_abs_phase_delta = max(abs(v) for v in phases)
    delta_f_hz = (max_abs_phase_delta * FB + (TWO_PI_Q4_20 // 2)) // TWO_PI_Q4_20
    mod_freq = CANDIDATES[freq_index]
    mf_tenths = (delta_f_hz * 10 + mod_freq // 2) // mod_freq
    return int(max_abs_phase_delta), int(delta_f_hz), int(mf_tenths)


def main():
    sim_dir = Path("sim")
    out_dir = Path("outputs/receiver_fm_delta")
    out_dir.mkdir(parents=True, exist_ok=True)

    carrier_cos_one, carrier_sin_one = make_carrier_lo()

    all_sample = []
    all_carrier_cos = []
    all_carrier_sin = []
    rows = []

    for case_idx, (mod_freq, mf) in enumerate(CASES):
        sample = make_fm_signal(mod_freq, mf, case_idx)
        freq_index = CANDIDATES.index(mod_freq)
        max_abs_phase_delta, delta_f_hz, mf_tenths = expected_for_case(
            sample,
            carrier_cos_one,
            carrier_sin_one,
            freq_index,
        )

        all_sample.extend(sample.tolist())
        all_carrier_cos.extend(carrier_cos_one.tolist())
        all_carrier_sin.extend(carrier_sin_one.tolist())

        rows.append({
            "case": case_idx,
            "true_freq": mod_freq,
            "freq_index": freq_index,
            "true_mf": mf,
            "true_delta_f_hz": int(round(mod_freq * mf)),
            "expected_max_abs_phase_delta": max_abs_phase_delta,
            "expected_delta_f_hz": delta_f_hz,
            "expected_mf_tenths": mf_tenths,
            "delta_f_error_hz_vs_true": delta_f_hz - int(round(mod_freq * mf)),
            "mf_error_vs_true": mf_tenths / 10.0 - mf,
        })

    (sim_dir / "receiver_fm_delta_sample.mem").write_text(
        "\n".join(twos_hex(v, SAMPLE_BITS) for v in all_sample) + "\n",
        encoding="ascii",
    )
    (sim_dir / "receiver_fm_delta_carrier_cos.mem").write_text(
        "\n".join(twos_hex(v, LO_BITS) for v in all_carrier_cos) + "\n",
        encoding="ascii",
    )
    (sim_dir / "receiver_fm_delta_carrier_sin.mem").write_text(
        "\n".join(twos_hex(v, LO_BITS) for v in all_carrier_sin) + "\n",
        encoding="ascii",
    )
    (sim_dir / "receiver_fm_delta_freq_index.mem").write_text(
        "\n".join(twos_hex(row["freq_index"], 3) for row in rows) + "\n",
        encoding="ascii",
    )
    (sim_dir / "receiver_fm_delta_exp_max_abs.mem").write_text(
        "\n".join(twos_hex(row["expected_max_abs_phase_delta"], 25) for row in rows) + "\n",
        encoding="ascii",
    )
    (sim_dir / "receiver_fm_delta_exp_delta_f.mem").write_text(
        "\n".join(twos_hex(row["expected_delta_f_hz"], 16) for row in rows) + "\n",
        encoding="ascii",
    )
    (sim_dir / "receiver_fm_delta_exp_mf_tenths.mem").write_text(
        "\n".join(twos_hex(row["expected_mf_tenths"], 8) for row in rows) + "\n",
        encoding="ascii",
    )

    csv_path = out_dir / "receiver_fm_delta_expected.csv"
    with csv_path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)

    print(csv_path.resolve())


if __name__ == "__main__":
    main()
