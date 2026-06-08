`timescale 1ns / 1ps

module tb_receiver_psk_rate_pipeline;

    localparam integer SAMPLE_BITS = 8;
    localparam integer LO_BITS = 12;
    localparam integer DECIM = 80;
    localparam integer BLOCK_LEN = 1024;
    localparam integer RAW_SAMPLES_PER_CASE = DECIM * BLOCK_LEN;
    localparam integer CASE_COUNT = 6;
    localparam integer TOTAL_SAMPLES = RAW_SAMPLES_PER_CASE * CASE_COUNT;
    localparam integer SCORE_BITS = 10;

    reg clk = 1'b0;
    reg rst_n = 1'b0;
    reg valid_in = 1'b0;
    reg signed [SAMPLE_BITS-1:0] sample_in = 0;
    reg signed [LO_BITS-1:0] carrier_cos_in = 0;
    reg signed [LO_BITS-1:0] carrier_sin_in = 0;

    wire psk_rate_valid;
    wire [1:0] rc_index;
    wire [15:0] rc_hz;
    wire [SCORE_BITS-1:0] score0;
    wire [SCORE_BITS-1:0] score1;
    wire [SCORE_BITS-1:0] score2;
    wire [SCORE_BITS-1:0] transition_count;

    reg signed [SAMPLE_BITS-1:0] sample_mem [0:TOTAL_SAMPLES-1];
    reg signed [LO_BITS-1:0] carrier_cos_mem [0:TOTAL_SAMPLES-1];
    reg signed [LO_BITS-1:0] carrier_sin_mem [0:TOTAL_SAMPLES-1];
    reg [1:0] exp_rc_index_mem [0:CASE_COUNT-1];
    reg [15:0] exp_rc_hz_mem [0:CASE_COUNT-1];
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
    reg [1:0] rc_index_captured;
    reg [15:0] rc_hz_captured;
    reg [SCORE_BITS-1:0] score0_captured;
    reg [SCORE_BITS-1:0] score1_captured;
    reg [SCORE_BITS-1:0] score2_captured;
    reg [SCORE_BITS-1:0] transition_count_captured;
    reg case_ok;

    receiver_psk_rate_pipeline #(
        .SAMPLE_BITS(SAMPLE_BITS),
        .LO_BITS(LO_BITS),
        .DECIM(DECIM),
        .DECIM_COUNT_BITS(7),
        .BLOCK_LEN(BLOCK_LEN),
        .BLOCK_COUNT_BITS(10),
        .SCORE_BITS(SCORE_BITS)
    ) dut (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(valid_in),
        .sample_in(sample_in),
        .carrier_cos_in(carrier_cos_in),
        .carrier_sin_in(carrier_sin_in),
        .psk_rate_valid(psk_rate_valid),
        .rc_index(rc_index),
        .rc_hz(rc_hz),
        .score0(score0),
        .score1(score1),
        .score2(score2),
        .transition_count(transition_count),
        .bit_valid_debug(),
        .bit_debug(),
        .phase_flip_event_debug()
    );

    always #5 clk = ~clk;

    task capture_result;
        begin
            #1;
            if (psk_rate_valid) begin
                result_seen = 1'b1;
                rc_index_captured = rc_index;
                rc_hz_captured = rc_hz;
                score0_captured = score0;
                score1_captured = score1;
                score2_captured = score2;
                transition_count_captured = transition_count;
            end
        end
    endtask

    initial begin
        $readmemh("sim/receiver_psk_rate_sample.mem", sample_mem);
        $readmemh("sim/receiver_psk_rate_carrier_cos.mem", carrier_cos_mem);
        $readmemh("sim/receiver_psk_rate_carrier_sin.mem", carrier_sin_mem);
        $readmemh("sim/receiver_psk_rate_expected_rc_index.mem", exp_rc_index_mem);
        $readmemh("sim/receiver_psk_rate_expected_rc_hz.mem", exp_rc_hz_mem);
        $readmemh("sim/receiver_psk_rate_score0.mem", exp_score0_mem);
        $readmemh("sim/receiver_psk_rate_score1.mem", exp_score1_mem);
        $readmemh("sim/receiver_psk_rate_score2.mem", exp_score2_mem);
        $readmemh("sim/receiver_psk_rate_transition_count.mem", exp_transition_count_mem);

        fd = $fopen("outputs/receiver_psk_rate/receiver_psk_rate_sim.csv", "w");
        $fwrite(fd, "case,rc_index,expected_rc_index,rc_hz,expected_rc_hz,score0,expected_score0,score1,expected_score1,score2,expected_score2,transition_count,expected_transition_count,result_seen,ok\n");
        error_count = 0;

        for (case_idx = 0; case_idx < CASE_COUNT; case_idx = case_idx + 1) begin
            rst_n = 1'b0;
            valid_in = 1'b0;
            sample_in = 0;
            carrier_cos_in = 0;
            carrier_sin_in = 0;
            result_seen = 1'b0;
            rc_index_captured = 2'd0;
            rc_hz_captured = 16'd0;
            score0_captured = {SCORE_BITS{1'b0}};
            score1_captured = {SCORE_BITS{1'b0}};
            score2_captured = {SCORE_BITS{1'b0}};
            transition_count_captured = {SCORE_BITS{1'b0}};
            repeat (4) @(posedge clk);
            rst_n = 1'b1;
            repeat (2) @(posedge clk);

            base_idx = case_idx * RAW_SAMPLES_PER_CASE;
            for (sample_idx = 0; sample_idx < RAW_SAMPLES_PER_CASE; sample_idx = sample_idx + 1) begin
                @(negedge clk);
                valid_in = 1'b1;
                sample_in = sample_mem[base_idx + sample_idx];
                carrier_cos_in = carrier_cos_mem[base_idx + sample_idx];
                carrier_sin_in = carrier_sin_mem[base_idx + sample_idx];
                @(posedge clk);
                capture_result();
            end

            @(negedge clk);
            valid_in = 1'b0;
            sample_in = 0;
            carrier_cos_in = 0;
            carrier_sin_in = 0;

            for (wait_idx = 0; wait_idx < 24; wait_idx = wait_idx + 1) begin
                @(posedge clk);
                capture_result();
            end

            case_ok = result_seen &&
                      (rc_index_captured === exp_rc_index_mem[case_idx]) &&
                      (rc_hz_captured === exp_rc_hz_mem[case_idx]) &&
                      (score0_captured === exp_score0_mem[case_idx]) &&
                      (score1_captured === exp_score1_mem[case_idx]) &&
                      (score2_captured === exp_score2_mem[case_idx]) &&
                      (transition_count_captured === exp_transition_count_mem[case_idx]);

            if (!case_ok) begin
                $display("ERROR[%0d]: receiver PSK rate mismatch", case_idx);
                error_count = error_count + 1;
            end

            $fwrite(fd, "%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d\n",
                case_idx,
                rc_index_captured,
                exp_rc_index_mem[case_idx],
                rc_hz_captured,
                exp_rc_hz_mem[case_idx],
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
            $display("PASS: receiver_psk_rate_pipeline");
        end else begin
            $display("FAIL: receiver_psk_rate_pipeline errors=%0d", error_count);
        end

        $finish;
    end

endmodule
