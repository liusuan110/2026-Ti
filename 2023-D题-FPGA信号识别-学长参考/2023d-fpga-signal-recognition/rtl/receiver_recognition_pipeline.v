`timescale 1ns / 1ps

module receiver_recognition_pipeline #(
    parameter integer SAMPLE_BITS = 8,
    parameter integer LO_BITS = 12,
    parameter integer REF_BITS = 12,
    parameter integer DECIM = 80,
    parameter integer DECIM_COUNT_BITS = 7,
    parameter integer DDC_PRODUCT_BITS = SAMPLE_BITS + LO_BITS + 1,
    parameter integer IQ_BITS = DDC_PRODUCT_BITS + DECIM_COUNT_BITS,
    parameter integer DATA_BITS = 32,
    parameter integer TONE_BLOCK_LEN = 250,
    parameter integer TONE_COUNT_BITS = 8,
    parameter integer COARSE_BLOCK_LEN = 64,
    parameter integer COARSE_COUNT_BITS = 6,
    parameter integer FEATURE_PRODUCT_BITS = 2 * IQ_BITS,
    parameter integer MAG_BITS = FEATURE_PRODUCT_BITS + 1,
    parameter integer DOT_BITS = FEATURE_PRODUCT_BITS + 1,
    parameter integer MAG_SUM_BITS = MAG_BITS + COARSE_COUNT_BITS,
    parameter integer ABS_DELTA_BITS = DOT_BITS + 1,
    parameter integer CROSS_SUM_BITS = ABS_DELTA_BITS + COARSE_COUNT_BITS,
    parameter integer TONE_MAG_SHIFT = 24,
    parameter integer TONE_CROSS_SHIFT = 22,
    parameter integer FM_SCORE_RATIO_SHIFT = 4,
    parameter integer SCORE_PRODUCT_BITS = DATA_BITS + REF_BITS,
    parameter integer SCORE_ACC_BITS = SCORE_PRODUCT_BITS + TONE_COUNT_BITS,
    parameter integer USE_POWER_SCORE = 1,
    parameter integer SCORE_BITS = (USE_POWER_SCORE != 0) ? (2 * SCORE_ACC_BITS + 1) : (SCORE_ACC_BITS + 1)
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
    output reg recognition_valid,
    output reg [2:0] kind_code,
    output reg [2:0] selected_freq_index,
    output reg [2:0] coarse_flags_out,
    output reg mag_active_out,
    output reg ask_like_out,
    output reg phase_active_out,
    output reg phase_flip_active_out,
    output reg [2:0] am_best_index_out,
    output reg [2:0] fm_best_index_out,
    output wire coarse_result_valid_debug,
    output wire am_result_valid_debug,
    output wire fm_result_valid_debug
);

    localparam [2:0] KIND_CW = 3'd0;
    localparam [2:0] KIND_AM_LIKE = 3'd1;
    localparam [2:0] KIND_FM_LIKE = 3'd2;
    localparam [2:0] KIND_PSK_LIKE = 3'd3;

    wire coarse_result_valid;
    wire mag_active;
    wire ask_like;
    wire phase_active;
    wire phase_flip_active;
    wire [2:0] coarse_flags;

    wire am_result_valid;
    wire [2:0] am_best_index;
    wire [SCORE_BITS-1:0] am_best_score_power;
    wire fm_result_valid;
    wire [2:0] fm_best_index;
    wire [SCORE_BITS-1:0] fm_best_score_power;

    reg coarse_seen;
    reg am_seen;
    reg fm_seen;
    reg [2:0] coarse_flags_hold;
    reg mag_active_hold;
    reg ask_like_hold;
    reg phase_active_hold;
    reg phase_flip_active_hold;
    reg [2:0] am_best_index_hold;
    reg [2:0] fm_best_index_hold;
    reg [SCORE_BITS-1:0] am_best_score_power_hold;
    reg [SCORE_BITS-1:0] fm_best_score_power_hold;

    wire coarse_seen_next;
    wire am_seen_next;
    wire fm_seen_next;
    wire [2:0] coarse_flags_next;
    wire mag_active_next;
    wire ask_like_next;
    wire phase_active_next;
    wire phase_flip_active_next;
    wire [2:0] am_best_index_next;
    wire [2:0] fm_best_index_next;
    wire [SCORE_BITS-1:0] am_best_score_power_next;
    wire [SCORE_BITS-1:0] fm_best_score_power_next;
    wire fm_tone_active_next;
    wire all_ready_next;
    wire [2:0] kind_code_next;
    wire [2:0] selected_freq_index_next;

    assign coarse_result_valid_debug = coarse_result_valid;
    assign am_result_valid_debug = am_result_valid;
    assign fm_result_valid_debug = fm_result_valid;

    assign coarse_seen_next = coarse_seen || coarse_result_valid;
    assign am_seen_next = am_seen || am_result_valid;
    assign fm_seen_next = fm_seen || fm_result_valid;
    assign coarse_flags_next = coarse_result_valid ? coarse_flags : coarse_flags_hold;
    assign mag_active_next = coarse_result_valid ? mag_active : mag_active_hold;
    assign ask_like_next = coarse_result_valid ? ask_like : ask_like_hold;
    assign phase_active_next = coarse_result_valid ? phase_active : phase_active_hold;
    assign phase_flip_active_next = coarse_result_valid ? phase_flip_active : phase_flip_active_hold;
    assign am_best_index_next = am_result_valid ? am_best_index : am_best_index_hold;
    assign fm_best_index_next = fm_result_valid ? fm_best_index : fm_best_index_hold;
    assign am_best_score_power_next = am_result_valid ? am_best_score_power : am_best_score_power_hold;
    assign fm_best_score_power_next = fm_result_valid ? fm_best_score_power : fm_best_score_power_hold;
    assign fm_tone_active_next = fm_best_score_power_next > (am_best_score_power_next << FM_SCORE_RATIO_SHIFT);
    assign all_ready_next = coarse_seen_next && am_seen_next && fm_seen_next;

    assign kind_code_next = phase_flip_active_next ? KIND_PSK_LIKE :
                            (phase_active_next || fm_tone_active_next) ? KIND_FM_LIKE :
                            mag_active_next ? KIND_AM_LIKE :
                            KIND_CW;

    assign selected_freq_index_next = (phase_active_next || fm_tone_active_next) ? fm_best_index_next :
                                      mag_active_next ? am_best_index_next :
                                      3'd0;

    receiver_coarse_pipeline #(
        .SAMPLE_BITS(SAMPLE_BITS),
        .LO_BITS(LO_BITS),
        .DECIM(DECIM),
        .DECIM_COUNT_BITS(DECIM_COUNT_BITS),
        .PRODUCT_BITS(DDC_PRODUCT_BITS),
        .IQ_BITS(IQ_BITS),
        .FEATURE_PRODUCT_BITS(FEATURE_PRODUCT_BITS),
        .MAG_BITS(MAG_BITS),
        .DOT_BITS(DOT_BITS),
        .BLOCK_LEN(COARSE_BLOCK_LEN),
        .BLOCK_COUNT_BITS(COARSE_COUNT_BITS),
        .MAG_SUM_BITS(MAG_SUM_BITS),
        .ABS_DELTA_BITS(ABS_DELTA_BITS),
        .CROSS_SUM_BITS(CROSS_SUM_BITS)
    ) coarse_i (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(valid_in),
        .sample_in(sample_in),
        .cos_in(carrier_cos_in),
        .sin_in(carrier_sin_in),
        .result_valid(coarse_result_valid),
        .mag_active(mag_active),
        .ask_like(ask_like),
        .phase_active(phase_active),
        .phase_flip_active(phase_flip_active),
        .coarse_flags(coarse_flags)
    );

    receiver_tone_score_pipeline #(
        .SAMPLE_BITS(SAMPLE_BITS),
        .LO_BITS(LO_BITS),
        .REF_BITS(REF_BITS),
        .DECIM(DECIM),
        .DECIM_COUNT_BITS(DECIM_COUNT_BITS),
        .DDC_PRODUCT_BITS(DDC_PRODUCT_BITS),
        .IQ_BITS(IQ_BITS),
        .DATA_BITS(DATA_BITS),
        .BLOCK_LEN(TONE_BLOCK_LEN),
        .SCORE_COUNT_BITS(TONE_COUNT_BITS),
        .FEATURE_PRODUCT_BITS(FEATURE_PRODUCT_BITS),
        .MAG_SQ_BITS(MAG_BITS),
        .DOT_BITS(DOT_BITS),
        .MAG_SHIFT(TONE_MAG_SHIFT),
        .CROSS_SHIFT(TONE_CROSS_SHIFT),
        .SCORE_PRODUCT_BITS(SCORE_PRODUCT_BITS),
        .SCORE_ACC_BITS(SCORE_ACC_BITS),
        .SCORE_BITS(SCORE_BITS),
        .USE_POWER_SCORE(USE_POWER_SCORE)
    ) tone_i (
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
        .am_result_valid(am_result_valid),
        .am_best_index(am_best_index),
        .am_best_score_power(am_best_score_power),
        .fm_result_valid(fm_result_valid),
        .fm_best_index(fm_best_index),
        .fm_best_score_power(fm_best_score_power),
        .ddc_valid_debug(),
        .i_sum_debug(),
        .q_sum_debug(),
        .feature_valid_debug(),
        .delta_valid_debug(),
        .am_feature_debug(),
        .fm_feature_debug()
    );

    always @(posedge clk) begin
        if (!rst_n) begin
            recognition_valid <= 1'b0;
            kind_code <= KIND_CW;
            selected_freq_index <= 3'd0;
            coarse_flags_out <= 3'd0;
            mag_active_out <= 1'b0;
            ask_like_out <= 1'b0;
            phase_active_out <= 1'b0;
            phase_flip_active_out <= 1'b0;
            am_best_index_out <= 3'd0;
            fm_best_index_out <= 3'd0;
            coarse_seen <= 1'b0;
            am_seen <= 1'b0;
            fm_seen <= 1'b0;
            coarse_flags_hold <= 3'd0;
            mag_active_hold <= 1'b0;
            ask_like_hold <= 1'b0;
            phase_active_hold <= 1'b0;
            phase_flip_active_hold <= 1'b0;
            am_best_index_hold <= 3'd0;
            fm_best_index_hold <= 3'd0;
            am_best_score_power_hold <= {SCORE_BITS{1'b0}};
            fm_best_score_power_hold <= {SCORE_BITS{1'b0}};
        end else begin
            recognition_valid <= 1'b0;

            if (all_ready_next) begin
                recognition_valid <= 1'b1;
                kind_code <= kind_code_next;
                selected_freq_index <= selected_freq_index_next;
                coarse_flags_out <= coarse_flags_next;
                mag_active_out <= mag_active_next;
                ask_like_out <= ask_like_next;
                phase_active_out <= phase_active_next;
                phase_flip_active_out <= phase_flip_active_next;
                am_best_index_out <= am_best_index_next;
                fm_best_index_out <= fm_best_index_next;
                coarse_seen <= 1'b0;
                am_seen <= 1'b0;
                fm_seen <= 1'b0;
            end else begin
                coarse_seen <= coarse_seen_next;
                am_seen <= am_seen_next;
                fm_seen <= fm_seen_next;
            end

            coarse_flags_hold <= coarse_flags_next;
            mag_active_hold <= mag_active_next;
            ask_like_hold <= ask_like_next;
            phase_active_hold <= phase_active_next;
            phase_flip_active_hold <= phase_flip_active_next;
            am_best_index_hold <= am_best_index_next;
            fm_best_index_hold <= fm_best_index_next;
            am_best_score_power_hold <= am_best_score_power_next;
            fm_best_score_power_hold <= fm_best_score_power_next;
        end
    end

endmodule
