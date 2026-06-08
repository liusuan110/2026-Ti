`timescale 1ns / 1ps

module ddc_phase_unwrap_core #(
    parameter integer SAMPLE_BITS = 8,
    parameter integer PHASE_BITS = 32,
    parameter integer LUT_ADDR_BITS = 10,
    parameter integer LO_BITS = 12,
    parameter integer PRODUCT_BITS = SAMPLE_BITS + LO_BITS + 1,
    parameter integer ANGLE_BITS = 24,
    parameter integer UNWRAPPED_BITS = 40,
    parameter integer INDEX_BITS = 32,
    parameter integer CORDIC_LATENCY = 20,
    parameter [PHASE_BITS-1:0] PHASE_STEP = 32'd429496730,
    parameter COS_MEM_FILE = "rtl/cos_1024x12.mem",
    parameter SIN_MEM_FILE = "rtl/sin_1024x12.mem"
) (
    input wire clk,
    input wire rst_n,
    input wire valid_in,
    input wire signed [SAMPLE_BITS-1:0] sample_in,
    output wire phase_unwrapped_valid,
    output wire signed [UNWRAPPED_BITS-1:0] phase_unwrapped_out,
    output wire signed [UNWRAPPED_BITS-1:0] wrap_offset_debug,
    output wire ddc_valid_debug,
    output reg [INDEX_BITS-1:0] phase_sample_index_debug,
    output reg signed [PRODUCT_BITS-1:0] i_phase_debug,
    output reg signed [PRODUCT_BITS-1:0] q_phase_debug,
    output reg signed [ANGLE_BITS-1:0] phase_wrapped_debug
);

    wire phase_wrapped_valid;
    wire signed [ANGLE_BITS-1:0] phase_wrapped;
    wire [INDEX_BITS-1:0] phase_sample_index_wrapped;
    wire signed [PRODUCT_BITS-1:0] i_phase_wrapped;
    wire signed [PRODUCT_BITS-1:0] q_phase_wrapped;
    wire signed [PRODUCT_BITS-1:0] i_baseband_unused;
    wire signed [PRODUCT_BITS-1:0] q_baseband_unused;

    ddc_phase_core_pipeline #(
        .SAMPLE_BITS(SAMPLE_BITS),
        .PHASE_BITS(PHASE_BITS),
        .LUT_ADDR_BITS(LUT_ADDR_BITS),
        .LO_BITS(LO_BITS),
        .PRODUCT_BITS(PRODUCT_BITS),
        .ANGLE_BITS(ANGLE_BITS),
        .INDEX_BITS(INDEX_BITS),
        .CORDIC_LATENCY(CORDIC_LATENCY),
        .PHASE_STEP(PHASE_STEP),
        .COS_MEM_FILE(COS_MEM_FILE),
        .SIN_MEM_FILE(SIN_MEM_FILE)
    ) u_ddc_phase_core_pipeline (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(valid_in),
        .sample_in(sample_in),
        .phase_valid(phase_wrapped_valid),
        .phase_out(phase_wrapped),
        .ddc_valid_debug(ddc_valid_debug),
        .i_baseband_debug(i_baseband_unused),
        .q_baseband_debug(q_baseband_unused),
        .phase_sample_index_debug(phase_sample_index_wrapped),
        .i_phase_debug(i_phase_wrapped),
        .q_phase_debug(q_phase_wrapped)
    );

    phase_unwrap #(
        .ANGLE_BITS(ANGLE_BITS),
        .UNWRAPPED_BITS(UNWRAPPED_BITS)
    ) u_phase_unwrap (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(phase_wrapped_valid),
        .phase_in(phase_wrapped),
        .valid_out(phase_unwrapped_valid),
        .phase_unwrapped_out(phase_unwrapped_out),
        .wrap_offset_debug(wrap_offset_debug)
    );

    always @(posedge clk) begin
        if (!rst_n) begin
            phase_sample_index_debug <= {INDEX_BITS{1'b0}};
            i_phase_debug <= {PRODUCT_BITS{1'b0}};
            q_phase_debug <= {PRODUCT_BITS{1'b0}};
            phase_wrapped_debug <= {ANGLE_BITS{1'b0}};
        end else begin
            if (phase_wrapped_valid) begin
                phase_sample_index_debug <= phase_sample_index_wrapped;
                i_phase_debug <= i_phase_wrapped;
                q_phase_debug <= q_phase_wrapped;
                phase_wrapped_debug <= phase_wrapped;
            end
        end
    end

endmodule
