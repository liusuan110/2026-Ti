from pathlib import Path
import csv


FREQS = [1000, 2000, 3000, 4000, 5000]

KIND_CW = 0
KIND_AM_LIKE = 1
KIND_FM_LIKE = 2
KIND_PSK_LIKE = 3


def hex_value(value, bits):
    return f"{value & ((1 << bits) - 1):0{(bits + 3) // 4}x}"


def freq_for_index(index):
    return FREQS[index] if 0 <= index < len(FREQS) else 0


def main():
    sim_dir = Path("sim")
    out_dir = Path("outputs/recognition_parameter_decoder")
    out_dir.mkdir(parents=True, exist_ok=True)

    rows = [
        {"kind": KIND_CW, "selected": 0, "am_best": 1, "fm_best": 2},
        {"kind": KIND_AM_LIKE, "selected": 0, "am_best": 0, "fm_best": 4},
        {"kind": KIND_AM_LIKE, "selected": 4, "am_best": 4, "fm_best": 3},
        {"kind": KIND_FM_LIKE, "selected": 1, "am_best": 3, "fm_best": 1},
        {"kind": KIND_FM_LIKE, "selected": 4, "am_best": 4, "fm_best": 4},
        {"kind": KIND_PSK_LIKE, "selected": 2, "am_best": 2, "fm_best": 2},
    ]

    for row in rows:
        kind = row["kind"]
        row["is_cw"] = int(kind == KIND_CW)
        row["is_am_like"] = int(kind == KIND_AM_LIKE)
        row["is_fm_like"] = int(kind == KIND_FM_LIKE)
        row["is_psk_like"] = int(kind == KIND_PSK_LIKE)
        row["mod_freq_valid"] = int(kind in (KIND_AM_LIKE, KIND_FM_LIKE))
        row["mod_freq_hz"] = freq_for_index(row["selected"]) if row["mod_freq_valid"] else 0
        row["am_freq_hz"] = freq_for_index(row["am_best"])
        row["fm_freq_hz"] = freq_for_index(row["fm_best"])

    mems = {
        "kind": (3, [row["kind"] for row in rows]),
        "selected": (3, [row["selected"] for row in rows]),
        "am_best": (3, [row["am_best"] for row in rows]),
        "fm_best": (3, [row["fm_best"] for row in rows]),
        "exp_is_cw": (1, [row["is_cw"] for row in rows]),
        "exp_is_am_like": (1, [row["is_am_like"] for row in rows]),
        "exp_is_fm_like": (1, [row["is_fm_like"] for row in rows]),
        "exp_is_psk_like": (1, [row["is_psk_like"] for row in rows]),
        "exp_mod_freq_valid": (1, [row["mod_freq_valid"] for row in rows]),
        "exp_mod_freq_hz": (16, [row["mod_freq_hz"] for row in rows]),
        "exp_am_freq_hz": (16, [row["am_freq_hz"] for row in rows]),
        "exp_fm_freq_hz": (16, [row["fm_freq_hz"] for row in rows]),
    }

    for name, (bits, values) in mems.items():
        (sim_dir / f"recognition_param_{name}.mem").write_text(
            "\n".join(hex_value(v, bits) for v in values) + "\n",
            encoding="ascii",
        )

    csv_path = out_dir / "recognition_parameter_decoder_expected.csv"
    with csv_path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)

    print(csv_path.resolve())


if __name__ == "__main__":
    main()
