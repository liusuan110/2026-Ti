from pathlib import Path
import csv
import numpy as np


FB = 250_000
BLOCK_LEN = 250
SAMPLES_PER_CASE = BLOCK_LEN + 1
CANDIDATES = [1_000, 2_000, 3_000, 4_000, 5_000]
IQ_BITS = 18
REF_BITS = 12
DATA_BITS = 24
COUNT_BITS = 8
MAG_SHIFT = 8
CROSS_SHIFT = 5
SCORE_PRODUCT_BITS = DATA_BITS + REF_BITS
SCORE_ACC_BITS = SCORE_PRODUCT_BITS + COUNT_BITS
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


def make_refs():
    n = np.arange(SAMPLES_PER_CASE)
    t = n / FB
    ref_cos = []
    ref_sin = []
    for freq in CANDIDATES:
        ref_cos.append(np.array([signed_clip(2047 * np.cos(2 * np.pi * freq * ti), REF_BITS) for ti in t], dtype=np.int64))
        ref_sin.append(np.array([signed_clip(2047 * np.sin(2 * np.pi * freq * ti), REF_BITS) for ti in t], dtype=np.int64))
    return ref_cos, ref_sin


def make_iq(mode, freq, case_idx):
    n = np.arange(SAMPLES_PER_CASE)
    t = n / FB
    phase0 = 0.21 + 0.29 * case_idx
    amp = 15_000 + 600 * case_idx

    if mode == MODE_AM:
        ma = 0.55
        env = 1.0 + ma * np.sin(2 * np.pi * freq * t + 0.37)
        phase = np.full_like(t, phase0)
        z = amp * env * np.exp(1j * phase)
    else:
        mf = 1.7
        phase = phase0 + mf * np.sin(2 * np.pi * freq * t + 0.43)
        z = amp * np.exp(1j * phase)

    i_data = np.array([signed_clip(v, IQ_BITS) for v in np.real(z)], dtype=np.int64)
    q_data = np.array([signed_clip(v, IQ_BITS) for v in np.imag(z)], dtype=np.int64)
    return i_data, q_data


def score_feature(feature, ref_cos, ref_sin):
    powers = []
    for c, s in zip(ref_cos, ref_sin):
        i_score = int(np.sum(feature * c))
        q_score = int(np.sum(feature * s))
        powers.append(int(i_score * i_score + q_score * q_score))
    best_index = int(np.argmax(powers))
    return best_index, powers


def expected_for_case(i_data, q_data, ref_cos, ref_sin):
    mag_sq = i_data * i_data + q_data * q_data
    cross = np.zeros_like(i_data)
    for idx in range(1, len(i_data)):
        cross[idx] = q_data[idx] * i_data[idx - 1] - i_data[idx] * q_data[idx - 1]

    am_feature = mag_sq[:BLOCK_LEN] >> MAG_SHIFT
    am_refs_cos = [r[:BLOCK_LEN] for r in ref_cos]
    am_refs_sin = [r[:BLOCK_LEN] for r in ref_sin]
    am_best, am_powers = score_feature(am_feature, am_refs_cos, am_refs_sin)

    fm_feature = cross[1:BLOCK_LEN + 1] >> CROSS_SHIFT
    fm_refs_cos = [r[1:BLOCK_LEN + 1] for r in ref_cos]
    fm_refs_sin = [r[1:BLOCK_LEN + 1] for r in ref_sin]
    fm_best, fm_powers = score_feature(fm_feature, fm_refs_cos, fm_refs_sin)

    return am_best, fm_best, am_powers, fm_powers


def main():
    sim_dir = Path("sim")
    out_dir = Path("outputs/iq_feature_tone_score")
    out_dir.mkdir(parents=True, exist_ok=True)

    ref_cos_one, ref_sin_one = make_refs()
    all_i = []
    all_q = []
    all_ref_cos = [[] for _ in CANDIDATES]
    all_ref_sin = [[] for _ in CANDIDATES]
    rows = []

    cases = [(MODE_AM, freq) for freq in CANDIDATES] + [(MODE_FM, freq) for freq in CANDIDATES]
    for case_idx, (mode, freq) in enumerate(cases):
        i_data, q_data = make_iq(mode, freq, case_idx)
        am_best, fm_best, am_powers, fm_powers = expected_for_case(i_data, q_data, ref_cos_one, ref_sin_one)

        all_i.extend(i_data.tolist())
        all_q.extend(q_data.tolist())
        for cand_idx in range(len(CANDIDATES)):
            all_ref_cos[cand_idx].extend(ref_cos_one[cand_idx].tolist())
            all_ref_sin[cand_idx].extend(ref_sin_one[cand_idx].tolist())

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

    (sim_dir / "iq_tone_i.mem").write_text(
        "\n".join(twos_hex(v, IQ_BITS) for v in all_i) + "\n",
        encoding="ascii",
    )
    (sim_dir / "iq_tone_q.mem").write_text(
        "\n".join(twos_hex(v, IQ_BITS) for v in all_q) + "\n",
        encoding="ascii",
    )

    for idx in range(len(CANDIDATES)):
        (sim_dir / f"iq_tone_ref{idx}_cos.mem").write_text(
            "\n".join(twos_hex(v, REF_BITS) for v in all_ref_cos[idx]) + "\n",
            encoding="ascii",
        )
        (sim_dir / f"iq_tone_ref{idx}_sin.mem").write_text(
            "\n".join(twos_hex(v, REF_BITS) for v in all_ref_sin[idx]) + "\n",
            encoding="ascii",
        )

    (sim_dir / "iq_tone_mode.mem").write_text(
        "\n".join(twos_hex(row["mode"], 1) for row in rows) + "\n",
        encoding="ascii",
    )
    (sim_dir / "iq_tone_exp_am_best.mem").write_text(
        "\n".join(twos_hex(row["expected_am_best_index"], 3) for row in rows) + "\n",
        encoding="ascii",
    )
    (sim_dir / "iq_tone_exp_fm_best.mem").write_text(
        "\n".join(twos_hex(row["expected_fm_best_index"], 3) for row in rows) + "\n",
        encoding="ascii",
    )

    csv_path = out_dir / "iq_feature_tone_score_expected.csv"
    with csv_path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)

    print(csv_path.resolve())


if __name__ == "__main__":
    main()
