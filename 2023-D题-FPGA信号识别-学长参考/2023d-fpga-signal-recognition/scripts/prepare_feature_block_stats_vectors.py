from pathlib import Path
import csv

import numpy as np


MAG_BITS = 57
DELTA_BITS = 57
BLOCK_LEN = 64
COUNT_BITS = 6
MAG_SUM_BITS = MAG_BITS + COUNT_BITS
ABS_DELTA_BITS = DELTA_BITS + 1
CROSS_SUM_BITS = ABS_DELTA_BITS + COUNT_BITS
SAMPLE_COUNT = 256
BLOCK_COUNT = SAMPLE_COUNT // BLOCK_LEN


def twos_hex(value, bits):
    if value < 0:
        value = (1 << bits) + value
    return f"{value & ((1 << bits) - 1):0{(bits + 3) // 4}x}"


def main():
    sim_dir = Path("sim")
    out_dir = Path("outputs/feature_block_stats")
    out_dir.mkdir(parents=True, exist_ok=True)

    n = np.arange(SAMPLE_COUNT)
    amp = 900_000 + 260_000 * np.sin(2 * np.pi * n / 47.0 + 0.5)
    phase = 0.11 * n + 1.1 * np.sin(2 * np.pi * n / 39.0 + 0.2)
    i = np.round(amp * np.cos(phase)).astype(np.int64)
    q = np.round(amp * np.sin(phase)).astype(np.int64)

    mag_sq = i * i + q * q
    dot = np.zeros(SAMPLE_COUNT, dtype=np.int64)
    cross = np.zeros(SAMPLE_COUNT, dtype=np.int64)
    delta_valid = np.zeros(SAMPLE_COUNT, dtype=np.int64)

    for idx in range(1, SAMPLE_COUNT):
        dot[idx] = i[idx] * i[idx - 1] + q[idx] * q[idx - 1]
        cross[idx] = q[idx] * i[idx - 1] - i[idx] * q[idx - 1]
        delta_valid[idx] = 1

    block_rows = []
    for block in range(BLOCK_COUNT):
        lo = block * BLOCK_LEN
        hi = lo + BLOCK_LEN
        mag_b = mag_sq[lo:hi]
        cross_b = cross[lo:hi]
        dot_b = dot[lo:hi]
        dv_b = delta_valid[lo:hi].astype(bool)
        block_rows.append({
            "mag_min": int(np.min(mag_b)),
            "mag_max": int(np.max(mag_b)),
            "mag_sum": int(np.sum(mag_b)),
            "abs_cross_sum": int(np.sum(np.abs(cross_b[dv_b]))),
            "negative_dot_count": int(np.sum(dot_b[dv_b] < 0)),
        })

    (sim_dir / "feature_stats_mag_sq.mem").write_text(
        "\n".join(twos_hex(int(v), MAG_BITS) for v in mag_sq) + "\n",
        encoding="ascii",
    )
    (sim_dir / "feature_stats_dot.mem").write_text(
        "\n".join(twos_hex(int(v), DELTA_BITS) for v in dot) + "\n",
        encoding="ascii",
    )
    (sim_dir / "feature_stats_cross.mem").write_text(
        "\n".join(twos_hex(int(v), DELTA_BITS) for v in cross) + "\n",
        encoding="ascii",
    )
    (sim_dir / "feature_stats_delta_valid.mem").write_text(
        "\n".join(str(int(v)) for v in delta_valid) + "\n",
        encoding="ascii",
    )

    for key, bits in [
        ("mag_min", MAG_BITS),
        ("mag_max", MAG_BITS),
        ("mag_sum", MAG_SUM_BITS),
        ("abs_cross_sum", CROSS_SUM_BITS),
        ("negative_dot_count", COUNT_BITS + 1),
    ]:
        (sim_dir / f"feature_stats_exp_{key}.mem").write_text(
            "\n".join(twos_hex(row[key], bits) for row in block_rows) + "\n",
            encoding="ascii",
        )

    csv_path = out_dir / "feature_block_stats_expected.csv"
    with csv_path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=["block", *block_rows[0].keys()])
        writer.writeheader()
        for idx, row in enumerate(block_rows):
            writer.writerow({"block": idx, **row})

    print(csv_path.resolve())


if __name__ == "__main__":
    main()
