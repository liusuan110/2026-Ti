`timescale 1ns / 1ps

module ddc_boxcar_decimator #(
    parameter integer SAMPLE_BITS = 8,
    parameter integer LO_BITS = 12,
    parameter integer DECIM = 80,
    parameter integer COUNT_BITS = 7,
    parameter integer PRODUCT_BITS = SAMPLE_BITS + LO_BITS + 1,
    parameter integer ACC_BITS = PRODUCT_BITS + COUNT_BITS
) (
    input wire clk,
    input wire rst_n,
    input wire valid_in,
    input wire signed [SAMPLE_BITS-1:0] sample_in,
    input wire signed [LO_BITS-1:0] cos_in,
    input wire signed [LO_BITS-1:0] sin_in,
    output reg valid_out,
    output reg signed [ACC_BITS-1:0] i_sum,
    output reg signed [ACC_BITS-1:0] q_sum
);

    localparam [COUNT_BITS-1:0] DECIM_LAST = DECIM - 1;
    localparam integer I_PRODUCT_BITS = SAMPLE_BITS + LO_BITS;

    reg [COUNT_BITS-1:0] count;
    reg signed [ACC_BITS-1:0] i_acc;
    reg signed [ACC_BITS-1:0] q_acc;

    wire signed [I_PRODUCT_BITS-1:0] i_product_raw;
    wire signed [LO_BITS:0] neg_sin_ext;
    wire signed [PRODUCT_BITS-1:0] q_product_raw;
    wire signed [PRODUCT_BITS-1:0] i_product;
    wire signed [ACC_BITS-1:0] i_product_ext;
    wire signed [ACC_BITS-1:0] q_product_ext;
    wire signed [ACC_BITS-1:0] i_next;
    wire signed [ACC_BITS-1:0] q_next;

    assign i_product_raw = sample_in * cos_in;
    assign neg_sin_ext = -$signed({sin_in[LO_BITS-1], sin_in});
    assign q_product_raw = sample_in * neg_sin_ext;
    assign i_product = {{(PRODUCT_BITS-I_PRODUCT_BITS){i_product_raw[I_PRODUCT_BITS-1]}}, i_product_raw};
    assign i_product_ext = {{(ACC_BITS-PRODUCT_BITS){i_product[PRODUCT_BITS-1]}}, i_product};
    assign q_product_ext = {{(ACC_BITS-PRODUCT_BITS){q_product_raw[PRODUCT_BITS-1]}}, q_product_raw};
    assign i_next = i_acc + i_product_ext;
    assign q_next = q_acc + q_product_ext;

    always @(posedge clk) begin
        if (!rst_n) begin
            count <= {COUNT_BITS{1'b0}};
            i_acc <= {ACC_BITS{1'b0}};
            q_acc <= {ACC_BITS{1'b0}};
            valid_out <= 1'b0;
            i_sum <= {ACC_BITS{1'b0}};
            q_sum <= {ACC_BITS{1'b0}};
        end else begin
            valid_out <= 1'b0;

            if (valid_in) begin
                if (count == DECIM_LAST) begin
                    valid_out <= 1'b1;
                    i_sum <= i_next;
                    q_sum <= q_next;
                    i_acc <= {ACC_BITS{1'b0}};
                    q_acc <= {ACC_BITS{1'b0}};
                    count <= {COUNT_BITS{1'b0}};
                end else begin
                    i_acc <= i_next;
                    q_acc <= q_next;
                    count <= count + 1'b1;
                end
            end
        end
    end

endmodule
