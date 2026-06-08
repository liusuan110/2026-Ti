from pathlib import Path
import csv


MAG_BITS = 57
COUNT_BITS = 6
MAG_SUM_BITS = MAG_BITS + COUNT_BITS
CROSS_SUM_BITS = 64
BLOCK_LEN = 64
MAG_RANGE_PERCENT = 25
ASK_MINMAX_PERCENT = 8
PHASE_ACTIVITY_PERCENT = 8
NEG_DOT_THRESHOLD = 1


def twos_hex(value, bits):
    if value < 0:
        value = (1 << bits) + value
    return f"{value & ((1 << bits) - 1):0{(bits + 3) // 4}x}"


def classify(row):
    mag_range = row["mag_max"] - row["mag_min"]
    mag_active = mag_range * BLOCK_LEN * 100 >= row["mag_sum"] * MAG_RANGE_PERCENT
    ask_like = mag_active and row["mag_min"] * 100 <= row["mag_max"] * ASK_MINMAX_PERCENT
    phase_active = row["abs_cross_sum"] * 100 >= row["mag_sum"] * PHASE_ACTIVITY_PERCENT
    phase_flip = row["negative_dot_count"] >= NEG_DOT_THRESHOLD
    flags = (int(phase_flip) << 2) | (int(phase_active) << 1) | int(mag_active)
    return {
        "mag_active": int(mag_active),
        "ask_like": int(ask_like),
        "phase_active": int(phase_active),
        "phase_flip_active": int(phase_flip),
        "coarse_flags": flags,
    }


def main():
    sim_dir = Path("sim")
    out_dir = Path("outputs/feature_coarse_classifier")
    out_dir.mkdir(parents=True, exist_ok=True)

    cases = [
        {"name": "cw_quiet", "mag_min": 900_000, "mag_max": 910_000, "mag_sum": 64 * 905_000, "abs_cross_sum": 64 * 1_000, "negative_dot_count": 0},
        {"name": "am_like", "mag_min": 300_000, "mag_max": 1_500_000, "mag_sum": 64 * 900_000, "abs_cross_sum": 64 * 1_500, "negative_dot_count": 0},
        {"name": "ask_like", "mag_min": 10_000, "mag_max": 2_000_000, "mag_sum": 64 * 800_000, "abs_cross_sum": 64 * 2_000, "negative_dot_count": 0},
        {"name": "fm_like", "mag_min": 900_000, "mag_max": 930_000, "mag_sum": 64 * 915_000, "abs_cross_sum": 64 * 120_000, "negative_dot_count": 0},
        {"name": "fsk_like", "mag_min": 850_000, "mag_max": 940_000, "mag_sum": 64 * 900_000, "abs_cross_sum": 64 * 180_000, "negative_dot_count": 0},
        {"name": "psk_flip", "mag_min": 880_000, "mag_max": 930_000, "mag_sum": 64 * 900_000, "abs_cross_sum": 64 * 20_000, "negative_dot_count": 3},
        {"name": "am_and_phase", "mag_min": 260_000, "mag_max": 1_600_000, "mag_sum": 64 * 910_000, "abs_cross_sum": 64 * 160_000, "negative_dot_count": 0},
        {"name": "active_all", "mag_min": 5_000, "mag_max": 2_200_000, "mag_sum": 64 * 760_000, "abs_cross_sum": 64 * 190_000, "negative_dot_count": 2},
    ]

    expected = [classify(c) for c in cases]

    for key, bits in [
        ("mag_min", MAG_BITS),
        ("mag_max", MAG_BITS),
        ("mag_sum", MAG_SUM_BITS),
        ("abs_cross_sum", CROSS_SUM_BITS),
        ("negative_dot_count", COUNT_BITS + 1),
    ]:
        (sim_dir / f"coarse_{key}.mem").write_text(
            "\n".join(twos_hex(c[key], bits) for c in cases) + "\n",
            encoding="ascii",
        )

    for key, bits in [
        ("mag_active", 1),
        ("ask_like", 1),
        ("phase_active", 1),
        ("phase_flip_active", 1),
        ("coarse_flags", 3),
    ]:
        (sim_dir / f"coarse_exp_{key}.mem").write_text(
            "\n".join(twos_hex(e[key], bits) for e in expected) + "\n",
            encoding="ascii",
        )

    csv_path = out_dir / "feature_coarse_classifier_expected.csv"
    with csv_path.open("w", newline="", encoding="utf-8") as f:
        fieldnames = ["index", "name", *cases[0].keys(), *expected[0].keys()]
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        for idx, (case, exp) in enumerate(zip(cases, expected)):
            writer.writerow({"index": idx, **case, **exp})

    print(csv_path.resolve())


if __name__ == "__main__":
    main()
