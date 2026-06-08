`timescale 1ns / 1ps

module transition_rate_estimator #(
    parameter integer PHASE_BITS = 24,
    parameter integer BIN_BITS = 4,
    parameter integer BIN_COUNT = 16,
    parameter integer SCORE_BITS = 12,
    parameter integer BLOCK_LEN = 2048,
    parameter integer COUNT_BITS = 12,
    parameter [PHASE_BITS-1:0] STEP0 = 24'd402653,
    parameter [PHASE_BITS-1:0] STEP1 = 24'd536871,
    parameter [PHASE_BITS-1:0] STEP2 = 24'd671089
) (
    input wire clk,
    input wire rst_n,
    input wire valid_in,
    input wire bit_in,
    output reg result_valid,
    output reg [1:0] best_index,
    output reg [SCORE_BITS-1:0] score0,
    output reg [SCORE_BITS-1:0] score1,
    output reg [SCORE_BITS-1:0] score2,
    output reg [SCORE_BITS-1:0] transition_count
);

    localparam [COUNT_BITS-1:0] BLOCK_LAST = BLOCK_LEN - 1;

    reg [COUNT_BITS-1:0] sample_count;
    reg prev_valid;
    reg prev_bit;
    reg [PHASE_BITS-1:0] phase0;
    reg [PHASE_BITS-1:0] phase1;
    reg [PHASE_BITS-1:0] phase2;
    reg [SCORE_BITS-1:0] transition_count_acc;
    reg [SCORE_BITS-1:0] score0_acc;
    reg [SCORE_BITS-1:0] score1_acc;
    reg [SCORE_BITS-1:0] score2_acc;
    reg [SCORE_BITS-1:0] bins0 [0:BIN_COUNT-1];
    reg [SCORE_BITS-1:0] bins1 [0:BIN_COUNT-1];
    reg [SCORE_BITS-1:0] bins2 [0:BIN_COUNT-1];

    wire transition_w;
    wire last_sample_w;
    wire [PHASE_BITS-1:0] phase0_next;
    wire [PHASE_BITS-1:0] phase1_next;
    wire [PHASE_BITS-1:0] phase2_next;
    wire [BIN_BITS-1:0] bin0_w;
    wire [BIN_BITS-1:0] bin1_w;
    wire [BIN_BITS-1:0] bin2_w;
    wire [SCORE_BITS-1:0] transition_inc_w;
    wire [SCORE_BITS-1:0] transition_count_next;
    wire [SCORE_BITS-1:0] bin0_count_next;
    wire [SCORE_BITS-1:0] bin1_count_next;
    wire [SCORE_BITS-1:0] bin2_count_next;
    wire [SCORE_BITS-1:0] score0_next;
    wire [SCORE_BITS-1:0] score1_next;
    wire [SCORE_BITS-1:0] score2_next;
    wire [1:0] best_index_next;

    integer k;

    assign transition_w = prev_valid && (bit_in != prev_bit);
    assign last_sample_w = (sample_count == BLOCK_LAST);
    assign phase0_next = phase0 + STEP0;
    assign phase1_next = phase1 + STEP1;
    assign phase2_next = phase2 + STEP2;
    assign bin0_w = phase0_next[PHASE_BITS-1 -: BIN_BITS];
    assign bin1_w = phase1_next[PHASE_BITS-1 -: BIN_BITS];
    assign bin2_w = phase2_next[PHASE_BITS-1 -: BIN_BITS];
    assign transition_inc_w = {{(SCORE_BITS-1){1'b0}}, transition_w};
    assign transition_count_next = transition_count_acc + transition_inc_w;
    assign bin0_count_next = bins0[bin0_w] + transition_inc_w;
    assign bin1_count_next = bins1[bin1_w] + transition_inc_w;
    assign bin2_count_next = bins2[bin2_w] + transition_inc_w;
    assign score0_next = (transition_w && (bin0_count_next > score0_acc)) ? bin0_count_next : score0_acc;
    assign score1_next = (transition_w && (bin1_count_next > score1_acc)) ? bin1_count_next : score1_acc;
    assign score2_next = (transition_w && (bin2_count_next > score2_acc)) ? bin2_count_next : score2_acc;
    assign best_index_next = ((score2_next >= score1_next) && (score2_next >= score0_next)) ? 2'd2 :
                             (score1_next >= score0_next) ? 2'd1 :
                             2'd0;

    always @(posedge clk) begin
        if (!rst_n) begin
            sample_count <= {COUNT_BITS{1'b0}};
            prev_valid <= 1'b0;
            prev_bit <= 1'b0;
            phase0 <= {PHASE_BITS{1'b0}};
            phase1 <= {PHASE_BITS{1'b0}};
            phase2 <= {PHASE_BITS{1'b0}};
            transition_count_acc <= {SCORE_BITS{1'b0}};
            score0_acc <= {SCORE_BITS{1'b0}};
            score1_acc <= {SCORE_BITS{1'b0}};
            score2_acc <= {SCORE_BITS{1'b0}};
            result_valid <= 1'b0;
            best_index <= 2'd0;
            score0 <= {SCORE_BITS{1'b0}};
            score1 <= {SCORE_BITS{1'b0}};
            score2 <= {SCORE_BITS{1'b0}};
            transition_count <= {SCORE_BITS{1'b0}};
            for (k = 0; k < BIN_COUNT; k = k + 1) begin
                bins0[k] <= {SCORE_BITS{1'b0}};
                bins1[k] <= {SCORE_BITS{1'b0}};
                bins2[k] <= {SCORE_BITS{1'b0}};
            end
        end else begin
            result_valid <= 1'b0;

            if (valid_in) begin
                if (last_sample_w) begin
                    result_valid <= 1'b1;
                    score0 <= score0_next;
                    score1 <= score1_next;
                    score2 <= score2_next;
                    transition_count <= transition_count_next;
                    best_index <= best_index_next;

                    sample_count <= {COUNT_BITS{1'b0}};
                    prev_valid <= 1'b0;
                    prev_bit <= 1'b0;
                    phase0 <= {PHASE_BITS{1'b0}};
                    phase1 <= {PHASE_BITS{1'b0}};
                    phase2 <= {PHASE_BITS{1'b0}};
                    transition_count_acc <= {SCORE_BITS{1'b0}};
                    score0_acc <= {SCORE_BITS{1'b0}};
                    score1_acc <= {SCORE_BITS{1'b0}};
                    score2_acc <= {SCORE_BITS{1'b0}};
                    for (k = 0; k < BIN_COUNT; k = k + 1) begin
                        bins0[k] <= {SCORE_BITS{1'b0}};
                        bins1[k] <= {SCORE_BITS{1'b0}};
                        bins2[k] <= {SCORE_BITS{1'b0}};
                    end
                end else begin
                    sample_count <= sample_count + 1'b1;
                    prev_valid <= 1'b1;
                    prev_bit <= bit_in;
                    phase0 <= phase0_next;
                    phase1 <= phase1_next;
                    phase2 <= phase2_next;

                    if (transition_w) begin
                        bins0[bin0_w] <= bins0[bin0_w] + 1'b1;
                        bins1[bin1_w] <= bins1[bin1_w] + 1'b1;
                        bins2[bin2_w] <= bins2[bin2_w] + 1'b1;
                        transition_count_acc <= transition_count_next;
                        score0_acc <= score0_next;
                        score1_acc <= score1_next;
                        score2_acc <= score2_next;
                    end
                end
            end
        end
    end

endmodule
