from pathlib import Path
import csv

import numpy as np
import matplotlib.pyplot as plt


LUT_ADDR_BITS = 10
AMP_BITS = 12
LUT_SIZE = 1 << LUT_ADDR_BITS


def quantize_signed(x, bits):
    max_code = (1 << (bits - 1)) - 1
    min_code = -(1 << (bits - 1))
    code = np.round(x * max_code).astype(int)
    return np.clip(code, min_code, max_code)


def signed_to_twos_complement(value, bits):
    if value < 0:
        value = (1 << bits) + value
    return value


def write_coe(path, values, bits, radix=10):
    with path.open("w", encoding="utf-8") as f:
        f.write(f"memory_initialization_radix={radix};\n")
        f.write("memory_initialization_vector=\n")
        for index, value in enumerate(values):
            if radix == 16:
                text = f"{signed_to_twos_complement(int(value), bits):0{(bits + 3) // 4}X}"
            else:
                text = str(int(value))
            suffix = "," if index < len(values) - 1 else ";"
            f.write(f"{text}{suffix}\n")


def build_full_wave_lut():
    addr = np.arange(LUT_SIZE)
    theta = 2 * np.pi * addr / LUT_SIZE
    ideal = np.cos(theta)
    codes = quantize_signed(ideal, AMP_BITS)
    reconstructed = codes / ((1 << (AMP_BITS - 1)) - 1)
    error = reconstructed - ideal
    return addr, theta, ideal, codes, reconstructed, error


def build_quarter_wave_lut():
    quarter_size = LUT_SIZE // 4 + 1
    qaddr = np.arange(quarter_size)
    theta = 0.5 * np.pi * qaddr / (quarter_size - 1)
    ideal = np.cos(theta)
    return quantize_signed(ideal, AMP_BITS)


def quarter_wave_cos_from_table(addr, qlut):
    quarter = LUT_SIZE // 4
    addr_mod = addr % LUT_SIZE
    quadrant = addr_mod // quarter
    offset = addr_mod % quarter

    if quadrant == 0:
        return int(qlut[offset])
    if quadrant == 1:
        return -int(qlut[quarter - offset])
    if quadrant == 2:
        return -int(qlut[offset])
    return int(qlut[quarter - offset])


def main():
    out_dir = Path("outputs/lesson_14")
    out_dir.mkdir(parents=True, exist_ok=True)

    addr, theta, ideal, codes, reconstructed, error = build_full_wave_lut()
    qlut = build_quarter_wave_lut()
    quarter_codes = np.array([quarter_wave_cos_from_table(int(a), qlut) for a in addr])

    csv_path = out_dir / "cos_lut_samples.csv"
    with csv_path.open("w", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(
            f,
            fieldnames=[
                "addr",
                "theta_rad",
                "ideal_cos",
                "full_lut_code",
                "quarter_lut_code",
                "quantization_error",
            ],
        )
        writer.writeheader()
        for row in range(LUT_SIZE):
            writer.writerow({
                "addr": int(addr[row]),
                "theta_rad": float(theta[row]),
                "ideal_cos": float(ideal[row]),
                "full_lut_code": int(codes[row]),
                "quarter_lut_code": int(quarter_codes[row]),
                "quantization_error": float(error[row]),
            })

    write_coe(out_dir / "cos_full_wave_1024x12.coe", codes, AMP_BITS, radix=10)
    write_coe(out_dir / "cos_full_wave_1024x12_hex.coe", codes, AMP_BITS, radix=16)

    max_abs_error = np.max(np.abs(error))
    rms_error = np.sqrt(np.mean(error * error))
    quarter_matches = np.all(quarter_codes == codes)

    fig, axes = plt.subplots(3, 1, figsize=(12, 8), sharex=True)
    axes[0].plot(addr, ideal, label="ideal cos")
    axes[0].step(addr, reconstructed, where="post", label="12-bit LUT output", alpha=0.8)
    axes[0].set_title("Cosine LUT: ideal value vs quantized table output")
    axes[0].set_ylabel("amplitude")
    axes[0].grid(True, alpha=0.3)
    axes[0].legend(loc="upper right")

    axes[1].plot(addr, error)
    axes[1].set_title("Quantization error from 12-bit amplitude")
    axes[1].set_ylabel("error")
    axes[1].grid(True, alpha=0.3)

    axes[2].plot(addr[:80], codes[:80], marker="o", linewidth=1.0)
    axes[2].set_title("First 80 integer ROM codes")
    axes[2].set_xlabel("LUT address")
    axes[2].set_ylabel("code")
    axes[2].grid(True, alpha=0.3)

    fig.tight_layout()
    png_path = out_dir / "cos_lut_generation.png"
    fig.savefig(png_path, dpi=150)

    print("Cos LUT generation demo")
    print(f"LUT_SIZE = {LUT_SIZE}")
    print(f"AMP_BITS = {AMP_BITS}")
    print(f"max_abs_error = {max_abs_error:.8f}")
    print(f"rms_error = {rms_error:.8f}")
    print(f"quarter_wave_reconstruction_matches_full_lut = {quarter_matches}")
    print(f"CSV written to: {csv_path.resolve()}")
    print(f"COE written to: {(out_dir / 'cos_full_wave_1024x12.coe').resolve()}")
    print(f"PNG written to: {png_path.resolve()}")


if __name__ == "__main__":
    main()
