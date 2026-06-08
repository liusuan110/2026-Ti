`timescale 1ns / 1ps

module tb_transition_rate_estimator;

    localparam integer BLOCK_LEN = 2048;
    localparam integer CASE_COUNT = 12;
    localparam integer TOTAL_SAMPLES = BLOCK_LEN * CASE_COUNT;
    localparam integer SCORE_BITS = 12;

    reg clk = 1'b0;
    reg rst_n = 1'b0;
    reg valid_in = 1'b0;
    reg bit_in = 1'b0;

    wire result_valid;
    wire [1:0] best_index;
    wire [SCORE_BITS-1:0] score0;
    wire [SCORE_BITS-1:0] score1;
    wire [SCORE_BITS-1:0] score2;
    wire [SCORE_BITS-1:0] transition_count;

    reg bit_mem [0:TOTAL_SAMPLES-1];
    reg [1:0] exp_best_mem [0:CASE_COUNT-1];
    reg [SCORE_BITS-1:0] exp_score0_mem [0:CASE_COUNT-1];
    reg [SCORE_BITS-1:0] exp_score1_mem [0:CASE_COUNT-1];
    reg [SCORE_BITS-1:0] exp_score2_mem [0:CASE_COUNT-1];
    reg [SCORE_BITS-1:0] exp_transition_count_mem [0:CASE_COUNT-1];

    integer case_idx;
    integer sample_idx;
    integer base_idx;
    integer wait_idx;
    integer fd;
    integer error_count;
    reg result_seen;
    reg [1:0] best_captured;
    reg [SCORE_BITS-1:0] score0_captured;
    reg [SCORE_BITS-1:0] score1_captured;
    reg [SCORE_BITS-1:0] score2_captured;
    reg [SCORE_BITS-1:0] transition_count_captured;
    reg case_ok;

    transition_rate_estimator #(
        .BLOCK_LEN(BLOCK_LEN),
        .COUNT_BITS(12),
        .SCORE_BITS(SCORE_BITS)
    ) dut (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(valid_in),
        .bit_in(bit_in),
        .result_valid(result_valid),
        .best_index(best_index),
        .score0(score0),
        .score1(score1),
        .score2(score2),
        .transition_count(transition_count)
    );

    always #5 clk = ~clk;

    task capture_result;
        begin
            #1;
            if (result_valid) begin
                result_seen = 1'b1;
                best_captured = best_index;
                score0_captured = score0;
                score1_captured = score1;
                score2_captured = score2;
                transition_count_captured = transition_count;
            end
        end
    endtask

    initial begin
        $readmemb("sim/transition_rate_bits.mem", bit_mem);
        $readmemh("sim/transition_rate_expected_best_index.mem", exp_best_mem);
        $readmemh("sim/transition_rate_score0.mem", exp_score0_mem);
        $readmemh("sim/transition_rate_score1.mem", exp_score1_mem);
        $readmemh("sim/transition_rate_score2.mem", exp_score2_mem);
        $readmemh("sim/transition_rate_transition_count.mem", exp_transition_count_mem);

        fd = $fopen("outputs/transition_rate/transition_rate_sim.csv", "w");
        $fwrite(fd, "case,best,expected_best,score0,expected_score0,score1,expected_score1,score2,expected_score2,transition_count,expected_transition_count,result_seen,ok\n");
        error_count = 0;

        for (case_idx = 0; case_idx < CASE_COUNT; case_idx = case_idx + 1) begin
            rst_n = 1'b0;
            valid_in = 1'b0;
            bit_in = 1'b0;
            result_seen = 1'b0;
            best_captured = 2'd0;
            score0_captured = {SCORE_BITS{1'b0}};
            score1_captured = {SCORE_BITS{1'b0}};
            score2_captured = {SCORE_BITS{1'b0}};
            transition_count_captured = {SCORE_BITS{1'b0}};
            repeat (4) @(posedge clk);
            rst_n = 1'b1;
            repeat (2) @(posedge clk);

            base_idx = case_idx * BLOCK_LEN;
            for (sample_idx = 0; sample_idx < BLOCK_LEN; sample_idx = sample_idx + 1) begin
                @(negedge clk);
                valid_in = 1'b1;
                bit_in = bit_mem[base_idx + sample_idx];
                @(posedge clk);
                capture_result();
            end

            @(negedge clk);
            valid_in = 1'b0;
            bit_in = 1'b0;

            for (wait_idx = 0; wait_idx < 8; wait_idx = wait_idx + 1) begin
                @(posedge clk);
                capture_result();
            end

            case_ok = result_seen &&
                      (best_captured === exp_best_mem[case_idx]) &&
                      (score0_captured === exp_score0_mem[case_idx]) &&
                      (score1_captured === exp_score1_mem[case_idx]) &&
                      (score2_captured === exp_score2_mem[case_idx]) &&
                      (transition_count_captured === exp_transition_count_mem[case_idx]);

            if (!case_ok) begin
                $display("ERROR[%0d]: transition rate mismatch", case_idx);
                error_count = error_count + 1;
            end

            $fwrite(fd, "%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d\n",
                case_idx,
                best_captured,
                exp_best_mem[case_idx],
                score0_captured,
                exp_score0_mem[case_idx],
                score1_captured,
                exp_score1_mem[case_idx],
                score2_captured,
                exp_score2_mem[case_idx],
                transition_count_captured,
                exp_transition_count_mem[case_idx],
                result_seen,
                case_ok);
        end

        $fclose(fd);

        if (error_count == 0) begin
            $display("PASS: transition_rate_estimator");
        end else begin
            $display("FAIL: transition_rate_estimator errors=%0d", error_count);
        end

        $finish;
    end

endmodule
