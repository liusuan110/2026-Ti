`timescale 1ns / 1ps

module receiver_recognition_with_params #(
    parameter integer SAMPLE_BITS = 8,
    parameter integer LO_BITS = 12,
    parameter integer REF_BITS = 12,
    parameter integer DECIM = 80,
    parameter integer DECIM_COUNT_BITS = 7,
    parameter integer FREQ_HZ_BITS = 16,
    parameter integer BASEBAND_SAMPLE_HZ = 250000,
    parameter integer USE_POWER_SCORE = 1
) (
    input wire clk,
    input wire rst_n,
    input wire valid_in,
    input wire signed [SAMPLE_BITS-1:0] sample_in,
    input wire signed [LO_BITS-1:0] carrier_cos_in,
    input wire signed [LO_BITS-1:0] carrier_sin_in,
    input wire signed [REF_BITS-1:0] ref0_cos,
    input wire signed [REF_BITS-1:0] ref0_sin,
    input wire signed [REF_BITS-1:0] ref1_cos,
    input wire signed [REF_BITS-1:0] ref1_sin,
    input wire signed [REF_BITS-1:0] ref2_cos,
    input wire signed [REF_BITS-1:0] ref2_sin,
    input wire signed [REF_BITS-1:0] ref3_cos,
    input wire signed [REF_BITS-1:0] ref3_sin,
    input wire signed [REF_BITS-1:0] ref4_cos,
    input wire signed [REF_BITS-1:0] ref4_sin,
    output wire param_valid,
    output wire [2:0] kind_code,
    output wire [2:0] selected_freq_index,
    output wire [2:0] coarse_flags,
    output wire [2:0] am_best_index,
    output wire [2:0] fm_best_index,
    output wire is_cw,
    output wire is_am_like,
    output wire is_fm_like,
    output wire is_psk_like,
    output wire mod_freq_valid,
    output wire [FREQ_HZ_BITS-1:0] mod_freq_hz,
    output wire [FREQ_HZ_BITS-1:0] am_freq_hz,
    output wire [FREQ_HZ_BITS-1:0] fm_freq_hz,
    output wire am_ma_valid,
    output wire [9:0] am_ma_permille,
    output wire fm_param_valid,
    output wire [FREQ_HZ_BITS-1:0] fm_delta_f_hz,
    output wire [7:0] fm_mf_tenths,
    output wire recognition_valid_debug
);

    wire recognition_valid;
    wire mag_active_unused;
    wire ask_like_unused;
    wire phase_active_unused;
    wire phase_flip_active_unused;
    wire am_ma_result_valid;
    wire [9:0] am_ma_raw;
    wire fm_delta_result_valid;
    wire [FREQ_HZ_BITS-1:0] fm_delta_f_raw;
    wire [24:0] fm_max_abs_phase_delta_unused;
    wire [FREQ_HZ_BITS-1:0] fm_mf_freq_hz_unused;
    wire params_ready_valid;
    wire recognition_seen_next;
    wire fm_delta_seen_next;
    reg [9:0] am_ma_hold;
    reg recognition_seen;
    reg fm_delta_seen;
    reg [FREQ_HZ_BITS-1:0] fm_delta_f_hold;

    assign recognition_valid_debug = recognition_valid;
    assign am_ma_valid = param_valid && is_am_like;
    assign am_ma_permille = am_ma_hold;
    assign fm_param_valid = param_valid && is_fm_like;
    assign fm_delta_f_hz = fm_delta_f_hold;
    assign recognition_seen_next = recognition_seen || recognition_valid;
    assign fm_delta_seen_next = fm_delta_seen || fm_delta_result_valid;
    assign params_ready_valid = recognition_seen_next && fm_delta_seen_next;

    receiver_recognition_pipeline #(
        .SAMPLE_BITS(SAMPLE_BITS),
        .LO_BITS(LO_BITS),
        .REF_BITS(REF_BITS),
        .DECIM(DECIM),
        .DECIM_COUNT_BITS(DECIM_COUNT_BITS),
        .USE_POWER_SCORE(USE_POWER_SCORE)
    ) recognition_i (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(valid_in),
        .sample_in(sample_in),
        .carrier_cos_in(carrier_cos_in),
        .carrier_sin_in(carrier_sin_in),
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
        .recognition_valid(recognition_valid),
        .kind_code(kind_code),
        .selected_freq_index(selected_freq_index),
        .coarse_flags_out(coarse_flags),
        .mag_active_out(mag_active_unused),
        .ask_like_out(ask_like_unused),
        .phase_active_out(phase_active_unused),
        .phase_flip_active_out(phase_flip_active_unused),
        .am_best_index_out(am_best_index),
        .fm_best_index_out(fm_best_index),
        .coarse_result_valid_debug(),
        .am_result_valid_debug(),
        .fm_result_valid_debug()
    );

    recognition_parameter_decoder #(
        .FREQ_HZ_BITS(FREQ_HZ_BITS)
    ) decoder_i (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(params_ready_valid),
        .kind_code(kind_code),
        .selected_freq_index(selected_freq_index),
        .am_best_index(am_best_index),
        .fm_best_index(fm_best_index),
        .param_valid(param_valid),
        .is_cw(is_cw),
        .is_am_like(is_am_like),
        .is_fm_like(is_fm_like),
        .is_psk_like(is_psk_like),
        .mod_freq_valid(mod_freq_valid),
        .mod_freq_hz(mod_freq_hz),
        .am_freq_hz(am_freq_hz),
        .fm_freq_hz(fm_freq_hz)
    );

    receiver_am_ma_pipeline #(
        .SAMPLE_BITS(SAMPLE_BITS),
        .LO_BITS(LO_BITS),
        .DECIM(DECIM),
        .DECIM_COUNT_BITS(DECIM_COUNT_BITS)
    ) am_ma_i (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(valid_in),
        .sample_in(sample_in),
        .carrier_cos_in(carrier_cos_in),
        .carrier_sin_in(carrier_sin_in),
        .ma_result_valid(am_ma_result_valid),
        .ma_permille(am_ma_raw),
        .ddc_valid_debug(),
        .i_sum_debug(),
        .q_sum_debug(),
        .feature_valid_debug(),
        .stats_valid_debug(),
        .mag_min_debug(),
        .mag_max_debug()
    );

    receiver_fm_delta_pipeline #(
        .SAMPLE_BITS(SAMPLE_BITS),
        .LO_BITS(LO_BITS),
        .DECIM(DECIM),
        .DECIM_COUNT_BITS(DECIM_COUNT_BITS),
        .FREQ_HZ_BITS(FREQ_HZ_BITS),
        .BASEBAND_SAMPLE_HZ(BASEBAND_SAMPLE_HZ)
    ) fm_delta_i (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(valid_in),
        .sample_in(sample_in),
        .carrier_cos_in(carrier_cos_in),
        .carrier_sin_in(carrier_sin_in),
        .delta_result_valid(fm_delta_result_valid),
        .delta_f_hz(fm_delta_f_raw),
        .max_abs_phase_delta(fm_max_abs_phase_delta_unused),
        .phase_delta_valid_debug(),
        .phase_delta_debug()
    );

    fm_mf_from_delta #(
        .FREQ_HZ_BITS(FREQ_HZ_BITS),
        .MF_BITS(8)
    ) fm_mf_i (
        .delta_f_hz(fm_delta_f_hold),
        .freq_index(fm_best_index),
        .mf_tenths(fm_mf_tenths),
        .mod_freq_hz(fm_mf_freq_hz_unused)
    );

    always @(posedge clk) begin
        if (!rst_n) begin
            am_ma_hold <= 10'd0;
            recognition_seen <= 1'b0;
            fm_delta_seen <= 1'b0;
            fm_delta_f_hold <= {FREQ_HZ_BITS{1'b0}};
        end else begin
            if (am_ma_result_valid) begin
                am_ma_hold <= am_ma_raw;
            end

            if (fm_delta_result_valid) begin
                fm_delta_f_hold <= fm_delta_f_raw;
            end

            if (params_ready_valid) begin
                recognition_seen <= 1'b0;
                fm_delta_seen <= 1'b0;
            end else begin
                recognition_seen <= recognition_seen_next;
                fm_delta_seen <= fm_delta_seen_next;
            end
        end
    end

endmodule
