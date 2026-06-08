`timescale 1ns / 1ps

module feature_coarse_classifier #(
    parameter integer MAG_BITS = 57,
    parameter integer COUNT_BITS = 6,
    parameter integer MAG_SUM_BITS = MAG_BITS + COUNT_BITS,
    parameter integer CROSS_SUM_BITS = 64,
    parameter integer BLOCK_LEN = 64,
    parameter integer MAG_RANGE_PERCENT = 25,
    parameter integer ASK_MINMAX_PERCENT = 8,
    parameter integer PHASE_ACTIVITY_PERCENT = 8,
    parameter integer NEG_DOT_THRESHOLD = 1
) (
    input wire clk,
    input wire rst_n,
    input wire stats_valid,
    input wire [MAG_BITS-1:0] mag_min,
    input wire [MAG_BITS-1:0] mag_max,
    input wire [MAG_SUM_BITS-1:0] mag_sum,
    input wire [CROSS_SUM_BITS-1:0] abs_cross_sum,
    input wire [COUNT_BITS:0] negative_dot_count,
    output reg result_valid,
    output reg mag_active,
    output reg ask_like,
    output reg phase_active,
    output reg phase_flip_active,
    output reg [2:0] coarse_flags
);

    localparam integer RANGE_SCALE_BITS = MAG_BITS + COUNT_BITS + 7;
    localparam integer SUM_SCALE_BITS = MAG_SUM_BITS + 7;
    localparam integer CROSS_SCALE_BITS = CROSS_SUM_BITS + 7;
    localparam integer MINMAX_SCALE_BITS = MAG_BITS + 7;
    localparam integer PHASE_COMP_BITS = (CROSS_SCALE_BITS > SUM_SCALE_BITS) ? CROSS_SCALE_BITS : SUM_SCALE_BITS;
    localparam [COUNT_BITS:0] NEG_DOT_THRESHOLD_U = NEG_DOT_THRESHOLD;

    wire [MAG_BITS-1:0] mag_range;
    wire [RANGE_SCALE_BITS-1:0] mag_range_scaled;
    wire [SUM_SCALE_BITS-1:0] mag_activity_threshold;
    wire [CROSS_SCALE_BITS-1:0] cross_scaled;
    wire [SUM_SCALE_BITS-1:0] phase_activity_threshold;
    wire [PHASE_COMP_BITS-1:0] cross_scaled_cmp;
    wire [PHASE_COMP_BITS-1:0] phase_activity_threshold_cmp;
    wire [MINMAX_SCALE_BITS-1:0] mag_min_scaled;
    wire [MINMAX_SCALE_BITS-1:0] ask_threshold;

    wire mag_active_w;
    wire ask_like_w;
    wire phase_active_w;
    wire phase_flip_active_w;

    assign mag_range = mag_max - mag_min;
    assign mag_range_scaled = mag_range * (BLOCK_LEN * 100);
    assign mag_activity_threshold = mag_sum * MAG_RANGE_PERCENT;
    assign cross_scaled = abs_cross_sum * 100;
    assign phase_activity_threshold = mag_sum * PHASE_ACTIVITY_PERCENT;
    assign cross_scaled_cmp = {{(PHASE_COMP_BITS-CROSS_SCALE_BITS){1'b0}}, cross_scaled};
    assign phase_activity_threshold_cmp = {{(PHASE_COMP_BITS-SUM_SCALE_BITS){1'b0}}, phase_activity_threshold};
    assign mag_min_scaled = mag_min * 100;
    assign ask_threshold = mag_max * ASK_MINMAX_PERCENT;

    assign mag_active_w = (mag_range_scaled >= {{(RANGE_SCALE_BITS-SUM_SCALE_BITS){1'b0}}, mag_activity_threshold});
    assign ask_like_w = mag_active_w && (mag_min_scaled <= ask_threshold);
    assign phase_active_w = (cross_scaled_cmp >= phase_activity_threshold_cmp);
    assign phase_flip_active_w = (negative_dot_count >= NEG_DOT_THRESHOLD_U);

    always @(posedge clk) begin
        if (!rst_n) begin
            result_valid <= 1'b0;
            mag_active <= 1'b0;
            ask_like <= 1'b0;
            phase_active <= 1'b0;
            phase_flip_active <= 1'b0;
            coarse_flags <= 3'b000;
        end else begin
            result_valid <= stats_valid;

            if (stats_valid) begin
                mag_active <= mag_active_w;
                ask_like <= ask_like_w;
                phase_active <= phase_active_w;
                phase_flip_active <= phase_flip_active_w;
                coarse_flags <= {phase_flip_active_w, phase_active_w, mag_active_w};
            end
        end
    end

endmodule
