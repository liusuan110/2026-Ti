from pathlib import Path

import numpy as np
import matplotlib.pyplot as plt


FS = 20_000_000
FC = 2_000_000
DURATION = 0.010
CANDIDATE_FREQS = np.array([1_000, 2_000, 3_000, 4_000, 5_000])


def am_signal(t, ma=0.6, fm=4_000):
    envelope = 1.0 + ma * np.sin(2 * np.pi * fm * t)
    carrier = np.cos(2 * np.pi * FC * t)
    return envelope * carrier, envelope


def moving_average(x, window_len):
    window = np.ones(window_len) / window_len
    return np.convolve(x, window, mode="same")


def estimate_envelope(x, window_len):
    rectified = np.abs(x)
    envelope_est = moving_average(rectified, window_len)
    return envelope_est * (np.pi / 2)


def correlation_scores(x, sample_rate, candidates):
    n = np.arange(len(x))
    scores = []

    for freq in candidates:
        phase = 2 * np.pi * freq * n / sample_rate
        i_score = np.sum(x * np.cos(phase))
        q_score = np.sum(x * np.sin(phase))
        scores.append(np.sqrt(i_score * i_score + q_score * q_score))

    scores = np.array(scores)
    return scores / np.max(scores)


def goertzel_power(x, sample_rate, target_freq):
    n_samples = len(x)
    k = target_freq * n_samples / sample_rate
    omega = 2 * np.pi * k / n_samples
    coeff = 2 * np.cos(omega)

    s_prev = 0.0
    s_prev2 = 0.0

    for sample in x:
        s = sample + coeff * s_prev - s_prev2
        s_prev2 = s_prev
        s_prev = s

    power = s_prev2 * s_prev2 + s_prev * s_prev - coeff * s_prev * s_prev2
    return power


def goertzel_scores(x, sample_rate, candidates):
    powers = np.array([goertzel_power(x, sample_rate, freq) for freq in candidates])
    return powers / np.max(powers)


def main():
    out_dir = Path("outputs/lesson_04")
    out_dir.mkdir(parents=True, exist_ok=True)

    ma_true = 0.6
    fm_true = 4_000
    t = np.arange(int(FS * DURATION)) / FS
    signal, _ = am_signal(t, ma=ma_true, fm=fm_true)

    adc_bits = 8
    adc_full_scale = 2.0
    adc_max = 2 ** (adc_bits - 1) - 1
    adc_min = -(2 ** (adc_bits - 1))
    adc_codes = np.round(signal / adc_full_scale * adc_max)
    adc_codes = np.clip(adc_codes, adc_min, adc_max)
    adc_signal = adc_codes / adc_max * adc_full_scale

    envelope_est = estimate_envelope(adc_signal, window_len=101)

    start = int(FS * 0.001)
    length = int(FS * 0.006)
    env_ac = envelope_est[start:start + length]
    env_ac = env_ac - np.mean(env_ac)

    corr = correlation_scores(env_ac, FS, CANDIDATE_FREQS)
    goertzel = goertzel_scores(env_ac, FS, CANDIDATE_FREQS)

    corr_f = int(CANDIDATE_FREQS[np.argmax(corr)])
    goertzel_f = int(CANDIDATE_FREQS[np.argmax(goertzel)])

    print(f"true_f_hz={fm_true}")
    print(f"correlation_estimated_f_hz={corr_f}")
    print(f"goertzel_estimated_f_hz={goertzel_f}")
    print("scores=")
    for freq, c_score, g_score in zip(CANDIDATE_FREQS, corr, goertzel):
        print(f"  {freq:5d} Hz: correlation={c_score:.4f}, goertzel={g_score:.4f}")

    labels = [f"{f // 1000} kHz" for f in CANDIDATE_FREQS]
    x_pos = np.arange(len(labels))
    width = 0.36

    fig, axes = plt.subplots(2, 1, figsize=(12, 7))

    plot_n = int(FS * 0.003)
    axes[0].plot(t[:plot_n] * 1e3, envelope_est[:plot_n], linewidth=1.1)
    axes[0].set_title("Estimated AM envelope")
    axes[0].set_xlabel("time (ms)")
    axes[0].set_ylabel("envelope")
    axes[0].grid(True, alpha=0.3)

    axes[1].bar(x_pos - width / 2, corr, width, label="correlation score")
    axes[1].bar(x_pos + width / 2, goertzel, width, label="Goertzel power")
    axes[1].set_xticks(x_pos)
    axes[1].set_xticklabels(labels)
    axes[1].set_ylim(0, 1.1)
    axes[1].set_title("Same frequency decision, two scoring methods")
    axes[1].set_xlabel("candidate F")
    axes[1].set_ylabel("normalized score")
    axes[1].grid(True, axis="y", alpha=0.3)
    axes[1].legend(loc="upper right")

    fig.tight_layout()
    png_path = out_dir / "goertzel_from_correlation.png"
    fig.savefig(png_path, dpi=150)
    print(f"PNG written to: {png_path.resolve()}")


if __name__ == "__main__":
    main()
