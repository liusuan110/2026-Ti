`timescale 1ns / 1ps

module iq_magnitude_approx #(
    parameter integer DATA_BITS = 21,
    parameter integer ABS_BITS = DATA_BITS + 1,
    parameter integer MAG_APPROX_BITS = ABS_BITS + 1,
    parameter integer MAG_SQ_BITS = 2 * ABS_BITS + 1
) (
    input wire clk,
    input wire rst_n,
    input wire valid_in,
    input wire signed [DATA_BITS-1:0] i_in,
    input wire signed [DATA_BITS-1:0] q_in,
    output reg valid_out,
    output reg [ABS_BITS-1:0] abs_i,
    output reg [ABS_BITS-1:0] abs_q,
    output reg [MAG_APPROX_BITS-1:0] mag_approx,
    output reg [MAG_SQ_BITS-1:0] mag_sq
);

    wire signed [ABS_BITS-1:0] i_ext;
    wire signed [ABS_BITS-1:0] q_ext;
    wire [ABS_BITS-1:0] abs_i_w;
    wire [ABS_BITS-1:0] abs_q_w;
    wire [ABS_BITS-1:0] max_abs_w;
    wire [ABS_BITS-1:0] min_abs_w;
    wire [MAG_APPROX_BITS-1:0] mag_approx_w;
    wire [MAG_SQ_BITS-1:0] i_sq_w;
    wire [MAG_SQ_BITS-1:0] q_sq_w;

    assign i_ext = {i_in[DATA_BITS-1], i_in};
    assign q_ext = {q_in[DATA_BITS-1], q_in};

    assign abs_i_w = i_ext[ABS_BITS-1] ? $unsigned(-i_ext) : $unsigned(i_ext);
    assign abs_q_w = q_ext[ABS_BITS-1] ? $unsigned(-q_ext) : $unsigned(q_ext);

    assign max_abs_w = (abs_i_w >= abs_q_w) ? abs_i_w : abs_q_w;
    assign min_abs_w = (abs_i_w >= abs_q_w) ? abs_q_w : abs_i_w;

    assign mag_approx_w = {1'b0, max_abs_w} + {2'b00, min_abs_w[ABS_BITS-1:1]};
    assign i_sq_w = abs_i_w * abs_i_w;
    assign q_sq_w = abs_q_w * abs_q_w;

    always @(posedge clk) begin
        if (!rst_n) begin
            valid_out <= 1'b0;
            abs_i <= {ABS_BITS{1'b0}};
            abs_q <= {ABS_BITS{1'b0}};
            mag_approx <= {MAG_APPROX_BITS{1'b0}};
            mag_sq <= {MAG_SQ_BITS{1'b0}};
        end else begin
            valid_out <= valid_in;

            if (valid_in) begin
                abs_i <= abs_i_w;
                abs_q <= abs_q_w;
                mag_approx <= mag_approx_w;
                mag_sq <= i_sq_w + q_sq_w;
            end
        end
    end

endmodule
