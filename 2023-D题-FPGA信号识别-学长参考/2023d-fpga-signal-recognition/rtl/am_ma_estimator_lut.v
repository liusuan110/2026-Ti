`timescale 1ns / 1ps

module am_ma_estimator_lut #(
    parameter integer MAG_BITS = 45
) (
    input wire clk,
    input wire rst_n,
    input wire valid_in,
    input wire [MAG_BITS-1:0] block_min,
    input wire [MAG_BITS-1:0] block_max,
    output reg result_valid,
    output reg [9:0] ma_permille
);

    localparam integer VALUE_BITS = MAG_BITS + 1;
    localparam integer CMP_BITS = MAG_BITS + 12;

    wire [VALUE_BITS-1:0] range_w;
    wire [VALUE_BITS-1:0] sum_w;
    wire [CMP_BITS-1:0] lhs;
    wire [CMP_BITS-1:0] th_035;
    wire [CMP_BITS-1:0] th_045;
    wire [CMP_BITS-1:0] th_055;
    wire [CMP_BITS-1:0] th_065;
    wire [CMP_BITS-1:0] th_075;
    wire [CMP_BITS-1:0] th_085;

    assign range_w = {1'b0, block_max} - {1'b0, block_min};
    assign sum_w = {1'b0, block_max} + {1'b0, block_min};

    // score = range / sum.  Compare score*1000 against thresholds without division.
    assign lhs = range_w * 11'd1000;
    assign th_035 = sum_w * 10'd623;
    assign th_045 = sum_w * 10'd748;
    assign th_055 = sum_w * 10'd846;
    assign th_065 = sum_w * 10'd914;
    assign th_075 = sum_w * 10'd960;
    assign th_085 = sum_w * 10'd986;

    always @(posedge clk) begin
        if (!rst_n) begin
            result_valid <= 1'b0;
            ma_permille <= 10'd0;
        end else begin
            result_valid <= valid_in;

            if (valid_in) begin
                if (lhs < th_035) begin
                    ma_permille <= 10'd300;
                end else if (lhs < th_045) begin
                    ma_permille <= 10'd400;
                end else if (lhs < th_055) begin
                    ma_permille <= 10'd500;
                end else if (lhs < th_065) begin
                    ma_permille <= 10'd600;
                end else if (lhs < th_075) begin
                    ma_permille <= 10'd700;
                end else if (lhs < th_085) begin
                    ma_permille <= 10'd800;
                end else begin
                    ma_permille <= 10'd900;
                end
            end
        end
    end

endmodule
