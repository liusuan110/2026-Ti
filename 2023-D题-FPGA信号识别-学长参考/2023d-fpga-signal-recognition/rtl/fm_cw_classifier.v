`timescale 1ns / 1ps

module fm_cw_classifier #(
    parameter integer PHASE_BITS = 24,
    parameter integer BLOCK_LEN = 40000,
    parameter integer COUNT_BITS = 16,
    // Default assumes phase is Q4.20 radians and threshold range is about 0.9 rad.
    parameter [PHASE_BITS:0] PHASE_RANGE_THRESHOLD = 25'd943718
) (
    input wire clk,
    input wire rst_n,
    input wire valid_in,
    input wire signed [PHASE_BITS-1:0] phase_in,
    output reg result_valid,
    output reg is_fm,
    output reg signed [PHASE_BITS-1:0] phase_min,
    output reg signed [PHASE_BITS-1:0] phase_max,
    output reg [PHASE_BITS:0] phase_range
);

    localparam [COUNT_BITS-1:0] BLOCK_LAST = BLOCK_LEN - 1;

    reg [COUNT_BITS-1:0] count;
    reg signed [PHASE_BITS-1:0] min_reg;
    reg signed [PHASE_BITS-1:0] max_reg;

    wire first_sample;
    wire last_sample;
    wire signed [PHASE_BITS-1:0] min_next;
    wire signed [PHASE_BITS-1:0] max_next;
    wire [PHASE_BITS:0] range_next;

    assign first_sample = (count == {COUNT_BITS{1'b0}});
    assign last_sample = (count == BLOCK_LAST);
    assign min_next = (first_sample || (phase_in < min_reg)) ? phase_in : min_reg;
    assign max_next = (first_sample || (phase_in > max_reg)) ? phase_in : max_reg;
    assign range_next = {max_next[PHASE_BITS-1], max_next} - {min_next[PHASE_BITS-1], min_next};

    always @(posedge clk) begin
        if (!rst_n) begin
            count <= {COUNT_BITS{1'b0}};
            min_reg <= {PHASE_BITS{1'b0}};
            max_reg <= {PHASE_BITS{1'b0}};
            result_valid <= 1'b0;
            is_fm <= 1'b0;
            phase_min <= {PHASE_BITS{1'b0}};
            phase_max <= {PHASE_BITS{1'b0}};
            phase_range <= {(PHASE_BITS+1){1'b0}};
        end else begin
            result_valid <= 1'b0;

            if (valid_in) begin
                min_reg <= min_next;
                max_reg <= max_next;

                if (last_sample) begin
                    result_valid <= 1'b1;
                    is_fm <= (range_next >= PHASE_RANGE_THRESHOLD);
                    phase_min <= min_next;
                    phase_max <= max_next;
                    phase_range <= range_next;
                    count <= {COUNT_BITS{1'b0}};
                end else begin
                    count <= count + 1'b1;
                end
            end
        end
    end

endmodule
