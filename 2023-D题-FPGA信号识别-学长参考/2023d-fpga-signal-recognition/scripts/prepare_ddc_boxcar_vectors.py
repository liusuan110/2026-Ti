from pathlib import Path
import csv
import math

import numpy as np


FS = 20_000_000
FC = 2_000_000
DECIM = 80
SAMPLE_COUNT = 1600
SAMPLE_BITS = 8
LO_BITS = 12
PRODUCT_BITS = SAMPLE_BITS + LO_BITS + 1
ACC_BITS = PRODUCT_BITS + 7


def twos_hex(value, bits):
    if value < 0:
        value = (1 << bits) + value
    return f"{value & ((1 << bits) - 1):0{(bits + 3) // 4}x}"


def clamp_round(x, lo, hi):
    return int(np.clip(np.round(x), lo, hi))


def main():
    sim_dir = Path("sim")
    out_dir = Path("outputs/ddc_boxcar")
    out_dir.mkdir(parents=True, exist_ok=True)

    n = np.arange(SAMPLE_COUNT)
    t = n / FS

    env = 1.0 + 0.55 * np.sin(2 * np.pi * 3_000 * t + 0.31)
    sig = env * np.cos(2 * np.pi * FC * t + 0.73)
    sig += 0.35 * np.cos(2 * np.pi * (FC + 18_000) * t + 1.1)

    sample = np.array([clamp_round(70 * v, -128, 127) for v in sig], dtype=np.int64)
    cosv = np.array([clamp_round(2047 * math.cos(2 * math.pi * FC * ti), -2048, 2047) for ti in t], dtype=np.int64)
    sinv = np.array([clamp_round(2047 * math.sin(2 * math.pi * FC * ti), -2048, 2047) for ti in t], dtype=np.int64)

    i_prod = sample * cosv
    q_prod = sample * (-sinv)
    i_sum = i_prod.reshape(-1, DECIM).sum(axis=1)
    q_sum = q_prod.reshape(-1, DECIM).sum(axis=1)

    (sim_dir / "ddc_boxcar_sample.mem").write_text(
        "\n".join(twos_hex(int(v), SAMPLE_BITS) for v in sample) + "\n",
        encoding="ascii",
    )
    (sim_dir / "ddc_boxcar_cos.mem").write_text(
        "\n".join(twos_hex(int(v), LO_BITS) for v in cosv) + "\n",
        encoding="ascii",
    )
    (sim_dir / "ddc_boxcar_sin.mem").write_text(
        "\n".join(twos_hex(int(v), LO_BITS) for v in sinv) + "\n",
        encoding="ascii",
    )
    (sim_dir / "ddc_boxcar_exp_i.mem").write_text(
        "\n".join(twos_hex(int(v), ACC_BITS) for v in i_sum) + "\n",
        encoding="ascii",
    )
    (sim_dir / "ddc_boxcar_exp_q.mem").write_text(
        "\n".join(twos_hex(int(v), ACC_BITS) for v in q_sum) + "\n",
        encoding="ascii",
    )

    csv_path = out_dir / "ddc_boxcar_expected.csv"
    with csv_path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(["index", "i_sum", "q_sum"])
        for idx, (ii, qq) in enumerate(zip(i_sum, q_sum)):
            writer.writerow([idx, int(ii), int(qq)])

    print(csv_path.resolve())


if __name__ == "__main__":
    main()
