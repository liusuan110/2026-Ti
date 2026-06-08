`timescale 1ns / 1ps

module tb_cordic_atan2_pipeline;

    localparam integer CLK_PERIOD_NS = 10;
    localparam integer NUM_VECTORS = 81;

    reg clk = 1'b0;
    reg rst_n = 1'b0;
    reg valid_in = 1'b0;
    reg signed [20:0] i_in = 21'sd0;
    reg signed [20:0] q_in = 21'sd0;

    wire valid_out;
    wire signed [23:0] phase_out;

    reg signed [20:0] i_mem [0:NUM_VECTORS-1];
    reg signed [20:0] q_mem [0:NUM_VECTORS-1];

    integer in_index;
    integer out_index;
    integer file_handle;

    always #(CLK_PERIOD_NS / 2) clk = ~clk;

    cordic_atan2_pipeline dut (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(valid_in),
        .i_in(i_in),
        .q_in(q_in),
        .valid_out(valid_out),
        .phase_out(phase_out)
    );

    always @(posedge clk) begin
        if (!rst_n) begin
            out_index <= 0;
        end else if (valid_out) begin
            $fwrite(
                file_handle,
                "%0d,%0d,%0d,%0d\n",
                out_index,
                i_mem[out_index],
                q_mem[out_index],
                phase_out
            );
            out_index <= out_index + 1;
        end
    end

    initial begin
        $readmemh("sim/pipeline_cordic_i_81.mem", i_mem);
        $readmemh("sim/pipeline_cordic_q_81.mem", q_mem);

        file_handle = $fopen("outputs/lesson_30/cordic_atan2_pipeline_sim.csv", "w");
        $fwrite(file_handle, "n,i,q,phase_q4_20\n");

        repeat (4) @(posedge clk);
        rst_n = 1'b1;

        for (in_index = 0; in_index < NUM_VECTORS; in_index = in_index + 1) begin
            @(negedge clk);
            i_in = i_mem[in_index];
            q_in = q_mem[in_index];
            valid_in = 1'b1;
        end

        @(negedge clk);
        valid_in = 1'b0;
        i_in = 21'sd0;
        q_in = 21'sd0;

        wait (out_index == NUM_VECTORS);
        repeat (4) @(posedge clk);
        $fclose(file_handle);
        $finish;
    end

endmodule
