`timescale 1ns / 1ps

module receiver_tone_score_pipeline #(
    parameter integer SAMPLE_BITS = 8,
    parameter integer LO_BITS = 12,
    parameter integer REF_BITS = 12,
    parameter integer DECIM = 80,
    parameter integer DECIM_COUNT_BITS = 7,
    parameter integer DDC_PRODUCT_BITS = SAMPLE_BITS + LO_BITS + 1,
    parameter integer IQ_BITS = DDC_PRODUCT_BITS + DECIM_COUNT_BITS,
    parameter integer DATA_BITS = 32,
    parameter integer BLOCK_LEN = 250,
    parameter integer SCORE_COUNT_BITS = 8,
    parameter integer FEATURE_PRODUCT_BITS = 2 * IQ_BITS,
    parameter integer MAG_SQ_BITS = FEATURE_PRODUCT_BITS + 1,
    parameter integer DOT_BITS = FEATURE_PRODUCT_BITS + 1,
    parameter integer MAG_SHIFT = 24,
    parameter integer CROSS_SHIFT = 22,
    parameter integer SCORE_PRODUCT_BITS = DATA_BITS + REF_BITS,
    parameter integer SCORE_ACC_BITS = SCORE_PRODUCT_BITS + SCORE_COUNT_BITS,
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
    output wire am_result_valid,
    output wire [2:0] am_best_index,
    output wire [SCORE_BITS-1:0] am_best_score_power,
    output wire fm_result_valid,
    output wire [2:0] fm_best_index,
    output wire [SCORE_BITS-1:0] fm_best_score_power,
    output wire ddc_valid_debug,
    output wire signed [IQ_BITS-1:0] i_sum_debug,
    output wire signed [IQ_BITS-1:0] q_sum_debug,
    output wire feature_valid_debug,
    output wire delta_valid_debug,
    output wire signed [DATA_BITS-1:0] am_feature_debug,
    output wire signed [DATA_BITS-1:0] fm_feature_debug
);

    wire ddc_valid;
    wire signed [IQ_BITS-1:0] i_sum;
    wire signed [IQ_BITS-1:0] q_sum;

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

    assign ddc_valid_debug = ddc_valid;
    assign i_sum_debug = i_sum;
    assign q_sum_debug = q_sum;

    ddc_boxcar_decimator #(
        .SAMPLE_BITS(SAMPLE_BITS),
        .LO_BITS(LO_BITS),
        .DECIM(DECIM),
        .COUNT_BITS(DECIM_COUNT_BITS),
        .PRODUCT_BITS(DDC_PRODUCT_BITS),
        .ACC_BITS(IQ_BITS)
    ) ddc_i (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(valid_in),
        .sample_in(sample_in),
        .cos_in(carrier_cos_in),
        .sin_in(carrier_sin_in),
        .valid_out(ddc_valid),
        .i_sum(i_sum),
        .q_sum(q_sum)
    );

    iq_feature_tone_score_pipeline #(
        .IQ_BITS(IQ_BITS),
        .REF_BITS(REF_BITS),
        .DATA_BITS(DATA_BITS),
        .BLOCK_LEN(BLOCK_LEN),
        .COUNT_BITS(SCORE_COUNT_BITS),
        .PRODUCT_BITS(FEATURE_PRODUCT_BITS),
        .MAG_SQ_BITS(MAG_SQ_BITS),
        .DOT_BITS(DOT_BITS),
        .MAG_SHIFT(MAG_SHIFT),
        .CROSS_SHIFT(CROSS_SHIFT),
        .SCORE_PRODUCT_BITS(SCORE_PRODUCT_BITS),
        .SCORE_ACC_BITS(SCORE_ACC_BITS),
        .SCORE_BITS(SCORE_BITS),
        .USE_POWER_SCORE(USE_POWER_SCORE)
    ) feature_score_i (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(ddc_valid),
        .i_in(i_sum),
        .q_in(q_sum),
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
        .am_result_valid(am_result_valid),
        .am_best_index(am_best_index),
        .am_best_score_power(am_best_score_power),
        .fm_result_valid(fm_result_valid),
        .fm_best_index(fm_best_index),
        .fm_best_score_power(fm_best_score_power),
        .feature_valid_debug(feature_valid_debug),
        .delta_valid_debug(delta_valid_debug),
        .am_feature_debug(am_feature_debug),
        .fm_feature_debug(fm_feature_debug)
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
