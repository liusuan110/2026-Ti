`timescale 1ns / 1ps

module tb_receiver_recognition_with_params;

    localparam integer SAMPLE_BITS = 8;
    localparam integer LO_BITS = 12;
    localparam integer REF_BITS = 12;
    localparam integer DECIM = 80;
    localparam integer TONE_BLOCK_LEN = 250;
    localparam integer DDC_SAMPLES_PER_CASE = TONE_BLOCK_LEN + 1;
    localparam integer RAW_SAMPLES_PER_CASE = DECIM * DDC_SAMPLES_PER_CASE;
    localparam integer CASE_COUNT = 11;
    localparam integer TOTAL_SAMPLES = RAW_SAMPLES_PER_CASE * CASE_COUNT;
    localparam integer FREQ_HZ_BITS = 16;

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

    wire param_valid;
    wire [2:0] kind_code;
    wire [2:0] selected_freq_index;
    wire [2:0] coarse_flags;
    wire [2:0] am_best_index;
    wire [2:0] fm_best_index;
    wire is_cw;
    wire is_am_like;
    wire is_fm_like;
    wire is_psk_like;
    wire mod_freq_valid;
    wire [FREQ_HZ_BITS-1:0] mod_freq_hz;
    wire [FREQ_HZ_BITS-1:0] am_freq_hz;
    wire [FREQ_HZ_BITS-1:0] fm_freq_hz;
    wire am_ma_valid;
    wire [9:0] am_ma_permille;
    wire fm_param_valid;
    wire [FREQ_HZ_BITS-1:0] fm_delta_f_hz;
    wire [7:0] fm_mf_tenths;
    wire recognition_valid_debug;

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
    reg [2:0] exp_coarse_flags_mem [0:CASE_COUNT-1];
    reg exp_am_ma_valid_mem [0:CASE_COUNT-1];
    reg [9:0] exp_am_ma_permille_mem [0:CASE_COUNT-1];
    reg exp_fm_param_valid_mem [0:CASE_COUNT-1];
    reg [FREQ_HZ_BITS-1:0] exp_fm_delta_f_mem [0:CASE_COUNT-1];
    reg [7:0] exp_fm_mf_tenths_mem [0:CASE_COUNT-1];

    integer case_idx;
    integer sample_idx;
    integer base_idx;
    integer wait_idx;
    integer fd;
    integer error_count;
    reg result_seen;
    reg case_ok;
    reg am_ma_valid_captured;
    reg [9:0] am_ma_permille_captured;
    reg fm_param_valid_captured;
    reg [FREQ_HZ_BITS-1:0] fm_delta_f_captured;
    reg [7:0] fm_mf_tenths_captured;

    function [FREQ_HZ_BITS-1:0] index_to_freq_hz;
        input [2:0] index;
        begin
            case (index)
                3'd0: index_to_freq_hz = 16'd1000;
                3'd1: index_to_freq_hz = 16'd2000;
                3'd2: index_to_freq_hz = 16'd3000;
                3'd3: index_to_freq_hz = 16'd4000;
                3'd4: index_to_freq_hz = 16'd5000;
                default: index_to_freq_hz = 16'd0;
            endcase
        end
    endfunction

    receiver_recognition_with_params dut (
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
        .param_valid(param_valid),
        .kind_code(kind_code),
        .selected_freq_index(selected_freq_index),
        .coarse_flags(coarse_flags),
        .am_best_index(am_best_index),
        .fm_best_index(fm_best_index),
        .is_cw(is_cw),
        .is_am_like(is_am_like),
        .is_fm_like(is_fm_like),
        .is_psk_like(is_psk_like),
        .mod_freq_valid(mod_freq_valid),
        .mod_freq_hz(mod_freq_hz),
        .am_freq_hz(am_freq_hz),
        .fm_freq_hz(fm_freq_hz),
        .am_ma_valid(am_ma_valid),
        .am_ma_permille(am_ma_permille),
        .fm_param_valid(fm_param_valid),
        .fm_delta_f_hz(fm_delta_f_hz),
        .fm_mf_tenths(fm_mf_tenths),
        .recognition_valid_debug(recognition_valid_debug)
    );

    always #5 clk = ~clk;

    task check_if_result;
        begin
            #1;
            if (param_valid) begin
                result_seen = 1'b1;
                am_ma_valid_captured = am_ma_valid;
                am_ma_permille_captured = am_ma_permille;
                fm_param_valid_captured = fm_param_valid;
                fm_delta_f_captured = fm_delta_f_hz;
                fm_mf_tenths_captured = fm_mf_tenths;
                case_ok = (kind_code === exp_kind_mem[case_idx]) &&
                          (selected_freq_index === exp_selected_mem[case_idx]) &&
                          (am_best_index === exp_am_best_mem[case_idx]) &&
                          (fm_best_index === exp_fm_best_mem[case_idx]) &&
                          (coarse_flags === exp_coarse_flags_mem[case_idx]) &&
                          (is_cw === (exp_kind_mem[case_idx] == 3'd0)) &&
                          (is_am_like === (exp_kind_mem[case_idx] == 3'd1)) &&
                          (is_fm_like === (exp_kind_mem[case_idx] == 3'd2)) &&
                          (is_psk_like === (exp_kind_mem[case_idx] == 3'd3)) &&
                          (mod_freq_valid === ((exp_kind_mem[case_idx] == 3'd1) || (exp_kind_mem[case_idx] == 3'd2))) &&
                          (mod_freq_hz === (((exp_kind_mem[case_idx] == 3'd1) || (exp_kind_mem[case_idx] == 3'd2)) ? index_to_freq_hz(exp_selected_mem[case_idx]) : 16'd0)) &&
                          (am_freq_hz === index_to_freq_hz(exp_am_best_mem[case_idx])) &&
                          (fm_freq_hz === index_to_freq_hz(exp_fm_best_mem[case_idx])) &&
                          (am_ma_valid === exp_am_ma_valid_mem[case_idx]) &&
                          (!exp_am_ma_valid_mem[case_idx] || (am_ma_permille === exp_am_ma_permille_mem[case_idx])) &&
                          (fm_param_valid === exp_fm_param_valid_mem[case_idx]) &&
                          (!exp_fm_param_valid_mem[case_idx] ||
                              ((fm_delta_f_hz === exp_fm_delta_f_mem[case_idx]) &&
                               (fm_mf_tenths === exp_fm_mf_tenths_mem[case_idx])));
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
        $readmemh("sim/receiver_recognition_exp_coarse_flags.mem", exp_coarse_flags_mem);
        $readmemb("sim/receiver_recognition_exp_am_ma_valid.mem", exp_am_ma_valid_mem);
        $readmemh("sim/receiver_recognition_exp_am_ma_permille.mem", exp_am_ma_permille_mem);
        $readmemb("sim/receiver_recognition_exp_fm_param_valid.mem", exp_fm_param_valid_mem);
        $readmemh("sim/receiver_recognition_exp_fm_delta_f_hz.mem", exp_fm_delta_f_mem);
        $readmemh("sim/receiver_recognition_exp_fm_mf_tenths.mem", exp_fm_mf_tenths_mem);

        fd = $fopen("outputs/receiver_recognition/receiver_recognition_with_params_sim.csv", "w");
        $fwrite(fd, "case,mode,kind,expected_kind,mod_freq_hz,selected,expected_selected,am_freq_hz,fm_freq_hz,am_ma_valid,am_ma_permille,expected_am_ma_permille,fm_param_valid,fm_delta_f_hz,expected_fm_delta_f_hz,fm_mf_tenths,expected_fm_mf_tenths,ok\n");
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
            am_ma_valid_captured = 1'b0;
            am_ma_permille_captured = 10'd0;
            fm_param_valid_captured = 1'b0;
            fm_delta_f_captured = {FREQ_HZ_BITS{1'b0}};
            fm_mf_tenths_captured = 8'd0;
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

            for (wait_idx = 0; wait_idx < 72; wait_idx = wait_idx + 1) begin
                @(posedge clk);
                check_if_result();
            end

            if (!result_seen || !case_ok) begin
                $display("ERROR[%0d]: recognition with params mismatch", case_idx);
                error_count = error_count + 1;
            end

            $fwrite(fd, "%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d\n",
                case_idx,
                exp_mode_mem[case_idx],
                kind_code,
                exp_kind_mem[case_idx],
                mod_freq_hz,
                selected_freq_index,
                exp_selected_mem[case_idx],
                am_freq_hz,
                fm_freq_hz,
                am_ma_valid_captured,
                am_ma_permille_captured,
                exp_am_ma_permille_mem[case_idx],
                fm_param_valid_captured,
                fm_delta_f_captured,
                exp_fm_delta_f_mem[case_idx],
                fm_mf_tenths_captured,
                exp_fm_mf_tenths_mem[case_idx],
                result_seen && case_ok);
        end

        $fclose(fd);

        if (error_count == 0) begin
            $display("PASS: receiver_recognition_with_params");
        end else begin
            $display("FAIL: receiver_recognition_with_params errors=%0d", error_count);
        end

        $finish;
    end

endmodule
