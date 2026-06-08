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
REF_BITS = 12
DDC_PRODUCT_BITS = SAMPLE_BITS + LO_BITS + 1
IQ_BITS = DDC_PRODUCT_BITS + 7
DATA_BITS = 32
SCORE_COUNT_BITS = 8
MAG_SHIFT = 24
CROSS_SHIFT = 22
SCORE_PRODUCT_BITS = DATA_BITS + REF_BITS
SCORE_ACC_BITS = SCORE_PRODUCT_BITS + SCORE_COUNT_BITS
SCORE_BITS = 2 * SCORE_ACC_BITS + 1

MODE_AM = 0
MODE_FM = 1


def twos_hex(value, bits):
    value = int(value)
    if value < 0:
        value = (1 << bits) + value
    return f"{value & ((1 << bits) - 1):0{(bits + 3) // 4}x}"


def signed_clip(value, bits):
    lo = -(1 << (bits - 1))
    hi = (1 << (bits - 1)) - 1
    return int(np.clip(np.round(value), lo, hi))


def make_signal(mode, freq, case_idx):
    n = np.arange(RAW_SAMPLES_PER_CASE)
    t = n / FS
    carrier_phase = 0.17 + 0.23 * case_idx
    mod_phase = 0.31 + 0.11 * case_idx

    if mode == MODE_AM:
        ma = 0.55
        adc_amp = 50.0
        env = 1.0 + ma * np.sin(2 * np.pi * freq * t + mod_phase)
        x = adc_amp * env * np.cos(2 * np.pi * FC * t + carrier_phase)
    else:
        mf = 1.7
        adc_amp = 76.0
        phase = 2 * np.pi * FC * t + carrier_phase
        phase = phase + mf * np.sin(2 * np.pi * freq * t + mod_phase)
        x = adc_amp * np.cos(phase)

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


def expected_for_case(sample, carrier_cos, carrier_sin, refs_raw_cos, refs_raw_sin):
    i_sum, q_sum = ddc_reference(sample, carrier_cos, carrier_sin)

    mag_sq = i_sum * i_sum + q_sum * q_sum
    cross = np.zeros_like(i_sum)
    for idx in range(1, len(i_sum)):
        cross[idx] = q_sum[idx] * i_sum[idx - 1] - i_sum[idx] * q_sum[idx - 1]

    refs_cos = [r[DECIM - 1::DECIM] for r in refs_raw_cos]
    refs_sin = [r[DECIM - 1::DECIM] for r in refs_raw_sin]

    am_feature = mag_sq[:BLOCK_LEN] >> MAG_SHIFT
    am_best, am_powers = score_feature(
        am_feature,
        [r[:BLOCK_LEN] for r in refs_cos],
        [r[:BLOCK_LEN] for r in refs_sin],
    )

    fm_feature = cross[1:BLOCK_LEN + 1] >> CROSS_SHIFT
    fm_best, fm_powers = score_feature(
        fm_feature,
        [r[1:BLOCK_LEN + 1] for r in refs_cos],
        [r[1:BLOCK_LEN + 1] for r in refs_sin],
    )

    return am_best, fm_best, am_powers, fm_powers


def main():
    sim_dir = Path("sim")
    out_dir = Path("outputs/receiver_tone_score")
    out_dir.mkdir(parents=True, exist_ok=True)

    carrier_cos_one, carrier_sin_one = make_carrier_lo()
    refs_raw_cos_one, refs_raw_sin_one = make_refs_raw()

    all_sample = []
    all_carrier_cos = []
    all_carrier_sin = []
    all_ref_cos = [[] for _ in CANDIDATES]
    all_ref_sin = [[] for _ in CANDIDATES]
    rows = []

    cases = [(MODE_AM, freq) for freq in CANDIDATES] + [(MODE_FM, freq) for freq in CANDIDATES]
    for case_idx, (mode, freq) in enumerate(cases):
        sample = make_signal(mode, freq, case_idx)
        am_best, fm_best, am_powers, fm_powers = expected_for_case(
            sample,
            carrier_cos_one,
            carrier_sin_one,
            refs_raw_cos_one,
            refs_raw_sin_one,
        )

        all_sample.extend(sample.tolist())
        all_carrier_cos.extend(carrier_cos_one.tolist())
        all_carrier_sin.extend(carrier_sin_one.tolist())
        for cand_idx in range(len(CANDIDATES)):
            all_ref_cos[cand_idx].extend(refs_raw_cos_one[cand_idx].tolist())
            all_ref_sin[cand_idx].extend(refs_raw_sin_one[cand_idx].tolist())

        rows.append({
            "case": case_idx,
            "mode": mode,
            "true_freq": freq,
            "expected_am_best_index": am_best,
            "expected_fm_best_index": fm_best,
            "expected_relevant_best_index": am_best if mode == MODE_AM else fm_best,
            "expected_relevant_freq": CANDIDATES[am_best if mode == MODE_AM else fm_best],
            **{f"am_score{idx}_power": am_powers[idx] for idx in range(len(CANDIDATES))},
            **{f"fm_score{idx}_power": fm_powers[idx] for idx in range(len(CANDIDATES))},
        })

    (sim_dir / "receiver_tone_sample.mem").write_text(
        "\n".join(twos_hex(v, SAMPLE_BITS) for v in all_sample) + "\n",
        encoding="ascii",
    )
    (sim_dir / "receiver_tone_carrier_cos.mem").write_text(
        "\n".join(twos_hex(v, LO_BITS) for v in all_carrier_cos) + "\n",
        encoding="ascii",
    )
    (sim_dir / "receiver_tone_carrier_sin.mem").write_text(
        "\n".join(twos_hex(v, LO_BITS) for v in all_carrier_sin) + "\n",
        encoding="ascii",
    )

    for idx in range(len(CANDIDATES)):
        (sim_dir / f"receiver_tone_ref{idx}_cos.mem").write_text(
            "\n".join(twos_hex(v, REF_BITS) for v in all_ref_cos[idx]) + "\n",
            encoding="ascii",
        )
        (sim_dir / f"receiver_tone_ref{idx}_sin.mem").write_text(
            "\n".join(twos_hex(v, REF_BITS) for v in all_ref_sin[idx]) + "\n",
            encoding="ascii",
        )

    (sim_dir / "receiver_tone_mode.mem").write_text(
        "\n".join(twos_hex(row["mode"], 1) for row in rows) + "\n",
        encoding="ascii",
    )
    (sim_dir / "receiver_tone_exp_am_best.mem").write_text(
        "\n".join(twos_hex(row["expected_am_best_index"], 3) for row in rows) + "\n",
        encoding="ascii",
    )
    (sim_dir / "receiver_tone_exp_fm_best.mem").write_text(
        "\n".join(twos_hex(row["expected_fm_best_index"], 3) for row in rows) + "\n",
        encoding="ascii",
    )

    csv_path = out_dir / "receiver_tone_score_expected.csv"
    with csv_path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)

    print(csv_path.resolve())


if __name__ == "__main__":
    main()
