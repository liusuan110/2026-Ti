`timescale 1ns / 1ps

module am_coarse_classifier #(
    parameter integer MAG_BITS = 45,
    parameter integer BLOCK_LEN = 32768,
    parameter integer COUNT_BITS = 16,
    parameter integer THRESHOLD_PERCENT = 25
) (
    input wire clk,
    input wire rst_n,
    input wire valid_in,
    input wire [MAG_BITS-1:0] mag_sq_in,
    output reg result_valid,
    output reg is_am,
    output reg [MAG_BITS-1:0] block_min,
    output reg [MAG_BITS-1:0] block_max,
    output reg [MAG_BITS:0] range_out,
    output reg [MAG_BITS:0] sum_out
);

    localparam integer SCORE_BITS = MAG_BITS + 8;
    localparam [COUNT_BITS-1:0] BLOCK_LAST = BLOCK_LEN - 1;

    reg [COUNT_BITS-1:0] count;
    reg [MAG_BITS-1:0] min_reg;
    reg [MAG_BITS-1:0] max_reg;

    wire first_sample;
    wire last_sample;
    wire [MAG_BITS-1:0] min_next;
    wire [MAG_BITS-1:0] max_next;
    wire [MAG_BITS:0] range_next;
    wire [MAG_BITS:0] sum_next;
    wire [SCORE_BITS-1:0] range_scaled;
    wire [SCORE_BITS-1:0] threshold_scaled;

    assign first_sample = (count == {COUNT_BITS{1'b0}});
    assign last_sample = (count == BLOCK_LAST);

    assign min_next = (first_sample || (mag_sq_in < min_reg)) ? mag_sq_in : min_reg;
    assign max_next = (first_sample || (mag_sq_in > max_reg)) ? mag_sq_in : max_reg;

    assign range_next = {1'b0, max_next} - {1'b0, min_next};
    assign sum_next = {1'b0, max_next} + {1'b0, min_next};
    assign range_scaled = range_next * 100;
    assign threshold_scaled = sum_next * THRESHOLD_PERCENT;

    always @(posedge clk) begin
        if (!rst_n) begin
            count <= {COUNT_BITS{1'b0}};
            min_reg <= {MAG_BITS{1'b0}};
            max_reg <= {MAG_BITS{1'b0}};
            result_valid <= 1'b0;
            is_am <= 1'b0;
            block_min <= {MAG_BITS{1'b0}};
            block_max <= {MAG_BITS{1'b0}};
            range_out <= {(MAG_BITS+1){1'b0}};
            sum_out <= {(MAG_BITS+1){1'b0}};
        end else begin
            result_valid <= 1'b0;

            if (valid_in) begin
                min_reg <= min_next;
                max_reg <= max_next;

                if (last_sample) begin
                    result_valid <= 1'b1;
                    is_am <= (range_scaled >= threshold_scaled);
                    block_min <= min_next;
                    block_max <= max_next;
                    range_out <= range_next;
                    sum_out <= sum_next;
                    count <= {COUNT_BITS{1'b0}};
                end else begin
                    count <= count + 1'b1;
                end
            end
        end
    end

endmodule
