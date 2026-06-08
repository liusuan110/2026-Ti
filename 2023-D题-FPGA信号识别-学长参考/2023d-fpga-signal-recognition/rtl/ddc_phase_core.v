`timescale 1ns / 1ps

module ddc_phase_core #(
    parameter integer SAMPLE_BITS = 8,
    parameter integer PHASE_BITS = 32,
    parameter integer LUT_ADDR_BITS = 10,
    parameter integer LO_BITS = 12,
    parameter integer PRODUCT_BITS = SAMPLE_BITS + LO_BITS + 1,
    parameter integer ANGLE_BITS = 24,
    parameter integer INDEX_BITS = 32,
    parameter [PHASE_BITS-1:0] PHASE_STEP = 32'd429496730,
    parameter COS_MEM_FILE = "rtl/cos_1024x12.mem",
    parameter SIN_MEM_FILE = "rtl/sin_1024x12.mem"
) (
    input wire clk,
    input wire rst_n,
    input wire valid_in,
    input wire signed [SAMPLE_BITS-1:0] sample_in,
    output wire phase_valid,
    output wire signed [ANGLE_BITS-1:0] phase_out,
    output wire ddc_valid_debug,
    output wire cordic_ready_debug,
    output wire cordic_busy_debug,
    output reg [INDEX_BITS-1:0] phase_sample_index_debug,
    output reg signed [PRODUCT_BITS-1:0] i_phase_debug,
    output reg signed [PRODUCT_BITS-1:0] q_phase_debug
);

    wire ddc_valid;
    wire signed [PRODUCT_BITS-1:0] i_baseband;
    wire signed [PRODUCT_BITS-1:0] q_baseband;
    wire signed [PRODUCT_BITS-1:0] i_raw_unused;
    wire signed [PRODUCT_BITS-1:0] q_raw_unused;
    wire signed [LO_BITS-1:0] cos_unused;
    wire signed [LO_BITS-1:0] sin_unused;
    wire [PHASE_BITS-1:0] phase_unused;

    wire cordic_ready;
    wire cordic_busy;
    wire cordic_accept;
    wire [5:0] cordic_iter_unused;

    reg [INDEX_BITS-1:0] ddc_sample_index;

    assign cordic_accept = ddc_valid & cordic_ready;
    assign ddc_valid_debug = ddc_valid;
    assign cordic_ready_debug = cordic_ready;
    assign cordic_busy_debug = cordic_busy;

    ddc_core #(
        .SAMPLE_BITS(SAMPLE_BITS),
        .PHASE_BITS(PHASE_BITS),
        .LUT_ADDR_BITS(LUT_ADDR_BITS),
        .LO_BITS(LO_BITS),
        .PRODUCT_BITS(PRODUCT_BITS),
        .PHASE_STEP(PHASE_STEP),
        .COS_MEM_FILE(COS_MEM_FILE),
        .SIN_MEM_FILE(SIN_MEM_FILE)
    ) u_ddc_core (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(valid_in),
        .sample_in(sample_in),
        .valid_out(ddc_valid),
        .i_baseband(i_baseband),
        .q_baseband(q_baseband),
        .i_raw_debug(i_raw_unused),
        .q_raw_debug(q_raw_unused),
        .cos_debug(cos_unused),
        .sin_debug(sin_unused),
        .phase_debug(phase_unused)
    );

    cordic_atan2_iterative #(
        .DATA_BITS(PRODUCT_BITS),
        .ANGLE_BITS(ANGLE_BITS)
    ) u_cordic (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(cordic_accept),
        .i_in(i_baseband),
        .q_in(q_baseband),
        .input_ready(cordic_ready),
        .busy(cordic_busy),
        .valid_out(phase_valid),
        .phase_out(phase_out),
        .iter_debug(cordic_iter_unused)
    );

    always @(posedge clk) begin
        if (!rst_n) begin
            ddc_sample_index <= {INDEX_BITS{1'b0}};
            phase_sample_index_debug <= {INDEX_BITS{1'b0}};
            i_phase_debug <= {PRODUCT_BITS{1'b0}};
            q_phase_debug <= {PRODUCT_BITS{1'b0}};
        end else begin
            if (ddc_valid) begin
                if (cordic_accept) begin
                    phase_sample_index_debug <= ddc_sample_index;
                    i_phase_debug <= i_baseband;
                    q_phase_debug <= q_baseband;
                end

                ddc_sample_index <= ddc_sample_index + 1'b1;
            end
        end
    end

endmodule
