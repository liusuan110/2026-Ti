`timescale 1ns / 1ps

module receiver_am_ma_pipeline #(
    parameter integer SAMPLE_BITS = 8,
    parameter integer LO_BITS = 12,
    parameter integer DECIM = 80,
    parameter integer DECIM_COUNT_BITS = 7,
    parameter integer DDC_PRODUCT_BITS = SAMPLE_BITS + LO_BITS + 1,
    parameter integer IQ_BITS = DDC_PRODUCT_BITS + DECIM_COUNT_BITS,
    parameter integer BLOCK_LEN = 250,
    parameter integer BLOCK_COUNT_BITS = 8,
    parameter integer FEATURE_PRODUCT_BITS = 2 * IQ_BITS,
    parameter integer MAG_SQ_BITS = FEATURE_PRODUCT_BITS + 1,
    parameter integer DOT_BITS = FEATURE_PRODUCT_BITS + 1,
    parameter integer MAG_SUM_BITS = MAG_SQ_BITS + BLOCK_COUNT_BITS,
    parameter integer ABS_DELTA_BITS = DOT_BITS + 1,
    parameter integer CROSS_SUM_BITS = ABS_DELTA_BITS + BLOCK_COUNT_BITS
) (
    input wire clk,
    input wire rst_n,
    input wire valid_in,
    input wire signed [SAMPLE_BITS-1:0] sample_in,
    input wire signed [LO_BITS-1:0] carrier_cos_in,
    input wire signed [LO_BITS-1:0] carrier_sin_in,
    output wire ma_result_valid,
    output wire [9:0] ma_permille,
    output wire ddc_valid_debug,
    output wire signed [IQ_BITS-1:0] i_sum_debug,
    output wire signed [IQ_BITS-1:0] q_sum_debug,
    output wire feature_valid_debug,
    output wire stats_valid_debug,
    output wire [MAG_SQ_BITS-1:0] mag_min_debug,
    output wire [MAG_SQ_BITS-1:0] mag_max_debug
);

    wire ddc_valid;
    wire signed [IQ_BITS-1:0] i_sum;
    wire signed [IQ_BITS-1:0] q_sum;
    wire feature_valid;
    wire delta_valid;
    wire [MAG_SQ_BITS-1:0] mag_sq;
    wire signed [DOT_BITS-1:0] dot;
    wire signed [DOT_BITS-1:0] cross;
    wire stats_valid;
    wire [MAG_SQ_BITS-1:0] mag_min;
    wire [MAG_SQ_BITS-1:0] mag_max;
    wire [MAG_SUM_BITS-1:0] mag_sum_unused;
    wire [CROSS_SUM_BITS-1:0] abs_cross_sum_unused;
    wire [BLOCK_COUNT_BITS:0] negative_dot_count_unused;

    assign ddc_valid_debug = ddc_valid;
    assign i_sum_debug = i_sum;
    assign q_sum_debug = q_sum;
    assign feature_valid_debug = feature_valid;
    assign stats_valid_debug = stats_valid;
    assign mag_min_debug = mag_min;
    assign mag_max_debug = mag_max;

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

    iq_feature_extractor #(
        .IQ_BITS(IQ_BITS),
        .PRODUCT_BITS(FEATURE_PRODUCT_BITS),
        .MAG_SQ_BITS(MAG_SQ_BITS),
        .DOT_BITS(DOT_BITS)
    ) feature_i (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(ddc_valid),
        .i_in(i_sum),
        .q_in(q_sum),
        .valid_out(feature_valid),
        .delta_valid(delta_valid),
        .mag_sq(mag_sq),
        .dot(dot),
        .cross(cross)
    );

    feature_block_stats #(
        .MAG_BITS(MAG_SQ_BITS),
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
        .mag_sum(mag_sum_unused),
        .abs_cross_sum(abs_cross_sum_unused),
        .negative_dot_count(negative_dot_count_unused)
    );

    am_ma_estimator_lut #(
        .MAG_BITS(MAG_SQ_BITS)
    ) ma_estimator_i (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(stats_valid),
        .block_min(mag_min),
        .block_max(mag_max),
        .result_valid(ma_result_valid),
        .ma_permille(ma_permille)
    );

endmodule
