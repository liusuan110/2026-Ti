`timescale 1ns / 1ps

module ddc_mixer #(
    parameter integer SAMPLE_BITS = 8,
    parameter integer LO_BITS = 12,
    parameter integer PRODUCT_BITS = SAMPLE_BITS + LO_BITS + 1
) (
    input wire clk,
    input wire rst_n,
    input wire valid_in,
    input wire signed [SAMPLE_BITS-1:0] sample_in,
    input wire signed [LO_BITS-1:0] cos_in,
    input wire signed [LO_BITS-1:0] sin_in,
    output reg valid_out,
    output reg signed [PRODUCT_BITS-1:0] i_raw,
    output reg signed [PRODUCT_BITS-1:0] q_raw
);

    localparam integer I_PRODUCT_BITS = SAMPLE_BITS + LO_BITS;

    wire signed [I_PRODUCT_BITS-1:0] i_product;
    wire signed [LO_BITS:0] neg_sin_ext;
    wire signed [PRODUCT_BITS-1:0] q_product;

    assign i_product = sample_in * cos_in;
    assign neg_sin_ext = -$signed({sin_in[LO_BITS-1], sin_in});
    assign q_product = sample_in * neg_sin_ext;

    always @(posedge clk) begin
        if (!rst_n) begin
            valid_out <= 1'b0;
            i_raw <= {PRODUCT_BITS{1'b0}};
            q_raw <= {PRODUCT_BITS{1'b0}};
        end else begin
            valid_out <= valid_in;

            if (valid_in) begin
                i_raw <= {{(PRODUCT_BITS-I_PRODUCT_BITS){i_product[I_PRODUCT_BITS-1]}}, i_product};
                q_raw <= q_product;
            end
        end
    end

endmodule
