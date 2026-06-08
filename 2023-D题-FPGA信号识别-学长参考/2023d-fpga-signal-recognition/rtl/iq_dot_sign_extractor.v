`timescale 1ns / 1ps

module iq_dot_sign_extractor #(
    parameter integer IQ_BITS = 28,
    parameter integer PRODUCT_BITS = 2 * IQ_BITS,
    parameter integer DOT_BITS = PRODUCT_BITS + 1
) (
    input wire clk,
    input wire rst_n,
    input wire valid_in,
    input wire signed [IQ_BITS-1:0] i_in,
    input wire signed [IQ_BITS-1:0] q_in,
    output reg valid_out,
    output reg delta_valid,
    output reg dot_negative
);

    reg prev_valid;
    reg signed [IQ_BITS-1:0] prev_i;
    reg signed [IQ_BITS-1:0] prev_q;

    wire signed [PRODUCT_BITS-1:0] ii_prev_w;
    wire signed [PRODUCT_BITS-1:0] qq_prev_w;
    wire signed [DOT_BITS-1:0] ii_prev_ext;
    wire signed [DOT_BITS-1:0] qq_prev_ext;
    wire signed [DOT_BITS-1:0] dot_w;

    assign ii_prev_w = i_in * prev_i;
    assign qq_prev_w = q_in * prev_q;
    assign ii_prev_ext = {ii_prev_w[PRODUCT_BITS-1], ii_prev_w};
    assign qq_prev_ext = {qq_prev_w[PRODUCT_BITS-1], qq_prev_w};
    assign dot_w = ii_prev_ext + qq_prev_ext;

    always @(posedge clk) begin
        if (!rst_n) begin
            prev_valid <= 1'b0;
            prev_i <= {IQ_BITS{1'b0}};
            prev_q <= {IQ_BITS{1'b0}};
            valid_out <= 1'b0;
            delta_valid <= 1'b0;
            dot_negative <= 1'b0;
        end else begin
            valid_out <= valid_in;
            delta_valid <= valid_in && prev_valid;

            if (valid_in) begin
                dot_negative <= prev_valid && dot_w[DOT_BITS-1];
                prev_i <= i_in;
                prev_q <= q_in;
                prev_valid <= 1'b1;
            end
        end
    end

endmodule
