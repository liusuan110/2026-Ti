`timescale 1ns / 1ps

module tone_score_accumulator #(
    parameter integer DATA_BITS = 45,
    parameter integer REF_BITS = 12,
    parameter integer BLOCK_LEN = 40000,
    parameter integer COUNT_BITS = 16,
    parameter integer PRODUCT_BITS = DATA_BITS + REF_BITS,
    parameter integer ACC_BITS = PRODUCT_BITS + COUNT_BITS,
    parameter integer USE_POWER_SCORE = 1,
    parameter integer SCORE_BITS = (USE_POWER_SCORE != 0) ? (2 * ACC_BITS + 1) : (ACC_BITS + 1)
) (
    input wire clk,
    input wire rst_n,
    input wire valid_in,
    input wire signed [DATA_BITS-1:0] data_in,
    input wire signed [REF_BITS-1:0] ref_cos,
    input wire signed [REF_BITS-1:0] ref_sin,
    output reg result_valid,
    output reg signed [ACC_BITS-1:0] i_score,
    output reg signed [ACC_BITS-1:0] q_score,
    output reg [SCORE_BITS-1:0] score_power
);

    localparam [COUNT_BITS-1:0] BLOCK_LAST = BLOCK_LEN - 1;

    reg [COUNT_BITS-1:0] count;
    reg signed [ACC_BITS-1:0] i_acc;
    reg signed [ACC_BITS-1:0] q_acc;

    wire last_sample;
    wire signed [PRODUCT_BITS-1:0] i_product;
    wire signed [PRODUCT_BITS-1:0] q_product;
    wire signed [ACC_BITS-1:0] i_product_ext;
    wire signed [ACC_BITS-1:0] q_product_ext;
    wire signed [ACC_BITS-1:0] i_next;
    wire signed [ACC_BITS-1:0] q_next;
    wire [SCORE_BITS-1:0] score_next;

    assign last_sample = (count == BLOCK_LAST);
    assign i_product = data_in * ref_cos;
    assign q_product = data_in * ref_sin;
    assign i_product_ext = {{(ACC_BITS-PRODUCT_BITS){i_product[PRODUCT_BITS-1]}}, i_product};
    assign q_product_ext = {{(ACC_BITS-PRODUCT_BITS){q_product[PRODUCT_BITS-1]}}, q_product};
    assign i_next = i_acc + i_product_ext;
    assign q_next = q_acc + q_product_ext;
    generate
        if (USE_POWER_SCORE) begin : power_score_gen
            wire [SCORE_BITS-1:0] i_sq;
            wire [SCORE_BITS-1:0] q_sq;

            assign i_sq = i_next * i_next;
            assign q_sq = q_next * q_next;
            assign score_next = i_sq + q_sq;
        end else begin : abs_score_gen
            wire signed [ACC_BITS:0] i_ext;
            wire signed [ACC_BITS:0] q_ext;
            wire [ACC_BITS:0] abs_i;
            wire [ACC_BITS:0] abs_q;
            wire [ACC_BITS:0] abs_sum;

            assign i_ext = {i_next[ACC_BITS-1], i_next};
            assign q_ext = {q_next[ACC_BITS-1], q_next};
            assign abs_i = i_ext[ACC_BITS] ? $unsigned(-i_ext) : $unsigned(i_ext);
            assign abs_q = q_ext[ACC_BITS] ? $unsigned(-q_ext) : $unsigned(q_ext);
            assign abs_sum = abs_i + abs_q;
            assign score_next = {{(SCORE_BITS-ACC_BITS-1){1'b0}}, abs_sum};
        end
    endgenerate

    always @(posedge clk) begin
        if (!rst_n) begin
            count <= {COUNT_BITS{1'b0}};
            i_acc <= {ACC_BITS{1'b0}};
            q_acc <= {ACC_BITS{1'b0}};
            result_valid <= 1'b0;
            i_score <= {ACC_BITS{1'b0}};
            q_score <= {ACC_BITS{1'b0}};
            score_power <= {SCORE_BITS{1'b0}};
        end else begin
            result_valid <= 1'b0;

            if (valid_in) begin
                if (last_sample) begin
                    result_valid <= 1'b1;
                    i_score <= i_next;
                    q_score <= q_next;
                    score_power <= score_next;
                    count <= {COUNT_BITS{1'b0}};
                    i_acc <= {ACC_BITS{1'b0}};
                    q_acc <= {ACC_BITS{1'b0}};
                end else begin
                    count <= count + 1'b1;
                    i_acc <= i_next;
                    q_acc <= q_next;
                end
            end
        end
    end

endmodule
