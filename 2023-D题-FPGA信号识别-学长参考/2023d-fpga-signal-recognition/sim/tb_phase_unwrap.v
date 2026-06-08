`timescale 1ns / 1ps

module tb_phase_unwrap;

    localparam integer CLK_PERIOD_NS = 10;
    localparam integer NUM_SAMPLES = 256;

    reg clk = 1'b0;
    reg rst_n = 1'b0;
    reg valid_in = 1'b0;
    reg signed [23:0] phase_in = 24'sd0;

    wire valid_out;
    wire signed [39:0] phase_unwrapped_out;
    wire signed [39:0] wrap_offset_debug;

    reg signed [23:0] phase_mem [0:NUM_SAMPLES-1];

    integer sample_index;
    integer out_index;
    integer file_handle;

    always #(CLK_PERIOD_NS / 2) clk = ~clk;

    phase_unwrap dut (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(valid_in),
        .phase_in(phase_in),
        .valid_out(valid_out),
        .phase_unwrapped_out(phase_unwrapped_out),
        .wrap_offset_debug(wrap_offset_debug)
    );

    always @(posedge clk) begin
        if (!rst_n) begin
            out_index <= 0;
        end else if (valid_out) begin
            $fwrite(
                file_handle,
                "%0d,%0d,%0d,%0d\n",
                out_index,
                phase_mem[out_index],
                phase_unwrapped_out,
                wrap_offset_debug
            );
            out_index <= out_index + 1;
        end
    end

    initial begin
        $readmemh("sim/phase_unwrap_input_256.mem", phase_mem);

        file_handle = $fopen("outputs/lesson_32/phase_unwrap_sim.csv", "w");
        $fwrite(file_handle, "n,phase_wrapped_q4_20,phase_unwrapped_q4_20,wrap_offset_q4_20\n");

        repeat (4) @(posedge clk);
        rst_n = 1'b1;

        for (sample_index = 0; sample_index < NUM_SAMPLES; sample_index = sample_index + 1) begin
            @(negedge clk);
            phase_in = phase_mem[sample_index];
            valid_in = 1'b1;
        end

        @(negedge clk);
        valid_in = 1'b0;

        wait (out_index == NUM_SAMPLES);
        repeat (4) @(posedge clk);
        $fclose(file_handle);
        $finish;
    end

endmodule
