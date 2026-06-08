from pathlib import Path
import argparse
import csv

import numpy as np


FS = 20_000_000
FB = 250_000
DECIM = FS // FB
FC = 2_000_000
DURATION = 0.012
F_CAND = np.array([1_000, 2_000, 3_000, 4_000, 5_000])
RC_CAND = np.array([6_000, 8_000, 10_000])


def ma(x, n):
    if n <= 1:
        return x
    return np.convolve(x, np.ones(n) / n, mode="same")


def q8(x, full_scale=1.6):
    y = np.round(x / full_scale * 127)
    y = np.clip(y, -128, 127)
    return y / 127 * full_scale


def impair(x, rng, amp=0.8, dc=0.0, noise=0.0):
    y = amp * x + dc
    if noise:
        y = y + rng.normal(0.0, noise, size=len(x))
    return q8(y)


def bits_limited(n, rng, max_run=3):
    out = []
    last = int(rng.integers(0, 2))
    run = 0
    for _ in range(n):
        if run >= max_run:
            bit = 1 - last
        else:
            bit = int(rng.integers(0, 2))
        if bit == last:
            run += 1
        else:
            last = bit
            run = 1
        out.append(bit)
    return np.array(out, dtype=np.int8)


def bitstream(t, rc, bits):
    idx = np.floor(t * rc).astype(np.int64)
    idx = np.clip(idx, 0, len(bits) - 1)
    return bits[idx]


def make_signal(kind, t, rng, **p):
    fc = FC + p.get("fo", 0.0)
    carrier_phase = p.get("carrier_phase", rng.uniform(0, 2 * np.pi))
    mod_phase = p.get("mod_phase", rng.uniform(0, 2 * np.pi))

    if kind == "CW":
        return np.cos(2 * np.pi * fc * t + carrier_phase)

    if kind == "AM":
        env = 1.0 + p["ma"] * np.sin(2 * np.pi * p["F"] * t + mod_phase)
        return env * np.cos(2 * np.pi * fc * t + carrier_phase)

    if kind == "FM":
        phase = 2 * np.pi * fc * t + carrier_phase
        phase = phase + p["mf"] * np.sin(2 * np.pi * p["F"] * t + mod_phase)
        return np.cos(phase)

    bit_count = int(DURATION * p["Rc"]) + 8
    bits = bits_limited(bit_count, rng)
    b = bitstream(t, p["Rc"], bits)

    if kind == "2ASK":
        amp = np.where(b > 0, 1.0, 0.08)
        return amp * np.cos(2 * np.pi * fc * t + carrier_phase)

    if kind == "2PSK":
        phase = np.where(b > 0, np.pi, 0.0)
        return np.cos(2 * np.pi * fc * t + carrier_phase + phase)

    if kind == "2FSK":
        sep = p["h"] * p["Rc"]
        freq = fc + np.where(b > 0, sep / 2.0, -sep / 2.0)
        phase = carrier_phase + 2 * np.pi * np.cumsum(freq) / FS
        return np.cos(phase)

    raise ValueError(kind)


def ddc_decimate(x):
    n = (len(x) // DECIM) * DECIM
    idx = np.arange(n)
    t = idx / FS
    lo = np.exp(-1j * 2 * np.pi * FC * t)
    mixed = 2.0 * x[:n] * lo
    z = mixed.reshape(-1, DECIM).mean(axis=1)
    tb = t.reshape(-1, DECIM).mean(axis=1)
    return tb, z


def fit_line(t, y):
    slope, intercept = np.polyfit(t, y, 1)
    return slope * t + intercept, slope


def sine_fit_bank(t, x, freqs):
    y = x - np.mean(x)
    energy = np.sum(y * y) + 1e-18
    rows = []
    for f in freqs:
        c = np.cos(2 * np.pi * f * t)
        s = np.sin(2 * np.pi * f * t)
        a = 2 * np.sum(y * c) / len(y)
        b = 2 * np.sum(y * s) / len(y)
        fit = a * c + b * s
        amp = np.hypot(a, b)
        r2 = max(0.0, 1.0 - np.sum((y - fit) ** 2) / energy)
        rows.append((int(f), amp, r2))
    return max(rows, key=lambda r: r[2])


def two_level(x):
    lo = np.percentile(x, 20)
    hi = np.percentile(x, 80)
    th = (lo + hi) / 2.0
    bits = x > th
    if np.any(bits) and np.any(~bits):
        lo_m = np.median(x[~bits])
        hi_m = np.median(x[bits])
    else:
        lo_m, hi_m = lo, hi
    sep = abs(hi_m - lo_m)
    spread = np.percentile(x, 95) - np.percentile(x, 5) + 1e-12
    score = sep / spread
    return bits, sep, score


def psk_bits(t, z):
    unit = z / (np.abs(z) + 1e-12)
    phase2 = np.unwrap(np.angle(unit * unit))
    line2, _ = fit_line(t, phase2)
    z_corr = z * np.exp(-0.5j * line2)
    ref = 0.5 * np.angle(np.mean(z_corr * z_corr) + 1e-12)
    z_corr = z_corr * np.exp(-1j * ref)
    strength = abs(np.mean((z_corr / (np.abs(z_corr) + 1e-12)) ** 2))
    return np.real(z_corr) > 0, strength


def grid_rate(t, bits, rates):
    transitions = np.flatnonzero(bits[1:] != bits[:-1]) + 1
    if len(transitions) < 2:
        return int(rates[0]), 1e9, 0.0
    tt = t[transitions]
    density = len(transitions) / len(bits)
    best = (int(rates[0]), 1e9)
    for r in rates:
        period = 1.0 / r
        offsets = np.linspace(0, period, 64, endpoint=False)
        err = 1e9
        for off in offsets:
            phase = np.mod(tt - off, period)
            dist = np.minimum(phase, period - phase)
            err = min(err, float(np.mean(dist) / period))
        if err < best[1]:
            best = (int(r), err)
    return best[0], best[1], density


def analyze(x):
    tb, z = ddc_decimate(x)
    start = np.searchsorted(tb, 0.001)
    stop = np.searchsorted(tb, DURATION - 0.001)
    t = tb[start:stop]
    z = z[start:stop]

    mag = np.abs(z)
    phase = np.unwrap(np.angle(z))
    line, slope = fit_line(t, phase)
    phase_det = phase - line
    inst_f = np.gradient(phase, 1 / FB) / (2 * np.pi)
    inst_f = ma(inst_f - np.median(inst_f), 7)

    mag_f, mag_amp, mag_r2 = sine_fit_bank(t, mag, F_CAND)
    ph_f, ph_amp, ph_r2 = sine_fit_bank(t, phase_det, F_CAND)
    ma_est = mag_amp / (np.mean(mag) + 1e-12)
    mf_est = ph_amp

    mag_bits, mag_sep, mag_two = two_level(mag)
    ask_rate, ask_err, ask_density = grid_rate(t, mag_bits, RC_CAND)

    freq_bits, freq_sep, freq_two = two_level(inst_f)
    fsk_rate, fsk_err, fsk_density = grid_rate(t, freq_bits, RC_CAND)
    h_est = freq_sep / fsk_rate

    pb, psk_strength = psk_bits(t, z)
    psk_rate, psk_err, psk_density = grid_rate(t, pb, RC_CAND)
    psk_balance = min(np.mean(pb), 1.0 - np.mean(pb))

    result = {
        "kind": "CW",
        "F": None,
        "ma": None,
        "mf": None,
        "df": None,
        "Rc": None,
        "h": None,
        "mag_r2": mag_r2,
        "phase_r2": ph_r2,
        "ask_err": ask_err,
        "fsk_err": fsk_err,
        "psk_err": psk_err,
        "freq_two": freq_two,
        "psk_strength": psk_strength,
    }

    if mag_r2 > 0.70 and ma_est > 0.18:
        result.update({"kind": "AM", "F": mag_f, "ma": ma_est})
        return result

    if mag_two > 0.72 and ask_err < 0.08 and ask_density > 0.0005:
        result.update({"kind": "2ASK", "Rc": ask_rate})
        return result

    if ph_r2 > 0.62 and mf_est > 0.45:
        result.update({"kind": "FM", "F": ph_f, "mf": mf_est, "df": mf_est * ph_f})
        return result

    if freq_two > 0.68 and fsk_err < 0.08 and 1.6 <= h_est <= 5.6 and fsk_density > 0.0005:
        result.update({"kind": "2FSK", "Rc": fsk_rate, "h": h_est})
        return result

    if psk_err < 0.08 and psk_density > 0.00005 and psk_balance > 0.05:
        result.update({"kind": "2PSK", "Rc": psk_rate})
        return result

    return result


def ok(kind, truth, r):
    if r["kind"] != kind:
        return False
    if "F" in truth and abs(r["F"] - truth["F"]) > 50:
        return False
    if "ma" in truth and abs(r["ma"] - truth["ma"]) > 0.1:
        return False
    if "mf" in truth and abs(r["mf"] - truth["mf"]) > 0.3:
        return False
    if "df" in truth and abs(r["df"] - truth["df"]) > 300:
        return False
    if "Rc" in truth and r["Rc"] != truth["Rc"]:
        return False
    if "h" in truth and abs(r["h"] - truth["h"]) > 0.45:
        return False
    return True


def deterministic_cases(rng):
    cases = []
    t = np.arange(int(FS * DURATION)) / FS
    for f in F_CAND:
        for m in [0.35, 0.60, 0.90]:
            p = {"F": int(f), "ma": m}
            cases.append(("AM", p, make_signal("AM", t, rng, **p)))
    for f in F_CAND:
        for m in [1.0, 2.5, 5.0]:
            p = {"F": int(f), "mf": m, "df": int(f) * m}
            cases.append(("FM", p, make_signal("FM", t, rng, **p)))
    cases.append(("CW", {}, make_signal("CW", t, rng)))
    for rc in RC_CAND:
        p = {"Rc": int(rc)}
        cases.append(("2ASK", p, make_signal("2ASK", t, rng, **p)))
        cases.append(("2PSK", p, make_signal("2PSK", t, rng, **p)))
        for h in [2.0, 3.5, 5.0]:
            p2 = {"Rc": int(rc), "h": h}
            cases.append(("2FSK", p2, make_signal("2FSK", t, rng, **p2)))
    return cases


def random_cases(rng, count_per_kind=12):
    cases = []
    t = np.arange(int(FS * DURATION)) / FS
    for _ in range(count_per_kind):
        f = int(rng.choice(F_CAND))
        ma_v = float(rng.uniform(0.32, 0.95))
        p = {"F": f, "ma": ma_v, "fo": float(rng.uniform(-300, 300))}
        cases.append(("AM", p, make_signal("AM", t, rng, **p)))

        f = int(rng.choice(F_CAND))
        mf_v = float(rng.uniform(1.0, 5.0))
        p = {"F": f, "mf": mf_v, "df": mf_v * f, "fo": float(rng.uniform(-300, 300))}
        cases.append(("FM", p, make_signal("FM", t, rng, **p)))

        p = {"fo": float(rng.uniform(-300, 300))}
        cases.append(("CW", {}, make_signal("CW", t, rng, **p)))

        rc = int(rng.choice(RC_CAND))
        p = {"Rc": rc, "fo": float(rng.uniform(-300, 300))}
        cases.append(("2ASK", {"Rc": rc}, make_signal("2ASK", t, rng, **p)))

        rc = int(rng.choice(RC_CAND))
        p = {"Rc": rc, "fo": float(rng.uniform(-300, 300))}
        cases.append(("2PSK", {"Rc": rc}, make_signal("2PSK", t, rng, **p)))

        rc = int(rng.choice(RC_CAND))
        h = float(rng.uniform(2.0, 5.0))
        p = {"Rc": rc, "h": h, "fo": float(rng.uniform(-300, 300))}
        cases.append(("2FSK", {"Rc": rc, "h": h}, make_signal("2FSK", t, rng, **p)))
    return cases


def run_suite(seed=20230515, random_count=10):
    rng = np.random.default_rng(seed)
    scenarios = [
        ("det_clean", deterministic_cases(rng), {"amp": 0.8, "dc": 0.0, "noise": 0.0}),
        ("det_mild", deterministic_cases(rng), {"amp": 0.72, "dc": 0.06, "noise": 0.012}),
        ("rand_mild", random_cases(rng, random_count), {"amp": 0.75, "dc": 0.05, "noise": 0.015}),
        ("rand_noisy", random_cases(rng, random_count), {"amp": 0.65, "dc": -0.04, "noise": 0.025}),
    ]
    rows = []
    for scenario, cases, imp in scenarios:
        for kind, truth, sig in cases:
            x = impair(sig, rng, **imp)
            r = analyze(x)
            rows.append({
                "scenario": scenario,
                "true_kind": kind,
                "decision": r["kind"],
                "ok": ok(kind, truth, r),
                "true_F": truth.get("F"),
                "est_F": r.get("F"),
                "true_ma": truth.get("ma"),
                "est_ma": r.get("ma"),
                "true_mf": truth.get("mf"),
                "est_mf": r.get("mf"),
                "true_df": truth.get("df"),
                "est_df": r.get("df"),
                "true_Rc": truth.get("Rc"),
                "est_Rc": r.get("Rc"),
                "true_h": truth.get("h"),
                "est_h": r.get("h"),
                "mag_r2": r["mag_r2"],
                "phase_r2": r["phase_r2"],
                "ask_err": r["ask_err"],
                "fsk_err": r["fsk_err"],
                "psk_err": r["psk_err"],
                "freq_two": r["freq_two"],
                "psk_strength": r["psk_strength"],
            })
    return rows


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--seed", type=int, default=20230515)
    parser.add_argument("--random-count", type=int, default=10)
    args = parser.parse_args()

    out_dir = Path("outputs/full_scope")
    out_dir.mkdir(parents=True, exist_ok=True)
    rows = run_suite(seed=args.seed, random_count=args.random_count)
    out = out_dir / "software_full_scope_advanced_results.csv"
    with out.open("w", encoding="utf-8", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        w.writeheader()
        w.writerows(rows)

    passed = sum(r["ok"] for r in rows)
    print(f"seed {args.seed}")
    print(f"random_count {args.random_count}")
    print(f"pass {passed}/{len(rows)}")
    for r in rows:
        if not r["ok"]:
            print(
                r["scenario"], r["true_kind"], "->", r["decision"],
                "F", r["true_F"], r["est_F"],
                "ma", r["true_ma"], r["est_ma"],
                "mf", r["true_mf"], r["est_mf"],
                "Rc", r["true_Rc"], r["est_Rc"],
                "h", r["true_h"], r["est_h"],
            )
    print(out.resolve())


if __name__ == "__main__":
    main()
