`timescale 1ns / 1ps

module tb_receiver_recognition_pipeline;

    localparam integer SAMPLE_BITS = 8;
    localparam integer LO_BITS = 12;
    localparam integer REF_BITS = 12;
    localparam integer DECIM = 80;
    localparam integer TONE_BLOCK_LEN = 250;
    localparam integer DDC_SAMPLES_PER_CASE = TONE_BLOCK_LEN + 1;
    localparam integer RAW_SAMPLES_PER_CASE = DECIM * DDC_SAMPLES_PER_CASE;
    localparam integer CASE_COUNT = 11;
    localparam integer TOTAL_SAMPLES = RAW_SAMPLES_PER_CASE * CASE_COUNT;

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

    wire recognition_valid;
    wire [2:0] kind_code;
    wire [2:0] selected_freq_index;
    wire [2:0] coarse_flags_out;
    wire mag_active_out;
    wire ask_like_out;
    wire phase_active_out;
    wire phase_flip_active_out;
    wire [2:0] am_best_index_out;
    wire [2:0] fm_best_index_out;

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

    reg [1:0] exp_mode_mem [0:CASE_COUNT-1];
    reg [2:0] exp_kind_mem [0:CASE_COUNT-1];
    reg [2:0] exp_selected_mem [0:CASE_COUNT-1];
    reg [2:0] exp_am_best_mem [0:CASE_COUNT-1];
    reg [2:0] exp_fm_best_mem [0:CASE_COUNT-1];
    reg exp_mag_active_mem [0:CASE_COUNT-1];
    reg exp_ask_like_mem [0:CASE_COUNT-1];
    reg exp_phase_active_mem [0:CASE_COUNT-1];
    reg exp_phase_flip_active_mem [0:CASE_COUNT-1];
    reg [2:0] exp_coarse_flags_mem [0:CASE_COUNT-1];

    integer case_idx;
    integer sample_idx;
    integer base_idx;
    integer wait_idx;
    integer fd;
    integer error_count;
    reg result_seen;
    reg case_ok;

    receiver_recognition_pipeline dut (
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
        .recognition_valid(recognition_valid),
        .kind_code(kind_code),
        .selected_freq_index(selected_freq_index),
        .coarse_flags_out(coarse_flags_out),
        .mag_active_out(mag_active_out),
        .ask_like_out(ask_like_out),
        .phase_active_out(phase_active_out),
        .phase_flip_active_out(phase_flip_active_out),
        .am_best_index_out(am_best_index_out),
        .fm_best_index_out(fm_best_index_out),
        .coarse_result_valid_debug(),
        .am_result_valid_debug(),
        .fm_result_valid_debug()
    );

    always #5 clk = ~clk;

    task check_if_result;
        begin
            #1;
            if (recognition_valid) begin
                result_seen = 1'b1;
                case_ok = (kind_code === exp_kind_mem[case_idx]) &&
                          (selected_freq_index === exp_selected_mem[case_idx]) &&
                          (am_best_index_out === exp_am_best_mem[case_idx]) &&
                          (fm_best_index_out === exp_fm_best_mem[case_idx]) &&
                          (mag_active_out === exp_mag_active_mem[case_idx]) &&
                          (ask_like_out === exp_ask_like_mem[case_idx]) &&
                          (phase_active_out === exp_phase_active_mem[case_idx]) &&
                          (phase_flip_active_out === exp_phase_flip_active_mem[case_idx]) &&
                          (coarse_flags_out === exp_coarse_flags_mem[case_idx]);
            end
        end
    endtask

    initial begin
        $readmemh("sim/receiver_recognition_sample.mem", sample_mem);
        $readmemh("sim/receiver_recognition_carrier_cos.mem", carrier_cos_mem);
        $readmemh("sim/receiver_recognition_carrier_sin.mem", carrier_sin_mem);
        $readmemh("sim/receiver_recognition_ref0_cos.mem", ref0_cos_mem);
        $readmemh("sim/receiver_recognition_ref0_sin.mem", ref0_sin_mem);
        $readmemh("sim/receiver_recognition_ref1_cos.mem", ref1_cos_mem);
        $readmemh("sim/receiver_recognition_ref1_sin.mem", ref1_sin_mem);
        $readmemh("sim/receiver_recognition_ref2_cos.mem", ref2_cos_mem);
        $readmemh("sim/receiver_recognition_ref2_sin.mem", ref2_sin_mem);
        $readmemh("sim/receiver_recognition_ref3_cos.mem", ref3_cos_mem);
        $readmemh("sim/receiver_recognition_ref3_sin.mem", ref3_sin_mem);
        $readmemh("sim/receiver_recognition_ref4_cos.mem", ref4_cos_mem);
        $readmemh("sim/receiver_recognition_ref4_sin.mem", ref4_sin_mem);
        $readmemh("sim/receiver_recognition_exp_mode.mem", exp_mode_mem);
        $readmemh("sim/receiver_recognition_exp_kind.mem", exp_kind_mem);
        $readmemh("sim/receiver_recognition_exp_selected_freq_index.mem", exp_selected_mem);
        $readmemh("sim/receiver_recognition_exp_am_best_index.mem", exp_am_best_mem);
        $readmemh("sim/receiver_recognition_exp_fm_best_index.mem", exp_fm_best_mem);
        $readmemb("sim/receiver_recognition_exp_mag_active.mem", exp_mag_active_mem);
        $readmemb("sim/receiver_recognition_exp_ask_like.mem", exp_ask_like_mem);
        $readmemb("sim/receiver_recognition_exp_phase_active.mem", exp_phase_active_mem);
        $readmemb("sim/receiver_recognition_exp_phase_flip_active.mem", exp_phase_flip_active_mem);
        $readmemh("sim/receiver_recognition_exp_coarse_flags.mem", exp_coarse_flags_mem);

        fd = $fopen("outputs/receiver_recognition/receiver_recognition_sim.csv", "w");
        $fwrite(fd, "case,mode,kind,expected_kind,selected,expected_selected,am_best,expected_am_best,fm_best,expected_fm_best,coarse_flags,expected_coarse_flags,ok\n");
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
            result_seen = 1'b0;
            case_ok = 1'b0;
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
                check_if_result();
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

            for (wait_idx = 0; wait_idx < 32; wait_idx = wait_idx + 1) begin
                @(posedge clk);
                check_if_result();
            end

            if (!result_seen || !case_ok) begin
                $display("ERROR[%0d]: recognition mismatch", case_idx);
                error_count = error_count + 1;
            end

            $fwrite(fd, "%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d\n",
                case_idx,
                exp_mode_mem[case_idx],
                kind_code,
                exp_kind_mem[case_idx],
                selected_freq_index,
                exp_selected_mem[case_idx],
                am_best_index_out,
                exp_am_best_mem[case_idx],
                fm_best_index_out,
                exp_fm_best_mem[case_idx],
                coarse_flags_out,
                exp_coarse_flags_mem[case_idx],
                result_seen && case_ok);
        end

        $fclose(fd);

        if (error_count == 0) begin
            $display("PASS: receiver_recognition_pipeline");
        end else begin
            $display("FAIL: receiver_recognition_pipeline errors=%0d", error_count);
        end

        $finish;
    end

endmodule
