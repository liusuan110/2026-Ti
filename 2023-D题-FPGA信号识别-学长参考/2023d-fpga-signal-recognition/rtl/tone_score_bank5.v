`timescale 1ns / 1ps

module tone_score_bank5 #(
    parameter integer DATA_BITS = 18,
    parameter integer REF_BITS = 12,
    parameter integer BLOCK_LEN = 250,
    parameter integer COUNT_BITS = 8,
    parameter integer PRODUCT_BITS = DATA_BITS + REF_BITS,
    parameter integer ACC_BITS = PRODUCT_BITS + COUNT_BITS,
    parameter integer USE_POWER_SCORE = 1,
    parameter integer SCORE_BITS = (USE_POWER_SCORE != 0) ? (2 * ACC_BITS + 1) : (ACC_BITS + 1)
) (
    input wire clk,
    input wire rst_n,
    input wire valid_in,
    input wire signed [DATA_BITS-1:0] data_in,
    input wire signed [REF_BITS-1:0] ref0_cos,
    input wire signed [REF_BITS-1:0] ref0_sin,
    input wire signed [REF_BITS-1:0] ref1_cos,
    input wire signed [REF_BITS-1:0] ref1_sin,
    input wire signed [REF_BITS-1:0] ref2_cos,
    input wire signed [REF_BITS-1:0] ref2_sin,
    input wire signed [REF_BITS-1:0] ref3_cos,
    input wire signed [REF_BITS-1:0] ref3_sin,
    input wire signed [REF_BITS-1:0] ref4_cos,
    input wire signed [REF_BITS-1:0] ref4_sin,
    output reg result_valid,
    output reg [2:0] best_index,
    output reg [SCORE_BITS-1:0] best_score_power,
    output wire [SCORE_BITS-1:0] score0_power,
    output wire [SCORE_BITS-1:0] score1_power,
    output wire [SCORE_BITS-1:0] score2_power,
    output wire [SCORE_BITS-1:0] score3_power,
    output wire [SCORE_BITS-1:0] score4_power
);

    wire valid0;
    wire valid1;
    wire valid2;
    wire valid3;
    wire valid4;

    wire signed [ACC_BITS-1:0] unused_i0;
    wire signed [ACC_BITS-1:0] unused_q0;
    wire signed [ACC_BITS-1:0] unused_i1;
    wire signed [ACC_BITS-1:0] unused_q1;
    wire signed [ACC_BITS-1:0] unused_i2;
    wire signed [ACC_BITS-1:0] unused_q2;
    wire signed [ACC_BITS-1:0] unused_i3;
    wire signed [ACC_BITS-1:0] unused_q3;
    wire signed [ACC_BITS-1:0] unused_i4;
    wire signed [ACC_BITS-1:0] unused_q4;

    wire [SCORE_BITS-1:0] max01_power;
    wire [SCORE_BITS-1:0] max23_power;
    wire [SCORE_BITS-1:0] max0123_power;
    wire [SCORE_BITS-1:0] best_power_w;
    wire [2:0] max01_index;
    wire [2:0] max23_index;
    wire [2:0] max0123_index;
    wire [2:0] best_index_w;

    assign max01_power = (score1_power > score0_power) ? score1_power : score0_power;
    assign max01_index = (score1_power > score0_power) ? 3'd1 : 3'd0;
    assign max23_power = (score3_power > score2_power) ? score3_power : score2_power;
    assign max23_index = (score3_power > score2_power) ? 3'd3 : 3'd2;
    assign max0123_power = (max23_power > max01_power) ? max23_power : max01_power;
    assign max0123_index = (max23_power > max01_power) ? max23_index : max01_index;
    assign best_power_w = (score4_power > max0123_power) ? score4_power : max0123_power;
    assign best_index_w = (score4_power > max0123_power) ? 3'd4 : max0123_index;

    tone_score_accumulator #(
        .DATA_BITS(DATA_BITS),
        .REF_BITS(REF_BITS),
        .BLOCK_LEN(BLOCK_LEN),
        .COUNT_BITS(COUNT_BITS),
        .PRODUCT_BITS(PRODUCT_BITS),
        .ACC_BITS(ACC_BITS),
        .SCORE_BITS(SCORE_BITS),
        .USE_POWER_SCORE(USE_POWER_SCORE)
    ) acc0_i (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(valid_in),
        .data_in(data_in),
        .ref_cos(ref0_cos),
        .ref_sin(ref0_sin),
        .result_valid(valid0),
        .i_score(unused_i0),
        .q_score(unused_q0),
        .score_power(score0_power)
    );

    tone_score_accumulator #(
        .DATA_BITS(DATA_BITS),
        .REF_BITS(REF_BITS),
        .BLOCK_LEN(BLOCK_LEN),
        .COUNT_BITS(COUNT_BITS),
        .PRODUCT_BITS(PRODUCT_BITS),
        .ACC_BITS(ACC_BITS),
        .SCORE_BITS(SCORE_BITS),
        .USE_POWER_SCORE(USE_POWER_SCORE)
    ) acc1_i (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(valid_in),
        .data_in(data_in),
        .ref_cos(ref1_cos),
        .ref_sin(ref1_sin),
        .result_valid(valid1),
        .i_score(unused_i1),
        .q_score(unused_q1),
        .score_power(score1_power)
    );

    tone_score_accumulator #(
        .DATA_BITS(DATA_BITS),
        .REF_BITS(REF_BITS),
        .BLOCK_LEN(BLOCK_LEN),
        .COUNT_BITS(COUNT_BITS),
        .PRODUCT_BITS(PRODUCT_BITS),
        .ACC_BITS(ACC_BITS),
        .SCORE_BITS(SCORE_BITS),
        .USE_POWER_SCORE(USE_POWER_SCORE)
    ) acc2_i (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(valid_in),
        .data_in(data_in),
        .ref_cos(ref2_cos),
        .ref_sin(ref2_sin),
        .result_valid(valid2),
        .i_score(unused_i2),
        .q_score(unused_q2),
        .score_power(score2_power)
    );

    tone_score_accumulator #(
        .DATA_BITS(DATA_BITS),
        .REF_BITS(REF_BITS),
        .BLOCK_LEN(BLOCK_LEN),
        .COUNT_BITS(COUNT_BITS),
        .PRODUCT_BITS(PRODUCT_BITS),
        .ACC_BITS(ACC_BITS),
        .SCORE_BITS(SCORE_BITS),
        .USE_POWER_SCORE(USE_POWER_SCORE)
    ) acc3_i (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(valid_in),
        .data_in(data_in),
        .ref_cos(ref3_cos),
        .ref_sin(ref3_sin),
        .result_valid(valid3),
        .i_score(unused_i3),
        .q_score(unused_q3),
        .score_power(score3_power)
    );

    tone_score_accumulator #(
        .DATA_BITS(DATA_BITS),
        .REF_BITS(REF_BITS),
        .BLOCK_LEN(BLOCK_LEN),
        .COUNT_BITS(COUNT_BITS),
        .PRODUCT_BITS(PRODUCT_BITS),
        .ACC_BITS(ACC_BITS),
        .SCORE_BITS(SCORE_BITS),
        .USE_POWER_SCORE(USE_POWER_SCORE)
    ) acc4_i (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(valid_in),
        .data_in(data_in),
        .ref_cos(ref4_cos),
        .ref_sin(ref4_sin),
        .result_valid(valid4),
        .i_score(unused_i4),
        .q_score(unused_q4),
        .score_power(score4_power)
    );

    always @(posedge clk) begin
        if (!rst_n) begin
            result_valid <= 1'b0;
            best_index <= 3'd0;
            best_score_power <= {SCORE_BITS{1'b0}};
        end else begin
            result_valid <= 1'b0;

            if (valid0 && valid1 && valid2 && valid3 && valid4) begin
                result_valid <= 1'b1;
                best_index <= best_index_w;
                best_score_power <= best_power_w;
            end
        end
    end

endmodule
