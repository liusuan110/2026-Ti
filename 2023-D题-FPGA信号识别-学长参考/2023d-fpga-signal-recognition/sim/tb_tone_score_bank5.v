`timescale 1ns / 1ps

module tb_tone_score_bank5;

    localparam integer DATA_BITS = 18;
    localparam integer REF_BITS = 12;
    localparam integer BLOCK_LEN = 250;
    localparam integer COUNT_BITS = 8;
    localparam integer PRODUCT_BITS = DATA_BITS + REF_BITS;
    localparam integer ACC_BITS = PRODUCT_BITS + COUNT_BITS;
    localparam integer SCORE_BITS = 2 * ACC_BITS + 1;
    localparam integer CASE_COUNT = 5;
    localparam integer TOTAL_SAMPLES = BLOCK_LEN * CASE_COUNT;

    reg clk = 1'b0;
    reg rst_n = 1'b0;
    reg valid_in = 1'b0;
    reg signed [DATA_BITS-1:0] data_in = 0;
    reg signed [REF_BITS-1:0] ref0_cos = 0;
    reg signed [REF_BITS-1:0] ref0_sin = 0;
    reg signed [REF_BITS-1:0] ref1_cos = 0;
    reg signed [REF_BITS-1:0] ref1_sin = 0;
    reg signed [REF_BITS-1:0] ref2_cos = 0;
    reg signed [REF_BITS-1:0] ref2_sin = 0;
    reg signed [REF_BITS-1:0] ref3_cos = 0;
    reg signed [REF_BITS-1:0] ref3_sin = 0;
    reg signed [REF_BITS-1:0] ref4_cos = 0;
    reg signed [REF_BITS-1:0] ref4_sin = 0;

    wire result_valid;
    wire [2:0] best_index;
    wire [SCORE_BITS-1:0] best_score_power;
    wire [SCORE_BITS-1:0] score0_power;
    wire [SCORE_BITS-1:0] score1_power;
    wire [SCORE_BITS-1:0] score2_power;
    wire [SCORE_BITS-1:0] score3_power;
    wire [SCORE_BITS-1:0] score4_power;

    reg signed [DATA_BITS-1:0] data_mem [0:TOTAL_SAMPLES-1];
    reg signed [REF_BITS-1:0] ref0_cos_mem [0:TOTAL_SAMPLES-1];
    reg signed [REF_BITS-1:0] ref0_sin_mem [0:TOTAL_SAMPLES-1];
    reg signed [REF_BITS-1:0] ref1_cos_mem [0:TOTAL_SAMPLES-1];
    reg signed [REF_BITS-1:0] ref1_sin_mem [0:TOTAL_SAMPLES-1];
    reg signed [REF_BITS-1:0] ref2_cos_mem [0:TOTAL_SAMPLES-1];
    reg signed [REF_BITS-1:0] ref2_sin_mem [0:TOTAL_SAMPLES-1];
    reg signed [REF_BITS-1:0] ref3_cos_mem [0:TOTAL_SAMPLES-1];
    reg signed [REF_BITS-1:0] ref3_sin_mem [0:TOTAL_SAMPLES-1];
    reg signed [REF_BITS-1:0] ref4_cos_mem [0:TOTAL_SAMPLES-1];
    reg signed [REF_BITS-1:0] ref4_sin_mem [0:TOTAL_SAMPLES-1];

    reg [2:0] exp_best_index_mem [0:CASE_COUNT-1];
    reg [SCORE_BITS-1:0] exp_score0_power_mem [0:CASE_COUNT-1];
    reg [SCORE_BITS-1:0] exp_score1_power_mem [0:CASE_COUNT-1];
    reg [SCORE_BITS-1:0] exp_score2_power_mem [0:CASE_COUNT-1];
    reg [SCORE_BITS-1:0] exp_score3_power_mem [0:CASE_COUNT-1];
    reg [SCORE_BITS-1:0] exp_score4_power_mem [0:CASE_COUNT-1];

    integer case_idx;
    integer sample_idx;
    integer base_idx;
    integer fd;
    integer error_count;
    reg case_ok;

    tone_score_bank5 #(
        .DATA_BITS(DATA_BITS),
        .REF_BITS(REF_BITS),
        .BLOCK_LEN(BLOCK_LEN),
        .COUNT_BITS(COUNT_BITS),
        .PRODUCT_BITS(PRODUCT_BITS),
        .ACC_BITS(ACC_BITS),
        .SCORE_BITS(SCORE_BITS)
    ) dut (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(valid_in),
        .data_in(data_in),
        .ref0_cos(ref0_cos),
        .ref0_sin(ref0_sin),
        .ref1_cos(ref1_cos),
        .ref1_sin(ref1_sin),
        .ref2_cos(ref2_cos),
        .ref2_sin(ref2_sin),
        .ref3_cos(ref3_cos),
        .ref3_sin(ref3_sin),
        .ref4_cos(ref4_cos),
        .ref4_sin(ref4_sin),
        .result_valid(result_valid),
        .best_index(best_index),
        .best_score_power(best_score_power),
        .score0_power(score0_power),
        .score1_power(score1_power),
        .score2_power(score2_power),
        .score3_power(score3_power),
        .score4_power(score4_power)
    );

    always #5 clk = ~clk;

    initial begin
        $readmemh("sim/tone_bank_data.mem", data_mem);
        $readmemh("sim/tone_bank_ref0_cos.mem", ref0_cos_mem);
        $readmemh("sim/tone_bank_ref0_sin.mem", ref0_sin_mem);
        $readmemh("sim/tone_bank_ref1_cos.mem", ref1_cos_mem);
        $readmemh("sim/tone_bank_ref1_sin.mem", ref1_sin_mem);
        $readmemh("sim/tone_bank_ref2_cos.mem", ref2_cos_mem);
        $readmemh("sim/tone_bank_ref2_sin.mem", ref2_sin_mem);
        $readmemh("sim/tone_bank_ref3_cos.mem", ref3_cos_mem);
        $readmemh("sim/tone_bank_ref3_sin.mem", ref3_sin_mem);
        $readmemh("sim/tone_bank_ref4_cos.mem", ref4_cos_mem);
        $readmemh("sim/tone_bank_ref4_sin.mem", ref4_sin_mem);
        $readmemh("sim/tone_bank_exp_best_index.mem", exp_best_index_mem);
        $readmemh("sim/tone_bank_exp_score0_power.mem", exp_score0_power_mem);
        $readmemh("sim/tone_bank_exp_score1_power.mem", exp_score1_power_mem);
        $readmemh("sim/tone_bank_exp_score2_power.mem", exp_score2_power_mem);
        $readmemh("sim/tone_bank_exp_score3_power.mem", exp_score3_power_mem);
        $readmemh("sim/tone_bank_exp_score4_power.mem", exp_score4_power_mem);

        fd = $fopen("outputs/tone_score_bank5/tone_score_bank5_sim.csv", "w");
        $fwrite(fd, "case,best_index,expected_best_index,score0_ok,score1_ok,score2_ok,score3_ok,score4_ok,ok\n");
        error_count = 0;

        for (case_idx = 0; case_idx < CASE_COUNT; case_idx = case_idx + 1) begin
            rst_n = 1'b0;
            valid_in = 1'b0;
            data_in = 0;
            ref0_cos = 0;
            ref0_sin = 0;
            ref1_cos = 0;
            ref1_sin = 0;
            ref2_cos = 0;
            ref2_sin = 0;
            ref3_cos = 0;
            ref3_sin = 0;
            ref4_cos = 0;
            ref4_sin = 0;
            repeat (4) @(posedge clk);
            rst_n = 1'b1;
            repeat (2) @(posedge clk);

            base_idx = case_idx * BLOCK_LEN;
            for (sample_idx = 0; sample_idx < BLOCK_LEN; sample_idx = sample_idx + 1) begin
                @(negedge clk);
                valid_in = 1'b1;
                data_in = data_mem[base_idx + sample_idx];
                ref0_cos = ref0_cos_mem[base_idx + sample_idx];
                ref0_sin = ref0_sin_mem[base_idx + sample_idx];
                ref1_cos = ref1_cos_mem[base_idx + sample_idx];
                ref1_sin = ref1_sin_mem[base_idx + sample_idx];
                ref2_cos = ref2_cos_mem[base_idx + sample_idx];
                ref2_sin = ref2_sin_mem[base_idx + sample_idx];
                ref3_cos = ref3_cos_mem[base_idx + sample_idx];
                ref3_sin = ref3_sin_mem[base_idx + sample_idx];
                ref4_cos = ref4_cos_mem[base_idx + sample_idx];
                ref4_sin = ref4_sin_mem[base_idx + sample_idx];
                @(posedge clk);
                #1;
            end

            @(posedge clk);
            #1;

            case_ok = result_valid &&
                      (best_index === exp_best_index_mem[case_idx]) &&
                      (score0_power === exp_score0_power_mem[case_idx]) &&
                      (score1_power === exp_score1_power_mem[case_idx]) &&
                      (score2_power === exp_score2_power_mem[case_idx]) &&
                      (score3_power === exp_score3_power_mem[case_idx]) &&
                      (score4_power === exp_score4_power_mem[case_idx]);

            if (!case_ok) begin
                $display("ERROR[%0d]: tone score bank mismatch", case_idx);
                error_count = error_count + 1;
            end

            $fwrite(fd, "%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d\n",
                case_idx,
                best_index,
                exp_best_index_mem[case_idx],
                score0_power === exp_score0_power_mem[case_idx],
                score1_power === exp_score1_power_mem[case_idx],
                score2_power === exp_score2_power_mem[case_idx],
                score3_power === exp_score3_power_mem[case_idx],
                score4_power === exp_score4_power_mem[case_idx],
                case_ok);

            @(negedge clk);
            valid_in = 1'b0;
            data_in = 0;
            ref0_cos = 0;
            ref0_sin = 0;
            ref1_cos = 0;
            ref1_sin = 0;
            ref2_cos = 0;
            ref2_sin = 0;
            ref3_cos = 0;
            ref3_sin = 0;
            ref4_cos = 0;
            ref4_sin = 0;
            repeat (2) @(posedge clk);
        end

        $fclose(fd);

        if (error_count == 0) begin
            $display("PASS: tone_score_bank5");
        end else begin
            $display("FAIL: tone_score_bank5 errors=%0d", error_count);
        end

        $finish;
    end

endmodule
