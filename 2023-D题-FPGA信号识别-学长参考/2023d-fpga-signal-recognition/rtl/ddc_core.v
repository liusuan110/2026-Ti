`timescale 1ns / 1ps

module ddc_core #(
    parameter integer SAMPLE_BITS = 8,
    parameter integer PHASE_BITS = 32,
    parameter integer LUT_ADDR_BITS = 10,
    parameter integer LO_BITS = 12,
    parameter integer PRODUCT_BITS = SAMPLE_BITS + LO_BITS + 1,
    parameter integer LPF_WINDOW_LEN = 16,
    parameter integer LPF_SHIFT_BITS = 4,
    parameter integer LPF_ACC_BITS = PRODUCT_BITS + LPF_SHIFT_BITS,
    parameter [PHASE_BITS-1:0] PHASE_STEP = 32'd429496730,
    parameter COS_MEM_FILE = "rtl/cos_1024x12.mem",
    parameter SIN_MEM_FILE = "rtl/sin_1024x12.mem"
) (
    input wire clk,
    input wire rst_n,
    input wire valid_in,
    input wire signed [SAMPLE_BITS-1:0] sample_in,
    output wire valid_out,
    output wire signed [PRODUCT_BITS-1:0] i_baseband,
    output wire signed [PRODUCT_BITS-1:0] q_baseband,
    output wire signed [PRODUCT_BITS-1:0] i_raw_debug,
    output wire signed [PRODUCT_BITS-1:0] q_raw_debug,
    output wire signed [LO_BITS-1:0] cos_debug,
    output wire signed [LO_BITS-1:0] sin_debug,
    output wire [PHASE_BITS-1:0] phase_debug
);

    reg signed [SAMPLE_BITS-1:0] sample_d1;

    wire dds_valid;
    wire mixer_valid;
    wire i_lpf_valid;
    wire q_lpf_valid;

    wire signed [LO_BITS-1:0] cos_lo;
    wire signed [LO_BITS-1:0] sin_lo;
    wire signed [PRODUCT_BITS-1:0] i_raw;
    wire signed [PRODUCT_BITS-1:0] q_raw;
    wire signed [LPF_ACC_BITS-1:0] i_sum_unused;
    wire signed [LPF_ACC_BITS-1:0] q_sum_unused;

    assign valid_out = i_lpf_valid & q_lpf_valid;
    assign i_raw_debug = i_raw;
    assign q_raw_debug = q_raw;
    assign cos_debug = cos_lo;
    assign sin_debug = sin_lo;

    always @(posedge clk) begin
        if (!rst_n) begin
            sample_d1 <= {SAMPLE_BITS{1'b0}};
        end else if (valid_in) begin
            sample_d1 <= sample_in;
        end
    end

    dds_nco #(
        .PHASE_BITS(PHASE_BITS),
        .LUT_ADDR_BITS(LUT_ADDR_BITS),
        .AMP_BITS(LO_BITS),
        .PHASE_STEP(PHASE_STEP),
        .COS_MEM_FILE(COS_MEM_FILE),
        .SIN_MEM_FILE(SIN_MEM_FILE)
    ) u_dds_nco (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(valid_in),
        .valid_out(dds_valid),
        .cos_out(cos_lo),
        .sin_out(sin_lo),
        .phase_out(phase_debug)
    );

    ddc_mixer #(
        .SAMPLE_BITS(SAMPLE_BITS),
        .LO_BITS(LO_BITS),
        .PRODUCT_BITS(PRODUCT_BITS)
    ) u_ddc_mixer (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(dds_valid),
        .sample_in(sample_d1),
        .cos_in(cos_lo),
        .sin_in(sin_lo),
        .valid_out(mixer_valid),
        .i_raw(i_raw),
        .q_raw(q_raw)
    );

    moving_average #(
        .DATA_BITS(PRODUCT_BITS),
        .WINDOW_LEN(LPF_WINDOW_LEN),
        .SHIFT_BITS(LPF_SHIFT_BITS),
        .ACC_BITS(LPF_ACC_BITS)
    ) u_i_lpf (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(mixer_valid),
        .data_in(i_raw),
        .valid_out(i_lpf_valid),
        .sum_out(i_sum_unused),
        .avg_out(i_baseband)
    );

    moving_average #(
        .DATA_BITS(PRODUCT_BITS),
        .WINDOW_LEN(LPF_WINDOW_LEN),
        .SHIFT_BITS(LPF_SHIFT_BITS),
        .ACC_BITS(LPF_ACC_BITS)
    ) u_q_lpf (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(mixer_valid),
        .data_in(q_raw),
        .valid_out(q_lpf_valid),
        .sum_out(q_sum_unused),
        .avg_out(q_baseband)
    );

endmodule
