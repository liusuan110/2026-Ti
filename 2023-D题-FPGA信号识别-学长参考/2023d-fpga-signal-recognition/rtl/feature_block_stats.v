`timescale 1ns / 1ps

module feature_block_stats #(
    parameter integer MAG_BITS = 57,
    parameter integer DELTA_BITS = 57,
    parameter integer BLOCK_LEN = 256,
    parameter integer COUNT_BITS = 8,
    parameter integer MAG_SUM_BITS = MAG_BITS + COUNT_BITS,
    parameter integer ABS_DELTA_BITS = DELTA_BITS + 1,
    parameter integer CROSS_SUM_BITS = ABS_DELTA_BITS + COUNT_BITS
) (
    input wire clk,
    input wire rst_n,
    input wire valid_in,
    input wire [MAG_BITS-1:0] mag_sq_in,
    input wire delta_valid_in,
    input wire signed [DELTA_BITS-1:0] dot_in,
    input wire signed [DELTA_BITS-1:0] cross_in,
    output reg result_valid,
    output reg [MAG_BITS-1:0] mag_min,
    output reg [MAG_BITS-1:0] mag_max,
    output reg [MAG_SUM_BITS-1:0] mag_sum,
    output reg [CROSS_SUM_BITS-1:0] abs_cross_sum,
    output reg [COUNT_BITS:0] negative_dot_count
);

    localparam [COUNT_BITS-1:0] BLOCK_LAST = BLOCK_LEN - 1;

    reg [COUNT_BITS-1:0] count;
    reg [MAG_BITS-1:0] mag_min_acc;
    reg [MAG_BITS-1:0] mag_max_acc;
    reg [MAG_SUM_BITS-1:0] mag_sum_acc;
    reg [CROSS_SUM_BITS-1:0] abs_cross_sum_acc;
    reg [COUNT_BITS:0] negative_dot_count_acc;

    wire first_sample;
    wire last_sample;
    wire [MAG_BITS-1:0] mag_min_next;
    wire [MAG_BITS-1:0] mag_max_next;
    wire [MAG_SUM_BITS-1:0] mag_sq_ext;
    wire [MAG_SUM_BITS-1:0] mag_sum_next;

    wire signed [DELTA_BITS:0] cross_ext;
    wire [ABS_DELTA_BITS-1:0] abs_cross;
    wire [CROSS_SUM_BITS-1:0] abs_cross_ext;
    wire [CROSS_SUM_BITS-1:0] abs_cross_sum_next;
    wire [COUNT_BITS:0] negative_dot_count_next;

    assign first_sample = (count == {COUNT_BITS{1'b0}});
    assign last_sample = (count == BLOCK_LAST);
    assign mag_min_next = (first_sample || (mag_sq_in < mag_min_acc)) ? mag_sq_in : mag_min_acc;
    assign mag_max_next = (first_sample || (mag_sq_in > mag_max_acc)) ? mag_sq_in : mag_max_acc;
    assign mag_sq_ext = {{COUNT_BITS{1'b0}}, mag_sq_in};
    assign mag_sum_next = mag_sum_acc + mag_sq_ext;

    assign cross_ext = {cross_in[DELTA_BITS-1], cross_in};
    assign abs_cross = cross_ext[DELTA_BITS] ? $unsigned(-cross_ext) : $unsigned(cross_ext);
    assign abs_cross_ext = {{COUNT_BITS{1'b0}}, abs_cross};
    assign abs_cross_sum_next = delta_valid_in ? (abs_cross_sum_acc + abs_cross_ext) : abs_cross_sum_acc;
    assign negative_dot_count_next = (delta_valid_in && dot_in[DELTA_BITS-1]) ?
        (negative_dot_count_acc + 1'b1) : negative_dot_count_acc;

    always @(posedge clk) begin
        if (!rst_n) begin
            count <= {COUNT_BITS{1'b0}};
            mag_min_acc <= {MAG_BITS{1'b0}};
            mag_max_acc <= {MAG_BITS{1'b0}};
            mag_sum_acc <= {MAG_SUM_BITS{1'b0}};
            abs_cross_sum_acc <= {CROSS_SUM_BITS{1'b0}};
            negative_dot_count_acc <= {(COUNT_BITS+1){1'b0}};
            result_valid <= 1'b0;
            mag_min <= {MAG_BITS{1'b0}};
            mag_max <= {MAG_BITS{1'b0}};
            mag_sum <= {MAG_SUM_BITS{1'b0}};
            abs_cross_sum <= {CROSS_SUM_BITS{1'b0}};
            negative_dot_count <= {(COUNT_BITS+1){1'b0}};
        end else begin
            result_valid <= 1'b0;

            if (valid_in) begin
                if (last_sample) begin
                    result_valid <= 1'b1;
                    mag_min <= mag_min_next;
                    mag_max <= mag_max_next;
                    mag_sum <= mag_sum_next;
                    abs_cross_sum <= abs_cross_sum_next;
                    negative_dot_count <= negative_dot_count_next;

                    count <= {COUNT_BITS{1'b0}};
                    mag_min_acc <= {MAG_BITS{1'b0}};
                    mag_max_acc <= {MAG_BITS{1'b0}};
                    mag_sum_acc <= {MAG_SUM_BITS{1'b0}};
                    abs_cross_sum_acc <= {CROSS_SUM_BITS{1'b0}};
                    negative_dot_count_acc <= {(COUNT_BITS+1){1'b0}};
                end else begin
                    count <= count + 1'b1;
                    mag_min_acc <= mag_min_next;
                    mag_max_acc <= mag_max_next;
                    mag_sum_acc <= mag_sum_next;
                    abs_cross_sum_acc <= abs_cross_sum_next;
                    negative_dot_count_acc <= negative_dot_count_next;
                end
            end
        end
    end

endmodule
