`timescale 1ns / 1ps

module receiver_ask_rate_pipeline #(
    parameter integer SAMPLE_BITS = 8,
    parameter integer LO_BITS = 12,
    parameter integer DECIM = 80,
    parameter integer DECIM_COUNT_BITS = 7,
    parameter integer DDC_PRODUCT_BITS = SAMPLE_BITS + LO_BITS + 1,
    parameter integer IQ_BITS = DDC_PRODUCT_BITS + DECIM_COUNT_BITS,
    parameter integer BLOCK_LEN = 1024,
    parameter integer BLOCK_COUNT_BITS = 10,
    parameter integer FEATURE_PRODUCT_BITS = 2 * IQ_BITS,
    parameter integer MAG_SQ_BITS = FEATURE_PRODUCT_BITS + 1,
    parameter integer DOT_BITS = FEATURE_PRODUCT_BITS + 1,
    parameter integer MAG_SUM_BITS = MAG_SQ_BITS + BLOCK_COUNT_BITS,
    parameter integer ABS_DELTA_BITS = DOT_BITS + 1,
    parameter integer CROSS_SUM_BITS = ABS_DELTA_BITS + BLOCK_COUNT_BITS,
    parameter integer SCORE_BITS = 10,
    parameter integer FREQ_HZ_BITS = 16,
    parameter integer ASK_MINMAX_PERCENT = 12
) (
    input wire clk,
    input wire rst_n,
    input wire valid_in,
    input wire signed [SAMPLE_BITS-1:0] sample_in,
    input wire signed [LO_BITS-1:0] carrier_cos_in,
    input wire signed [LO_BITS-1:0] carrier_sin_in,
    output wire ask_rate_valid,
    output wire [1:0] rc_index,
    output wire [FREQ_HZ_BITS-1:0] rc_hz,
    output wire ask_like,
    output wire [SCORE_BITS-1:0] score0,
    output wire [SCORE_BITS-1:0] score1,
    output wire [SCORE_BITS-1:0] score2,
    output wire [SCORE_BITS-1:0] transition_count,
    output wire threshold_ready_debug,
    output wire bit_valid_debug,
    output wire bit_debug
);

    wire ddc_valid;
    wire signed [IQ_BITS-1:0] i_sum;
    wire signed [IQ_BITS-1:0] q_sum;
    wire feature_valid;
    wire [MAG_SQ_BITS-1:0] mag_sq;
    wire stats_valid;
    wire [MAG_SQ_BITS-1:0] mag_min;
    wire [MAG_SQ_BITS-1:0] mag_max;
    wire [MAG_SQ_BITS:0] threshold_sum;
    wire [MAG_SQ_BITS+7-1:0] mag_min_scaled;
    wire [MAG_SQ_BITS+7-1:0] ask_threshold;
    wire bit_in_w;

    reg threshold_ready;
    reg [MAG_SQ_BITS-1:0] threshold;
    reg ask_like_hold;

    assign threshold_sum = {1'b0, mag_min} + {1'b0, mag_max};
    assign mag_min_scaled = mag_min * 7'd100;
    assign ask_threshold = mag_max * ASK_MINMAX_PERCENT;
    assign bit_in_w = mag_sq > threshold;
    assign threshold_ready_debug = threshold_ready;
    assign bit_valid_debug = feature_valid && threshold_ready;
    assign bit_debug = bit_in_w;
    assign ask_like = ask_like_hold;
    assign rc_hz = (rc_index == 2'd0) ? 16'd6000 :
                   (rc_index == 2'd1) ? 16'd8000 :
                   (rc_index == 2'd2) ? 16'd10000 :
                   16'd0;

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

    iq_mag_sq_extractor #(
        .IQ_BITS(IQ_BITS),
        .PRODUCT_BITS(FEATURE_PRODUCT_BITS),
        .MAG_SQ_BITS(MAG_SQ_BITS)
    ) feature_i (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(ddc_valid),
        .i_in(i_sum),
        .q_in(q_sum),
        .valid_out(feature_valid),
        .mag_sq(mag_sq)
    );

    unsigned_block_minmax #(
        .DATA_BITS(MAG_SQ_BITS),
        .BLOCK_LEN(BLOCK_LEN),
        .COUNT_BITS(BLOCK_COUNT_BITS)
    ) stats_i (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(feature_valid),
        .data_in(mag_sq),
        .result_valid(stats_valid),
        .min_out(mag_min),
        .max_out(mag_max)
    );

    transition_rate_estimator #(
        .BLOCK_LEN(BLOCK_LEN),
        .COUNT_BITS(BLOCK_COUNT_BITS),
        .SCORE_BITS(SCORE_BITS)
    ) rate_i (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(feature_valid && threshold_ready),
        .bit_in(bit_in_w),
        .result_valid(ask_rate_valid),
        .best_index(rc_index),
        .score0(score0),
        .score1(score1),
        .score2(score2),
        .transition_count(transition_count)
    );

    always @(posedge clk) begin
        if (!rst_n) begin
            threshold_ready <= 1'b0;
            threshold <= {MAG_SQ_BITS{1'b0}};
            ask_like_hold <= 1'b0;
        end else if (stats_valid) begin
            threshold <= threshold_sum[MAG_SQ_BITS:1];
            threshold_ready <= 1'b1;
            ask_like_hold <= (mag_min_scaled <= ask_threshold);
        end
    end

endmodule
