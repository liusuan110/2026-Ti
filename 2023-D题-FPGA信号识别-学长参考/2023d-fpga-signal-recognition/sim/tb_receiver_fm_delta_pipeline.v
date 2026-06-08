`timescale 1ns / 1ps

module tb_receiver_fm_delta_pipeline;

    localparam integer SAMPLE_BITS = 8;
    localparam integer LO_BITS = 12;
    localparam integer DECIM = 80;
    localparam integer BLOCK_LEN = 250;
    localparam integer DDC_SAMPLES_PER_CASE = BLOCK_LEN + 1;
    localparam integer RAW_SAMPLES_PER_CASE = DECIM * DDC_SAMPLES_PER_CASE;
    localparam integer CASE_COUNT = 10;
    localparam integer TOTAL_SAMPLES = RAW_SAMPLES_PER_CASE * CASE_COUNT;

    reg clk = 1'b0;
    reg rst_n = 1'b0;
    reg valid_in = 1'b0;
    reg signed [SAMPLE_BITS-1:0] sample_in = 0;
    reg signed [LO_BITS-1:0] carrier_cos_in = 0;
    reg signed [LO_BITS-1:0] carrier_sin_in = 0;

    wire delta_result_valid;
    wire [15:0] delta_f_hz;
    wire [24:0] max_abs_phase_delta;
    wire [7:0] mf_tenths;
    wire [15:0] mod_freq_hz;

    reg signed [SAMPLE_BITS-1:0] sample_mem [0:TOTAL_SAMPLES-1];
    reg signed [LO_BITS-1:0] carrier_cos_mem [0:TOTAL_SAMPLES-1];
    reg signed [LO_BITS-1:0] carrier_sin_mem [0:TOTAL_SAMPLES-1];
    reg [2:0] freq_index_mem [0:CASE_COUNT-1];
    reg [24:0] exp_max_abs_mem [0:CASE_COUNT-1];
    reg [15:0] exp_delta_f_mem [0:CASE_COUNT-1];
    reg [7:0] exp_mf_tenths_mem [0:CASE_COUNT-1];

    integer case_idx;
    integer sample_idx;
    integer base_idx;
    integer wait_idx;
    integer fd;
    integer error_count;
    reg result_seen;
    reg [15:0] delta_f_captured;
    reg [24:0] max_abs_captured;
    reg [7:0] mf_tenths_captured;
    reg case_ok;

    receiver_fm_delta_pipeline #(
        .SAMPLE_BITS(SAMPLE_BITS),
        .LO_BITS(LO_BITS),
        .DECIM(DECIM),
        .DECIM_COUNT_BITS(7),
        .BLOCK_LEN(BLOCK_LEN),
        .BLOCK_COUNT_BITS(8)
    ) dut (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(valid_in),
        .sample_in(sample_in),
        .carrier_cos_in(carrier_cos_in),
        .carrier_sin_in(carrier_sin_in),
        .delta_result_valid(delta_result_valid),
        .delta_f_hz(delta_f_hz),
        .max_abs_phase_delta(max_abs_phase_delta),
        .phase_delta_valid_debug(),
        .phase_delta_debug()
    );

    fm_mf_from_delta mf_i (
        .delta_f_hz(delta_f_hz),
        .freq_index(freq_index_mem[case_idx]),
        .mf_tenths(mf_tenths),
        .mod_freq_hz(mod_freq_hz)
    );

    always #5 clk = ~clk;

    task capture_result;
        begin
            #1;
            if (delta_result_valid) begin
                result_seen = 1'b1;
                delta_f_captured = delta_f_hz;
                max_abs_captured = max_abs_phase_delta;
                mf_tenths_captured = mf_tenths;
            end
        end
    endtask

    initial begin
        $readmemh("sim/receiver_fm_delta_sample.mem", sample_mem);
        $readmemh("sim/receiver_fm_delta_carrier_cos.mem", carrier_cos_mem);
        $readmemh("sim/receiver_fm_delta_carrier_sin.mem", carrier_sin_mem);
        $readmemh("sim/receiver_fm_delta_freq_index.mem", freq_index_mem);
        $readmemh("sim/receiver_fm_delta_exp_max_abs.mem", exp_max_abs_mem);
        $readmemh("sim/receiver_fm_delta_exp_delta_f.mem", exp_delta_f_mem);
        $readmemh("sim/receiver_fm_delta_exp_mf_tenths.mem", exp_mf_tenths_mem);

        fd = $fopen("outputs/receiver_fm_delta/receiver_fm_delta_sim.csv", "w");
        $fwrite(fd, "case,max_abs_phase_delta,expected_max_abs_phase_delta,delta_f_hz,expected_delta_f_hz,mf_tenths,expected_mf_tenths,result_seen,ok\n");
        error_count = 0;

        for (case_idx = 0; case_idx < CASE_COUNT; case_idx = case_idx + 1) begin
            rst_n = 1'b0;
            valid_in = 1'b0;
            sample_in = 0;
            carrier_cos_in = 0;
            carrier_sin_in = 0;
            result_seen = 1'b0;
            delta_f_captured = 16'd0;
            max_abs_captured = 25'd0;
            mf_tenths_captured = 8'd0;
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

            for (wait_idx = 0; wait_idx < 64; wait_idx = wait_idx + 1) begin
                @(posedge clk);
                capture_result();
            end

            case_ok = result_seen &&
                      (max_abs_captured === exp_max_abs_mem[case_idx]) &&
                      (delta_f_captured === exp_delta_f_mem[case_idx]) &&
                      (mf_tenths_captured === exp_mf_tenths_mem[case_idx]);

            if (!case_ok) begin
                $display("ERROR[%0d]: FM delta mismatch", case_idx);
                error_count = error_count + 1;
            end

            $fwrite(fd, "%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d\n",
                case_idx,
                max_abs_captured,
                exp_max_abs_mem[case_idx],
                delta_f_captured,
                exp_delta_f_mem[case_idx],
                mf_tenths_captured,
                exp_mf_tenths_mem[case_idx],
                result_seen,
                case_ok);
        end

        $fclose(fd);

        if (error_count == 0) begin
            $display("PASS: receiver_fm_delta_pipeline");
        end else begin
            $display("FAIL: receiver_fm_delta_pipeline errors=%0d", error_count);
        end

        $finish;
    end

endmodule
