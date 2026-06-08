`timescale 1ns / 1ps

module tb_receiver_coarse_pipeline;

    localparam integer SAMPLE_BITS = 8;
    localparam integer LO_BITS = 12;
    localparam integer DECIM = 80;
    localparam integer BLOCK_LEN = 64;
    localparam integer SAMPLES_PER_CASE = DECIM * BLOCK_LEN;
    localparam integer CASE_COUNT = 5;
    localparam integer TOTAL_SAMPLES = SAMPLES_PER_CASE * CASE_COUNT;

    reg clk = 1'b0;
    reg rst_n = 1'b0;
    reg valid_in = 1'b0;
    reg signed [SAMPLE_BITS-1:0] sample_in = 0;
    reg signed [LO_BITS-1:0] cos_in = 0;
    reg signed [LO_BITS-1:0] sin_in = 0;

    wire result_valid;
    wire mag_active;
    wire ask_like;
    wire phase_active;
    wire phase_flip_active;
    wire [2:0] coarse_flags;

    reg signed [SAMPLE_BITS-1:0] sample_mem [0:TOTAL_SAMPLES-1];
    reg signed [LO_BITS-1:0] cos_mem [0:TOTAL_SAMPLES-1];
    reg signed [LO_BITS-1:0] sin_mem [0:TOTAL_SAMPLES-1];

    reg exp_mag_active_mem [0:CASE_COUNT-1];
    reg exp_ask_like_mem [0:CASE_COUNT-1];
    reg exp_phase_active_mem [0:CASE_COUNT-1];
    reg exp_phase_flip_active_mem [0:CASE_COUNT-1];
    reg [2:0] exp_coarse_flags_mem [0:CASE_COUNT-1];

    integer case_idx;
    integer sample_idx;
    integer base_idx;
    integer error_count;
    integer wait_count;
    integer fd;

    receiver_coarse_pipeline #(
        .SAMPLE_BITS(SAMPLE_BITS),
        .LO_BITS(LO_BITS),
        .DECIM(DECIM),
        .DECIM_COUNT_BITS(7),
        .BLOCK_LEN(BLOCK_LEN),
        .BLOCK_COUNT_BITS(6)
    ) dut (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(valid_in),
        .sample_in(sample_in),
        .cos_in(cos_in),
        .sin_in(sin_in),
        .result_valid(result_valid),
        .mag_active(mag_active),
        .ask_like(ask_like),
        .phase_active(phase_active),
        .phase_flip_active(phase_flip_active),
        .coarse_flags(coarse_flags)
    );

    always #5 clk = ~clk;

    initial begin
        $readmemh("sim/receiver_coarse_sample.mem", sample_mem);
        $readmemh("sim/receiver_coarse_cos.mem", cos_mem);
        $readmemh("sim/receiver_coarse_sin.mem", sin_mem);
        $readmemb("sim/receiver_coarse_exp_mag_active.mem", exp_mag_active_mem);
        $readmemb("sim/receiver_coarse_exp_ask_like.mem", exp_ask_like_mem);
        $readmemb("sim/receiver_coarse_exp_phase_active.mem", exp_phase_active_mem);
        $readmemb("sim/receiver_coarse_exp_phase_flip_active.mem", exp_phase_flip_active_mem);
        $readmemh("sim/receiver_coarse_exp_coarse_flags.mem", exp_coarse_flags_mem);

        fd = $fopen("outputs/receiver_coarse_pipeline/receiver_coarse_pipeline_sim.csv", "w");
        $fwrite(fd, "case,mag_active,expected_mag_active,ask_like,expected_ask_like,phase_active,expected_phase_active,phase_flip_active,expected_phase_flip_active,coarse_flags,expected_coarse_flags,ok\n");

        error_count = 0;

        for (case_idx = 0; case_idx < CASE_COUNT; case_idx = case_idx + 1) begin
            rst_n = 1'b0;
            valid_in = 1'b0;
            sample_in = 0;
            cos_in = 0;
            sin_in = 0;
            repeat (5) @(posedge clk);
            rst_n = 1'b1;
            repeat (2) @(posedge clk);

            base_idx = case_idx * SAMPLES_PER_CASE;
            for (sample_idx = 0; sample_idx < SAMPLES_PER_CASE; sample_idx = sample_idx + 1) begin
                @(posedge clk);
                valid_in <= 1'b1;
                sample_in <= sample_mem[base_idx + sample_idx];
                cos_in <= cos_mem[base_idx + sample_idx];
                sin_in <= sin_mem[base_idx + sample_idx];
            end

            @(posedge clk);
            valid_in <= 1'b0;
            sample_in <= 0;
            cos_in <= 0;
            sin_in <= 0;

            wait_count = 0;
            while (!result_valid && wait_count < 200) begin
                @(posedge clk);
                wait_count = wait_count + 1;
            end

            if (!result_valid) begin
                $display("ERROR[%0d]: timeout waiting for result", case_idx);
                error_count = error_count + 1;
            end else begin
                if ((mag_active !== exp_mag_active_mem[case_idx]) ||
                    (ask_like !== exp_ask_like_mem[case_idx]) ||
                    (phase_active !== exp_phase_active_mem[case_idx]) ||
                    (phase_flip_active !== exp_phase_flip_active_mem[case_idx]) ||
                    (coarse_flags !== exp_coarse_flags_mem[case_idx])) begin
                    $display("ERROR[%0d]: result mismatch", case_idx);
                    error_count = error_count + 1;
                end

                $fwrite(fd, "%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d\n",
                    case_idx,
                    mag_active, exp_mag_active_mem[case_idx],
                    ask_like, exp_ask_like_mem[case_idx],
                    phase_active, exp_phase_active_mem[case_idx],
                    phase_flip_active, exp_phase_flip_active_mem[case_idx],
                    coarse_flags, exp_coarse_flags_mem[case_idx],
                    ((mag_active === exp_mag_active_mem[case_idx]) &&
                     (ask_like === exp_ask_like_mem[case_idx]) &&
                     (phase_active === exp_phase_active_mem[case_idx]) &&
                     (phase_flip_active === exp_phase_flip_active_mem[case_idx]) &&
                     (coarse_flags === exp_coarse_flags_mem[case_idx])));
            end
        end

        $fclose(fd);

        if (error_count == 0) begin
            $display("PASS: receiver_coarse_pipeline");
        end else begin
            $display("FAIL: receiver_coarse_pipeline errors=%0d", error_count);
        end

        $finish;
    end

endmodule
