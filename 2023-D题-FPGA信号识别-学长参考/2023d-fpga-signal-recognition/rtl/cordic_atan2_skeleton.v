`timescale 1ns / 1ps

module cordic_atan2_skeleton #(
    parameter integer DATA_BITS = 21,
    parameter integer ANGLE_BITS = 24,
    parameter integer ITERATIONS = 20
) (
    input wire clk,
    input wire rst_n,
    input wire valid_in,
    input wire signed [DATA_BITS-1:0] i_in,
    input wire signed [DATA_BITS-1:0] q_in,
    output reg valid_out,
    output reg signed [ANGLE_BITS-1:0] phase_out
);

    // Teaching skeleton only:
    // A production CORDIC will contain ITERATIONS pipeline stages or a
    // sequential iteration controller.  This file documents the intended
    // interface and fixed-point convention for later integration.
    //
    // phase_out is intended to use Q4.20 radians when ANGLE_BITS=24:
    //   1 rad ~= 2^20
    //   pi    ~= 3294199
    //
    // The algorithm will:
    //   1. pre-rotate (I,Q) into the right half-plane for full atan2 range,
    //   2. iteratively rotate the vector toward Q=0,
    //   3. accumulate +/- atan(2^-k) into phase_out.

    always @(posedge clk) begin
        if (!rst_n) begin
            valid_out <= 1'b0;
            phase_out <= {ANGLE_BITS{1'b0}};
        end else begin
            valid_out <= valid_in;

            if (valid_in) begin
                // Placeholder output so downstream modules can be wired during
                // top-level bring-up.  Replace with real CORDIC stages before
                // using phase_out for FM/CW decisions.
                phase_out <= {ANGLE_BITS{1'b0}};
            end
        end
    end

endmodule
