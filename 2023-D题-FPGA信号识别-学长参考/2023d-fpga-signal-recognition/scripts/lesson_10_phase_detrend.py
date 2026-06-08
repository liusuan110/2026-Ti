from pathlib import Path

import numpy as np
import matplotlib.pyplot as plt


FS = 20_000_000
FC = 2_000_000
DURATION = 0.008
CANDIDATE_FREQS = np.array([1_000, 2_000, 3_000, 4_000, 5_000])


def fm_signal(t, mf=3.0, fm=5_000, fc=FC):
    phase_mod = mf * np.sin(2 * np.pi * fm * t)
    return np.cos(2 * np.pi * fc * t + phase_mod)


def cw_signal(t, fc=FC):
    return np.cos(2 * np.pi * fc * t)


def moving_average(x, window_len):
    window = np.ones(window_len) / window_len
    return np.convolve(x, window, mode="same")


def quantize_8bit(x, full_scale=2.0):
    adc_max = 127
    adc_min = -128
    codes = np.round(x / full_scale * adc_max)
    codes = np.clip(codes, adc_min, adc_max)
    return codes / adc_max * full_scale


def ddc_iq(x, t, window_len=201):
    lo_i = 2.0 * np.cos(2 * np.pi * FC * t)
    lo_q = -2.0 * np.sin(2 * np.pi * FC * t)
    i_baseband = moving_average(x * lo_i, window_len)
    q_baseband = moving_average(x * lo_q, window_len)
    return i_baseband, q_baseband


def fit_phase_line(t, phase):
    slope, intercept = np.polyfit(t, phase, 1)
    line = slope * t + intercept
    freq_offset_hz = slope / (2 * np.pi)
    return line, freq_offset_hz


def phase_features(x, t, start, stop):
    x = x - np.mean(x[start:stop])
    i_baseband, q_baseband = ddc_iq(x, t)
    phase = np.unwrap(np.arctan2(q_baseband, i_baseband))

    t_u = t[start:stop]
    phase_u = phase[start:stop]
    phase_ac = phase_u - np.mean(phase_u)
    phase_span_raw = (np.max(phase_ac) - np.min(phase_ac)) / 2.0

    line, freq_offset_hz = fit_phase_line(t_u, phase_u)
    phase_detrended = phase_u - line
    phase_detrended = phase_detrended - np.mean(phase_detrended)
    phase_span_detrended = (np.max(phase_detrended) - np.min(phase_detrended)) / 2.0

    return {
        "phase": phase,
        "t_u": t_u,
        "phase_u": phase_u,
        "line": line,
        "phase_detrended": phase_detrended,
        "phase_span_raw": phase_span_raw,
        "phase_span_detrended": phase_span_detrended,
        "freq_offset_hz": freq_offset_hz,
    }


def main():
    out_dir = Path("outputs/lesson_10")
    out_dir.mkdir(parents=True, exist_ok=True)

    t = np.arange(int(FS * DURATION)) / FS
    carrier_offset_hz = 200

    signals = {
        "CW + 200 Hz carrier offset": cw_signal(t, fc=FC + carrier_offset_hz),
        "FM + 200 Hz carrier offset": fm_signal(t, mf=3.0, fm=5_000, fc=FC + carrier_offset_hz),
    }

    start = int(FS * 0.001)
    stop = int(FS * 0.006)
    plot_start = start
    plot_stop = start + int(FS * 0.003)

    results = {}
    print("phase detrend demo")
    for name, raw in signals.items():
        adc_signal = quantize_8bit(raw)
        features = phase_features(adc_signal, t, start, stop)
        results[name] = features
        print(
            f"{name}: "
            f"estimated_freq_offset_hz={features['freq_offset_hz']:.2f}, "
            f"raw_phase_span={features['phase_span_raw']:.4f}, "
            f"detrended_phase_span={features['phase_span_detrended']:.4f}"
        )

    fig, axes = plt.subplots(2, 1, figsize=(12, 7))

    for name, features in results.items():
        idx0 = plot_start - start
        idx1 = plot_stop - start
        axes[0].plot(
            features["t_u"][idx0:idx1] * 1e3,
            features["phase_u"][idx0:idx1],
            linewidth=1.0,
            label=name,
        )
    axes[0].set_title("Raw phase: carrier offset creates a slope")
    axes[0].set_xlabel("time (ms)")
    axes[0].set_ylabel("phase (rad)")
    axes[0].grid(True, alpha=0.3)
    axes[0].legend(loc="upper left")

    for name, features in results.items():
        idx0 = plot_start - start
        idx1 = plot_stop - start
        axes[1].plot(
            features["t_u"][idx0:idx1] * 1e3,
            features["phase_detrended"][idx0:idx1],
            linewidth=1.0,
            label=name,
        )
    axes[1].set_title("Detrended phase: CW becomes flat, FM keeps sinusoidal phase")
    axes[1].set_xlabel("time (ms)")
    axes[1].set_ylabel("phase after detrend (rad)")
    axes[1].grid(True, alpha=0.3)
    axes[1].legend(loc="upper left")

    fig.tight_layout()
    png_path = out_dir / "phase_detrend.png"
    fig.savefig(png_path, dpi=150)
    print(f"PNG written to: {png_path.resolve()}")


if __name__ == "__main__":
    main()
