# 2023D Hardware Integration Roadmap

This file links the algorithm-learning project with the ACX720 + ACM2108 hardware project.

## Current Hardware Evidence

Hardware project:

`D:\fpga\projects\acx720_acm2108_2mhz_dac`

Latest demo build:

- `D:\fpga\projects\acx720_acm2108_2mhz_dac\bit\acm2108_2023d_source_demo.bit`
- `D:\fpga\projects\acx720_acm2108_2mhz_dac\bit\acm2108_2023d_source_demo.ltx`

This build generates three 2023D-style source modes through the ACM2108 DAC path:

- CW: 2 MHz carrier.
- AM: 2 MHz carrier, 5 kHz modulation, ma about 0.5.
- FM: 2 MHz carrier, 5 kHz modulation, mf about 2.5 rad.

S4 cycles `CW -> AM -> FM -> CW`. The DAC samples are visible in `hw_ila_3 (ila_dac)`.

## Current Software Evidence

Full-scope software reference:

`D:\fpga\projects\02_2023_d\scripts\software_full_scope_reference.py`

Latest result:

```text
pass 92/92
```

Covered categories:

- AM / FM / CW.
- 2ASK / 2PSK / 2FSK.
- AM and FM modulation frequency candidates.
- AM `m_a`.
- FM `m_f` and `delta_f_max`.
- Digital keying rate `R_c`.
- 2FSK keying coefficient `h`.

Details:

`D:\fpga\projects\02_2023_d\docs\software_full_scope_reference.md`

## Near-Term Recruiting Demo

Show this to a possible communications/electronics teammate:

> I have brought up ACX720 + ACM2108 with Vivado 2023.1 and can already generate 2 MHz CW/AM/FM signals on FPGA. The next step is DAC-to-ADC SMA loopback, then FPGA DDC, classification, and parameter estimation for 2023D. I mainly need help with analog front-end, measurement, calibration, and communications-theory details.

## Road To A Full 2023D Prototype

1. Internal DAC source demo.
   - Done: CW/AM/FM generator.
   - Done: ILA digital observation.
2. DAC-to-ADC loopback after SMA cable arrives.
   - Connect DAC0 to ADC0.
   - Observe ADC0 in `hw_ila_1`.
   - Check DC offset, amplitude range, and clipping.
3. Export ADC samples.
   - First compare in Python.
   - Verify DDC, magnitude, phase, unwrap, and parameter estimators.
4. FPGA receiver path.
   - ADC sample capture.
   - DDS/NCO local oscillator.
   - Mixer/DDC.
   - Low-pass filter or moving average.
   - AM magnitude path.
   - FM phase path.
5. Classifier.
   - CW / AM / FM mode decision.
   - Prefer robust windowed statistics over single max/min samples.
6. Parameter estimators.
   - AM modulation index.
   - AM/FM modulation frequency.
   - FM modulation index or max frequency deviation.
7. Output and demo polish.
   - ILA first.
   - UART or screen after the core path works.
8. Analog front-end.
   - AD8367 and attenuation/protection only after digital loopback is stable.
