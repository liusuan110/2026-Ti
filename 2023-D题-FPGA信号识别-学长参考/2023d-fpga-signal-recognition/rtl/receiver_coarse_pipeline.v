`timescale 1ns / 1ps

module receiver_coarse_pipeline #(
    parameter integer SAMPLE_BITS = 8,
    parameter integer LO_BITS = 12,
    parameter integer DECIM = 80,
    parameter integer DECIM_COUNT_BITS = 7,
    parameter integer PRODUCT_BITS = SAMPLE_BITS + LO_BITS + 1,
    parameter integer IQ_BITS = PRODUCT_BITS + DECIM_COUNT_BITS,
    parameter integer FEATURE_PRODUCT_BITS = 2 * IQ_BITS,
    parameter integer MAG_BITS = FEATURE_PRODUCT_BITS + 1,
    parameter integer DOT_BITS = FEATURE_PRODUCT_BITS + 1,
    parameter integer BLOCK_LEN = 64,
    parameter integer BLOCK_COUNT_BITS = 6,
    parameter integer MAG_SUM_BITS = MAG_BITS + BLOCK_COUNT_BITS,
    parameter integer ABS_DELTA_BITS = DOT_BITS + 1,
    parameter integer CROSS_SUM_BITS = ABS_DELTA_BITS + BLOCK_COUNT_BITS
) (
    input wire clk,
    input wire rst_n,
    input wire valid_in,
    input wire signed [SAMPLE_BITS-1:0] sample_in,
    input wire signed [LO_BITS-1:0] cos_in,
    input wire signed [LO_BITS-1:0] sin_in,
    output wire result_valid,
    output wire mag_active,
    output wire ask_like,
    output wire phase_active,
    output wire phase_flip_active,
    output wire [2:0] coarse_flags
);

    wire iq_valid;
    wire signed [IQ_BITS-1:0] i_sum;
    wire signed [IQ_BITS-1:0] q_sum;

    wire feature_valid;
    wire delta_valid;
    wire [MAG_BITS-1:0] mag_sq;
    wire signed [DOT_BITS-1:0] dot;
    wire signed [DOT_BITS-1:0] cross;

    wire stats_valid;
    wire [MAG_BITS-1:0] mag_min;
    wire [MAG_BITS-1:0] mag_max;
    wire [MAG_SUM_BITS-1:0] mag_sum;
    wire [CROSS_SUM_BITS-1:0] abs_cross_sum;
    wire [BLOCK_COUNT_BITS:0] negative_dot_count;

    ddc_boxcar_decimator #(
        .SAMPLE_BITS(SAMPLE_BITS),
        .LO_BITS(LO_BITS),
        .DECIM(DECIM),
        .COUNT_BITS(DECIM_COUNT_BITS),
        .PRODUCT_BITS(PRODUCT_BITS),
        .ACC_BITS(IQ_BITS)
    ) ddc_i (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(valid_in),
        .sample_in(sample_in),
        .cos_in(cos_in),
        .sin_in(sin_in),
        .valid_out(iq_valid),
        .i_sum(i_sum),
        .q_sum(q_sum)
    );

    iq_feature_extractor #(
        .IQ_BITS(IQ_BITS),
        .PRODUCT_BITS(FEATURE_PRODUCT_BITS),
        .MAG_SQ_BITS(MAG_BITS),
        .DOT_BITS(DOT_BITS)
    ) feature_i (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(iq_valid),
        .i_in(i_sum),
        .q_in(q_sum),
        .valid_out(feature_valid),
        .delta_valid(delta_valid),
        .mag_sq(mag_sq),
        .dot(dot),
        .cross(cross)
    );

    feature_block_stats #(
        .MAG_BITS(MAG_BITS),
        .DELTA_BITS(DOT_BITS),
        .BLOCK_LEN(BLOCK_LEN),
        .COUNT_BITS(BLOCK_COUNT_BITS),
        .MAG_SUM_BITS(MAG_SUM_BITS),
        .ABS_DELTA_BITS(ABS_DELTA_BITS),
        .CROSS_SUM_BITS(CROSS_SUM_BITS)
    ) stats_i (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(feature_valid),
        .mag_sq_in(mag_sq),
        .delta_valid_in(delta_valid),
        .dot_in(dot),
        .cross_in(cross),
        .result_valid(stats_valid),
        .mag_min(mag_min),
        .mag_max(mag_max),
        .mag_sum(mag_sum),
        .abs_cross_sum(abs_cross_sum),
        .negative_dot_count(negative_dot_count)
    );

    feature_coarse_classifier #(
        .MAG_BITS(MAG_BITS),
        .COUNT_BITS(BLOCK_COUNT_BITS),
        .MAG_SUM_BITS(MAG_SUM_BITS),
        .CROSS_SUM_BITS(CROSS_SUM_BITS),
        .BLOCK_LEN(BLOCK_LEN),
        .MAG_RANGE_PERCENT(25),
        .ASK_MINMAX_PERCENT(8),
        .PHASE_ACTIVITY_PERCENT(8),
        .NEG_DOT_THRESHOLD(1)
    ) classifier_i (
        .clk(clk),
        .rst_n(rst_n),
        .stats_valid(stats_valid),
        .mag_min(mag_min),
        .mag_max(mag_max),
        .mag_sum(mag_sum),
        .abs_cross_sum(abs_cross_sum),
        .negative_dot_count(negative_dot_count),
        .result_valid(result_valid),
        .mag_active(mag_active),
        .ask_like(ask_like),
        .phase_active(phase_active),
        .phase_flip_active(phase_flip_active),
        .coarse_flags(coarse_flags)
    );

endmodule
