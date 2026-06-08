`timescale 1ns / 1ps

module tb_moving_average;

    localparam integer CLK_PERIOD_NS = 50;
    localparam integer NUM_SAMPLES = 80;

    reg clk = 1'b0;
    reg rst_n = 1'b0;
    reg valid_in = 1'b0;
    reg signed [20:0] i_raw_in = 21'sd0;
    reg signed [20:0] q_raw_in = 21'sd0;

    wire i_valid;
    wire q_valid;
    wire signed [24:0] i_sum;
    wire signed [24:0] q_sum;
    wire signed [20:0] i_avg;
    wire signed [20:0] q_avg;

    reg signed [20:0] i_mem [0:NUM_SAMPLES-1];
    reg signed [20:0] q_mem [0:NUM_SAMPLES-1];

    integer sample_index;
    integer file_handle;

    always #(CLK_PERIOD_NS / 2) clk = ~clk;

    moving_average i_lpf (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(valid_in),
        .data_in(i_raw_in),
        .valid_out(i_valid),
        .sum_out(i_sum),
        .avg_out(i_avg)
    );

    moving_average q_lpf (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(valid_in),
        .data_in(q_raw_in),
        .valid_out(q_valid),
        .sum_out(q_sum),
        .avg_out(q_avg)
    );

    initial begin
        $readmemh("sim/lpf_i_raw_80.mem", i_mem);
        $readmemh("sim/lpf_q_raw_80.mem", q_mem);

        file_handle = $fopen("outputs/lesson_17/moving_average_sim.csv", "w");
        $fwrite(file_handle, "n,i_raw,q_raw,i_sum,q_sum,i_avg,q_avg\n");

        repeat (4) @(posedge clk);
        rst_n = 1'b1;

        for (sample_index = 0; sample_index < NUM_SAMPLES; sample_index = sample_index + 1) begin
            @(negedge clk);
            valid_in = 1'b1;
            i_raw_in = i_mem[sample_index];
            q_raw_in = q_mem[sample_index];

            @(posedge clk);
            #1;
            if (i_valid && q_valid) begin
                $fwrite(
                    file_handle,
                    "%0d,%0d,%0d,%0d,%0d,%0d,%0d\n",
                    sample_index,
                    i_raw_in,
                    q_raw_in,
                    i_sum,
                    q_sum,
                    i_avg,
                    q_avg
                );
            end
        end

        @(negedge clk);
        valid_in = 1'b0;
        repeat (4) @(posedge clk);
        $fclose(file_handle);
        $finish;
    end

endmodule
