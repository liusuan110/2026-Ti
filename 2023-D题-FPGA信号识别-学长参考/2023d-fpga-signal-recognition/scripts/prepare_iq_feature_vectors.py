from pathlib import Path
import csv

import numpy as np


IQ_BITS = 28
PRODUCT_BITS = 2 * IQ_BITS
MAG_SQ_BITS = PRODUCT_BITS + 1
DOT_BITS = PRODUCT_BITS + 1
SAMPLE_COUNT = 256


def twos_hex(value, bits):
    if value < 0:
        value = (1 << bits) + value
    return f"{value & ((1 << bits) - 1):0{(bits + 3) // 4}x}"


def main():
    sim_dir = Path("sim")
    out_dir = Path("outputs/iq_feature")
    out_dir.mkdir(parents=True, exist_ok=True)

    n = np.arange(SAMPLE_COUNT)
    amp = 850_000 + 250_000 * np.sin(2 * np.pi * n / 53.0 + 0.2)
    phase = 0.17 * n + 0.9 * np.sin(2 * np.pi * n / 41.0 + 0.4)
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

    (sim_dir / "iq_feature_i.mem").write_text(
        "\n".join(twos_hex(int(v), IQ_BITS) for v in i) + "\n",
        encoding="ascii",
    )
    (sim_dir / "iq_feature_q.mem").write_text(
        "\n".join(twos_hex(int(v), IQ_BITS) for v in q) + "\n",
        encoding="ascii",
    )
    (sim_dir / "iq_feature_mag_sq.mem").write_text(
        "\n".join(twos_hex(int(v), MAG_SQ_BITS) for v in mag_sq) + "\n",
        encoding="ascii",
    )
    (sim_dir / "iq_feature_dot.mem").write_text(
        "\n".join(twos_hex(int(v), DOT_BITS) for v in dot) + "\n",
        encoding="ascii",
    )
    (sim_dir / "iq_feature_cross.mem").write_text(
        "\n".join(twos_hex(int(v), DOT_BITS) for v in cross) + "\n",
        encoding="ascii",
    )
    (sim_dir / "iq_feature_delta_valid.mem").write_text(
        "\n".join(str(int(v)) for v in delta_valid) + "\n",
        encoding="ascii",
    )

    csv_path = out_dir / "iq_feature_expected.csv"
    with csv_path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(["index", "i", "q", "mag_sq", "delta_valid", "dot", "cross"])
        for idx in range(SAMPLE_COUNT):
            writer.writerow([
                idx,
                int(i[idx]),
                int(q[idx]),
                int(mag_sq[idx]),
                int(delta_valid[idx]),
                int(dot[idx]),
                int(cross[idx]),
            ])

    print(csv_path.resolve())


if __name__ == "__main__":
    main()
