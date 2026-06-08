# 2023D Full-Scope Software Reference

This is the fast software-side golden model for the 2023D task. It is meant to reduce Vivado iteration time: first make the algorithm pass here, then port stable pieces to RTL and hardware.

## Script

`D:\fpga\projects\02_2023_d\scripts\software_full_scope_reference.py`

Run:

```powershell
python .\scripts\software_full_scope_reference.py
```

Output:

`D:\fpga\projects\02_2023_d\outputs\full_scope\software_full_scope_results.csv`

Latest result:

```text
pass 92/92
```

## Covered Items

Basic requirements:

- AM / FM / CW classification.
- AM modulation frequency `F` in `1, 2, 3, 4, 5 kHz`.
- AM modulation index `m_a`.
- FM modulation frequency `F` in `1, 2, 3, 4, 5 kHz`.
- FM modulation index `m_f`.
- FM maximum deviation `delta_f_max = m_f * F`.

Advanced requirements:

- 2ASK / 2PSK / 2FSK classification.
- Keying rate `R_c` in `6, 8, 10 kbps`.
- 2FSK keying coefficient `h`.

## Test Matrix

The current script tests two scenarios:

- `clean`
- `mild_noise`

Each scenario contains:

- `15` AM cases: 5 modulation frequencies times 3 modulation depths.
- `15` FM cases: 5 modulation frequencies times 3 FM indices.
- `1` CW case.
- `3` 2ASK cases.
- `3` 2PSK cases.
- `9` 2FSK cases: 3 keying rates times 3 h values.

Total:

```text
46 cases/scenario * 2 scenarios = 92 cases
```

## Core Feature Strategy

The model uses a common DDC front end:

```text
input -> 2 MHz complex mixer -> moving-average low-pass -> complex baseband z(t)
```

Then it extracts:

- Magnitude `abs(z)` for AM and 2ASK.
- Detrended phase `unwrap(angle(z)) - fitted_line` for FM.
- Instantaneous frequency for 2FSK.
- BPSK-like two-phase clustering for 2PSK.

Important distinction:

- FM is a continuous sinusoidal phase modulation, so its detrended phase has a high sinusoid fit score.
- 2FSK is a two-level frequency-switching signal, so its instantaneous frequency aligns to symbol-rate transitions.
- 2PSK is mainly a two-phase cluster problem, not an amplitude or continuous-FM problem.

## Engineering Meaning

This does not prove the final hardware is done. It proves the software/algorithm reference can cover all scoring signal categories under controlled simulation. The next engineering task is to port stable pieces into RTL and compare each RTL module against this reference before running full Vivado implementation.

Recommended build strategy:

1. Use Python for algorithm exploration and full test sweeps.
2. Use xsim for module-level RTL verification.
3. Use Vivado implementation only after an interface or timing-relevant RTL batch is ready.
4. Use hardware ILA only for board-level ADC/DAC verification and final demos.

