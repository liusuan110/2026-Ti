`timescale 1ns / 1ps

module ddc_phase_core_pipeline #(
    parameter integer SAMPLE_BITS = 8,
    parameter integer PHASE_BITS = 32,
    parameter integer LUT_ADDR_BITS = 10,
    parameter integer LO_BITS = 12,
    parameter integer PRODUCT_BITS = SAMPLE_BITS + LO_BITS + 1,
    parameter integer ANGLE_BITS = 24,
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
    output wire phase_valid,
    output wire signed [ANGLE_BITS-1:0] phase_out,
    output wire ddc_valid_debug,
    output wire signed [PRODUCT_BITS-1:0] i_baseband_debug,
    output wire signed [PRODUCT_BITS-1:0] q_baseband_debug,
    output wire [INDEX_BITS-1:0] phase_sample_index_debug,
    output wire signed [PRODUCT_BITS-1:0] i_phase_debug,
    output wire signed [PRODUCT_BITS-1:0] q_phase_debug
);

    wire ddc_valid;
    wire signed [PRODUCT_BITS-1:0] i_baseband;
    wire signed [PRODUCT_BITS-1:0] q_baseband;
    wire signed [PRODUCT_BITS-1:0] i_raw_unused;
    wire signed [PRODUCT_BITS-1:0] q_raw_unused;
    wire signed [LO_BITS-1:0] cos_unused;
    wire signed [LO_BITS-1:0] sin_unused;
    wire [PHASE_BITS-1:0] phase_unused;

    reg [INDEX_BITS-1:0] ddc_sample_index;
    reg [INDEX_BITS-1:0] index_pipe [0:CORDIC_LATENCY];
    reg signed [PRODUCT_BITS-1:0] i_pipe [0:CORDIC_LATENCY];
    reg signed [PRODUCT_BITS-1:0] q_pipe [0:CORDIC_LATENCY];

    integer k;

    assign ddc_valid_debug = ddc_valid;
    assign i_baseband_debug = i_baseband;
    assign q_baseband_debug = q_baseband;
    assign phase_sample_index_debug = index_pipe[CORDIC_LATENCY];
    assign i_phase_debug = i_pipe[CORDIC_LATENCY];
    assign q_phase_debug = q_pipe[CORDIC_LATENCY];

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

    cordic_atan2_pipeline #(
        .DATA_BITS(PRODUCT_BITS),
        .ANGLE_BITS(ANGLE_BITS),
        .ITERATIONS(CORDIC_LATENCY)
    ) u_cordic_pipeline (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(ddc_valid),
        .i_in(i_baseband),
        .q_in(q_baseband),
        .valid_out(phase_valid),
        .phase_out(phase_out)
    );

    always @(posedge clk) begin
        if (!rst_n) begin
            ddc_sample_index <= {INDEX_BITS{1'b0}};
            for (k = 0; k <= CORDIC_LATENCY; k = k + 1) begin
                index_pipe[k] <= {INDEX_BITS{1'b0}};
                i_pipe[k] <= {PRODUCT_BITS{1'b0}};
                q_pipe[k] <= {PRODUCT_BITS{1'b0}};
            end
        end else begin
            if (ddc_valid) begin
                index_pipe[0] <= ddc_sample_index;
                i_pipe[0] <= i_baseband;
                q_pipe[0] <= q_baseband;
                ddc_sample_index <= ddc_sample_index + 1'b1;
            end else begin
                index_pipe[0] <= {INDEX_BITS{1'b0}};
                i_pipe[0] <= {PRODUCT_BITS{1'b0}};
                q_pipe[0] <= {PRODUCT_BITS{1'b0}};
            end

            for (k = 0; k < CORDIC_LATENCY; k = k + 1) begin
                index_pipe[k + 1] <= index_pipe[k];
                i_pipe[k + 1] <= i_pipe[k];
                q_pipe[k + 1] <= q_pipe[k];
            end
        end
    end

endmodule
