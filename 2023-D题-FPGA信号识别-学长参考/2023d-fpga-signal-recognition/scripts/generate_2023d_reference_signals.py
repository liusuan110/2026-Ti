from pathlib import Path

import numpy as np
import matplotlib.pyplot as plt


FS = 20_000_000
FC = 2_000_000
DURATION = 0.003


def am_signal(t, ma=0.6, fm=1_000):
    envelope = 1.0 + ma * np.sin(2 * np.pi * fm * t)
    carrier = np.cos(2 * np.pi * FC * t)
    return envelope * carrier, envelope


def fm_signal(t, mf=3.0, fm=5_000):
    phase = 2 * np.pi * FC * t + mf * np.sin(2 * np.pi * fm * t)
    signal = np.cos(phase)
    inst_freq = FC + mf * fm * np.cos(2 * np.pi * fm * t)
    return signal, inst_freq


def cw_signal(t):
    return np.cos(2 * np.pi * FC * t)


def main():
    out_dir = Path("outputs/lesson_01")
    out_dir.mkdir(parents=True, exist_ok=True)

    t = np.arange(int(FS * DURATION)) / FS
    am, am_env = am_signal(t)
    fm, fm_inst = fm_signal(t)
    cw = cw_signal(t)

    zoom_samples = int(FS * 20e-6)
    feature_samples = int(FS * 2.5e-3)

    fig, axes = plt.subplots(4, 1, figsize=(12, 9))

    axes[0].plot(t[:zoom_samples] * 1e6, am[:zoom_samples], label="AM", linewidth=0.9)
    axes[0].plot(t[:zoom_samples] * 1e6, fm[:zoom_samples], label="FM", linewidth=0.9, alpha=0.75)
    axes[0].plot(t[:zoom_samples] * 1e6, cw[:zoom_samples], label="CW", linewidth=0.9, alpha=0.75)
    axes[0].set_title("2 MHz carrier, first 20 us")
    axes[0].set_ylabel("normalized u_M")
    axes[0].grid(True, alpha=0.3)
    axes[0].legend(loc="upper right")

    axes[1].plot(t[:feature_samples] * 1e3, am_env[:feature_samples], color="#d62728", linewidth=1.2)
    axes[1].set_title("AM envelope: F = 1 kHz, m_a = 0.6")
    axes[1].set_ylabel("envelope")
    axes[1].grid(True, alpha=0.3)

    axes[2].plot(t[:feature_samples] * 1e3, (fm_inst[:feature_samples] - FC) / 1_000, color="#1f77b4", linewidth=1.2)
    axes[2].set_title("FM instantaneous frequency deviation: F = 5 kHz, m_f = 3, Delta f max = 15 kHz")
    axes[2].set_ylabel("deviation (kHz)")
    axes[2].grid(True, alpha=0.3)

    axes[3].plot(t[:feature_samples] * 1e3, np.ones(feature_samples), color="#2ca02c", linewidth=1.2, label="envelope")
    axes[3].plot(t[:feature_samples] * 1e3, np.zeros(feature_samples), color="#9467bd", linewidth=1.2, label="frequency deviation")
    axes[3].set_title("CW features: constant envelope and zero frequency deviation")
    axes[3].set_xlabel("time")
    axes[3].set_ylabel("feature")
    axes[3].grid(True, alpha=0.3)
    axes[3].legend(loc="upper right")

    fig.tight_layout()
    png_path = out_dir / "reference_signals.png"
    fig.savefig(png_path, dpi=150)

    csv_path = out_dir / "reference_samples.csv"
    decim = 20
    data = np.column_stack((t[::decim], am[::decim], fm[::decim], cw[::decim], am_env[::decim], fm_inst[::decim]))
    np.savetxt(
        csv_path,
        data,
        delimiter=",",
        header="time_s,am,fm,cw,am_envelope,fm_inst_freq_hz",
        comments="",
    )

    print(f"PNG written to: {png_path.resolve()}")
    print(f"CSV written to: {csv_path.resolve()}")


if __name__ == "__main__":
    main()
