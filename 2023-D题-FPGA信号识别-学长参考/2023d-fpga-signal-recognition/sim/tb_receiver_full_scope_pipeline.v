`timescale 1ns / 1ps

module tb_receiver_full_scope_pipeline;

    localparam integer SAMPLE_BITS = 8;
    localparam integer LO_BITS = 12;
    localparam integer REF_BITS = 12;
    localparam integer DECIM = 80;
    localparam integer KEY_BLOCK_LEN = 1024;
    localparam integer DDC_SAMPLES_PER_CASE = KEY_BLOCK_LEN * 2 + 1;
    localparam integer RAW_SAMPLES_PER_CASE = DECIM * DDC_SAMPLES_PER_CASE;
    localparam integer CASE_COUNT = 12;
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

    wire result_valid;
    wire [2:0] signal_kind_code;
    wire mod_freq_valid;
    wire [FREQ_HZ_BITS-1:0] mod_freq_hz;
    wire am_ma_valid;
    wire [9:0] am_ma_permille;
    wire fm_param_valid;
    wire [FREQ_HZ_BITS-1:0] fm_delta_f_hz;
    wire [7:0] fm_mf_tenths;
    wire keying_param_valid;
    wire [FREQ_HZ_BITS-1:0] rc_hz;
    wire fsk_param_valid;
    wire [FREQ_HZ_BITS-1:0] fsk_freq_sep_hz;
    wire [7:0] fsk_h_tenths;
    wire [2:0] analog_kind_debug;
    wire [2:0] keying_kind_debug;

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

    reg [2:0] exp_kind_mem [0:CASE_COUNT-1];
    reg exp_mod_freq_valid_mem [0:CASE_COUNT-1];
    reg [FREQ_HZ_BITS-1:0] exp_mod_freq_hz_mem [0:CASE_COUNT-1];
    reg exp_am_ma_valid_mem [0:CASE_COUNT-1];
    reg [9:0] exp_am_ma_permille_mem [0:CASE_COUNT-1];
    reg exp_fm_param_valid_mem [0:CASE_COUNT-1];
    reg [FREQ_HZ_BITS-1:0] exp_fm_delta_f_hz_mem [0:CASE_COUNT-1];
    reg [7:0] exp_fm_mf_tenths_mem [0:CASE_COUNT-1];
    reg exp_keying_param_valid_mem [0:CASE_COUNT-1];
    reg [FREQ_HZ_BITS-1:0] exp_rc_hz_mem [0:CASE_COUNT-1];
    reg exp_fsk_param_valid_mem [0:CASE_COUNT-1];
    reg [FREQ_HZ_BITS-1:0] exp_fsk_freq_sep_hz_mem [0:CASE_COUNT-1];
    reg [7:0] exp_fsk_h_tenths_mem [0:CASE_COUNT-1];

    integer case_idx;
    integer sample_idx;
    integer base_idx;
    integer wait_idx;
    integer fd;
    integer error_count;
    reg result_seen;
    reg [2:0] kind_captured;
    reg mod_freq_valid_captured;
    reg [FREQ_HZ_BITS-1:0] mod_freq_hz_captured;
    reg am_ma_valid_captured;
    reg [9:0] am_ma_permille_captured;
    reg fm_param_valid_captured;
    reg [FREQ_HZ_BITS-1:0] fm_delta_f_hz_captured;
    reg [7:0] fm_mf_tenths_captured;
    reg keying_param_valid_captured;
    reg [FREQ_HZ_BITS-1:0] rc_hz_captured;
    reg fsk_param_valid_captured;
    reg [FREQ_HZ_BITS-1:0] fsk_freq_sep_hz_captured;
    reg [7:0] fsk_h_tenths_captured;
    reg [2:0] analog_kind_captured;
    reg [2:0] keying_kind_captured;
    reg case_ok;

    receiver_full_scope_pipeline dut (
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
        .result_valid(result_valid),
        .signal_kind_code(signal_kind_code),
        .mod_freq_valid(mod_freq_valid),
        .mod_freq_hz(mod_freq_hz),
        .am_ma_valid(am_ma_valid),
        .am_ma_permille(am_ma_permille),
        .fm_param_valid(fm_param_valid),
        .fm_delta_f_hz(fm_delta_f_hz),
        .fm_mf_tenths(fm_mf_tenths),
        .keying_param_valid(keying_param_valid),
        .rc_hz(rc_hz),
        .fsk_param_valid(fsk_param_valid),
        .fsk_freq_sep_hz(fsk_freq_sep_hz),
        .fsk_h_tenths(fsk_h_tenths),
        .analog_kind_debug(analog_kind_debug),
        .keying_kind_debug(keying_kind_debug)
    );

    always #5 clk = ~clk;

    task capture_result;
        begin
            #1;
            if (result_valid) begin
                result_seen = 1'b1;
                kind_captured = signal_kind_code;
                mod_freq_valid_captured = mod_freq_valid;
                mod_freq_hz_captured = mod_freq_hz;
                am_ma_valid_captured = am_ma_valid;
                am_ma_permille_captured = am_ma_permille;
                fm_param_valid_captured = fm_param_valid;
                fm_delta_f_hz_captured = fm_delta_f_hz;
                fm_mf_tenths_captured = fm_mf_tenths;
                keying_param_valid_captured = keying_param_valid;
                rc_hz_captured = rc_hz;
                fsk_param_valid_captured = fsk_param_valid;
                fsk_freq_sep_hz_captured = fsk_freq_sep_hz;
                fsk_h_tenths_captured = fsk_h_tenths;
                analog_kind_captured = analog_kind_debug;
                keying_kind_captured = keying_kind_debug;
            end
        end
    endtask

    initial begin
        $readmemh("sim/receiver_full_scope_sample.mem", sample_mem);
        $readmemh("sim/receiver_full_scope_carrier_cos.mem", carrier_cos_mem);
        $readmemh("sim/receiver_full_scope_carrier_sin.mem", carrier_sin_mem);
        $readmemh("sim/receiver_full_scope_ref0_cos.mem", ref0_cos_mem);
        $readmemh("sim/receiver_full_scope_ref0_sin.mem", ref0_sin_mem);
        $readmemh("sim/receiver_full_scope_ref1_cos.mem", ref1_cos_mem);
        $readmemh("sim/receiver_full_scope_ref1_sin.mem", ref1_sin_mem);
        $readmemh("sim/receiver_full_scope_ref2_cos.mem", ref2_cos_mem);
        $readmemh("sim/receiver_full_scope_ref2_sin.mem", ref2_sin_mem);
        $readmemh("sim/receiver_full_scope_ref3_cos.mem", ref3_cos_mem);
        $readmemh("sim/receiver_full_scope_ref3_sin.mem", ref3_sin_mem);
        $readmemh("sim/receiver_full_scope_ref4_cos.mem", ref4_cos_mem);
        $readmemh("sim/receiver_full_scope_ref4_sin.mem", ref4_sin_mem);
        $readmemh("sim/receiver_full_scope_expected_kind_code.mem", exp_kind_mem);
        $readmemh("sim/receiver_full_scope_expected_mod_freq_valid.mem", exp_mod_freq_valid_mem);
        $readmemh("sim/receiver_full_scope_expected_mod_freq_hz.mem", exp_mod_freq_hz_mem);
        $readmemh("sim/receiver_full_scope_expected_am_ma_valid.mem", exp_am_ma_valid_mem);
        $readmemh("sim/receiver_full_scope_expected_am_ma_permille.mem", exp_am_ma_permille_mem);
        $readmemh("sim/receiver_full_scope_expected_fm_param_valid.mem", exp_fm_param_valid_mem);
        $readmemh("sim/receiver_full_scope_expected_fm_delta_f_hz.mem", exp_fm_delta_f_hz_mem);
        $readmemh("sim/receiver_full_scope_expected_fm_mf_tenths.mem", exp_fm_mf_tenths_mem);
        $readmemh("sim/receiver_full_scope_expected_keying_param_valid.mem", exp_keying_param_valid_mem);
        $readmemh("sim/receiver_full_scope_expected_rc_hz.mem", exp_rc_hz_mem);
        $readmemh("sim/receiver_full_scope_expected_fsk_param_valid.mem", exp_fsk_param_valid_mem);
        $readmemh("sim/receiver_full_scope_expected_fsk_freq_sep_hz.mem", exp_fsk_freq_sep_hz_mem);
        $readmemh("sim/receiver_full_scope_expected_fsk_h_tenths.mem", exp_fsk_h_tenths_mem);

        fd = $fopen("outputs/receiver_full_scope/receiver_full_scope_sim.csv", "w");
        $fwrite(fd, "case,kind,expected_kind,mod_freq_valid,expected_mod_freq_valid,mod_freq_hz,expected_mod_freq_hz,am_ma_valid,expected_am_ma_valid,am_ma_permille,expected_am_ma_permille,fm_param_valid,expected_fm_param_valid,fm_delta_f_hz,expected_fm_delta_f_hz,fm_mf_tenths,expected_fm_mf_tenths,keying_param_valid,expected_keying_param_valid,rc_hz,expected_rc_hz,fsk_param_valid,expected_fsk_param_valid,fsk_freq_sep_hz,expected_fsk_freq_sep_hz,fsk_h_tenths,expected_fsk_h_tenths,analog_kind,keying_kind,result_seen,ok\n");
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
            kind_captured = 3'd0;
            mod_freq_valid_captured = 1'b0;
            mod_freq_hz_captured = 16'd0;
            am_ma_valid_captured = 1'b0;
            am_ma_permille_captured = 10'd0;
            fm_param_valid_captured = 1'b0;
            fm_delta_f_hz_captured = 16'd0;
            fm_mf_tenths_captured = 8'd0;
            keying_param_valid_captured = 1'b0;
            rc_hz_captured = 16'd0;
            fsk_param_valid_captured = 1'b0;
            fsk_freq_sep_hz_captured = 16'd0;
            fsk_h_tenths_captured = 8'd0;
            analog_kind_captured = 3'd0;
            keying_kind_captured = 3'd0;
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
                capture_result();
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

            for (wait_idx = 0; wait_idx < 128; wait_idx = wait_idx + 1) begin
                @(posedge clk);
                capture_result();
            end

            case_ok = result_seen &&
                      (kind_captured === exp_kind_mem[case_idx]) &&
                      (mod_freq_valid_captured === exp_mod_freq_valid_mem[case_idx]) &&
                      (mod_freq_hz_captured === exp_mod_freq_hz_mem[case_idx]) &&
                      (am_ma_valid_captured === exp_am_ma_valid_mem[case_idx]) &&
                      (am_ma_permille_captured === exp_am_ma_permille_mem[case_idx]) &&
                      (fm_param_valid_captured === exp_fm_param_valid_mem[case_idx]) &&
                      (fm_delta_f_hz_captured === exp_fm_delta_f_hz_mem[case_idx]) &&
                      (fm_mf_tenths_captured === exp_fm_mf_tenths_mem[case_idx]) &&
                      (keying_param_valid_captured === exp_keying_param_valid_mem[case_idx]) &&
                      (rc_hz_captured === exp_rc_hz_mem[case_idx]) &&
                      (fsk_param_valid_captured === exp_fsk_param_valid_mem[case_idx]) &&
                      (fsk_freq_sep_hz_captured === exp_fsk_freq_sep_hz_mem[case_idx]) &&
                      (fsk_h_tenths_captured === exp_fsk_h_tenths_mem[case_idx]);

            if (!case_ok) begin
                $display("ERROR[%0d]: receiver full-scope mismatch", case_idx);
                error_count = error_count + 1;
            end

            $fwrite(fd, "%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d\n",
                case_idx,
                kind_captured,
                exp_kind_mem[case_idx],
                mod_freq_valid_captured,
                exp_mod_freq_valid_mem[case_idx],
                mod_freq_hz_captured,
                exp_mod_freq_hz_mem[case_idx],
                am_ma_valid_captured,
                exp_am_ma_valid_mem[case_idx],
                am_ma_permille_captured,
                exp_am_ma_permille_mem[case_idx],
                fm_param_valid_captured,
                exp_fm_param_valid_mem[case_idx],
                fm_delta_f_hz_captured,
                exp_fm_delta_f_hz_mem[case_idx],
                fm_mf_tenths_captured,
                exp_fm_mf_tenths_mem[case_idx],
                keying_param_valid_captured,
                exp_keying_param_valid_mem[case_idx],
                rc_hz_captured,
                exp_rc_hz_mem[case_idx],
                fsk_param_valid_captured,
                exp_fsk_param_valid_mem[case_idx],
                fsk_freq_sep_hz_captured,
                exp_fsk_freq_sep_hz_mem[case_idx],
                fsk_h_tenths_captured,
                exp_fsk_h_tenths_mem[case_idx],
                analog_kind_captured,
                keying_kind_captured,
                result_seen,
                case_ok);
        end

        $fclose(fd);

        if (error_count == 0) begin
            $display("PASS: receiver_full_scope_pipeline");
        end else begin
            $display("FAIL: receiver_full_scope_pipeline errors=%0d", error_count);
        end

        $finish;
    end

endmodule
