from pathlib import Path
import csv
import numpy as np


FB = 250_000
BLOCK_LEN = 2048
CASE_COUNT = 12
RATES = [6_000, 8_000, 10_000]
PHASE_BITS = 24
BIN_BITS = 4
BIN_COUNT = 1 << BIN_BITS
SCORE_BITS = 12
STEPS = [402_653, 536_871, 671_089]


def twos_hex(value, bits):
    value = int(value)
    if value < 0:
        value = (1 << bits) + value
    return f"{value & ((1 << bits) - 1):0{(bits + 3) // 4}x}"


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


def make_case(rate, case_idx, rng):
    offset = rng.uniform(0.0, 1.0 / rate)
    symbol_count = int(np.ceil((BLOCK_LEN / FB + offset) * rate)) + 8
    bits = limited_run_bits(symbol_count, rng)
    n = np.arange(BLOCK_LEN)
    t = n / FB + offset
    idx = np.floor(t * rate).astype(np.int64)
    idx = np.clip(idx, 0, len(bits) - 1)
    stream = bits[idx].astype(np.int8)
    if case_idx % 2:
        stream = 1 - stream
    return stream


def model(stream):
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


def main():
    rng = np.random.default_rng(20230515)
    sim_dir = Path("sim")
    out_dir = Path("outputs/transition_rate")
    out_dir.mkdir(parents=True, exist_ok=True)

    cases = []
    for idx in range(CASE_COUNT):
        rate_idx = idx % len(RATES)
        stream = make_case(RATES[rate_idx], idx, rng)
        best, scores, transition_count = model(stream)
        cases.append({
            "case": idx,
            "rate": RATES[rate_idx],
            "expected_best_index": best,
            "score0": scores[0],
            "score1": scores[1],
            "score2": scores[2],
            "transition_count": transition_count,
            "bits": stream.tolist(),
        })

    all_bits = []
    for row in cases:
        all_bits.extend(row["bits"])

    (sim_dir / "transition_rate_bits.mem").write_text(
        "\n".join(str(v) for v in all_bits) + "\n",
        encoding="ascii",
    )
    for key, bits in [
        ("expected_best_index", 2),
        ("score0", SCORE_BITS),
        ("score1", SCORE_BITS),
        ("score2", SCORE_BITS),
        ("transition_count", SCORE_BITS),
    ]:
        (sim_dir / f"transition_rate_{key}.mem").write_text(
            "\n".join(twos_hex(row[key], bits) for row in cases) + "\n",
            encoding="ascii",
        )

    csv_rows = [{k: v for k, v in row.items() if k != "bits"} for row in cases]
    csv_path = out_dir / "transition_rate_expected.csv"
    with csv_path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(csv_rows[0].keys()))
        writer.writeheader()
        writer.writerows(csv_rows)

    print(csv_path.resolve())


if __name__ == "__main__":
    main()
