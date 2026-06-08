from pathlib import Path

import numpy as np
import matplotlib.pyplot as plt


FS = 20_000_000
FC = 2_000_000
DURATION = 0.010
CANDIDATE_FREQS = np.array([1_000, 2_000, 3_000, 4_000, 5_000])


def am_signal(t, ma=0.6, fm=3_000):
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


def estimate_ma(envelope_est):
    env_max = np.max(envelope_est)
    env_min = np.min(envelope_est)
    ma_est = (env_max - env_min) / (env_max + env_min)
    return ma_est, env_min, env_max


def score_candidate_frequencies(t, envelope_est, candidates):
    env_ac = envelope_est - np.mean(envelope_est)
    scores = []

    for freq in candidates:
        ref_cos = np.cos(2 * np.pi * freq * t)
        ref_sin = np.sin(2 * np.pi * freq * t)
        i_score = np.sum(env_ac * ref_cos)
        q_score = np.sum(env_ac * ref_sin)
        scores.append(np.sqrt(i_score * i_score + q_score * q_score))

    scores = np.array(scores)
    return scores / np.max(scores)


def main():
    out_dir = Path("outputs/lesson_03")
    out_dir.mkdir(parents=True, exist_ok=True)

    ma_true = 0.6
    fm_true = 3_000
    t = np.arange(int(FS * DURATION)) / FS
    signal, envelope_true = am_signal(t, ma=ma_true, fm=fm_true)

    adc_bits = 8
    adc_full_scale = 2.0
    adc_max = 2 ** (adc_bits - 1) - 1
    adc_min = -(2 ** (adc_bits - 1))
    adc_codes = np.round(signal / adc_full_scale * adc_max)
    adc_codes = np.clip(adc_codes, adc_min, adc_max)
    adc_signal = adc_codes / adc_max * adc_full_scale

    window_len = 101
    settle_samples = 4 * window_len
    envelope_est = estimate_envelope(adc_signal, window_len=window_len)

    usable_t = t[settle_samples:-settle_samples]
    usable_env = envelope_est[settle_samples:-settle_samples]

    ma_est, env_min, env_max = estimate_ma(usable_env)
    scores = score_candidate_frequencies(usable_t, usable_env, CANDIDATE_FREQS)
    estimated_f = int(CANDIDATE_FREQS[np.argmax(scores)])

    print(f"true_f_hz={fm_true}")
    print(f"estimated_f_hz={estimated_f}")
    print(f"true_ma={ma_true:.4f}")
    print(f"estimated_ma={ma_est:.4f}")
    print(f"estimated_envelope_min={env_min:.4f}")
    print(f"estimated_envelope_max={env_max:.4f}")
    print("candidate_scores=")
    for freq, score in zip(CANDIDATE_FREQS, scores):
        print(f"  {freq:5d} Hz: {score:.4f}")

    plot_samples = int(FS * 0.003)
    fig, axes = plt.subplots(2, 1, figsize=(12, 7))

    axes[0].plot(t[:plot_samples] * 1e3, envelope_true[:plot_samples], linewidth=1.3, label="true envelope")
    axes[0].plot(t[:plot_samples] * 1e3, envelope_est[:plot_samples], linewidth=1.0, label="estimated envelope")
    axes[0].set_title(f"AM envelope, true F={fm_true} Hz, estimated F={estimated_f} Hz")
    axes[0].set_xlabel("time (ms)")
    axes[0].set_ylabel("envelope")
    axes[0].grid(True, alpha=0.3)
    axes[0].legend(loc="upper right")

    axes[1].bar([str(f // 1000) + " kHz" for f in CANDIDATE_FREQS], scores, color="#1f77b4")
    axes[1].set_title("Candidate frequency scores")
    axes[1].set_xlabel("candidate F")
    axes[1].set_ylabel("normalized score")
    axes[1].set_ylim(0, 1.1)
    axes[1].grid(True, axis="y", alpha=0.3)

    fig.tight_layout()
    png_path = out_dir / "am_frequency_estimator.png"
    fig.savefig(png_path, dpi=150)

    csv_path = out_dir / "am_frequency_estimator.csv"
    decim = 20
    data = np.column_stack(
        (
            t[::decim],
            signal[::decim],
            adc_signal[::decim],
            envelope_true[::decim],
            envelope_est[::decim],
        )
    )
    np.savetxt(
        csv_path,
        data,
        delimiter=",",
        header="time_s,ideal_signal,adc_signal,true_envelope,estimated_envelope",
        comments="",
    )

    print(f"PNG written to: {png_path.resolve()}")
    print(f"CSV written to: {csv_path.resolve()}")


if __name__ == "__main__":
    main()
