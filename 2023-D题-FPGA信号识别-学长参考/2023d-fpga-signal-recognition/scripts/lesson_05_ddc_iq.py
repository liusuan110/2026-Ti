from pathlib import Path

import numpy as np
import matplotlib.pyplot as plt


FS = 20_000_000
FC = 2_000_000
DURATION = 0.006


def am_signal(t, ma=0.6, fm=1_000):
    envelope = 1.0 + ma * np.sin(2 * np.pi * fm * t)
    carrier = np.cos(2 * np.pi * FC * t)
    return envelope * carrier, envelope


def fm_signal(t, mf=3.0, fm=5_000):
    phase_mod = mf * np.sin(2 * np.pi * fm * t)
    signal = np.cos(2 * np.pi * FC * t + phase_mod)
    inst_freq = FC + mf * fm * np.cos(2 * np.pi * fm * t)
    return signal, phase_mod, inst_freq


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

    i_mixed = x * lo_i
    q_mixed = x * lo_q

    i_baseband = moving_average(i_mixed, window_len)
    q_baseband = moving_average(q_mixed, window_len)

    return i_baseband, q_baseband


def main():
    out_dir = Path("outputs/lesson_05")
    out_dir.mkdir(parents=True, exist_ok=True)

    t = np.arange(int(FS * DURATION)) / FS
    am, am_envelope = am_signal(t)
    fm, fm_phase, fm_inst_freq = fm_signal(t)
    cw = cw_signal(t)

    am_adc = quantize_8bit(am)
    fm_adc = quantize_8bit(fm)
    cw_adc = quantize_8bit(cw)

    window_len = 201
    settle = 4 * window_len

    am_i, am_q = ddc_iq(am_adc, t, window_len)
    fm_i, fm_q = ddc_iq(fm_adc, t, window_len)
    cw_i, cw_q = ddc_iq(cw_adc, t, window_len)

    am_mag = np.sqrt(am_i * am_i + am_q * am_q)
    fm_mag = np.sqrt(fm_i * fm_i + fm_q * fm_q)
    cw_mag = np.sqrt(cw_i * cw_i + cw_q * cw_q)

    fm_phase_est = np.unwrap(np.arctan2(fm_q, fm_i))
    cw_phase_est = np.unwrap(np.arctan2(cw_q, cw_i))

    start = settle
    stop = start + int(FS * 0.0025)
    zoom_stop = int(FS * 12e-6)

    print(f"sample_rate_hz={FS}")
    print(f"carrier_hz={FC}")
    print(f"ddc_window_len_samples={window_len}")
    print(f"ddc_window_time_us={window_len / FS * 1e6:.3f}")
    print(f"am_mag_min={np.min(am_mag[start:stop]):.4f}")
    print(f"am_mag_max={np.max(am_mag[start:stop]):.4f}")
    print(f"fm_mag_mean={np.mean(fm_mag[start:stop]):.4f}")
    print(f"cw_mag_mean={np.mean(cw_mag[start:stop]):.4f}")

    fig, axes = plt.subplots(4, 1, figsize=(12, 10))

    axes[0].plot(t[:zoom_stop] * 1e6, am_adc[:zoom_stop], linewidth=0.9, label="AM ADC signal")
    axes[0].plot(t[:zoom_stop] * 1e6, fm_adc[:zoom_stop], linewidth=0.9, alpha=0.75, label="FM ADC signal")
    axes[0].set_title("Before DDC: ADC still sees a 2 MHz carrier")
    axes[0].set_xlabel("time (us)")
    axes[0].set_ylabel("normalized")
    axes[0].grid(True, alpha=0.3)
    axes[0].legend(loc="upper right")

    axes[1].plot(t[start:stop] * 1e3, am_envelope[start:stop], linewidth=1.3, label="AM true envelope")
    axes[1].plot(t[start:stop] * 1e3, am_mag[start:stop], linewidth=1.0, label="sqrt(I^2+Q^2)")
    axes[1].set_title("AM after DDC: magnitude is the envelope")
    axes[1].set_xlabel("time (ms)")
    axes[1].set_ylabel("magnitude")
    axes[1].grid(True, alpha=0.3)
    axes[1].legend(loc="upper right")

    axes[2].plot(t[start:stop] * 1e3, fm_phase[start:stop], linewidth=1.3, label="FM true phase modulation")
    axes[2].plot(t[start:stop] * 1e3, fm_phase_est[start:stop], linewidth=1.0, label="atan2(Q, I)")
    axes[2].set_title("FM after DDC: phase carries modulation")
    axes[2].set_xlabel("time (ms)")
    axes[2].set_ylabel("phase (rad)")
    axes[2].grid(True, alpha=0.3)
    axes[2].legend(loc="upper right")

    axes[3].plot(t[start:stop] * 1e3, cw_mag[start:stop], linewidth=1.1, label="CW magnitude")
    axes[3].plot(t[start:stop] * 1e3, cw_phase_est[start:stop], linewidth=1.1, label="CW phase")
    axes[3].set_title("CW after DDC: magnitude and phase are nearly constant")
    axes[3].set_xlabel("time (ms)")
    axes[3].set_ylabel("feature")
    axes[3].grid(True, alpha=0.3)
    axes[3].legend(loc="upper right")

    fig.tight_layout()
    png_path = out_dir / "ddc_iq.png"
    fig.savefig(png_path, dpi=150)

    csv_path = out_dir / "ddc_iq.csv"
    decim = 20
    data = np.column_stack(
        (
            t[::decim],
            am_adc[::decim],
            am_i[::decim],
            am_q[::decim],
            am_mag[::decim],
            fm_adc[::decim],
            fm_i[::decim],
            fm_q[::decim],
            fm_phase_est[::decim],
            cw_adc[::decim],
            cw_mag[::decim],
            cw_phase_est[::decim],
        )
    )
    np.savetxt(
        csv_path,
        data,
        delimiter=",",
        header="time_s,am_adc,am_i,am_q,am_mag,fm_adc,fm_i,fm_q,fm_phase_est,cw_adc,cw_mag,cw_phase_est",
        comments="",
    )

    print(f"PNG written to: {png_path.resolve()}")
    print(f"CSV written to: {csv_path.resolve()}")


if __name__ == "__main__":
    main()
