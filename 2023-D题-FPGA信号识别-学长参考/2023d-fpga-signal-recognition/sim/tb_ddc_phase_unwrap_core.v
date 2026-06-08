`timescale 1ns / 1ps

module tb_ddc_phase_unwrap_core;

    localparam integer CLK_PERIOD_NS = 50;
    localparam integer NUM_SAMPLES = 20000;

    reg clk = 1'b0;
    reg rst_n = 1'b0;
    reg valid_in = 1'b0;
    reg signed [7:0] sample_in = 8'sd0;

    wire phase_unwrapped_valid;
    wire signed [39:0] phase_unwrapped_out;
    wire signed [39:0] wrap_offset_debug;
    wire ddc_valid_debug;
    wire [31:0] phase_sample_index_debug;
    wire signed [20:0] i_phase_debug;
    wire signed [20:0] q_phase_debug;
    wire signed [23:0] phase_wrapped_debug;

    reg signed [7:0] sample_mem [0:NUM_SAMPLES-1];

    integer sample_index;
    integer file_handle;

    always #(CLK_PERIOD_NS / 2) clk = ~clk;

    ddc_phase_unwrap_core dut (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(valid_in),
        .sample_in(sample_in),
        .phase_unwrapped_valid(phase_unwrapped_valid),
        .phase_unwrapped_out(phase_unwrapped_out),
        .wrap_offset_debug(wrap_offset_debug),
        .ddc_valid_debug(ddc_valid_debug),
        .phase_sample_index_debug(phase_sample_index_debug),
        .i_phase_debug(i_phase_debug),
        .q_phase_debug(q_phase_debug),
        .phase_wrapped_debug(phase_wrapped_debug)
    );

    initial begin
        $readmemh("sim/phase_unwrap_core_sample_20000.mem", sample_mem);

        file_handle = $fopen("outputs/lesson_33/ddc_phase_unwrap_core_sim.csv", "w");
        $fwrite(file_handle, "phase_sample_index,i_baseband,q_baseband,phase_wrapped_q4_20,phase_unwrapped_q4_20,wrap_offset_q4_20\n");

        repeat (4) @(posedge clk);
        rst_n = 1'b1;

        for (sample_index = 0; sample_index < NUM_SAMPLES; sample_index = sample_index + 1) begin
            @(negedge clk);
            valid_in = 1'b1;
            sample_in = sample_mem[sample_index];

            @(posedge clk);
            #1;
            if (phase_unwrapped_valid) begin
                $fwrite(
                    file_handle,
                    "%0d,%0d,%0d,%0d,%0d,%0d\n",
                    phase_sample_index_debug,
                    i_phase_debug,
                    q_phase_debug,
                    phase_wrapped_debug,
                    phase_unwrapped_out,
                    wrap_offset_debug
                );
            end
        end

        @(negedge clk);
        valid_in = 1'b0;

        repeat (120) begin
            @(posedge clk);
            #1;
            if (phase_unwrapped_valid) begin
                $fwrite(
                    file_handle,
                    "%0d,%0d,%0d,%0d,%0d,%0d\n",
                    phase_sample_index_debug,
                    i_phase_debug,
                    q_phase_debug,
                    phase_wrapped_debug,
                    phase_unwrapped_out,
                    wrap_offset_debug
                );
            end
        end

        $fclose(file_handle);
        $finish;
    end

endmodule
