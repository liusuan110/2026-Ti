`timescale 1ns / 1ps

module phase_unwrap #(
    parameter integer ANGLE_BITS = 24,
    parameter integer UNWRAPPED_BITS = 40
) (
    input wire clk,
    input wire rst_n,
    input wire valid_in,
    input wire signed [ANGLE_BITS-1:0] phase_in,
    output reg valid_out,
    output reg signed [UNWRAPPED_BITS-1:0] phase_unwrapped_out,
    output reg signed [UNWRAPPED_BITS-1:0] wrap_offset_debug
);

    localparam signed [ANGLE_BITS:0] PI_Q4_20 = 25'sd3294199;
    localparam signed [ANGLE_BITS-1:0] TWO_PI_Q4_20 = 24'sd6588398;

    reg have_prev;
    reg signed [ANGLE_BITS-1:0] prev_phase;
    reg signed [UNWRAPPED_BITS-1:0] wrap_offset;

    wire signed [ANGLE_BITS:0] phase_ext_delta;
    wire signed [ANGLE_BITS:0] prev_ext_delta;
    wire signed [ANGLE_BITS:0] delta;
    wire signed [UNWRAPPED_BITS-1:0] phase_ext;
    wire signed [UNWRAPPED_BITS-1:0] two_pi_ext;
    wire signed [UNWRAPPED_BITS-1:0] offset_next;

    wire jump_down;
    wire jump_up;

    assign phase_ext_delta = {phase_in[ANGLE_BITS-1], phase_in};
    assign prev_ext_delta = {prev_phase[ANGLE_BITS-1], prev_phase};
    assign delta = phase_ext_delta - prev_ext_delta;

    assign phase_ext = {{(UNWRAPPED_BITS-ANGLE_BITS){phase_in[ANGLE_BITS-1]}}, phase_in};
    assign two_pi_ext = {{(UNWRAPPED_BITS-ANGLE_BITS){TWO_PI_Q4_20[ANGLE_BITS-1]}}, TWO_PI_Q4_20};

    assign jump_down = have_prev && (delta < -PI_Q4_20);
    assign jump_up = have_prev && (delta > PI_Q4_20);

    assign offset_next = jump_down ? (wrap_offset + two_pi_ext) :
                         jump_up ? (wrap_offset - two_pi_ext) :
                         wrap_offset;

    always @(posedge clk) begin
        if (!rst_n) begin
            have_prev <= 1'b0;
            prev_phase <= {ANGLE_BITS{1'b0}};
            wrap_offset <= {UNWRAPPED_BITS{1'b0}};
            wrap_offset_debug <= {UNWRAPPED_BITS{1'b0}};
            phase_unwrapped_out <= {UNWRAPPED_BITS{1'b0}};
            valid_out <= 1'b0;
        end else begin
            valid_out <= 1'b0;

            if (valid_in) begin
                have_prev <= 1'b1;
                prev_phase <= phase_in;
                wrap_offset <= offset_next;
                wrap_offset_debug <= offset_next;
                phase_unwrapped_out <= phase_ext + offset_next;
                valid_out <= 1'b1;
            end
        end
    end

endmodule
