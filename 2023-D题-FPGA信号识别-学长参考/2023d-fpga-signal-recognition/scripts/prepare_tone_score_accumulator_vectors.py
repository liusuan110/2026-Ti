from pathlib import Path
import csv
import math

import numpy as np


FB = 250_000
BLOCK_LEN = 250
CASE_COUNT = 3
DATA_BITS = 18
REF_BITS = 12
COUNT_BITS = 8
PRODUCT_BITS = DATA_BITS + REF_BITS
ACC_BITS = PRODUCT_BITS + COUNT_BITS
SCORE_BITS = 2 * ACC_BITS + 1


def twos_hex(value, bits):
    value = int(value)
    if value < 0:
        value = (1 << bits) + value
    return f"{value & ((1 << bits) - 1):0{(bits + 3) // 4}x}"


def signed_clip(value, bits):
    lo = -(1 << (bits - 1))
    hi = (1 << (bits - 1)) - 1
    return int(np.clip(np.round(value), lo, hi))


def make_case(true_freq, cand_freq, phase, amp, dc):
    n = np.arange(BLOCK_LEN)
    t = n / FB
    data = dc + amp * np.cos(2 * np.pi * true_freq * t + phase)
    ref_cos = 2047 * np.cos(2 * np.pi * cand_freq * t)
    ref_sin = 2047 * np.sin(2 * np.pi * cand_freq * t)

    data_i = np.array([signed_clip(x, DATA_BITS) for x in data], dtype=np.int64)
    cos_i = np.array([signed_clip(x, REF_BITS) for x in ref_cos], dtype=np.int64)
    sin_i = np.array([signed_clip(x, REF_BITS) for x in ref_sin], dtype=np.int64)

    i_score = int(np.sum(data_i * cos_i))
    q_score = int(np.sum(data_i * sin_i))
    score_power = int(i_score * i_score + q_score * q_score)
    return data_i, cos_i, sin_i, i_score, q_score, score_power


def main():
    sim_dir = Path("sim")
    out_dir = Path("outputs/tone_score_accumulator")
    out_dir.mkdir(parents=True, exist_ok=True)

    specs = [
        {"name": "match_3khz", "true_freq": 3_000, "cand_freq": 3_000, "phase": 0.37, "amp": 35_000, "dc": 12_000},
        {"name": "mismatch_3khz_vs_1khz", "true_freq": 3_000, "cand_freq": 1_000, "phase": 0.37, "amp": 35_000, "dc": 12_000},
        {"name": "match_5khz_negative_dc", "true_freq": 5_000, "cand_freq": 5_000, "phase": -0.81, "amp": 28_000, "dc": -4_000},
    ]

    all_data = []
    all_cos = []
    all_sin = []
    rows = []

    for idx, spec in enumerate(specs):
        data_i, cos_i, sin_i, i_score, q_score, score_power = make_case(**{k: spec[k] for k in ["true_freq", "cand_freq", "phase", "amp", "dc"]})
        all_data.extend(data_i.tolist())
        all_cos.extend(cos_i.tolist())
        all_sin.extend(sin_i.tolist())
        rows.append({
            "case": idx,
            "name": spec["name"],
            "true_freq": spec["true_freq"],
            "cand_freq": spec["cand_freq"],
            "i_score": i_score,
            "q_score": q_score,
            "score_power": score_power,
        })

    (sim_dir / "tone_score_data.mem").write_text(
        "\n".join(twos_hex(v, DATA_BITS) for v in all_data) + "\n",
        encoding="ascii",
    )
    (sim_dir / "tone_score_ref_cos.mem").write_text(
        "\n".join(twos_hex(v, REF_BITS) for v in all_cos) + "\n",
        encoding="ascii",
    )
    (sim_dir / "tone_score_ref_sin.mem").write_text(
        "\n".join(twos_hex(v, REF_BITS) for v in all_sin) + "\n",
        encoding="ascii",
    )
    (sim_dir / "tone_score_exp_i.mem").write_text(
        "\n".join(twos_hex(row["i_score"], ACC_BITS) for row in rows) + "\n",
        encoding="ascii",
    )
    (sim_dir / "tone_score_exp_q.mem").write_text(
        "\n".join(twos_hex(row["q_score"], ACC_BITS) for row in rows) + "\n",
        encoding="ascii",
    )
    (sim_dir / "tone_score_exp_power.mem").write_text(
        "\n".join(twos_hex(row["score_power"], SCORE_BITS) for row in rows) + "\n",
        encoding="ascii",
    )

    csv_path = out_dir / "tone_score_accumulator_expected.csv"
    with csv_path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)

    print(csv_path.resolve())


if __name__ == "__main__":
    main()
