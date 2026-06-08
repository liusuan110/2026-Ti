`timescale 1ns / 1ps

module tb_cordic_atan2_iterative;

    localparam integer CLK_PERIOD_NS = 10;
    localparam integer NUM_VECTORS = 81;

    reg clk = 1'b0;
    reg rst_n = 1'b0;
    reg valid_in = 1'b0;
    reg signed [20:0] i_in = 21'sd0;
    reg signed [20:0] q_in = 21'sd0;

    wire input_ready;
    wire busy;
    wire valid_out;
    wire signed [23:0] phase_out;
    wire [5:0] iter_debug;

    reg signed [20:0] i_mem [0:NUM_VECTORS-1];
    reg signed [20:0] q_mem [0:NUM_VECTORS-1];

    integer vector_index;
    integer file_handle;

    always #(CLK_PERIOD_NS / 2) clk = ~clk;

    cordic_atan2_iterative dut (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(valid_in),
        .i_in(i_in),
        .q_in(q_in),
        .input_ready(input_ready),
        .busy(busy),
        .valid_out(valid_out),
        .phase_out(phase_out),
        .iter_debug(iter_debug)
    );

    initial begin
        $readmemh("sim/cordic_i_81.mem", i_mem);
        $readmemh("sim/cordic_q_81.mem", q_mem);

        file_handle = $fopen("outputs/lesson_28/cordic_atan2_sim.csv", "w");
        $fwrite(file_handle, "n,i,q,phase_q4_20\n");

        repeat (4) @(posedge clk);
        rst_n = 1'b1;

        for (vector_index = 0; vector_index < NUM_VECTORS; vector_index = vector_index + 1) begin
            @(posedge clk);
            while (!input_ready) begin
                @(posedge clk);
            end

            @(negedge clk);
            i_in = i_mem[vector_index];
            q_in = q_mem[vector_index];
            valid_in = 1'b1;

            @(negedge clk);
            valid_in = 1'b0;

            @(posedge clk);
            while (!valid_out) begin
                @(posedge clk);
            end
            #1;
            $fwrite(file_handle, "%0d,%0d,%0d,%0d\n", vector_index, i_mem[vector_index], q_mem[vector_index], phase_out);
        end

        repeat (4) @(posedge clk);
        $fclose(file_handle);
        $finish;
    end

endmodule
