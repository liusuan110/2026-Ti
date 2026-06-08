`timescale 1ns / 1ps

module tb_receiver_am_ma_pipeline;

    localparam integer SAMPLE_BITS = 8;
    localparam integer LO_BITS = 12;
    localparam integer DECIM = 80;
    localparam integer BLOCK_LEN = 250;
    localparam integer RAW_SAMPLES_PER_CASE = DECIM * BLOCK_LEN;
    localparam integer CASE_COUNT = 10;
    localparam integer TOTAL_SAMPLES = RAW_SAMPLES_PER_CASE * CASE_COUNT;

    reg clk = 1'b0;
    reg rst_n = 1'b0;
    reg valid_in = 1'b0;
    reg signed [SAMPLE_BITS-1:0] sample_in = 0;
    reg signed [LO_BITS-1:0] carrier_cos_in = 0;
    reg signed [LO_BITS-1:0] carrier_sin_in = 0;

    wire ma_result_valid;
    wire [9:0] ma_permille;
    wire ddc_valid_debug;
    wire feature_valid_debug;
    wire stats_valid_debug;

    reg signed [SAMPLE_BITS-1:0] sample_mem [0:TOTAL_SAMPLES-1];
    reg signed [LO_BITS-1:0] carrier_cos_mem [0:TOTAL_SAMPLES-1];
    reg signed [LO_BITS-1:0] carrier_sin_mem [0:TOTAL_SAMPLES-1];
    reg [9:0] expected_ma_mem [0:CASE_COUNT-1];

    integer case_idx;
    integer sample_idx;
    integer base_idx;
    integer wait_idx;
    integer fd;
    integer error_count;
    reg result_seen;
    reg [9:0] ma_captured;
    reg case_ok;

    receiver_am_ma_pipeline #(
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
        .ma_result_valid(ma_result_valid),
        .ma_permille(ma_permille),
        .ddc_valid_debug(ddc_valid_debug),
        .i_sum_debug(),
        .q_sum_debug(),
        .feature_valid_debug(feature_valid_debug),
        .stats_valid_debug(stats_valid_debug),
        .mag_min_debug(),
        .mag_max_debug()
    );

    always #5 clk = ~clk;

    task capture_result;
        begin
            #1;
            if (ma_result_valid) begin
                result_seen = 1'b1;
                ma_captured = ma_permille;
            end
        end
    endtask

    initial begin
        $readmemh("sim/receiver_am_ma_sample.mem", sample_mem);
        $readmemh("sim/receiver_am_ma_carrier_cos.mem", carrier_cos_mem);
        $readmemh("sim/receiver_am_ma_carrier_sin.mem", carrier_sin_mem);
        $readmemh("sim/receiver_am_ma_expected.mem", expected_ma_mem);

        fd = $fopen("outputs/receiver_am_ma/receiver_am_ma_sim.csv", "w");
        $fwrite(fd, "case,ma_permille,expected_ma_permille,result_seen,ok\n");
        error_count = 0;

        for (case_idx = 0; case_idx < CASE_COUNT; case_idx = case_idx + 1) begin
            rst_n = 1'b0;
            valid_in = 1'b0;
            sample_in = 0;
            carrier_cos_in = 0;
            carrier_sin_in = 0;
            result_seen = 1'b0;
            ma_captured = 10'd0;
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

            for (wait_idx = 0; wait_idx < 32; wait_idx = wait_idx + 1) begin
                @(posedge clk);
                capture_result();
            end

            case_ok = result_seen && (ma_captured === expected_ma_mem[case_idx]);

            if (!case_ok) begin
                $display("ERROR[%0d]: AM ma mismatch got=%0d expected=%0d seen=%0d",
                    case_idx,
                    ma_captured,
                    expected_ma_mem[case_idx],
                    result_seen);
                error_count = error_count + 1;
            end

            $fwrite(fd, "%0d,%0d,%0d,%0d,%0d\n",
                case_idx,
                ma_captured,
                expected_ma_mem[case_idx],
                result_seen,
                case_ok);
        end

        $fclose(fd);

        if (error_count == 0) begin
            $display("PASS: receiver_am_ma_pipeline");
        end else begin
            $display("FAIL: receiver_am_ma_pipeline errors=%0d", error_count);
        end

        $finish;
    end

endmodule
