from pathlib import Path
import csv
import numpy as np


FS = 20_000_000
FC = 2_000_000
DECIM = 80
BLOCK_LEN = 250
RAW_SAMPLES_PER_CASE = DECIM * BLOCK_LEN

SAMPLE_BITS = 8
LO_BITS = 12

CASES = [
    (0.32, 1_000, 44.0),
    (0.40, 2_000, 50.0),
    (0.48, 3_000, 54.0),
    (0.55, 4_000, 48.0),
    (0.60, 5_000, 52.0),
    (0.67, 1_000, 45.0),
    (0.73, 2_000, 50.0),
    (0.80, 3_000, 46.0),
    (0.88, 4_000, 49.0),
    (0.92, 5_000, 48.0),
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


def make_am_signal(ma, mod_freq, adc_amp, case_idx):
    n = np.arange(RAW_SAMPLES_PER_CASE)
    t = n / FS
    carrier_phase = 0.13 + 0.19 * case_idx
    mod_phase = 0.27 + 0.17 * case_idx
    envelope = 1.0 + ma * np.sin(2 * np.pi * mod_freq * t + mod_phase)
    x = adc_amp * envelope * np.cos(2 * np.pi * FC * t + carrier_phase)
    return np.array([signed_clip(v, SAMPLE_BITS) for v in x], dtype=np.int64)


def ddc_reference(sample, carrier_cos, carrier_sin):
    i_prod = sample * carrier_cos
    q_prod = sample * (-carrier_sin)
    i_sum = i_prod.reshape(-1, DECIM).sum(axis=1)
    q_sum = q_prod.reshape(-1, DECIM).sum(axis=1)
    return i_sum.astype(np.int64), q_sum.astype(np.int64)


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


def expected_for_case(sample, carrier_cos, carrier_sin):
    i_sum, q_sum = ddc_reference(sample, carrier_cos, carrier_sin)
    mag_sq = i_sum * i_sum + q_sum * q_sum
    block_min = int(np.min(mag_sq[:BLOCK_LEN]))
    block_max = int(np.max(mag_sq[:BLOCK_LEN]))
    return ma_lut(block_min, block_max), block_min, block_max


def main():
    sim_dir = Path("sim")
    out_dir = Path("outputs/receiver_am_ma")
    out_dir.mkdir(parents=True, exist_ok=True)

    carrier_cos_one, carrier_sin_one = make_carrier_lo()

    all_sample = []
    all_carrier_cos = []
    all_carrier_sin = []
    rows = []

    for case_idx, (ma, mod_freq, adc_amp) in enumerate(CASES):
        sample = make_am_signal(ma, mod_freq, adc_amp, case_idx)
        expected_ma, block_min, block_max = expected_for_case(
            sample,
            carrier_cos_one,
            carrier_sin_one,
        )

        all_sample.extend(sample.tolist())
        all_carrier_cos.extend(carrier_cos_one.tolist())
        all_carrier_sin.extend(carrier_sin_one.tolist())

        rows.append({
            "case": case_idx,
            "true_ma": ma,
            "true_freq": mod_freq,
            "adc_amp": adc_amp,
            "expected_ma_permille": expected_ma,
            "block_min": block_min,
            "block_max": block_max,
        })

    (sim_dir / "receiver_am_ma_sample.mem").write_text(
        "\n".join(twos_hex(v, SAMPLE_BITS) for v in all_sample) + "\n",
        encoding="ascii",
    )
    (sim_dir / "receiver_am_ma_carrier_cos.mem").write_text(
        "\n".join(twos_hex(v, LO_BITS) for v in all_carrier_cos) + "\n",
        encoding="ascii",
    )
    (sim_dir / "receiver_am_ma_carrier_sin.mem").write_text(
        "\n".join(twos_hex(v, LO_BITS) for v in all_carrier_sin) + "\n",
        encoding="ascii",
    )
    (sim_dir / "receiver_am_ma_expected.mem").write_text(
        "\n".join(twos_hex(row["expected_ma_permille"], 10) for row in rows) + "\n",
        encoding="ascii",
    )

    csv_path = out_dir / "receiver_am_ma_expected.csv"
    with csv_path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)

    print(csv_path.resolve())


if __name__ == "__main__":
    main()
