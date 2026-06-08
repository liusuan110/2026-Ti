`timescale 1ns / 1ps

module tb_ddc_core;

    localparam integer CLK_PERIOD_NS = 50;
    localparam integer NUM_SAMPLES = 80;
    localparam integer LPF_WINDOW_LEN = 16;

    reg clk = 1'b0;
    reg rst_n = 1'b0;
    reg valid_in = 1'b0;
    reg signed [7:0] sample_in = 8'sd0;

    wire valid_out;
    wire signed [20:0] i_baseband;
    wire signed [20:0] q_baseband;
    wire signed [20:0] i_raw_debug;
    wire signed [20:0] q_raw_debug;
    wire signed [11:0] cos_debug;
    wire signed [11:0] sin_debug;
    wire [31:0] phase_debug;

    reg signed [7:0] sample_mem [0:NUM_SAMPLES-1];

    integer sample_index;
    integer output_index;
    integer output_n;
    integer file_handle;

    always #(CLK_PERIOD_NS / 2) clk = ~clk;

    ddc_core dut (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(valid_in),
        .sample_in(sample_in),
        .valid_out(valid_out),
        .i_baseband(i_baseband),
        .q_baseband(q_baseband),
        .i_raw_debug(i_raw_debug),
        .q_raw_debug(q_raw_debug),
        .cos_debug(cos_debug),
        .sin_debug(sin_debug),
        .phase_debug(phase_debug)
    );

    initial begin
        $readmemh("sim/ddc_core_sample_80.mem", sample_mem);

        file_handle = $fopen("outputs/lesson_18/ddc_core_sim.csv", "w");
        $fwrite(file_handle, "n,i_baseband,q_baseband\n");

        output_index = 0;

        repeat (4) @(posedge clk);
        rst_n = 1'b1;

        for (sample_index = 0; sample_index < NUM_SAMPLES; sample_index = sample_index + 1) begin
            @(negedge clk);
            valid_in = 1'b1;
            sample_in = sample_mem[sample_index];

            @(posedge clk);
            #1;
            if (valid_out) begin
                output_n = output_index + LPF_WINDOW_LEN - 1;
                $fwrite(file_handle, "%0d,%0d,%0d\n", output_n, i_baseband, q_baseband);
                output_index = output_index + 1;
            end
        end

        @(negedge clk);
        valid_in = 1'b0;

        repeat (6) begin
            @(posedge clk);
            #1;
            if (valid_out) begin
                output_n = output_index + LPF_WINDOW_LEN - 1;
                $fwrite(file_handle, "%0d,%0d,%0d\n", output_n, i_baseband, q_baseband);
                output_index = output_index + 1;
            end
        end

        $fclose(file_handle);
        $finish;
    end

endmodule
