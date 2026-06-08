`timescale 1ns / 1ps

module phase_slope_mean_estimator #(
    parameter integer PHASE_BITS = 40,
    parameter integer WINDOW_SHIFT = 13,
    parameter integer AVG_SHIFT = 4,
    parameter integer COUNT_BITS = 14,
    parameter integer SUM_BITS = PHASE_BITS + AVG_SHIFT
) (
    input wire clk,
    input wire rst_n,
    input wire valid_in,
    input wire signed [PHASE_BITS-1:0] phase_unwrapped_in,
    output reg estimate_valid,
    output reg signed [SUM_BITS-1:0] start_sum_debug,
    output reg signed [SUM_BITS-1:0] end_sum_debug,
    output reg signed [SUM_BITS:0] delta_sum_out,
    output reg signed [PHASE_BITS-1:0] slope_per_sample_out,
    output reg [COUNT_BITS-1:0] block_sample_count_debug
);

    localparam [COUNT_BITS-1:0] AVG_COUNT = (1 << AVG_SHIFT);
    localparam [COUNT_BITS-1:0] END_FIRST = (1 << WINDOW_SHIFT);
    localparam [COUNT_BITS-1:0] END_LAST = (1 << WINDOW_SHIFT) + (1 << AVG_SHIFT) - 1;

    reg [COUNT_BITS-1:0] sample_count;
    reg signed [SUM_BITS-1:0] start_sum;
    reg signed [SUM_BITS-1:0] end_sum;

    wire in_start_region;
    wire in_end_region;
    wire signed [SUM_BITS-1:0] phase_ext;
    wire signed [SUM_BITS-1:0] start_sum_next;
    wire signed [SUM_BITS-1:0] end_sum_next;
    wire signed [SUM_BITS:0] delta_sum_next;
    wire signed [SUM_BITS:0] slope_full;

    assign in_start_region = sample_count < AVG_COUNT;
    assign in_end_region = (sample_count >= END_FIRST) && (sample_count <= END_LAST);
    assign phase_ext = {{(SUM_BITS-PHASE_BITS){phase_unwrapped_in[PHASE_BITS-1]}}, phase_unwrapped_in};
    assign start_sum_next = in_start_region ? (start_sum + phase_ext) : start_sum;
    assign end_sum_next = in_end_region ? (end_sum + phase_ext) : end_sum;
    assign delta_sum_next = {end_sum_next[SUM_BITS-1], end_sum_next} - {start_sum_next[SUM_BITS-1], start_sum_next};
    assign slope_full = delta_sum_next >>> (WINDOW_SHIFT + AVG_SHIFT);

    always @(posedge clk) begin
        if (!rst_n) begin
            sample_count <= {COUNT_BITS{1'b0}};
            start_sum <= {SUM_BITS{1'b0}};
            end_sum <= {SUM_BITS{1'b0}};
            estimate_valid <= 1'b0;
            start_sum_debug <= {SUM_BITS{1'b0}};
            end_sum_debug <= {SUM_BITS{1'b0}};
            delta_sum_out <= {(SUM_BITS+1){1'b0}};
            slope_per_sample_out <= {PHASE_BITS{1'b0}};
            block_sample_count_debug <= {COUNT_BITS{1'b0}};
        end else begin
            estimate_valid <= 1'b0;

            if (valid_in) begin
                if (sample_count == END_LAST) begin
                    start_sum_debug <= start_sum_next;
                    end_sum_debug <= end_sum_next;
                    delta_sum_out <= delta_sum_next;
                    slope_per_sample_out <= slope_full[PHASE_BITS-1:0];
                    block_sample_count_debug <= END_LAST + 1'b1;
                    estimate_valid <= 1'b1;

                    sample_count <= {COUNT_BITS{1'b0}};
                    start_sum <= {SUM_BITS{1'b0}};
                    end_sum <= {SUM_BITS{1'b0}};
                end else begin
                    sample_count <= sample_count + 1'b1;
                    start_sum <= start_sum_next;
                    end_sum <= end_sum_next;
                end
            end
        end
    end

endmodule
