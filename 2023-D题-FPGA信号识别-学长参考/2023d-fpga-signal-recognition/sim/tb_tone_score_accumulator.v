`timescale 1ns / 1ps

module tb_tone_score_accumulator;

    localparam integer DATA_BITS = 18;
    localparam integer REF_BITS = 12;
    localparam integer BLOCK_LEN = 250;
    localparam integer COUNT_BITS = 8;
    localparam integer PRODUCT_BITS = DATA_BITS + REF_BITS;
    localparam integer ACC_BITS = PRODUCT_BITS + COUNT_BITS;
    localparam integer SCORE_BITS = 2 * ACC_BITS + 1;
    localparam integer CASE_COUNT = 3;
    localparam integer TOTAL_SAMPLES = BLOCK_LEN * CASE_COUNT;

    reg clk = 1'b0;
    reg rst_n = 1'b0;
    reg valid_in = 1'b0;
    reg signed [DATA_BITS-1:0] data_in = 0;
    reg signed [REF_BITS-1:0] ref_cos = 0;
    reg signed [REF_BITS-1:0] ref_sin = 0;

    wire result_valid;
    wire signed [ACC_BITS-1:0] i_score;
    wire signed [ACC_BITS-1:0] q_score;
    wire [SCORE_BITS-1:0] score_power;

    reg signed [DATA_BITS-1:0] data_mem [0:TOTAL_SAMPLES-1];
    reg signed [REF_BITS-1:0] cos_mem [0:TOTAL_SAMPLES-1];
    reg signed [REF_BITS-1:0] sin_mem [0:TOTAL_SAMPLES-1];
    reg signed [ACC_BITS-1:0] exp_i_mem [0:CASE_COUNT-1];
    reg signed [ACC_BITS-1:0] exp_q_mem [0:CASE_COUNT-1];
    reg [SCORE_BITS-1:0] exp_power_mem [0:CASE_COUNT-1];

    integer case_idx;
    integer sample_idx;
    integer base_idx;
    integer fd;
    integer error_count;
    reg case_ok;

    tone_score_accumulator #(
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
        .ref_cos(ref_cos),
        .ref_sin(ref_sin),
        .result_valid(result_valid),
        .i_score(i_score),
        .q_score(q_score),
        .score_power(score_power)
    );

    always #5 clk = ~clk;

    initial begin
        $readmemh("sim/tone_score_data.mem", data_mem);
        $readmemh("sim/tone_score_ref_cos.mem", cos_mem);
        $readmemh("sim/tone_score_ref_sin.mem", sin_mem);
        $readmemh("sim/tone_score_exp_i.mem", exp_i_mem);
        $readmemh("sim/tone_score_exp_q.mem", exp_q_mem);
        $readmemh("sim/tone_score_exp_power.mem", exp_power_mem);

        fd = $fopen("outputs/tone_score_accumulator/tone_score_accumulator_sim.csv", "w");
        $fwrite(fd, "case,i_score,expected_i_score,q_score,expected_q_score,score_power,expected_score_power,ok\n");
        error_count = 0;

        for (case_idx = 0; case_idx < CASE_COUNT; case_idx = case_idx + 1) begin
            rst_n = 1'b0;
            valid_in = 1'b0;
            data_in = 0;
            ref_cos = 0;
            ref_sin = 0;
            repeat (4) @(posedge clk);
            rst_n = 1'b1;
            repeat (2) @(posedge clk);

            base_idx = case_idx * BLOCK_LEN;
            for (sample_idx = 0; sample_idx < BLOCK_LEN; sample_idx = sample_idx + 1) begin
                @(negedge clk);
                valid_in = 1'b1;
                data_in = data_mem[base_idx + sample_idx];
                ref_cos = cos_mem[base_idx + sample_idx];
                ref_sin = sin_mem[base_idx + sample_idx];
                @(posedge clk);
                #1;
            end

            case_ok = result_valid &&
                      (i_score === exp_i_mem[case_idx]) &&
                      (q_score === exp_q_mem[case_idx]) &&
                      (score_power === exp_power_mem[case_idx]);

            if (!case_ok) begin
                $display("ERROR[%0d]: tone score mismatch", case_idx);
                error_count = error_count + 1;
            end

            $fwrite(fd, "%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d\n",
                case_idx,
                i_score, exp_i_mem[case_idx],
                q_score, exp_q_mem[case_idx],
                score_power, exp_power_mem[case_idx],
                case_ok);

            @(negedge clk);
            valid_in = 1'b0;
            data_in = 0;
            ref_cos = 0;
            ref_sin = 0;
            repeat (2) @(posedge clk);
        end

        $fclose(fd);

        if (error_count == 0) begin
            $display("PASS: tone_score_accumulator");
        end else begin
            $display("FAIL: tone_score_accumulator errors=%0d", error_count);
        end

        $finish;
    end

endmodule
