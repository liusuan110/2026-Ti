from pathlib import Path
import csv
import numpy as np


FS = 20_000_000
FC = 2_000_000
DECIM = 80
FB = FS // DECIM
BLOCK_LEN = 1024
RAW_SAMPLES_PER_CASE = DECIM * BLOCK_LEN * 2
CASE_COUNT = 6
RATES = [6_000, 8_000, 10_000]

SAMPLE_BITS = 8
LO_BITS = 12
PHASE_BITS = 24
BIN_BITS = 4
BIN_COUNT = 1 << BIN_BITS
SCORE_BITS = 10
STEPS = [402_653, 536_871, 671_089]


def twos_hex(value, bits):
    value = int(value)
    if value < 0:
        value = (1 << bits) + value
    return f"{value & ((1 << bits) - 1):0{(bits + 3) // 4}x}"


def signed_clip(value, bits):
    lo = -(1 << (bits - 1))
    hi = (1 << (bits - 1)) - 1
    return int(np.clip(np.round(value), lo, hi))


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


def make_ask_signal(rate, case_idx, rng):
    n = np.arange(RAW_SAMPLES_PER_CASE)
    t = n / FS
    offset = rng.uniform(0.0, 1.0 / rate)
    symbol_count = int(np.ceil((RAW_SAMPLES_PER_CASE / FS + offset) * rate)) + 8
    bits = limited_run_bits(symbol_count, rng)
    bit_index = np.floor((t + offset) * rate).astype(np.int64)
    bit_index = np.clip(bit_index, 0, len(bits) - 1)
    b = bits[bit_index]
    amp = np.where(b > 0, 74.0, 5.5)
    carrier_phase = 0.18 + 0.31 * case_idx
    x = amp * np.cos(2 * np.pi * FC * t + carrier_phase)
    return np.array([signed_clip(v, SAMPLE_BITS) for v in x], dtype=np.int64)


def ddc_reference(sample, carrier_cos, carrier_sin):
    i_prod = sample * carrier_cos
    q_prod = sample * (-carrier_sin)
    i_sum = i_prod.reshape(-1, DECIM).sum(axis=1)
    q_sum = q_prod.reshape(-1, DECIM).sum(axis=1)
    return i_sum.astype(np.int64), q_sum.astype(np.int64)


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


def expected_for_case(sample, carrier_cos, carrier_sin):
    i_sum, q_sum = ddc_reference(sample, carrier_cos, carrier_sin)
    mag_sq = i_sum * i_sum + q_sum * q_sum
    train = mag_sq[:BLOCK_LEN]
    threshold = (int(np.min(train)) + int(np.max(train))) // 2
    bits = (mag_sq[BLOCK_LEN:2 * BLOCK_LEN] > threshold).astype(np.int8)
    best, scores, transition_count = transition_rate_model(bits)
    return threshold, best, scores, transition_count


def main():
    rng = np.random.default_rng(20230516)
    sim_dir = Path("sim")
    out_dir = Path("outputs/receiver_ask_rate")
    out_dir.mkdir(parents=True, exist_ok=True)

    carrier_cos_one, carrier_sin_one = make_carrier_lo()

    all_sample = []
    all_carrier_cos = []
    all_carrier_sin = []
    rows = []

    for case_idx in range(CASE_COUNT):
        rate_idx = case_idx % len(RATES)
        sample = make_ask_signal(RATES[rate_idx], case_idx, rng)
        threshold, best, scores, transition_count = expected_for_case(
            sample,
            carrier_cos_one,
            carrier_sin_one,
        )

        all_sample.extend(sample.tolist())
        all_carrier_cos.extend(carrier_cos_one.tolist())
        all_carrier_sin.extend(carrier_sin_one.tolist())
        rows.append({
            "case": case_idx,
            "true_rate": RATES[rate_idx],
            "expected_rc_index": best,
            "expected_rc_hz": RATES[best],
            "threshold": threshold,
            "score0": scores[0],
            "score1": scores[1],
            "score2": scores[2],
            "transition_count": transition_count,
        })

    (sim_dir / "receiver_ask_rate_sample.mem").write_text(
        "\n".join(twos_hex(v, SAMPLE_BITS) for v in all_sample) + "\n",
        encoding="ascii",
    )
    (sim_dir / "receiver_ask_rate_carrier_cos.mem").write_text(
        "\n".join(twos_hex(v, LO_BITS) for v in all_carrier_cos) + "\n",
        encoding="ascii",
    )
    (sim_dir / "receiver_ask_rate_carrier_sin.mem").write_text(
        "\n".join(twos_hex(v, LO_BITS) for v in all_carrier_sin) + "\n",
        encoding="ascii",
    )
    for key, bits in [
        ("expected_rc_index", 2),
        ("expected_rc_hz", 16),
        ("score0", SCORE_BITS),
        ("score1", SCORE_BITS),
        ("score2", SCORE_BITS),
        ("transition_count", SCORE_BITS),
    ]:
        (sim_dir / f"receiver_ask_rate_{key}.mem").write_text(
            "\n".join(twos_hex(row[key], bits) for row in rows) + "\n",
            encoding="ascii",
        )

    csv_path = out_dir / "receiver_ask_rate_expected.csv"
    with csv_path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)

    print(csv_path.resolve())


if __name__ == "__main__":
    main()
