`timescale 1ns / 1ps

module iq_mag_sq_extractor #(
    parameter integer IQ_BITS = 28,
    parameter integer PRODUCT_BITS = 2 * IQ_BITS,
    parameter integer MAG_SQ_BITS = PRODUCT_BITS + 1
) (
    input wire clk,
    input wire rst_n,
    input wire valid_in,
    input wire signed [IQ_BITS-1:0] i_in,
    input wire signed [IQ_BITS-1:0] q_in,
    output reg valid_out,
    output reg [MAG_SQ_BITS-1:0] mag_sq
);

    wire signed [PRODUCT_BITS-1:0] i_sq_w;
    wire signed [PRODUCT_BITS-1:0] q_sq_w;

    assign i_sq_w = i_in * i_in;
    assign q_sq_w = q_in * q_in;

    always @(posedge clk) begin
        if (!rst_n) begin
            valid_out <= 1'b0;
            mag_sq <= {MAG_SQ_BITS{1'b0}};
        end else begin
            valid_out <= valid_in;

            if (valid_in) begin
                mag_sq <= {1'b0, i_sq_w} + {1'b0, q_sq_w};
            end
        end
    end

endmodule
