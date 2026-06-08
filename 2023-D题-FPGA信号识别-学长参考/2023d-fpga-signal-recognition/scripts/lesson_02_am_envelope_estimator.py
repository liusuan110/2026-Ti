from pathlib import Path

import numpy as np
import matplotlib.pyplot as plt


FS = 20_000_000
FC = 2_000_000
DURATION = 0.004


def am_signal(t, ma=0.6, fm=1_000):
    envelope = 1.0 + ma * np.sin(2 * np.pi * fm * t)
    carrier = np.cos(2 * np.pi * FC * t)
    return envelope * carrier, envelope


def moving_average(x, window_len):
    window = np.ones(window_len) / window_len
    return np.convolve(x, window, mode="same")


def estimate_envelope_and_ma(x, settle_samples, window_len):
    rectified = np.abs(x)
    envelope_est = moving_average(rectified, window_len)

    # Full-wave rectified cosine has average 2/pi, so undo that scale.
    envelope_est = envelope_est * (np.pi / 2)

    usable = envelope_est[settle_samples:-settle_samples]
    env_max = np.max(usable)
    env_min = np.min(usable)
    ma_est = (env_max - env_min) / (env_max + env_min)

    return rectified, envelope_est, ma_est, env_min, env_max


def main():
    out_dir = Path("outputs/lesson_02")
    out_dir.mkdir(parents=True, exist_ok=True)

    ma_true = 0.6
    fm = 1_000
    t = np.arange(int(FS * DURATION)) / FS
    signal, envelope_true = am_signal(t, ma=ma_true, fm=fm)

    adc_bits = 8
    adc_full_scale = 2.0
    adc_max = 2 ** (adc_bits - 1) - 1
    adc_min = -(2 ** (adc_bits - 1))
    adc_codes = np.round(signal / adc_full_scale * adc_max)
    adc_codes = np.clip(adc_codes, adc_min, adc_max)
    adc_signal = adc_codes / adc_max * adc_full_scale

    window_len = 101
    settle_samples = 4 * window_len
    rectified, envelope_est, ma_est, env_min, env_max = estimate_envelope_and_ma(
        adc_signal,
        settle_samples=settle_samples,
        window_len=window_len,
    )

    print(f"true_ma={ma_true:.4f}")
    print(f"estimated_ma={ma_est:.4f}")
    print(f"absolute_error={abs(ma_est - ma_true):.4f}")
    print(f"estimated_envelope_min={env_min:.4f}")
    print(f"estimated_envelope_max={env_max:.4f}")
    print(f"adc_bits={adc_bits}")
    print(f"sample_rate_hz={FS}")
    print(f"carrier_hz={FC}")
    print(f"modulation_hz={fm}")

    zoom = int(FS * 0.0025)
    carrier_zoom = int(FS * 15e-6)

    fig, axes = plt.subplots(3, 1, figsize=(12, 8))

    axes[0].plot(t[:carrier_zoom] * 1e6, adc_signal[:carrier_zoom], linewidth=0.9, label="ADC signal")
    axes[0].plot(t[:carrier_zoom] * 1e6, envelope_true[:carrier_zoom], linewidth=1.2, label="true envelope")
    axes[0].plot(t[:carrier_zoom] * 1e6, -envelope_true[:carrier_zoom], linewidth=1.2, label="-true envelope")
    axes[0].set_title("What ADC sees: AM carrier inside a slow envelope")
    axes[0].set_xlabel("time (us)")
    axes[0].set_ylabel("normalized")
    axes[0].grid(True, alpha=0.3)
    axes[0].legend(loc="upper right")

    axes[1].plot(t[:carrier_zoom] * 1e6, rectified[:carrier_zoom], linewidth=0.9, color="#ff7f0e")
    axes[1].set_title("Step 1: full-wave rectification, abs(x[n])")
    axes[1].set_xlabel("time (us)")
    axes[1].set_ylabel("abs")
    axes[1].grid(True, alpha=0.3)

    axes[2].plot(t[:zoom] * 1e3, envelope_true[:zoom], linewidth=1.4, label="true envelope")
    axes[2].plot(t[:zoom] * 1e3, envelope_est[:zoom], linewidth=1.1, label="estimated envelope")
    axes[2].set_title(f"Step 2: moving average envelope, ma true={ma_true:.2f}, estimated={ma_est:.3f}")
    axes[2].set_xlabel("time (ms)")
    axes[2].set_ylabel("envelope")
    axes[2].grid(True, alpha=0.3)
    axes[2].legend(loc="upper right")

    fig.tight_layout()
    png_path = out_dir / "am_envelope_estimator.png"
    fig.savefig(png_path, dpi=150)

    csv_path = out_dir / "am_envelope_estimator.csv"
    decim = 20
    data = np.column_stack(
        (
            t[::decim],
            signal[::decim],
            adc_signal[::decim],
            envelope_true[::decim],
            rectified[::decim],
            envelope_est[::decim],
        )
    )
    np.savetxt(
        csv_path,
        data,
        delimiter=",",
        header="time_s,ideal_signal,adc_signal,true_envelope,rectified,estimated_envelope",
        comments="",
    )

    print(f"PNG written to: {png_path.resolve()}")
    print(f"CSV written to: {csv_path.resolve()}")


if __name__ == "__main__":
    main()
