from pathlib import Path

import numpy as np
import matplotlib.pyplot as plt


FS = 20_000_000
FC = 2_000_000
DURATION = 0.008


def am_signal(t, ma=0.6, fm=2_000):
    envelope = 1.0 + ma * np.sin(2 * np.pi * fm * t)
    return envelope * np.cos(2 * np.pi * FC * t)


def fm_signal(t, mf=3.0, fm=5_000):
    phase_mod = mf * np.sin(2 * np.pi * fm * t)
    return np.cos(2 * np.pi * FC * t + phase_mod)


def cw_signal(t):
    return np.cos(2 * np.pi * FC * t)


def moving_average(x, window_len):
    window = np.ones(window_len) / window_len
    return np.convolve(x, window, mode="same")


def quantize_8bit(x, full_scale=2.0):
    adc_max = 127
    adc_min = -128
    codes = np.round(x / full_scale * adc_max)
    codes = np.clip(codes, adc_min, adc_max)
    return codes / adc_max * full_scale


def ddc_iq(x, t, window_len):
    lo_i = 2.0 * np.cos(2 * np.pi * FC * t)
    lo_q = -2.0 * np.sin(2 * np.pi * FC * t)
    i_baseband = moving_average(x * lo_i, window_len)
    q_baseband = moving_average(x * lo_q, window_len)
    return i_baseband, q_baseband


def feature_stats(x, t, start, stop):
    i_baseband, q_baseband = ddc_iq(x, t, window_len=201)
    mag = np.sqrt(i_baseband * i_baseband + q_baseband * q_baseband)
    phase = np.unwrap(np.arctan2(q_baseband, i_baseband))

    mag_u = mag[start:stop]
    phase_u = phase[start:stop]
    phase_ac = phase_u - np.mean(phase_u)

    mag_depth = (np.max(mag_u) - np.min(mag_u)) / (np.max(mag_u) + np.min(mag_u))
    phase_span = (np.max(phase_ac) - np.min(phase_ac)) / 2.0
    phase_std = np.std(phase_ac)

    return {
        "i": i_baseband,
        "q": q_baseband,
        "mag": mag,
        "phase": phase,
        "mag_depth": mag_depth,
        "phase_span": phase_span,
        "phase_std": phase_std,
    }


def classify(stats):
    mag_depth = stats["mag_depth"]
    phase_span = stats["phase_span"]

    if mag_depth > 0.18:
        return "AM"
    if phase_span > 0.45:
        return "FM"
    return "CW"


def main():
    out_dir = Path("outputs/lesson_07")
    out_dir.mkdir(parents=True, exist_ok=True)

    t = np.arange(int(FS * DURATION)) / FS
    raw_signals = {
        "AM": am_signal(t),
        "FM": fm_signal(t),
        "CW": cw_signal(t),
    }
    adc_signals = {name: quantize_8bit(sig) for name, sig in raw_signals.items()}

    start = int(FS * 0.001)
    stop = int(FS * 0.006)
    plot_stop = start + int(FS * 0.0025)

    all_stats = {}
    print("classification_results=")
    for name, sig in adc_signals.items():
        stats = feature_stats(sig, t, start, stop)
        decision = classify(stats)
        all_stats[name] = stats
        print(
            f"  true={name:2s}, decision={decision:2s}, "
            f"mag_depth={stats['mag_depth']:.4f}, "
            f"phase_span={stats['phase_span']:.4f}, "
            f"phase_std={stats['phase_std']:.4f}"
        )

    fig, axes = plt.subplots(3, 1, figsize=(12, 9), sharex=True)

    for name in ["AM", "FM", "CW"]:
        axes[0].plot(t[start:plot_stop] * 1e3, all_stats[name]["mag"][start:plot_stop], linewidth=1.0, label=name)
    axes[0].set_title("I/Q magnitude feature")
    axes[0].set_ylabel("magnitude")
    axes[0].grid(True, alpha=0.3)
    axes[0].legend(loc="upper right")

    for name in ["AM", "FM", "CW"]:
        phase = all_stats[name]["phase"][start:plot_stop]
        phase = phase - np.mean(phase)
        axes[1].plot(t[start:plot_stop] * 1e3, phase, linewidth=1.0, label=name)
    axes[1].set_title("I/Q phase feature")
    axes[1].set_ylabel("phase AC (rad)")
    axes[1].grid(True, alpha=0.3)
    axes[1].legend(loc="upper right")

    labels = ["AM", "FM", "CW"]
    mag_depths = [all_stats[name]["mag_depth"] for name in labels]
    phase_spans = [all_stats[name]["phase_span"] for name in labels]
    x_pos = np.arange(len(labels))
    width = 0.36
    axes[2].bar(x_pos - width / 2, mag_depths, width, label="magnitude depth")
    axes[2].bar(x_pos + width / 2, phase_spans, width, label="phase span")
    axes[2].axhline(0.18, color="#1f77b4", linestyle="--", linewidth=1.0, alpha=0.7)
    axes[2].axhline(0.45, color="#ff7f0e", linestyle="--", linewidth=1.0, alpha=0.7)
    axes[2].set_xticks(x_pos)
    axes[2].set_xticklabels(labels)
    axes[2].set_title("Simple classifier features")
    axes[2].set_xlabel("true signal")
    axes[2].set_ylabel("feature value")
    axes[2].grid(True, axis="y", alpha=0.3)
    axes[2].legend(loc="upper right")

    fig.tight_layout()
    png_path = out_dir / "am_fm_cw_classifier.png"
    fig.savefig(png_path, dpi=150)

    print(f"PNG written to: {png_path.resolve()}")


if __name__ == "__main__":
    main()
