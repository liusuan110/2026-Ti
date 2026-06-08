`timescale 1ns / 1ps

module fm_delta_phase_stats #(
    parameter integer ANGLE_BITS = 24,
    parameter integer BLOCK_LEN = 250,
    parameter integer COUNT_BITS = 8,
    parameter integer ABS_BITS = ANGLE_BITS + 1
) (
    input wire clk,
    input wire rst_n,
    input wire valid_in,
    input wire signed [ANGLE_BITS-1:0] phase_delta_in,
    output reg result_valid,
    output reg [ABS_BITS-1:0] max_abs_phase_delta
);

    localparam [COUNT_BITS-1:0] BLOCK_LAST = BLOCK_LEN - 1;

    reg [COUNT_BITS-1:0] count;
    reg [ABS_BITS-1:0] max_abs_acc;

    wire signed [ANGLE_BITS:0] phase_ext;
    wire [ABS_BITS-1:0] abs_phase;
    wire [ABS_BITS-1:0] max_abs_next;

    assign phase_ext = {phase_delta_in[ANGLE_BITS-1], phase_delta_in};
    assign abs_phase = phase_ext[ANGLE_BITS] ? $unsigned(-phase_ext) : $unsigned(phase_ext);
    assign max_abs_next = (count == {COUNT_BITS{1'b0}} || abs_phase > max_abs_acc) ? abs_phase : max_abs_acc;

    always @(posedge clk) begin
        if (!rst_n) begin
            count <= {COUNT_BITS{1'b0}};
            max_abs_acc <= {ABS_BITS{1'b0}};
            result_valid <= 1'b0;
            max_abs_phase_delta <= {ABS_BITS{1'b0}};
        end else begin
            result_valid <= 1'b0;

            if (valid_in) begin
                if (count == BLOCK_LAST) begin
                    result_valid <= 1'b1;
                    max_abs_phase_delta <= max_abs_next;
                    count <= {COUNT_BITS{1'b0}};
                    max_abs_acc <= {ABS_BITS{1'b0}};
                end else begin
                    count <= count + 1'b1;
                    max_abs_acc <= max_abs_next;
                end
            end
        end
    end

endmodule
