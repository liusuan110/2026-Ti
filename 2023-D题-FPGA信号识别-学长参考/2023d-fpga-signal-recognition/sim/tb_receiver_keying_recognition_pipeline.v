`timescale 1ns / 1ps

module tb_receiver_keying_recognition_pipeline;

    localparam integer SAMPLE_BITS = 8;
    localparam integer LO_BITS = 12;
    localparam integer DECIM = 80;
    localparam integer BLOCK_LEN = 1024;
    localparam integer DDC_SAMPLES_PER_CASE = BLOCK_LEN * 2 + 1;
    localparam integer RAW_SAMPLES_PER_CASE = DECIM * DDC_SAMPLES_PER_CASE;
    localparam integer CASE_COUNT = 9;
    localparam integer TOTAL_SAMPLES = RAW_SAMPLES_PER_CASE * CASE_COUNT;
    localparam integer SCORE_BITS = 10;

    reg clk = 1'b0;
    reg rst_n = 1'b0;
    reg valid_in = 1'b0;
    reg signed [SAMPLE_BITS-1:0] sample_in = 0;
    reg signed [LO_BITS-1:0] carrier_cos_in = 0;
    reg signed [LO_BITS-1:0] carrier_sin_in = 0;

    wire keying_valid;
    wire [2:0] keying_kind_code;
    wire [1:0] rc_index;
    wire [15:0] rc_hz;
    wire [15:0] freq_sep_hz;
    wire [7:0] h_tenths;
    wire ask_like;
    wire [SCORE_BITS-1:0] ask_best_score;
    wire [SCORE_BITS-1:0] psk_best_score;
    wire [SCORE_BITS-1:0] fsk_best_score;
    wire [SCORE_BITS-1:0] ask_transition_count;
    wire [SCORE_BITS-1:0] psk_transition_count;
    wire [SCORE_BITS-1:0] fsk_transition_count;
    wire [1:0] ask_rc_index;
    wire [1:0] psk_rc_index;
    wire [1:0] fsk_rc_index;

    reg signed [SAMPLE_BITS-1:0] sample_mem [0:TOTAL_SAMPLES-1];
    reg signed [LO_BITS-1:0] carrier_cos_mem [0:TOTAL_SAMPLES-1];
    reg signed [LO_BITS-1:0] carrier_sin_mem [0:TOTAL_SAMPLES-1];
    reg [2:0] exp_kind_code_mem [0:CASE_COUNT-1];
    reg [1:0] exp_rc_index_mem [0:CASE_COUNT-1];
    reg [15:0] exp_rc_hz_mem [0:CASE_COUNT-1];
    reg [15:0] exp_freq_sep_hz_mem [0:CASE_COUNT-1];
    reg [7:0] exp_h_tenths_mem [0:CASE_COUNT-1];

    integer case_idx;
    integer sample_idx;
    integer base_idx;
    integer wait_idx;
    integer fd;
    integer error_count;
    reg result_seen;
    reg [2:0] kind_code_captured;
    reg [1:0] rc_index_captured;
    reg [15:0] rc_hz_captured;
    reg [15:0] freq_sep_hz_captured;
    reg [7:0] h_tenths_captured;
    reg ask_like_captured;
    reg [SCORE_BITS-1:0] ask_best_score_captured;
    reg [SCORE_BITS-1:0] psk_best_score_captured;
    reg [SCORE_BITS-1:0] fsk_best_score_captured;
    reg [SCORE_BITS-1:0] ask_transition_count_captured;
    reg [SCORE_BITS-1:0] psk_transition_count_captured;
    reg [SCORE_BITS-1:0] fsk_transition_count_captured;
    reg [1:0] ask_rc_index_captured;
    reg [1:0] psk_rc_index_captured;
    reg [1:0] fsk_rc_index_captured;
    reg case_ok;
    reg fsk_ok;

    receiver_keying_recognition_pipeline #(
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
        .keying_valid(keying_valid),
        .keying_kind_code(keying_kind_code),
        .rc_index(rc_index),
        .rc_hz(rc_hz),
        .freq_sep_hz(freq_sep_hz),
        .h_tenths(h_tenths),
        .ask_like(ask_like),
        .ask_best_score(ask_best_score),
        .psk_best_score(psk_best_score),
        .fsk_best_score(fsk_best_score),
        .ask_transition_count(ask_transition_count),
        .psk_transition_count(psk_transition_count),
        .fsk_transition_count(fsk_transition_count),
        .ask_rc_index(ask_rc_index),
        .psk_rc_index(psk_rc_index),
        .fsk_rc_index(fsk_rc_index)
    );

    always #5 clk = ~clk;

    task capture_result;
        begin
            #1;
            if (keying_valid) begin
                result_seen = 1'b1;
                kind_code_captured = keying_kind_code;
                rc_index_captured = rc_index;
                rc_hz_captured = rc_hz;
                freq_sep_hz_captured = freq_sep_hz;
                h_tenths_captured = h_tenths;
                ask_like_captured = ask_like;
                ask_best_score_captured = ask_best_score;
                psk_best_score_captured = psk_best_score;
                fsk_best_score_captured = fsk_best_score;
                ask_transition_count_captured = ask_transition_count;
                psk_transition_count_captured = psk_transition_count;
                fsk_transition_count_captured = fsk_transition_count;
                ask_rc_index_captured = ask_rc_index;
                psk_rc_index_captured = psk_rc_index;
                fsk_rc_index_captured = fsk_rc_index;
            end
        end
    endtask

    initial begin
        $readmemh("sim/receiver_keying_recognition_sample.mem", sample_mem);
        $readmemh("sim/receiver_keying_recognition_carrier_cos.mem", carrier_cos_mem);
        $readmemh("sim/receiver_keying_recognition_carrier_sin.mem", carrier_sin_mem);
        $readmemh("sim/receiver_keying_recognition_expected_kind_code.mem", exp_kind_code_mem);
        $readmemh("sim/receiver_keying_recognition_expected_rc_index.mem", exp_rc_index_mem);
        $readmemh("sim/receiver_keying_recognition_expected_rc_hz.mem", exp_rc_hz_mem);
        $readmemh("sim/receiver_keying_recognition_expected_freq_sep_hz.mem", exp_freq_sep_hz_mem);
        $readmemh("sim/receiver_keying_recognition_expected_h_tenths.mem", exp_h_tenths_mem);

        fd = $fopen("outputs/receiver_keying_recognition/receiver_keying_recognition_sim.csv", "w");
        $fwrite(fd, "case,kind,expected_kind,rc_index,expected_rc_index,rc_hz,expected_rc_hz,freq_sep_hz,expected_freq_sep_hz,h_tenths,expected_h_tenths,ask_like,ask_score,psk_score,fsk_score,ask_transitions,psk_transitions,fsk_transitions,ask_rc_index,psk_rc_index,fsk_rc_index,result_seen,ok\n");
        error_count = 0;

        for (case_idx = 0; case_idx < CASE_COUNT; case_idx = case_idx + 1) begin
            rst_n = 1'b0;
            valid_in = 1'b0;
            sample_in = 0;
            carrier_cos_in = 0;
            carrier_sin_in = 0;
            result_seen = 1'b0;
            kind_code_captured = 3'd0;
            rc_index_captured = 2'd0;
            rc_hz_captured = 16'd0;
            freq_sep_hz_captured = 16'd0;
            h_tenths_captured = 8'd0;
            ask_like_captured = 1'b0;
            ask_best_score_captured = {SCORE_BITS{1'b0}};
            psk_best_score_captured = {SCORE_BITS{1'b0}};
            fsk_best_score_captured = {SCORE_BITS{1'b0}};
            ask_transition_count_captured = {SCORE_BITS{1'b0}};
            psk_transition_count_captured = {SCORE_BITS{1'b0}};
            fsk_transition_count_captured = {SCORE_BITS{1'b0}};
            ask_rc_index_captured = 2'd0;
            psk_rc_index_captured = 2'd0;
            fsk_rc_index_captured = 2'd0;
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

            for (wait_idx = 0; wait_idx < 96; wait_idx = wait_idx + 1) begin
                @(posedge clk);
                capture_result();
            end

            fsk_ok = (exp_kind_code_mem[case_idx] != 3'd6) ||
                     ((freq_sep_hz_captured === exp_freq_sep_hz_mem[case_idx]) &&
                      (h_tenths_captured === exp_h_tenths_mem[case_idx]));
            case_ok = result_seen &&
                      (kind_code_captured === exp_kind_code_mem[case_idx]) &&
                      (rc_index_captured === exp_rc_index_mem[case_idx]) &&
                      (rc_hz_captured === exp_rc_hz_mem[case_idx]) &&
                      fsk_ok;

            if (!case_ok) begin
                $display("ERROR[%0d]: receiver keying recognition mismatch", case_idx);
                error_count = error_count + 1;
            end

            $fwrite(fd, "%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d\n",
                case_idx,
                kind_code_captured,
                exp_kind_code_mem[case_idx],
                rc_index_captured,
                exp_rc_index_mem[case_idx],
                rc_hz_captured,
                exp_rc_hz_mem[case_idx],
                freq_sep_hz_captured,
                exp_freq_sep_hz_mem[case_idx],
                h_tenths_captured,
                exp_h_tenths_mem[case_idx],
                ask_like_captured,
                ask_best_score_captured,
                psk_best_score_captured,
                fsk_best_score_captured,
                ask_transition_count_captured,
                psk_transition_count_captured,
                fsk_transition_count_captured,
                ask_rc_index_captured,
                psk_rc_index_captured,
                fsk_rc_index_captured,
                result_seen,
                case_ok);
        end

        $fclose(fd);

        if (error_count == 0) begin
            $display("PASS: receiver_keying_recognition_pipeline");
        end else begin
            $display("FAIL: receiver_keying_recognition_pipeline errors=%0d", error_count);
        end

        $finish;
    end

endmodule
