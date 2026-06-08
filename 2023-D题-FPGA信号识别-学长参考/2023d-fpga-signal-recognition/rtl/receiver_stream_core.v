`timescale 1ns / 1ps

module receiver_stream_core #(
    parameter integer SAMPLE_BITS = 8,
    parameter integer LO_BITS = 12,
    parameter integer REF_BITS = 12,
    parameter integer DECIM = 80,
    parameter integer DECIM_COUNT_BITS = 7,
    parameter integer FREQ_HZ_BITS = 16,
    parameter integer BASEBAND_SAMPLE_HZ = 250000,
    parameter [31:0] CARRIER_PHASE_STEP = 32'd429496730,
    parameter [31:0] REF0_PHASE_STEP = 32'd17179869,
    parameter [31:0] REF1_PHASE_STEP = 32'd34359738,
    parameter [31:0] REF2_PHASE_STEP = 32'd51539608,
    parameter [31:0] REF3_PHASE_STEP = 32'd68719477,
    parameter [31:0] REF4_PHASE_STEP = 32'd85899346,
    parameter integer USE_POWER_SCORE = 1,
    parameter integer FSK_MIN_H_TENTHS = 18
) (
    input wire clk,
    input wire rst_n,
    input wire valid_in,
    input wire [SAMPLE_BITS-1:0] sample_u8_in,
    output wire result_valid,
    output wire [2:0] signal_kind_code,
    output wire mod_freq_valid,
    output wire [FREQ_HZ_BITS-1:0] mod_freq_hz,
    output wire am_ma_valid,
    output wire [9:0] am_ma_permille,
    output wire fm_param_valid,
    output wire [FREQ_HZ_BITS-1:0] fm_delta_f_hz,
    output wire [7:0] fm_mf_tenths,
    output wire keying_param_valid,
    output wire [FREQ_HZ_BITS-1:0] rc_hz,
    output wire fsk_param_valid,
    output wire [FREQ_HZ_BITS-1:0] fsk_freq_sep_hz,
    output wire [7:0] fsk_h_tenths,
    output wire [2:0] analog_kind_debug,
    output wire [2:0] keying_kind_debug
);

    wire signed [SAMPLE_BITS:0] sample_centered_w;
    wire signed [SAMPLE_BITS-1:0] sample_signed_w;
    wire frontend_valid;
    wire signed [SAMPLE_BITS-1:0] frontend_sample;
    wire signed [LO_BITS-1:0] carrier_cos;
    wire signed [LO_BITS-1:0] carrier_sin;
    wire signed [REF_BITS-1:0] ref0_cos;
    wire signed [REF_BITS-1:0] ref0_sin;
    wire signed [REF_BITS-1:0] ref1_cos;
    wire signed [REF_BITS-1:0] ref1_sin;
    wire signed [REF_BITS-1:0] ref2_cos;
    wire signed [REF_BITS-1:0] ref2_sin;
    wire signed [REF_BITS-1:0] ref3_cos;
    wire signed [REF_BITS-1:0] ref3_sin;
    wire signed [REF_BITS-1:0] ref4_cos;
    wire signed [REF_BITS-1:0] ref4_sin;

    assign sample_centered_w = $signed({1'b0, sample_u8_in}) - 9'sd128;
    assign sample_signed_w = sample_centered_w[SAMPLE_BITS-1:0];

    receiver_nco_frontend #(
        .SAMPLE_BITS(SAMPLE_BITS),
        .AMP_BITS(LO_BITS),
        .DECIM(DECIM),
        .DECIM_COUNT_BITS(DECIM_COUNT_BITS),
        .CARRIER_PHASE_STEP(CARRIER_PHASE_STEP),
        .REF0_PHASE_STEP(REF0_PHASE_STEP),
        .REF1_PHASE_STEP(REF1_PHASE_STEP),
        .REF2_PHASE_STEP(REF2_PHASE_STEP),
        .REF3_PHASE_STEP(REF3_PHASE_STEP),
        .REF4_PHASE_STEP(REF4_PHASE_STEP)
    ) frontend_i (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(valid_in),
        .sample_in(sample_signed_w),
        .valid_out(frontend_valid),
        .sample_out(frontend_sample),
        .carrier_cos(carrier_cos),
        .carrier_sin(carrier_sin),
        .ref0_cos(ref0_cos),
        .ref0_sin(ref0_sin),
        .ref1_cos(ref1_cos),
        .ref1_sin(ref1_sin),
        .ref2_cos(ref2_cos),
        .ref2_sin(ref2_sin),
        .ref3_cos(ref3_cos),
        .ref3_sin(ref3_sin),
        .ref4_cos(ref4_cos),
        .ref4_sin(ref4_sin),
        .decim_count_debug()
    );

    receiver_full_scope_pipeline #(
        .SAMPLE_BITS(SAMPLE_BITS),
        .LO_BITS(LO_BITS),
        .REF_BITS(REF_BITS),
        .DECIM(DECIM),
        .DECIM_COUNT_BITS(DECIM_COUNT_BITS),
        .FREQ_HZ_BITS(FREQ_HZ_BITS),
        .BASEBAND_SAMPLE_HZ(BASEBAND_SAMPLE_HZ),
        .USE_POWER_SCORE(USE_POWER_SCORE),
        .FSK_MIN_H_TENTHS(FSK_MIN_H_TENTHS)
    ) receiver_i (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(frontend_valid),
        .sample_in(frontend_sample),
        .carrier_cos_in(carrier_cos),
        .carrier_sin_in(carrier_sin),
        .ref0_cos(ref0_cos),
        .ref0_sin(ref0_sin),
        .ref1_cos(ref1_cos),
        .ref1_sin(ref1_sin),
        .ref2_cos(ref2_cos),
        .ref2_sin(ref2_sin),
        .ref3_cos(ref3_cos),
        .ref3_sin(ref3_sin),
        .ref4_cos(ref4_cos),
        .ref4_sin(ref4_sin),
        .result_valid(result_valid),
        .signal_kind_code(signal_kind_code),
        .mod_freq_valid(mod_freq_valid),
        .mod_freq_hz(mod_freq_hz),
        .am_ma_valid(am_ma_valid),
        .am_ma_permille(am_ma_permille),
        .fm_param_valid(fm_param_valid),
        .fm_delta_f_hz(fm_delta_f_hz),
        .fm_mf_tenths(fm_mf_tenths),
        .keying_param_valid(keying_param_valid),
        .rc_hz(rc_hz),
        .fsk_param_valid(fsk_param_valid),
        .fsk_freq_sep_hz(fsk_freq_sep_hz),
        .fsk_h_tenths(fsk_h_tenths),
        .analog_kind_debug(analog_kind_debug),
        .keying_kind_debug(keying_kind_debug)
    );

endmodule
