`timescale 1ns / 1ps

module tb_feature_coarse_classifier;

    localparam integer MAG_BITS = 57;
    localparam integer COUNT_BITS = 6;
    localparam integer MAG_SUM_BITS = MAG_BITS + COUNT_BITS;
    localparam integer CROSS_SUM_BITS = 64;
    localparam integer CASE_COUNT = 8;

    reg clk = 1'b0;
    reg rst_n = 1'b0;
    reg stats_valid = 1'b0;
    reg [MAG_BITS-1:0] mag_min = 0;
    reg [MAG_BITS-1:0] mag_max = 0;
    reg [MAG_SUM_BITS-1:0] mag_sum = 0;
    reg [CROSS_SUM_BITS-1:0] abs_cross_sum = 0;
    reg [COUNT_BITS:0] negative_dot_count = 0;

    wire result_valid;
    wire mag_active;
    wire ask_like;
    wire phase_active;
    wire phase_flip_active;
    wire [2:0] coarse_flags;

    reg [MAG_BITS-1:0] mag_min_mem [0:CASE_COUNT-1];
    reg [MAG_BITS-1:0] mag_max_mem [0:CASE_COUNT-1];
    reg [MAG_SUM_BITS-1:0] mag_sum_mem [0:CASE_COUNT-1];
    reg [CROSS_SUM_BITS-1:0] abs_cross_sum_mem [0:CASE_COUNT-1];
    reg [COUNT_BITS:0] negative_dot_count_mem [0:CASE_COUNT-1];

    reg exp_mag_active_mem [0:CASE_COUNT-1];
    reg exp_ask_like_mem [0:CASE_COUNT-1];
    reg exp_phase_active_mem [0:CASE_COUNT-1];
    reg exp_phase_flip_active_mem [0:CASE_COUNT-1];
    reg [2:0] exp_coarse_flags_mem [0:CASE_COUNT-1];

    integer input_idx;
    integer output_idx;
    integer error_count;
    integer fd;

    feature_coarse_classifier #(
        .MAG_BITS(MAG_BITS),
        .COUNT_BITS(COUNT_BITS),
        .MAG_SUM_BITS(MAG_SUM_BITS),
        .CROSS_SUM_BITS(CROSS_SUM_BITS),
        .BLOCK_LEN(64),
        .MAG_RANGE_PERCENT(25),
        .ASK_MINMAX_PERCENT(8),
        .PHASE_ACTIVITY_PERCENT(8),
        .NEG_DOT_THRESHOLD(1)
    ) dut (
        .clk(clk),
        .rst_n(rst_n),
        .stats_valid(stats_valid),
        .mag_min(mag_min),
        .mag_max(mag_max),
        .mag_sum(mag_sum),
        .abs_cross_sum(abs_cross_sum),
        .negative_dot_count(negative_dot_count),
        .result_valid(result_valid),
        .mag_active(mag_active),
        .ask_like(ask_like),
        .phase_active(phase_active),
        .phase_flip_active(phase_flip_active),
        .coarse_flags(coarse_flags)
    );

    always #5 clk = ~clk;

    initial begin
        $readmemh("sim/coarse_mag_min.mem", mag_min_mem);
        $readmemh("sim/coarse_mag_max.mem", mag_max_mem);
        $readmemh("sim/coarse_mag_sum.mem", mag_sum_mem);
        $readmemh("sim/coarse_abs_cross_sum.mem", abs_cross_sum_mem);
        $readmemh("sim/coarse_negative_dot_count.mem", negative_dot_count_mem);
        $readmemb("sim/coarse_exp_mag_active.mem", exp_mag_active_mem);
        $readmemb("sim/coarse_exp_ask_like.mem", exp_ask_like_mem);
        $readmemb("sim/coarse_exp_phase_active.mem", exp_phase_active_mem);
        $readmemb("sim/coarse_exp_phase_flip_active.mem", exp_phase_flip_active_mem);
        $readmemh("sim/coarse_exp_coarse_flags.mem", exp_coarse_flags_mem);

        fd = $fopen("outputs/feature_coarse_classifier/feature_coarse_classifier_sim.csv", "w");
        $fwrite(fd, "index,mag_active,expected_mag_active,ask_like,expected_ask_like,phase_active,expected_phase_active,phase_flip_active,expected_phase_flip_active,coarse_flags,expected_coarse_flags,ok\n");

        input_idx = 0;
        output_idx = 0;
        error_count = 0;

        repeat (5) @(posedge clk);
        rst_n = 1'b1;

        while (input_idx < CASE_COUNT) begin
            @(posedge clk);
            stats_valid <= 1'b1;
            mag_min <= mag_min_mem[input_idx];
            mag_max <= mag_max_mem[input_idx];
            mag_sum <= mag_sum_mem[input_idx];
            abs_cross_sum <= abs_cross_sum_mem[input_idx];
            negative_dot_count <= negative_dot_count_mem[input_idx];
            input_idx = input_idx + 1;
        end

        @(posedge clk);
        stats_valid <= 1'b0;

        repeat (10) @(posedge clk);

        if (output_idx != CASE_COUNT) begin
            $display("ERROR: output count mismatch. got=%0d expected=%0d", output_idx, CASE_COUNT);
            error_count = error_count + 1;
        end

        $fclose(fd);

        if (error_count == 0) begin
            $display("PASS: feature_coarse_classifier");
        end else begin
            $display("FAIL: feature_coarse_classifier errors=%0d", error_count);
        end

        $finish;
    end

    always @(posedge clk) begin
        #1;
        if (result_valid) begin
            if ((mag_active !== exp_mag_active_mem[output_idx]) ||
                (ask_like !== exp_ask_like_mem[output_idx]) ||
                (phase_active !== exp_phase_active_mem[output_idx]) ||
                (phase_flip_active !== exp_phase_flip_active_mem[output_idx]) ||
                (coarse_flags !== exp_coarse_flags_mem[output_idx])) begin
                $display("ERROR[%0d]: coarse flags mismatch", output_idx);
                error_count = error_count + 1;
            end

            $fwrite(fd, "%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d\n",
                output_idx,
                mag_active, exp_mag_active_mem[output_idx],
                ask_like, exp_ask_like_mem[output_idx],
                phase_active, exp_phase_active_mem[output_idx],
                phase_flip_active, exp_phase_flip_active_mem[output_idx],
                coarse_flags, exp_coarse_flags_mem[output_idx],
                ((mag_active === exp_mag_active_mem[output_idx]) &&
                 (ask_like === exp_ask_like_mem[output_idx]) &&
                 (phase_active === exp_phase_active_mem[output_idx]) &&
                 (phase_flip_active === exp_phase_flip_active_mem[output_idx]) &&
                 (coarse_flags === exp_coarse_flags_mem[output_idx])));

            output_idx = output_idx + 1;
        end
    end

endmodule
