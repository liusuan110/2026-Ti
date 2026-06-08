`timescale 1ns / 1ps

module iq_feature_extractor #(
    parameter integer IQ_BITS = 28,
    parameter integer PRODUCT_BITS = 2 * IQ_BITS,
    parameter integer MAG_SQ_BITS = PRODUCT_BITS + 1,
    parameter integer DOT_BITS = PRODUCT_BITS + 1
) (
    input wire clk,
    input wire rst_n,
    input wire valid_in,
    input wire signed [IQ_BITS-1:0] i_in,
    input wire signed [IQ_BITS-1:0] q_in,
    output reg valid_out,
    output reg delta_valid,
    output reg [MAG_SQ_BITS-1:0] mag_sq,
    output reg signed [DOT_BITS-1:0] dot,
    output reg signed [DOT_BITS-1:0] cross
);

    reg prev_valid;
    reg signed [IQ_BITS-1:0] prev_i;
    reg signed [IQ_BITS-1:0] prev_q;

    wire signed [PRODUCT_BITS-1:0] i_sq_w;
    wire signed [PRODUCT_BITS-1:0] q_sq_w;
    wire signed [PRODUCT_BITS-1:0] ii_prev_w;
    wire signed [PRODUCT_BITS-1:0] qq_prev_w;
    wire signed [PRODUCT_BITS-1:0] qi_prev_w;
    wire signed [PRODUCT_BITS-1:0] iq_prev_w;

    wire signed [DOT_BITS-1:0] ii_prev_ext;
    wire signed [DOT_BITS-1:0] qq_prev_ext;
    wire signed [DOT_BITS-1:0] qi_prev_ext;
    wire signed [DOT_BITS-1:0] iq_prev_ext;

    assign i_sq_w = i_in * i_in;
    assign q_sq_w = q_in * q_in;
    assign ii_prev_w = i_in * prev_i;
    assign qq_prev_w = q_in * prev_q;
    assign qi_prev_w = q_in * prev_i;
    assign iq_prev_w = i_in * prev_q;

    assign ii_prev_ext = {ii_prev_w[PRODUCT_BITS-1], ii_prev_w};
    assign qq_prev_ext = {qq_prev_w[PRODUCT_BITS-1], qq_prev_w};
    assign qi_prev_ext = {qi_prev_w[PRODUCT_BITS-1], qi_prev_w};
    assign iq_prev_ext = {iq_prev_w[PRODUCT_BITS-1], iq_prev_w};

    always @(posedge clk) begin
        if (!rst_n) begin
            prev_valid <= 1'b0;
            prev_i <= {IQ_BITS{1'b0}};
            prev_q <= {IQ_BITS{1'b0}};
            valid_out <= 1'b0;
            delta_valid <= 1'b0;
            mag_sq <= {MAG_SQ_BITS{1'b0}};
            dot <= {DOT_BITS{1'b0}};
            cross <= {DOT_BITS{1'b0}};
        end else begin
            valid_out <= valid_in;
            delta_valid <= valid_in && prev_valid;

            if (valid_in) begin
                mag_sq <= {1'b0, i_sq_w} + {1'b0, q_sq_w};

                if (prev_valid) begin
                    dot <= ii_prev_ext + qq_prev_ext;
                    cross <= qi_prev_ext - iq_prev_ext;
                end else begin
                    dot <= {DOT_BITS{1'b0}};
                    cross <= {DOT_BITS{1'b0}};
                end

                prev_i <= i_in;
                prev_q <= q_in;
                prev_valid <= 1'b1;
            end
        end
    end

endmodule
