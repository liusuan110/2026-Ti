from pathlib import Path
import csv
import math

import numpy as np


FS = 20_000_000
FC = 2_000_000
DECIM = 80
BLOCK_LEN = 64
SAMPLES_PER_CASE = DECIM * BLOCK_LEN
CASE_COUNT = 5
SAMPLE_BITS = 8
LO_BITS = 12

MAG_RANGE_PERCENT = 25
ASK_MINMAX_PERCENT = 8
PHASE_ACTIVITY_PERCENT = 8
NEG_DOT_THRESHOLD = 1


def twos_hex(value, bits):
    if value < 0:
        value = (1 << bits) + value
    return f"{value & ((1 << bits) - 1):0{(bits + 3) // 4}x}"


def q_sample(x):
    return int(np.clip(np.round(85 * x), -128, 127))


def alternating_bits(t, rc):
    return (np.floor(t * rc).astype(np.int64) & 1).astype(np.int64)


def make_case(name, t):
    if name == "cw":
        return np.cos(2 * np.pi * FC * t + 0.4)
    if name == "am":
        env = 1.0 + 0.65 * np.sin(2 * np.pi * 3_000 * t + 0.2)
        return env * np.cos(2 * np.pi * FC * t + 0.4)
    if name == "ask":
        b = alternating_bits(t, 8_000)
        amp = np.where(b > 0, 1.0, 0.06)
        return amp * np.cos(2 * np.pi * FC * t + 0.4)
    if name == "fm":
        phase = 2 * np.pi * FC * t + 2.8 * np.sin(2 * np.pi * 5_000 * t + 0.1)
        return np.cos(phase)
    if name == "psk":
        b = alternating_bits(t, 8_000)
        phase = np.where(b > 0, np.pi, 0.0)
        return np.cos(2 * np.pi * FC * t + 0.4 + phase)
    raise ValueError(name)


def coarse_reference(sample, cosv, sinv):
    i_prod = sample * cosv
    q_prod = sample * (-sinv)
    i_sum = i_prod.reshape(-1, DECIM).sum(axis=1)
    q_sum = q_prod.reshape(-1, DECIM).sum(axis=1)

    mag_sq = i_sum * i_sum + q_sum * q_sum
    dot = np.zeros(BLOCK_LEN, dtype=np.int64)
    cross = np.zeros(BLOCK_LEN, dtype=np.int64)
    delta_valid = np.zeros(BLOCK_LEN, dtype=bool)
    for idx in range(1, BLOCK_LEN):
        dot[idx] = i_sum[idx] * i_sum[idx - 1] + q_sum[idx] * q_sum[idx - 1]
        cross[idx] = q_sum[idx] * i_sum[idx - 1] - i_sum[idx] * q_sum[idx - 1]
        delta_valid[idx] = True

    mag_min = int(np.min(mag_sq))
    mag_max = int(np.max(mag_sq))
    mag_sum = int(np.sum(mag_sq))
    abs_cross_sum = int(np.sum(np.abs(cross[delta_valid])))
    negative_dot_count = int(np.sum(dot[delta_valid] < 0))

    mag_range = mag_max - mag_min
    mag_active = mag_range * BLOCK_LEN * 100 >= mag_sum * MAG_RANGE_PERCENT
    ask_like = mag_active and mag_min * 100 <= mag_max * ASK_MINMAX_PERCENT
    phase_active = abs_cross_sum * 100 >= mag_sum * PHASE_ACTIVITY_PERCENT
    phase_flip_active = negative_dot_count >= NEG_DOT_THRESHOLD
    coarse_flags = (int(phase_flip_active) << 2) | (int(phase_active) << 1) | int(mag_active)

    return {
        "mag_active": int(mag_active),
        "ask_like": int(ask_like),
        "phase_active": int(phase_active),
        "phase_flip_active": int(phase_flip_active),
        "coarse_flags": coarse_flags,
        "mag_min": mag_min,
        "mag_max": mag_max,
        "mag_sum": mag_sum,
        "abs_cross_sum": abs_cross_sum,
        "negative_dot_count": negative_dot_count,
    }


def main():
    sim_dir = Path("sim")
    out_dir = Path("outputs/receiver_coarse_pipeline")
    out_dir.mkdir(parents=True, exist_ok=True)

    names = ["cw", "am", "ask", "fm", "psk"]
    all_sample = []
    all_cos = []
    all_sin = []
    rows = []

    n = np.arange(SAMPLES_PER_CASE)
    t = n / FS
    cosv = np.array([int(np.clip(np.round(2047 * math.cos(2 * math.pi * FC * ti)), -2048, 2047)) for ti in t], dtype=np.int64)
    sinv = np.array([int(np.clip(np.round(2047 * math.sin(2 * math.pi * FC * ti)), -2048, 2047)) for ti in t], dtype=np.int64)

    for name in names:
        sig = make_case(name, t)
        sample = np.array([q_sample(v) for v in sig], dtype=np.int64)
        ref = coarse_reference(sample, cosv, sinv)
        rows.append({"name": name, **ref})
        all_sample.extend(sample.tolist())
        all_cos.extend(cosv.tolist())
        all_sin.extend(sinv.tolist())

    (sim_dir / "receiver_coarse_sample.mem").write_text(
        "\n".join(twos_hex(v, SAMPLE_BITS) for v in all_sample) + "\n",
        encoding="ascii",
    )
    (sim_dir / "receiver_coarse_cos.mem").write_text(
        "\n".join(twos_hex(v, LO_BITS) for v in all_cos) + "\n",
        encoding="ascii",
    )
    (sim_dir / "receiver_coarse_sin.mem").write_text(
        "\n".join(twos_hex(v, LO_BITS) for v in all_sin) + "\n",
        encoding="ascii",
    )

    for key, bits in [
        ("mag_active", 1),
        ("ask_like", 1),
        ("phase_active", 1),
        ("phase_flip_active", 1),
        ("coarse_flags", 3),
    ]:
        (sim_dir / f"receiver_coarse_exp_{key}.mem").write_text(
            "\n".join(twos_hex(row[key], bits) for row in rows) + "\n",
            encoding="ascii",
        )

    csv_path = out_dir / "receiver_coarse_pipeline_expected.csv"
    with csv_path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=["case", *rows[0].keys()])
        writer.writeheader()
        for idx, row in enumerate(rows):
            writer.writerow({"case": idx, **row})

    print(csv_path.resolve())


if __name__ == "__main__":
    main()
