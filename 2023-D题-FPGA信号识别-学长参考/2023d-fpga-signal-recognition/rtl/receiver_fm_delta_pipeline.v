`timescale 1ns / 1ps

module receiver_fm_delta_pipeline #(
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
    parameter integer CORDIC_DATA_BITS = 25,
    parameter integer DOT_SHIFT = 32,
    parameter integer ANGLE_BITS = 24,
    parameter integer CORDIC_LATENCY = 20,
    parameter integer ABS_PHASE_BITS = ANGLE_BITS + 1,
    parameter integer FREQ_HZ_BITS = 16,
    parameter integer BASEBAND_SAMPLE_HZ = 250000
) (
    input wire clk,
    input wire rst_n,
    input wire valid_in,
    input wire signed [SAMPLE_BITS-1:0] sample_in,
    input wire signed [LO_BITS-1:0] carrier_cos_in,
    input wire signed [LO_BITS-1:0] carrier_sin_in,
    output wire delta_result_valid,
    output wire [FREQ_HZ_BITS-1:0] delta_f_hz,
    output wire [ABS_PHASE_BITS-1:0] max_abs_phase_delta,
    output wire phase_delta_valid_debug,
    output wire signed [ANGLE_BITS-1:0] phase_delta_debug
);

    wire ddc_valid;
    wire signed [IQ_BITS-1:0] i_sum;
    wire signed [IQ_BITS-1:0] q_sum;
    wire feature_valid;
    wire delta_valid;
    wire [MAG_SQ_BITS-1:0] mag_sq_unused;
    wire signed [DOT_BITS-1:0] dot;
    wire signed [DOT_BITS-1:0] cross;
    wire signed [DOT_BITS-1:0] dot_shifted_full;
    wire signed [DOT_BITS-1:0] cross_shifted_full;
    wire signed [CORDIC_DATA_BITS-1:0] dot_scaled;
    wire signed [CORDIC_DATA_BITS-1:0] cross_scaled;
    wire phase_delta_valid;
    wire signed [ANGLE_BITS-1:0] phase_delta;

    assign dot_shifted_full = dot >>> DOT_SHIFT;
    assign cross_shifted_full = cross >>> DOT_SHIFT;
    assign dot_scaled = dot_shifted_full[CORDIC_DATA_BITS-1:0];
    assign cross_scaled = cross_shifted_full[CORDIC_DATA_BITS-1:0];
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
        .mag_sq(mag_sq_unused),
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

    fm_delta_phase_stats #(
        .ANGLE_BITS(ANGLE_BITS),
        .BLOCK_LEN(BLOCK_LEN),
        .COUNT_BITS(BLOCK_COUNT_BITS),
        .ABS_BITS(ABS_PHASE_BITS)
    ) stats_i (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(phase_delta_valid),
        .phase_delta_in(phase_delta),
        .result_valid(delta_result_valid),
        .max_abs_phase_delta(max_abs_phase_delta)
    );

    fm_delta_to_hz #(
        .ANGLE_BITS(ANGLE_BITS),
        .ABS_BITS(ABS_PHASE_BITS),
        .FREQ_HZ_BITS(FREQ_HZ_BITS),
        .BASEBAND_SAMPLE_HZ(BASEBAND_SAMPLE_HZ)
    ) delta_hz_i (
        .max_abs_phase_delta(max_abs_phase_delta),
        .delta_f_hz(delta_f_hz)
    );

endmodule
