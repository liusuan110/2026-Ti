`timescale 1ns / 1ps

module tb_feature_block_stats;

    localparam integer MAG_BITS = 57;
    localparam integer DELTA_BITS = 57;
    localparam integer BLOCK_LEN = 64;
    localparam integer COUNT_BITS = 6;
    localparam integer MAG_SUM_BITS = MAG_BITS + COUNT_BITS;
    localparam integer ABS_DELTA_BITS = DELTA_BITS + 1;
    localparam integer CROSS_SUM_BITS = ABS_DELTA_BITS + COUNT_BITS;
    localparam integer SAMPLE_COUNT = 256;
    localparam integer BLOCK_COUNT = SAMPLE_COUNT / BLOCK_LEN;

    reg clk = 1'b0;
    reg rst_n = 1'b0;
    reg valid_in = 1'b0;
    reg [MAG_BITS-1:0] mag_sq_in = 0;
    reg delta_valid_in = 1'b0;
    reg signed [DELTA_BITS-1:0] dot_in = 0;
    reg signed [DELTA_BITS-1:0] cross_in = 0;

    wire result_valid;
    wire [MAG_BITS-1:0] mag_min;
    wire [MAG_BITS-1:0] mag_max;
    wire [MAG_SUM_BITS-1:0] mag_sum;
    wire [CROSS_SUM_BITS-1:0] abs_cross_sum;
    wire [COUNT_BITS:0] negative_dot_count;

    reg [MAG_BITS-1:0] mag_sq_mem [0:SAMPLE_COUNT-1];
    reg signed [DELTA_BITS-1:0] dot_mem [0:SAMPLE_COUNT-1];
    reg signed [DELTA_BITS-1:0] cross_mem [0:SAMPLE_COUNT-1];
    reg delta_valid_mem [0:SAMPLE_COUNT-1];

    reg [MAG_BITS-1:0] exp_mag_min_mem [0:BLOCK_COUNT-1];
    reg [MAG_BITS-1:0] exp_mag_max_mem [0:BLOCK_COUNT-1];
    reg [MAG_SUM_BITS-1:0] exp_mag_sum_mem [0:BLOCK_COUNT-1];
    reg [CROSS_SUM_BITS-1:0] exp_abs_cross_sum_mem [0:BLOCK_COUNT-1];
    reg [COUNT_BITS:0] exp_negative_dot_count_mem [0:BLOCK_COUNT-1];

    integer input_idx;
    integer block_idx;
    integer error_count;
    integer fd;

    feature_block_stats #(
        .MAG_BITS(MAG_BITS),
        .DELTA_BITS(DELTA_BITS),
        .BLOCK_LEN(BLOCK_LEN),
        .COUNT_BITS(COUNT_BITS),
        .MAG_SUM_BITS(MAG_SUM_BITS),
        .ABS_DELTA_BITS(ABS_DELTA_BITS),
        .CROSS_SUM_BITS(CROSS_SUM_BITS)
    ) dut (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(valid_in),
        .mag_sq_in(mag_sq_in),
        .delta_valid_in(delta_valid_in),
        .dot_in(dot_in),
        .cross_in(cross_in),
        .result_valid(result_valid),
        .mag_min(mag_min),
        .mag_max(mag_max),
        .mag_sum(mag_sum),
        .abs_cross_sum(abs_cross_sum),
        .negative_dot_count(negative_dot_count)
    );

    always #5 clk = ~clk;

    initial begin
        $readmemh("sim/feature_stats_mag_sq.mem", mag_sq_mem);
        $readmemh("sim/feature_stats_dot.mem", dot_mem);
        $readmemh("sim/feature_stats_cross.mem", cross_mem);
        $readmemb("sim/feature_stats_delta_valid.mem", delta_valid_mem);
        $readmemh("sim/feature_stats_exp_mag_min.mem", exp_mag_min_mem);
        $readmemh("sim/feature_stats_exp_mag_max.mem", exp_mag_max_mem);
        $readmemh("sim/feature_stats_exp_mag_sum.mem", exp_mag_sum_mem);
        $readmemh("sim/feature_stats_exp_abs_cross_sum.mem", exp_abs_cross_sum_mem);
        $readmemh("sim/feature_stats_exp_negative_dot_count.mem", exp_negative_dot_count_mem);

        fd = $fopen("outputs/feature_block_stats/feature_block_stats_sim.csv", "w");
        $fwrite(fd, "block,mag_min,expected_mag_min,mag_max,expected_mag_max,mag_sum,expected_mag_sum,abs_cross_sum,expected_abs_cross_sum,negative_dot_count,expected_negative_dot_count,ok\n");

        input_idx = 0;
        block_idx = 0;
        error_count = 0;

        repeat (5) @(posedge clk);
        rst_n = 1'b1;

        while (input_idx < SAMPLE_COUNT) begin
            @(posedge clk);
            valid_in <= 1'b1;
            mag_sq_in <= mag_sq_mem[input_idx];
            delta_valid_in <= delta_valid_mem[input_idx];
            dot_in <= dot_mem[input_idx];
            cross_in <= cross_mem[input_idx];
            input_idx = input_idx + 1;
        end

        @(posedge clk);
        valid_in <= 1'b0;
        mag_sq_in <= 0;
        delta_valid_in <= 1'b0;
        dot_in <= 0;
        cross_in <= 0;

        repeat (20) @(posedge clk);

        if (block_idx != BLOCK_COUNT) begin
            $display("ERROR: block count mismatch. got=%0d expected=%0d", block_idx, BLOCK_COUNT);
            error_count = error_count + 1;
        end

        $fclose(fd);

        if (error_count == 0) begin
            $display("PASS: feature_block_stats");
        end else begin
            $display("FAIL: feature_block_stats errors=%0d", error_count);
        end

        $finish;
    end

    always @(posedge clk) begin
        #1;
        if (result_valid) begin
            if ((mag_min !== exp_mag_min_mem[block_idx]) ||
                (mag_max !== exp_mag_max_mem[block_idx]) ||
                (mag_sum !== exp_mag_sum_mem[block_idx]) ||
                (abs_cross_sum !== exp_abs_cross_sum_mem[block_idx]) ||
                (negative_dot_count !== exp_negative_dot_count_mem[block_idx])) begin
                $display("ERROR[%0d]: stats mismatch", block_idx);
                error_count = error_count + 1;
            end

            $fwrite(fd, "%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d\n",
                block_idx,
                mag_min, exp_mag_min_mem[block_idx],
                mag_max, exp_mag_max_mem[block_idx],
                mag_sum, exp_mag_sum_mem[block_idx],
                abs_cross_sum, exp_abs_cross_sum_mem[block_idx],
                negative_dot_count, exp_negative_dot_count_mem[block_idx],
                ((mag_min === exp_mag_min_mem[block_idx]) &&
                 (mag_max === exp_mag_max_mem[block_idx]) &&
                 (mag_sum === exp_mag_sum_mem[block_idx]) &&
                 (abs_cross_sum === exp_abs_cross_sum_mem[block_idx]) &&
                 (negative_dot_count === exp_negative_dot_count_mem[block_idx])));

            block_idx = block_idx + 1;
        end
    end

endmodule
