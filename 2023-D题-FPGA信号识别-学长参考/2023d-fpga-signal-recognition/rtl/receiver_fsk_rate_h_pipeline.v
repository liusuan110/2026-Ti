`timescale 1ns / 1ps

module receiver_fsk_rate_h_pipeline #(
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
    parameter integer CORDIC_DATA_BITS = 25,
    parameter integer DOT_SHIFT = 32,
    parameter integer ANGLE_BITS = 24,
    parameter integer CORDIC_LATENCY = 20,
    parameter integer ABS_PHASE_BITS = ANGLE_BITS + 1,
    parameter integer SCORE_BITS = 10,
    parameter integer FREQ_HZ_BITS = 16,
    parameter integer BASEBAND_SAMPLE_HZ = 250000
) (
    input wire clk,
    input wire rst_n,
    input wire valid_in,
    input wire signed [SAMPLE_BITS-1:0] sample_in,
    input wire signed [LO_BITS-1:0] carrier_cos_in,
    input wire signed [LO_BITS-1:0] carrier_sin_in,
    output wire fsk_result_valid,
    output wire [1:0] rc_index,
    output wire [FREQ_HZ_BITS-1:0] rc_hz,
    output wire [FREQ_HZ_BITS-1:0] freq_sep_hz,
    output wire [7:0] h_tenths,
    output wire [SCORE_BITS-1:0] score0,
    output wire [SCORE_BITS-1:0] score1,
    output wire [SCORE_BITS-1:0] score2,
    output wire [SCORE_BITS-1:0] transition_count,
    output wire threshold_ready_debug,
    output wire bit_valid_debug,
    output wire bit_debug,
    output wire phase_delta_valid_debug,
    output wire signed [ANGLE_BITS-1:0] phase_delta_debug
);

    wire ddc_valid;
    wire signed [IQ_BITS-1:0] i_sum;
    wire signed [IQ_BITS-1:0] q_sum;
    wire feature_valid;
    wire delta_valid;
    wire signed [DOT_BITS-1:0] dot;
    wire signed [DOT_BITS-1:0] cross;
    wire signed [DOT_BITS-1:0] dot_shifted_full;
    wire signed [DOT_BITS-1:0] cross_shifted_full;
    wire signed [CORDIC_DATA_BITS-1:0] dot_scaled;
    wire signed [CORDIC_DATA_BITS-1:0] cross_scaled;
    wire phase_delta_valid;
    wire signed [ANGLE_BITS-1:0] phase_delta;
    wire minmax_valid;
    wire signed [ANGLE_BITS-1:0] phase_min;
    wire signed [ANGLE_BITS-1:0] phase_max;
    wire signed [ANGLE_BITS:0] threshold_sum;
    wire signed [ANGLE_BITS-1:0] threshold_next;
    wire signed [ANGLE_BITS:0] phase_min_ext;
    wire signed [ANGLE_BITS:0] phase_max_ext;
    wire [ABS_PHASE_BITS-1:0] sep_phase_next;
    wire bit_in_w;

    reg threshold_ready;
    reg signed [ANGLE_BITS-1:0] threshold;
    reg [ABS_PHASE_BITS-1:0] sep_phase_hold;

    assign dot_shifted_full = dot >>> DOT_SHIFT;
    assign cross_shifted_full = cross >>> DOT_SHIFT;
    assign dot_scaled = dot_shifted_full[CORDIC_DATA_BITS-1:0];
    assign cross_scaled = cross_shifted_full[CORDIC_DATA_BITS-1:0];
    assign threshold_sum = {phase_min[ANGLE_BITS-1], phase_min} + {phase_max[ANGLE_BITS-1], phase_max};
    assign threshold_next = threshold_sum >>> 1;
    assign phase_min_ext = {phase_min[ANGLE_BITS-1], phase_min};
    assign phase_max_ext = {phase_max[ANGLE_BITS-1], phase_max};
    assign sep_phase_next = $unsigned(phase_max_ext - phase_min_ext);
    assign bit_in_w = phase_delta > threshold;
    assign threshold_ready_debug = threshold_ready;
    assign bit_valid_debug = phase_delta_valid && threshold_ready;
    assign bit_debug = bit_in_w;
    assign phase_delta_valid_debug = phase_delta_valid;
    assign phase_delta_debug = phase_delta;

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

    iq_dot_cross_extractor #(
        .IQ_BITS(IQ_BITS),
        .PRODUCT_BITS(FEATURE_PRODUCT_BITS),
        .DOT_BITS(DOT_BITS)
    ) feature_i (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(ddc_valid),
        .i_in(i_sum),
        .q_in(q_sum),
        .valid_out(feature_valid),
        .delta_valid(delta_valid),
        .dot(dot),
        .cross(cross)
    );

    cordic_atan2_pipeline #(
        .DATA_BITS(CORDIC_DATA_BITS),
        .XY_BITS(CORDIC_DATA_BITS + 4),
        .ANGLE_BITS(ANGLE_BITS),
        .ITERATIONS(CORDIC_LATENCY)
    ) delta_cordic_i (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(delta_valid),
        .i_in(dot_scaled),
        .q_in(cross_scaled),
        .valid_out(phase_delta_valid),
        .phase_out(phase_delta)
    );

    signed_block_minmax #(
        .DATA_BITS(ANGLE_BITS),
        .BLOCK_LEN(BLOCK_LEN),
        .COUNT_BITS(BLOCK_COUNT_BITS)
    ) minmax_i (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(phase_delta_valid),
        .data_in(phase_delta),
        .result_valid(minmax_valid),
        .min_out(phase_min),
        .max_out(phase_max)
    );

    transition_rate_estimator #(
        .BLOCK_LEN(BLOCK_LEN),
        .COUNT_BITS(BLOCK_COUNT_BITS),
        .SCORE_BITS(SCORE_BITS)
    ) rate_i (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(phase_delta_valid && threshold_ready),
        .bit_in(bit_in_w),
        .result_valid(fsk_result_valid),
        .best_index(rc_index),
        .score0(score0),
        .score1(score1),
        .score2(score2),
        .transition_count(transition_count)
    );

    fm_delta_to_hz #(
        .ANGLE_BITS(ANGLE_BITS),
        .ABS_BITS(ABS_PHASE_BITS),
        .FREQ_HZ_BITS(FREQ_HZ_BITS),
        .BASEBAND_SAMPLE_HZ(BASEBAND_SAMPLE_HZ)
    ) sep_hz_i (
        .max_abs_phase_delta(sep_phase_hold),
        .delta_f_hz(freq_sep_hz)
    );

    fsk_h_from_sep #(
        .FREQ_HZ_BITS(FREQ_HZ_BITS),
        .H_BITS(8)
    ) h_i (
        .freq_sep_hz(freq_sep_hz),
        .rc_index(rc_index),
        .rc_hz(rc_hz),
        .h_tenths(h_tenths)
    );

    always @(posedge clk) begin
        if (!rst_n) begin
            threshold_ready <= 1'b0;
            threshold <= {ANGLE_BITS{1'b0}};
            sep_phase_hold <= {ABS_PHASE_BITS{1'b0}};
        end else if (minmax_valid) begin
            threshold_ready <= 1'b1;
            threshold <= threshold_next;
            sep_phase_hold <= sep_phase_next;
        end
    end

endmodule
