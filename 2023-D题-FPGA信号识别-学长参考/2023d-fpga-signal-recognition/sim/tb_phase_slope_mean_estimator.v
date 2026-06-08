`timescale 1ns / 1ps

module tb_phase_slope_mean_estimator;

    localparam integer CLK_PERIOD_NS = 10;
    localparam integer NUM_SAMPLES = 19985;

    reg clk = 1'b0;
    reg rst_n = 1'b0;
    reg valid_in = 1'b0;
    reg signed [39:0] phase_unwrapped_in = 40'sd0;

    wire estimate_valid;
    wire signed [43:0] start_sum_debug;
    wire signed [43:0] end_sum_debug;
    wire signed [44:0] delta_sum_out;
    wire signed [39:0] slope_per_sample_out;
    wire [13:0] block_sample_count_debug;

    reg signed [39:0] phase_mem [0:NUM_SAMPLES-1];

    integer sample_index;
    integer block_index;
    integer file_handle;

    always #(CLK_PERIOD_NS / 2) clk = ~clk;

    phase_slope_mean_estimator dut (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(valid_in),
        .phase_unwrapped_in(phase_unwrapped_in),
        .estimate_valid(estimate_valid),
        .start_sum_debug(start_sum_debug),
        .end_sum_debug(end_sum_debug),
        .delta_sum_out(delta_sum_out),
        .slope_per_sample_out(slope_per_sample_out),
        .block_sample_count_debug(block_sample_count_debug)
    );

    initial begin
        $readmemh("sim/phase_slope_mean_input_19985.mem", phase_mem);

        file_handle = $fopen("outputs/lesson_35/phase_slope_mean_estimator_sim.csv", "w");
        $fwrite(file_handle, "block_index,block_sample_count,start_sum_q4_20,end_sum_q4_20,delta_sum_q4_20,slope_per_sample_q4_20\n");

        block_index = 0;

        repeat (4) @(posedge clk);
        rst_n = 1'b1;

        for (sample_index = 0; sample_index < NUM_SAMPLES; sample_index = sample_index + 1) begin
            @(negedge clk);
            valid_in = 1'b1;
            phase_unwrapped_in = phase_mem[sample_index];

            @(posedge clk);
            #1;
            if (estimate_valid) begin
                $fwrite(
                    file_handle,
                    "%0d,%0d,%0d,%0d,%0d,%0d\n",
                    block_index,
                    block_sample_count_debug,
                    start_sum_debug,
                    end_sum_debug,
                    delta_sum_out,
                    slope_per_sample_out
                );
                block_index = block_index + 1;
            end
        end

        @(negedge clk);
        valid_in = 1'b0;

        repeat (8) @(posedge clk);
        $fclose(file_handle);
        $finish;
    end

endmodule
