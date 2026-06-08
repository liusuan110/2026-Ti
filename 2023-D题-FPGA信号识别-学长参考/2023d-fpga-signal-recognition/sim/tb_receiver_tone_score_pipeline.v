`timescale 1ns / 1ps

module tb_receiver_tone_score_pipeline;

    localparam integer SAMPLE_BITS = 8;
    localparam integer LO_BITS = 12;
    localparam integer REF_BITS = 12;
    localparam integer DECIM = 80;
    localparam integer BLOCK_LEN = 250;
    localparam integer DDC_SAMPLES_PER_CASE = BLOCK_LEN + 1;
    localparam integer RAW_SAMPLES_PER_CASE = DECIM * DDC_SAMPLES_PER_CASE;
    localparam integer CASE_COUNT = 10;
    localparam integer TOTAL_SAMPLES = RAW_SAMPLES_PER_CASE * CASE_COUNT;
    localparam integer DATA_BITS = 32;
    localparam integer SCORE_COUNT_BITS = 8;
    localparam integer SCORE_PRODUCT_BITS = DATA_BITS + REF_BITS;
    localparam integer SCORE_ACC_BITS = SCORE_PRODUCT_BITS + SCORE_COUNT_BITS;
    localparam integer SCORE_BITS = 2 * SCORE_ACC_BITS + 1;

    reg clk = 1'b0;
    reg rst_n = 1'b0;
    reg valid_in = 1'b0;
    reg signed [SAMPLE_BITS-1:0] sample_in = 0;
    reg signed [LO_BITS-1:0] carrier_cos_in = 0;
    reg signed [LO_BITS-1:0] carrier_sin_in = 0;
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

    wire am_result_valid;
    wire [2:0] am_best_index;
    wire [SCORE_BITS-1:0] am_best_score_power;
    wire fm_result_valid;
    wire [2:0] fm_best_index;
    wire [SCORE_BITS-1:0] fm_best_score_power;
    wire ddc_valid_debug;
    wire feature_valid_debug;
    wire delta_valid_debug;
    wire signed [DATA_BITS-1:0] am_feature_debug;
    wire signed [DATA_BITS-1:0] fm_feature_debug;

    reg signed [SAMPLE_BITS-1:0] sample_mem [0:TOTAL_SAMPLES-1];
    reg signed [LO_BITS-1:0] carrier_cos_mem [0:TOTAL_SAMPLES-1];
    reg signed [LO_BITS-1:0] carrier_sin_mem [0:TOTAL_SAMPLES-1];
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

    reg mode_mem [0:CASE_COUNT-1];
    reg [2:0] exp_am_best_mem [0:CASE_COUNT-1];
    reg [2:0] exp_fm_best_mem [0:CASE_COUNT-1];

    integer case_idx;
    integer sample_idx;
    integer base_idx;
    integer wait_idx;
    integer fd;
    integer error_count;
    reg am_seen;
    reg fm_seen;
    reg [2:0] am_best_captured;
    reg [2:0] fm_best_captured;
    reg case_ok;

    receiver_tone_score_pipeline #(
        .SAMPLE_BITS(SAMPLE_BITS),
        .LO_BITS(LO_BITS),
        .REF_BITS(REF_BITS),
        .DECIM(DECIM),
        .DECIM_COUNT_BITS(7),
        .DATA_BITS(DATA_BITS),
        .BLOCK_LEN(BLOCK_LEN),
        .SCORE_COUNT_BITS(SCORE_COUNT_BITS),
        .MAG_SHIFT(24),
        .CROSS_SHIFT(22)
    ) dut (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(valid_in),
        .sample_in(sample_in),
        .carrier_cos_in(carrier_cos_in),
        .carrier_sin_in(carrier_sin_in),
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
        .am_result_valid(am_result_valid),
        .am_best_index(am_best_index),
        .am_best_score_power(am_best_score_power),
        .fm_result_valid(fm_result_valid),
        .fm_best_index(fm_best_index),
        .fm_best_score_power(fm_best_score_power),
        .ddc_valid_debug(ddc_valid_debug),
        .i_sum_debug(),
        .q_sum_debug(),
        .feature_valid_debug(feature_valid_debug),
        .delta_valid_debug(delta_valid_debug),
        .am_feature_debug(am_feature_debug),
        .fm_feature_debug(fm_feature_debug)
    );

    always #5 clk = ~clk;

    task capture_results;
        begin
            #1;
            if (am_result_valid) begin
                am_seen = 1'b1;
                am_best_captured = am_best_index;
            end
            if (fm_result_valid) begin
                fm_seen = 1'b1;
                fm_best_captured = fm_best_index;
            end
        end
    endtask

    initial begin
        $readmemh("sim/receiver_tone_sample.mem", sample_mem);
        $readmemh("sim/receiver_tone_carrier_cos.mem", carrier_cos_mem);
        $readmemh("sim/receiver_tone_carrier_sin.mem", carrier_sin_mem);
        $readmemh("sim/receiver_tone_ref0_cos.mem", ref0_cos_mem);
        $readmemh("sim/receiver_tone_ref0_sin.mem", ref0_sin_mem);
        $readmemh("sim/receiver_tone_ref1_cos.mem", ref1_cos_mem);
        $readmemh("sim/receiver_tone_ref1_sin.mem", ref1_sin_mem);
        $readmemh("sim/receiver_tone_ref2_cos.mem", ref2_cos_mem);
        $readmemh("sim/receiver_tone_ref2_sin.mem", ref2_sin_mem);
        $readmemh("sim/receiver_tone_ref3_cos.mem", ref3_cos_mem);
        $readmemh("sim/receiver_tone_ref3_sin.mem", ref3_sin_mem);
        $readmemh("sim/receiver_tone_ref4_cos.mem", ref4_cos_mem);
        $readmemh("sim/receiver_tone_ref4_sin.mem", ref4_sin_mem);
        $readmemb("sim/receiver_tone_mode.mem", mode_mem);
        $readmemh("sim/receiver_tone_exp_am_best.mem", exp_am_best_mem);
        $readmemh("sim/receiver_tone_exp_fm_best.mem", exp_fm_best_mem);

        fd = $fopen("outputs/receiver_tone_score/receiver_tone_score_sim.csv", "w");
        $fwrite(fd, "case,mode,am_best,expected_am_best,fm_best,expected_fm_best,am_seen,fm_seen,ok\n");
        error_count = 0;

        for (case_idx = 0; case_idx < CASE_COUNT; case_idx = case_idx + 1) begin
            rst_n = 1'b0;
            valid_in = 1'b0;
            sample_in = 0;
            carrier_cos_in = 0;
            carrier_sin_in = 0;
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
            am_seen = 1'b0;
            fm_seen = 1'b0;
            am_best_captured = 3'd0;
            fm_best_captured = 3'd0;
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
                capture_results();
            end

            @(negedge clk);
            valid_in = 1'b0;
            sample_in = 0;
            carrier_cos_in = 0;
            carrier_sin_in = 0;
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

            for (wait_idx = 0; wait_idx < 24; wait_idx = wait_idx + 1) begin
                @(posedge clk);
                capture_results();
            end

            case_ok = am_seen &&
                      fm_seen &&
                      (am_best_captured === exp_am_best_mem[case_idx]) &&
                      (fm_best_captured === exp_fm_best_mem[case_idx]);

            if (!case_ok) begin
                $display("ERROR[%0d]: receiver tone score mismatch", case_idx);
                error_count = error_count + 1;
            end

            $fwrite(fd, "%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d\n",
                case_idx,
                mode_mem[case_idx],
                am_best_captured,
                exp_am_best_mem[case_idx],
                fm_best_captured,
                exp_fm_best_mem[case_idx],
                am_seen,
                fm_seen,
                case_ok);
        end

        $fclose(fd);

        if (error_count == 0) begin
            $display("PASS: receiver_tone_score_pipeline");
        end else begin
            $display("FAIL: receiver_tone_score_pipeline errors=%0d", error_count);
        end

        $finish;
    end

endmodule
