from pathlib import Path

import numpy as np
import matplotlib.pyplot as plt


FS = 20_000_000
FC = 2_000_000
DURATION = 0.006
CANDIDATE_FREQS = np.array([1_000, 2_000, 3_000, 4_000, 5_000])


def fm_signal(t, mf=3.0, fm=5_000):
    phase_mod = mf * np.sin(2 * np.pi * fm * t)
    signal = np.cos(2 * np.pi * FC * t + phase_mod)
    inst_freq = FC + mf * fm * np.cos(2 * np.pi * fm * t)
    return signal, phase_mod, inst_freq


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


def score_candidate_frequencies(t, feature, candidates):
    feature_ac = feature - np.mean(feature)
    scores = []

    for freq in candidates:
        ref_cos = np.cos(2 * np.pi * freq * t)
        ref_sin = np.sin(2 * np.pi * freq * t)
        i_score = np.sum(feature_ac * ref_cos)
        q_score = np.sum(feature_ac * ref_sin)
        scores.append(np.sqrt(i_score * i_score + q_score * q_score))

    scores = np.array(scores)
    return scores / np.max(scores)


def estimate_fm_params(phase_est, t, candidates):
    phase_ac = phase_est - np.mean(phase_est)
    mf_est = (np.max(phase_ac) - np.min(phase_ac)) / 2.0
    scores = score_candidate_frequencies(t, phase_ac, candidates)
    freq_est = int(candidates[np.argmax(scores)])
    delta_f_est = mf_est * freq_est
    return mf_est, freq_est, delta_f_est, scores, phase_ac


def main():
    out_dir = Path("outputs/lesson_06")
    out_dir.mkdir(parents=True, exist_ok=True)

    mf_true = 3.0
    fm_true = 5_000
    delta_f_true = mf_true * fm_true

    t = np.arange(int(FS * DURATION)) / FS
    signal, phase_true, inst_freq_true = fm_signal(t, mf=mf_true, fm=fm_true)
    adc_signal = quantize_8bit(signal)

    window_len = 201
    settle = 4 * window_len
    i_baseband, q_baseband = ddc_iq(adc_signal, t, window_len)
    phase_est = np.unwrap(np.arctan2(q_baseband, i_baseband))

    start = int(FS * 0.001)
    length = int(FS * 0.004)
    stop = start + length

    usable_t = t[start:stop]
    usable_phase = phase_est[start:stop]

    mf_est, freq_est, delta_f_est, scores, phase_ac = estimate_fm_params(
        usable_phase,
        usable_t,
        CANDIDATE_FREQS,
    )

    print(f"true_f_hz={fm_true}")
    print(f"estimated_f_hz={freq_est}")
    print(f"true_mf={mf_true:.4f}")
    print(f"estimated_mf={mf_est:.4f}")
    print(f"true_delta_f_hz={delta_f_true:.2f}")
    print(f"estimated_delta_f_hz={delta_f_est:.2f}")
    print(f"delta_f_error_hz={abs(delta_f_est - delta_f_true):.2f}")
    print("candidate_scores=")
    for freq, score in zip(CANDIDATE_FREQS, scores):
        print(f"  {freq:5d} Hz: {score:.4f}")

    plot_stop = start + int(FS * 0.0025)
    labels = [f"{f // 1000} kHz" for f in CANDIDATE_FREQS]

    fig, axes = plt.subplots(3, 1, figsize=(12, 9))

    axes[0].plot(t[start:plot_stop] * 1e3, phase_true[start:plot_stop], linewidth=1.3, label="true phase_mod")
    axes[0].plot(t[start:plot_stop] * 1e3, phase_est[start:plot_stop], linewidth=1.0, label="estimated phase")
    axes[0].set_title("FM phase after DDC")
    axes[0].set_xlabel("time (ms)")
    axes[0].set_ylabel("phase (rad)")
    axes[0].grid(True, alpha=0.3)
    axes[0].legend(loc="upper right")

    axes[1].plot(t[start:plot_stop] * 1e3, (inst_freq_true[start:plot_stop] - FC) / 1_000, linewidth=1.3)
    axes[1].set_title(f"True instantaneous frequency deviation, Delta f max={delta_f_true/1000:.1f} kHz")
    axes[1].set_xlabel("time (ms)")
    axes[1].set_ylabel("deviation (kHz)")
    axes[1].grid(True, alpha=0.3)

    axes[2].bar(labels, scores, color="#1f77b4")
    axes[2].set_ylim(0, 1.1)
    axes[2].set_title(f"FM modulation frequency score, estimated F={freq_est} Hz")
    axes[2].set_xlabel("candidate F")
    axes[2].set_ylabel("normalized score")
    axes[2].grid(True, axis="y", alpha=0.3)

    fig.tight_layout()
    png_path = out_dir / "fm_parameter_estimator.png"
    fig.savefig(png_path, dpi=150)

    csv_path = out_dir / "fm_parameter_estimator.csv"
    decim = 20
    data = np.column_stack(
        (
            t[::decim],
            adc_signal[::decim],
            phase_true[::decim],
            phase_est[::decim],
            inst_freq_true[::decim],
            i_baseband[::decim],
            q_baseband[::decim],
        )
    )
    np.savetxt(
        csv_path,
        data,
        delimiter=",",
        header="time_s,adc_signal,true_phase,estimated_phase,true_inst_freq_hz,i_baseband,q_baseband",
        comments="",
    )

    print(f"PNG written to: {png_path.resolve()}")
    print(f"CSV written to: {csv_path.resolve()}")


if __name__ == "__main__":
    main()
