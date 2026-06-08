`timescale 1ns / 1ps

module tb_ddc_mixer;

    localparam integer CLK_PERIOD_NS = 50;
    localparam integer NUM_SAMPLES = 80;

    reg clk = 1'b0;
    reg rst_n = 1'b0;
    reg valid_in = 1'b0;
    reg signed [7:0] sample_in = 8'sd0;
    reg signed [11:0] cos_in = 12'sd0;
    reg signed [11:0] sin_in = 12'sd0;

    wire valid_out;
    wire signed [20:0] i_raw;
    wire signed [20:0] q_raw;

    reg signed [7:0] sample_mem [0:NUM_SAMPLES-1];
    reg signed [11:0] cos_mem [0:NUM_SAMPLES-1];
    reg signed [11:0] sin_mem [0:NUM_SAMPLES-1];

    integer sample_index;
    integer file_handle;

    always #(CLK_PERIOD_NS / 2) clk = ~clk;

    ddc_mixer dut (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(valid_in),
        .sample_in(sample_in),
        .cos_in(cos_in),
        .sin_in(sin_in),
        .valid_out(valid_out),
        .i_raw(i_raw),
        .q_raw(q_raw)
    );

    initial begin
        $readmemh("sim/mixer_sample_80.mem", sample_mem);
        $readmemh("sim/mixer_cos_80.mem", cos_mem);
        $readmemh("sim/mixer_sin_80.mem", sin_mem);

        file_handle = $fopen("outputs/lesson_16/ddc_mixer_sim.csv", "w");
        $fwrite(file_handle, "n,sample_in,cos_in,sin_in,i_raw,q_raw\n");

        repeat (4) @(posedge clk);
        rst_n = 1'b1;

        for (sample_index = 0; sample_index < NUM_SAMPLES; sample_index = sample_index + 1) begin
            @(negedge clk);
            valid_in = 1'b1;
            sample_in = sample_mem[sample_index];
            cos_in = cos_mem[sample_index];
            sin_in = sin_mem[sample_index];

            @(posedge clk);
            #1;
            $fwrite(
                file_handle,
                "%0d,%0d,%0d,%0d,%0d,%0d\n",
                sample_index,
                sample_in,
                cos_in,
                sin_in,
                i_raw,
                q_raw
            );
        end

        @(negedge clk);
        valid_in = 1'b0;
        repeat (4) @(posedge clk);
        $fclose(file_handle);
        $finish;
    end

endmodule
