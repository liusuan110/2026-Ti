`timescale 1ns / 1ps

module tb_dds_nco;

    localparam integer CLK_PERIOD_NS = 50;
    localparam integer NUM_SAMPLES = 80;

    reg clk = 1'b0;
    reg rst_n = 1'b0;
    reg valid_in = 1'b0;

    wire valid_out;
    wire signed [11:0] cos_out;
    wire signed [11:0] sin_out;
    wire [31:0] phase_out;

    integer sample_index;
    integer file_handle;

    always #(CLK_PERIOD_NS / 2) clk = ~clk;

    dds_nco dut (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(valid_in),
        .valid_out(valid_out),
        .cos_out(cos_out),
        .sin_out(sin_out),
        .phase_out(phase_out)
    );

    initial begin
        file_handle = $fopen("outputs/lesson_15/dds_nco_sim.csv", "w");
        $fwrite(file_handle, "n,phase,cos_code,sin_code\n");

        repeat (4) @(posedge clk);
        rst_n = 1'b1;
        valid_in = 1'b1;

        sample_index = 0;
        while (sample_index < NUM_SAMPLES) begin
            @(posedge clk);
            #1;
            if (valid_out) begin
                $fwrite(
                    file_handle,
                    "%0d,%0d,%0d,%0d\n",
                    sample_index,
                    phase_out,
                    cos_out,
                    sin_out
                );
                sample_index = sample_index + 1;
            end
        end

        valid_in = 1'b0;
        repeat (4) @(posedge clk);
        $fclose(file_handle);
        $finish;
    end

endmodule
