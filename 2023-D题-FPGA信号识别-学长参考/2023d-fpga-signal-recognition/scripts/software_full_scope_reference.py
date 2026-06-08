from pathlib import Path
import csv

import numpy as np


FS = 20_000_000
FC = 2_000_000
DURATION = 0.020
CANDIDATE_F = np.array([1_000, 2_000, 3_000, 4_000, 5_000])
CANDIDATE_RC = np.array([6_000, 8_000, 10_000])


def moving_average(x, window_len):
    window = np.ones(window_len, dtype=np.float64) / window_len
    return np.convolve(x, window, mode="same")


def quantize_8bit(x, full_scale=1.6):
    codes = np.round(x / full_scale * 127)
    codes = np.clip(codes, -128, 127)
    return codes / 127 * full_scale


def add_impairments(x, rng, amplitude=0.8, dc_offset=0.0, noise_rms=0.0, carrier_leak=0.0):
    y = amplitude * x + dc_offset
    if carrier_leak:
        t = np.arange(len(x)) / FS
        y = y + carrier_leak * np.cos(2 * np.pi * FC * t)
    if noise_rms:
        y = y + rng.normal(0.0, noise_rms, size=len(x))
    return quantize_8bit(y)


def am_signal(t, ma, fm):
    return (1.0 + ma * np.sin(2 * np.pi * fm * t)) * np.cos(2 * np.pi * FC * t)


def fm_signal(t, mf, fm):
    return np.cos(2 * np.pi * FC * t + mf * np.sin(2 * np.pi * fm * t))


def cw_signal(t):
    return np.cos(2 * np.pi * FC * t)


def limited_run_bits(count, rng, max_run=3):
    bits = []
    last = int(rng.integers(0, 2))
    run = 0
    for _ in range(count):
        if run >= max_run:
            bit = 1 - last
        else:
            bit = int(rng.integers(0, 2))
        if bit == last:
            run += 1
        else:
            last = bit
            run = 1
        bits.append(bit)
    return np.array(bits, dtype=np.int8)


def nrz_bits_at(t, rc, bits):
    idx = np.floor(t * rc).astype(np.int64)
    idx = np.clip(idx, 0, len(bits) - 1)
    return bits[idx]


def ask_signal(t, rc, bits):
    b = nrz_bits_at(t, rc, bits)
    amp = np.where(b > 0, 1.0, 0.08)
    return amp * np.cos(2 * np.pi * FC * t)


def psk_signal(t, rc, bits):
    b = nrz_bits_at(t, rc, bits)
    phase = np.where(b > 0, np.pi, 0.0)
    return np.cos(2 * np.pi * FC * t + phase)


def fsk_signal(t, rc, h, bits):
    b = nrz_bits_at(t, rc, bits)
    sep = h * rc
    inst_freq = FC + np.where(b > 0, sep / 2.0, -sep / 2.0)
    phase = 2 * np.pi * np.cumsum(inst_freq) / FS
    return np.cos(phase)


def ddc(x, t):
    lo = 2.0 * np.exp(-1j * 2 * np.pi * FC * t)
    mixed = x * lo
    i = moving_average(np.real(mixed), 101)
    q = moving_average(np.imag(mixed), 101)
    return i + 1j * q


def fit_line(t, y):
    slope, intercept = np.polyfit(t, y, 1)
    return slope * t + intercept, slope


def sinusoid_scores(t, feature, candidates):
    f = feature - np.mean(feature)
    scores = []
    amps = []
    r2s = []
    total_energy = np.sum(f * f) + 1e-18
    for freq in candidates:
        c = np.cos(2 * np.pi * freq * t)
        s = np.sin(2 * np.pi * freq * t)
        ci = np.sum(f * c)
        sq = np.sum(f * s)
        a = 2.0 * ci / len(f)
        b = 2.0 * sq / len(f)
        fitted = a * c + b * s
        score = np.hypot(ci, sq)
        scores.append(score)
        amps.append(2.0 * score / len(f))
        r2s.append(max(0.0, 1.0 - np.sum((f - fitted) * (f - fitted)) / total_energy))
    scores = np.array(scores)
    amps = np.array(amps)
    r2s = np.array(r2s)
    if np.max(scores) > 0:
        scores = scores / np.max(scores)
    return scores, amps, r2s


def estimate_grid_rate(t, bit_like, candidates):
    transitions = np.flatnonzero(bit_like[1:] != bit_like[:-1]) + 1
    if len(transitions) < 3:
        return int(candidates[0]), 1e9
    trans_t = t[transitions]
    best_rate = int(candidates[0])
    best_err = 1e9
    for rate in candidates:
        period = 1.0 / rate
        offsets = np.linspace(0, period, 64, endpoint=False)
        errs = []
        for offset in offsets:
            phase = np.mod(trans_t - offset, period)
            dist = np.minimum(phase, period - phase)
            errs.append(np.mean(dist) / period)
        err = min(errs)
        if err < best_err:
            best_err = err
            best_rate = int(rate)
    return best_rate, best_err


def threshold_bits(feature):
    lo = np.percentile(feature, 20)
    hi = np.percentile(feature, 80)
    th = (lo + hi) / 2.0
    return feature > th


def psk_bits_from_complex(z):
    phase2 = 0.5 * np.angle(np.mean(z * z) + 1e-12)
    zr = z * np.exp(-1j * phase2)
    return np.real(zr) > 0


def two_level_separation(feature):
    lo = np.percentile(feature, 20)
    hi = np.percentile(feature, 80)
    return hi - lo, lo, hi


def analyze_signal(x):
    t = np.arange(len(x)) / FS
    start = int(FS * 0.002)
    stop = int(FS * 0.018)
    tt = t[start:stop]

    x = x - np.mean(x[start:stop])
    z = ddc(x, t)
    z = z[start:stop]
    mag = np.abs(z)
    raw_phase = np.angle(z)
    phase = np.unwrap(raw_phase)
    line, slope = fit_line(tt, phase)
    phase_det = phase - line
    inst_freq = moving_average(np.gradient(phase, 1 / FS) / (2 * np.pi), 401)
    inst_freq = inst_freq - np.median(inst_freq)

    mag_mean = np.mean(mag) + 1e-12
    mag_cv = np.std(mag) / mag_mean
    mag_scores, mag_amps, mag_r2s = sinusoid_scores(tt, mag, CANDIDATE_F)
    phase_scores, phase_amps, phase_r2s = sinusoid_scores(tt, phase_det, CANDIDATE_F)

    mag_best = int(np.argmax(mag_scores))
    phase_best = int(np.argmax(phase_scores))
    ma_est = mag_amps[mag_best] / mag_mean
    mf_est = phase_amps[phase_best]
    mag_r2 = float(mag_r2s[mag_best])
    phase_r2 = float(phase_r2s[phase_best])

    mag_bits = threshold_bits(mag)
    psk_bits = psk_bits_from_complex(z)
    freq_sep, freq_lo, freq_hi = two_level_separation(inst_freq)
    freq_bits = threshold_bits(inst_freq)

    mag_rate, mag_grid_err = estimate_grid_rate(tt, mag_bits, CANDIDATE_RC)
    psk_rate, psk_grid_err = estimate_grid_rate(tt, psk_bits, CANDIDATE_RC)
    fsk_rate, fsk_grid_err = estimate_grid_rate(tt, freq_bits, CANDIDATE_RC)

    psk_real = psk_bits.astype(np.int8)
    psk_transition_density = np.mean(psk_real[1:] != psk_real[:-1])

    mag_tone_quality = float(np.max(mag_scores))
    phase_tone_quality = float(np.max(phase_scores))
    freq_activity = np.percentile(np.abs(inst_freq), 90)

    result = {
        "kind": "CW",
        "F_hz": None,
        "ma": None,
        "mf": None,
        "delta_f_hz": None,
        "Rc_hz": None,
        "h": None,
        "debug_mag_cv": mag_cv,
        "debug_freq_activity": freq_activity,
        "debug_mag_grid_err": mag_grid_err,
        "debug_psk_grid_err": psk_grid_err,
        "debug_fsk_grid_err": fsk_grid_err,
        "debug_mag_r2": mag_r2,
        "debug_phase_r2": phase_r2,
    }

    if mag_cv > 0.28 and mag_r2 < 0.45:
        result.update({"kind": "2ASK", "Rc_hz": mag_rate})
        return result

    if ma_est > 0.18 and mag_cv > 0.10 and mag_r2 > 0.65:
        result.update({"kind": "AM", "F_hz": int(CANDIDATE_F[mag_best]), "ma": ma_est})
        return result

    if mf_est > 0.45 and phase_r2 > 0.65:
        freq = int(CANDIDATE_F[phase_best])
        result.update({
            "kind": "FM",
            "F_hz": freq,
            "mf": mf_est,
            "delta_f_hz": mf_est * freq,
        })
        return result

    if freq_activity > 4_000 and fsk_grid_err < 0.08:
        result.update({
            "kind": "2FSK",
            "Rc_hz": fsk_rate,
            "h": freq_sep / fsk_rate,
        })
        return result

    if psk_transition_density > 0.00005 and psk_grid_err < 0.08:
        result.update({"kind": "2PSK", "Rc_hz": psk_rate})
        return result

    return result


def make_cases():
    rng = np.random.default_rng(2023)
    t = np.arange(int(FS * DURATION)) / FS
    cases = []

    for fm in CANDIDATE_F:
        for ma in [0.35, 0.60, 0.90]:
            cases.append(("AM", {"F_hz": int(fm), "ma": ma}, am_signal(t, ma, fm)))

    for fm in CANDIDATE_F:
        for mf in [1.0, 2.5, 5.0]:
            cases.append(("FM", {"F_hz": int(fm), "mf": mf, "delta_f_hz": mf * fm}, fm_signal(t, mf, fm)))

    cases.append(("CW", {}, cw_signal(t)))

    for rc in CANDIDATE_RC:
        bits = limited_run_bits(int(DURATION * rc) + 4, rng)
        cases.append(("2ASK", {"Rc_hz": int(rc)}, ask_signal(t, rc, bits)))

    for rc in CANDIDATE_RC:
        bits = limited_run_bits(int(DURATION * rc) + 4, rng)
        cases.append(("2PSK", {"Rc_hz": int(rc)}, psk_signal(t, rc, bits)))

    for rc in CANDIDATE_RC:
        for h in [2.0, 3.5, 5.0]:
            bits = limited_run_bits(int(DURATION * rc) + 4, rng)
            cases.append(("2FSK", {"Rc_hz": int(rc), "h": h}, fsk_signal(t, rc, h, bits)))

    return cases


def case_ok(kind, truth, result):
    if result["kind"] != kind:
        return False
    if "F_hz" in truth and abs(result["F_hz"] - truth["F_hz"]) > 50:
        return False
    if "ma" in truth and abs(result["ma"] - truth["ma"]) > 0.1:
        return False
    if "mf" in truth and abs(result["mf"] - truth["mf"]) > 0.3:
        return False
    if "delta_f_hz" in truth and abs(result["delta_f_hz"] - truth["delta_f_hz"]) > 300:
        return False
    if "Rc_hz" in truth and result["Rc_hz"] != truth["Rc_hz"]:
        return False
    if "h" in truth and abs(result["h"] - truth["h"]) > 0.4:
        return False
    return True


def fmt(value, digits=3):
    if value is None:
        return ""
    if isinstance(value, (int, np.integer)):
        return str(int(value))
    return f"{float(value):.{digits}f}"


def main():
    out_dir = Path("outputs/full_scope")
    out_dir.mkdir(parents=True, exist_ok=True)

    scenarios = [
        ("clean", {"amplitude": 0.8, "dc_offset": 0.0, "noise_rms": 0.0}),
        ("mild_noise", {"amplitude": 0.7, "dc_offset": 0.08, "noise_rms": 0.015}),
    ]

    rows = []
    rng = np.random.default_rng(7)
    for scenario_name, impair_kwargs in scenarios:
        for true_kind, truth, raw in make_cases():
            x = add_impairments(raw, rng=rng, **impair_kwargs)
            result = analyze_signal(x)
            rows.append({
                "scenario": scenario_name,
                "true_kind": true_kind,
                "decision": result["kind"],
                "ok": case_ok(true_kind, truth, result),
                "true_F_hz": truth.get("F_hz"),
                "estimated_F_hz": result.get("F_hz"),
                "true_ma": truth.get("ma"),
                "estimated_ma": result.get("ma"),
                "true_mf": truth.get("mf"),
                "estimated_mf": result.get("mf"),
                "true_delta_f_hz": truth.get("delta_f_hz"),
                "estimated_delta_f_hz": result.get("delta_f_hz"),
                "true_Rc_hz": truth.get("Rc_hz"),
                "estimated_Rc_hz": result.get("Rc_hz"),
                "true_h": truth.get("h"),
                "estimated_h": result.get("h"),
                "debug_mag_cv": result.get("debug_mag_cv"),
                "debug_freq_activity": result.get("debug_freq_activity"),
                "debug_mag_grid_err": result.get("debug_mag_grid_err"),
                "debug_psk_grid_err": result.get("debug_psk_grid_err"),
                "debug_fsk_grid_err": result.get("debug_fsk_grid_err"),
                "debug_mag_r2": result.get("debug_mag_r2"),
                "debug_phase_r2": result.get("debug_phase_r2"),
            })

    csv_path = out_dir / "software_full_scope_results.csv"
    with csv_path.open("w", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)

    total = len(rows)
    passed = sum(bool(r["ok"]) for r in rows)
    print(f"pass {passed}/{total}")
    print("scenario,true,decision,ok,F,ma,mf,df,Rc,h")
    for r in rows:
        if not r["ok"]:
            print(
                f"{r['scenario']},{r['true_kind']},{r['decision']},{r['ok']},"
                f"{fmt(r['estimated_F_hz'], 0)},{fmt(r['estimated_ma'])},"
                f"{fmt(r['estimated_mf'])},{fmt(r['estimated_delta_f_hz'], 1)},"
                f"{fmt(r['estimated_Rc_hz'], 0)},{fmt(r['estimated_h'])}"
            )
    print(f"CSV written to: {csv_path.resolve()}")


if __name__ == "__main__":
    main()
