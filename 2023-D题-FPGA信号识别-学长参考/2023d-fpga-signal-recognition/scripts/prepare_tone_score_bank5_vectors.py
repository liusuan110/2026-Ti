from pathlib import Path
import csv
import numpy as np


FB = 250_000
BLOCK_LEN = 250
CASE_COUNT = 5
DATA_BITS = 18
REF_BITS = 12
COUNT_BITS = 8
PRODUCT_BITS = DATA_BITS + REF_BITS
ACC_BITS = PRODUCT_BITS + COUNT_BITS
SCORE_BITS = 2 * ACC_BITS + 1
CANDIDATES = [1_000, 2_000, 3_000, 4_000, 5_000]


def twos_hex(value, bits):
    value = int(value)
    if value < 0:
        value = (1 << bits) + value
    return f"{value & ((1 << bits) - 1):0{(bits + 3) // 4}x}"


def signed_clip(value, bits):
    lo = -(1 << (bits - 1))
    hi = (1 << (bits - 1)) - 1
    return int(np.clip(np.round(value), lo, hi))


def refs(freq):
    n = np.arange(BLOCK_LEN)
    t = n / FB
    c = np.array([signed_clip(2047 * np.cos(2 * np.pi * freq * ti), REF_BITS) for ti in t], dtype=np.int64)
    s = np.array([signed_clip(2047 * np.sin(2 * np.pi * freq * ti), REF_BITS) for ti in t], dtype=np.int64)
    return c, s


def main():
    sim_dir = Path("sim")
    out_dir = Path("outputs/tone_score_bank5")
    out_dir.mkdir(parents=True, exist_ok=True)

    all_data = []
    all_cos = [[] for _ in CANDIDATES]
    all_sin = [[] for _ in CANDIDATES]
    rows = []

    n = np.arange(BLOCK_LEN)
    t = n / FB

    ref_pairs = [refs(freq) for freq in CANDIDATES]

    for case_idx, true_freq in enumerate(CANDIDATES):
        phase = 0.23 + 0.31 * case_idx
        amp = 25_000 + 2_000 * case_idx
        dc = 9_000 - 1_000 * case_idx
        data = dc + amp * np.cos(2 * np.pi * true_freq * t + phase)
        data_i = np.array([signed_clip(x, DATA_BITS) for x in data], dtype=np.int64)
        all_data.extend(data_i.tolist())

        powers = []
        for cand_idx, (cos_i, sin_i) in enumerate(ref_pairs):
            all_cos[cand_idx].extend(cos_i.tolist())
            all_sin[cand_idx].extend(sin_i.tolist())
            i_score = int(np.sum(data_i * cos_i))
            q_score = int(np.sum(data_i * sin_i))
            powers.append(int(i_score * i_score + q_score * q_score))

        best_index = int(np.argmax(powers))
        rows.append({
            "case": case_idx,
            "true_freq": true_freq,
            "expected_best_index": best_index,
            "expected_best_freq": CANDIDATES[best_index],
            **{f"score{idx}_power": powers[idx] for idx in range(len(CANDIDATES))},
        })

    (sim_dir / "tone_bank_data.mem").write_text(
        "\n".join(twos_hex(v, DATA_BITS) for v in all_data) + "\n",
        encoding="ascii",
    )

    for idx in range(len(CANDIDATES)):
        (sim_dir / f"tone_bank_ref{idx}_cos.mem").write_text(
            "\n".join(twos_hex(v, REF_BITS) for v in all_cos[idx]) + "\n",
            encoding="ascii",
        )
        (sim_dir / f"tone_bank_ref{idx}_sin.mem").write_text(
            "\n".join(twos_hex(v, REF_BITS) for v in all_sin[idx]) + "\n",
            encoding="ascii",
        )

    (sim_dir / "tone_bank_exp_best_index.mem").write_text(
        "\n".join(twos_hex(row["expected_best_index"], 3) for row in rows) + "\n",
        encoding="ascii",
    )
    for idx in range(len(CANDIDATES)):
        (sim_dir / f"tone_bank_exp_score{idx}_power.mem").write_text(
            "\n".join(twos_hex(row[f"score{idx}_power"], SCORE_BITS) for row in rows) + "\n",
            encoding="ascii",
        )

    csv_path = out_dir / "tone_score_bank5_expected.csv"
    with csv_path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)

    print(csv_path.resolve())


if __name__ == "__main__":
    main()
