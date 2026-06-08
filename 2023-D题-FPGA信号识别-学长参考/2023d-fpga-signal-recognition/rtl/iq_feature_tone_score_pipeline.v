`timescale 1ns / 1ps

module iq_feature_tone_score_pipeline #(
    parameter integer IQ_BITS = 18,
    parameter integer REF_BITS = 12,
    parameter integer DATA_BITS = 24,
    parameter integer BLOCK_LEN = 250,
    parameter integer COUNT_BITS = 8,
    parameter integer PRODUCT_BITS = 2 * IQ_BITS,
    parameter integer MAG_SQ_BITS = PRODUCT_BITS + 1,
    parameter integer DOT_BITS = PRODUCT_BITS + 1,
    parameter integer MAG_SHIFT = 8,
    parameter integer CROSS_SHIFT = 5,
    parameter integer SCORE_PRODUCT_BITS = DATA_BITS + REF_BITS,
    parameter integer SCORE_ACC_BITS = SCORE_PRODUCT_BITS + COUNT_BITS,
    parameter integer USE_POWER_SCORE = 1,
    parameter integer SCORE_BITS = (USE_POWER_SCORE != 0) ? (2 * SCORE_ACC_BITS + 1) : (SCORE_ACC_BITS + 1)
) (
    input wire clk,
    input wire rst_n,
    input wire valid_in,
    input wire signed [IQ_BITS-1:0] i_in,
    input wire signed [IQ_BITS-1:0] q_in,
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
    output wire am_result_valid,
    output wire [2:0] am_best_index,
    output wire [SCORE_BITS-1:0] am_best_score_power,
    output wire fm_result_valid,
    output wire [2:0] fm_best_index,
    output wire [SCORE_BITS-1:0] fm_best_score_power,
    output wire feature_valid_debug,
    output wire delta_valid_debug,
    output wire signed [DATA_BITS-1:0] am_feature_debug,
    output wire signed [DATA_BITS-1:0] fm_feature_debug
);

    wire feature_valid;
    wire delta_valid;
    wire [MAG_SQ_BITS-1:0] mag_sq;
    wire signed [DOT_BITS-1:0] dot_unused;
    wire signed [DOT_BITS-1:0] cross;

    wire [MAG_SQ_BITS-1:0] mag_shifted;
    wire signed [DOT_BITS-1:0] cross_shifted;
    wire signed [DATA_BITS-1:0] am_feature;
    wire signed [DATA_BITS-1:0] fm_feature;

    reg signed [REF_BITS-1:0] ref0_cos_d1;
    reg signed [REF_BITS-1:0] ref0_sin_d1;
    reg signed [REF_BITS-1:0] ref1_cos_d1;
    reg signed [REF_BITS-1:0] ref1_sin_d1;
    reg signed [REF_BITS-1:0] ref2_cos_d1;
    reg signed [REF_BITS-1:0] ref2_sin_d1;
    reg signed [REF_BITS-1:0] ref3_cos_d1;
    reg signed [REF_BITS-1:0] ref3_sin_d1;
    reg signed [REF_BITS-1:0] ref4_cos_d1;
    reg signed [REF_BITS-1:0] ref4_sin_d1;

    assign mag_shifted = mag_sq >> MAG_SHIFT;
    assign cross_shifted = cross >>> CROSS_SHIFT;
    assign am_feature = mag_shifted[DATA_BITS-1:0];
    assign fm_feature = cross_shifted[DATA_BITS-1:0];

    assign feature_valid_debug = feature_valid;
    assign delta_valid_debug = delta_valid;
    assign am_feature_debug = am_feature;
    assign fm_feature_debug = fm_feature;

    iq_feature_extractor #(
        .IQ_BITS(IQ_BITS),
        .PRODUCT_BITS(PRODUCT_BITS),
        .MAG_SQ_BITS(MAG_SQ_BITS),
        .DOT_BITS(DOT_BITS)
    ) feature_i (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(valid_in),
        .i_in(i_in),
        .q_in(q_in),
        .valid_out(feature_valid),
        .delta_valid(delta_valid),
        .mag_sq(mag_sq),
        .dot(dot_unused),
        .cross(cross)
    );

    tone_score_bank5 #(
        .DATA_BITS(DATA_BITS),
        .REF_BITS(REF_BITS),
        .BLOCK_LEN(BLOCK_LEN),
        .COUNT_BITS(COUNT_BITS),
        .PRODUCT_BITS(SCORE_PRODUCT_BITS),
        .ACC_BITS(SCORE_ACC_BITS),
        .SCORE_BITS(SCORE_BITS),
        .USE_POWER_SCORE(USE_POWER_SCORE)
    ) am_score_i (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(feature_valid),
        .data_in(am_feature),
        .ref0_cos(ref0_cos_d1),
        .ref0_sin(ref0_sin_d1),
        .ref1_cos(ref1_cos_d1),
        .ref1_sin(ref1_sin_d1),
        .ref2_cos(ref2_cos_d1),
        .ref2_sin(ref2_sin_d1),
        .ref3_cos(ref3_cos_d1),
        .ref3_sin(ref3_sin_d1),
        .ref4_cos(ref4_cos_d1),
        .ref4_sin(ref4_sin_d1),
        .result_valid(am_result_valid),
        .best_index(am_best_index),
        .best_score_power(am_best_score_power),
        .score0_power(),
        .score1_power(),
        .score2_power(),
        .score3_power(),
        .score4_power()
    );

    tone_score_bank5 #(
        .DATA_BITS(DATA_BITS),
        .REF_BITS(REF_BITS),
        .BLOCK_LEN(BLOCK_LEN),
        .COUNT_BITS(COUNT_BITS),
        .PRODUCT_BITS(SCORE_PRODUCT_BITS),
        .ACC_BITS(SCORE_ACC_BITS),
        .SCORE_BITS(SCORE_BITS),
        .USE_POWER_SCORE(USE_POWER_SCORE)
    ) fm_score_i (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(delta_valid),
        .data_in(fm_feature),
        .ref0_cos(ref0_cos_d1),
        .ref0_sin(ref0_sin_d1),
        .ref1_cos(ref1_cos_d1),
        .ref1_sin(ref1_sin_d1),
        .ref2_cos(ref2_cos_d1),
        .ref2_sin(ref2_sin_d1),
        .ref3_cos(ref3_cos_d1),
        .ref3_sin(ref3_sin_d1),
        .ref4_cos(ref4_cos_d1),
        .ref4_sin(ref4_sin_d1),
        .result_valid(fm_result_valid),
        .best_index(fm_best_index),
        .best_score_power(fm_best_score_power),
        .score0_power(),
        .score1_power(),
        .score2_power(),
        .score3_power(),
        .score4_power()
    );

    always @(posedge clk) begin
        if (!rst_n) begin
            ref0_cos_d1 <= {REF_BITS{1'b0}};
            ref0_sin_d1 <= {REF_BITS{1'b0}};
            ref1_cos_d1 <= {REF_BITS{1'b0}};
            ref1_sin_d1 <= {REF_BITS{1'b0}};
            ref2_cos_d1 <= {REF_BITS{1'b0}};
            ref2_sin_d1 <= {REF_BITS{1'b0}};
            ref3_cos_d1 <= {REF_BITS{1'b0}};
            ref3_sin_d1 <= {REF_BITS{1'b0}};
            ref4_cos_d1 <= {REF_BITS{1'b0}};
            ref4_sin_d1 <= {REF_BITS{1'b0}};
        end else if (valid_in) begin
            ref0_cos_d1 <= ref0_cos;
            ref0_sin_d1 <= ref0_sin;
            ref1_cos_d1 <= ref1_cos;
            ref1_sin_d1 <= ref1_sin;
            ref2_cos_d1 <= ref2_cos;
            ref2_sin_d1 <= ref2_sin;
            ref3_cos_d1 <= ref3_cos;
            ref3_sin_d1 <= ref3_sin;
            ref4_cos_d1 <= ref4_cos;
            ref4_sin_d1 <= ref4_sin;
        end
    end

endmodule
