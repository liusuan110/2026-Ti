`timescale 1ns / 1ps

module tb_iq_feature_extractor;

    localparam integer IQ_BITS = 28;
    localparam integer PRODUCT_BITS = 2 * IQ_BITS;
    localparam integer MAG_SQ_BITS = PRODUCT_BITS + 1;
    localparam integer DOT_BITS = PRODUCT_BITS + 1;
    localparam integer SAMPLE_COUNT = 256;

    reg clk = 1'b0;
    reg rst_n = 1'b0;
    reg valid_in = 1'b0;
    reg signed [IQ_BITS-1:0] i_in = 0;
    reg signed [IQ_BITS-1:0] q_in = 0;

    wire valid_out;
    wire delta_valid;
    wire [MAG_SQ_BITS-1:0] mag_sq;
    wire signed [DOT_BITS-1:0] dot;
    wire signed [DOT_BITS-1:0] cross;

    reg signed [IQ_BITS-1:0] i_mem [0:SAMPLE_COUNT-1];
    reg signed [IQ_BITS-1:0] q_mem [0:SAMPLE_COUNT-1];
    reg [MAG_SQ_BITS-1:0] exp_mag_sq_mem [0:SAMPLE_COUNT-1];
    reg signed [DOT_BITS-1:0] exp_dot_mem [0:SAMPLE_COUNT-1];
    reg signed [DOT_BITS-1:0] exp_cross_mem [0:SAMPLE_COUNT-1];
    reg exp_delta_valid_mem [0:SAMPLE_COUNT-1];

    integer input_idx;
    integer output_idx;
    integer error_count;
    integer fd;

    iq_feature_extractor #(
        .IQ_BITS(IQ_BITS),
        .PRODUCT_BITS(PRODUCT_BITS),
        .MAG_SQ_BITS(MAG_SQ_BITS),
        .DOT_BITS(DOT_BITS)
    ) dut (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(valid_in),
        .i_in(i_in),
        .q_in(q_in),
        .valid_out(valid_out),
        .delta_valid(delta_valid),
        .mag_sq(mag_sq),
        .dot(dot),
        .cross(cross)
    );

    always #5 clk = ~clk;

    initial begin
        $readmemh("sim/iq_feature_i.mem", i_mem);
        $readmemh("sim/iq_feature_q.mem", q_mem);
        $readmemh("sim/iq_feature_mag_sq.mem", exp_mag_sq_mem);
        $readmemh("sim/iq_feature_dot.mem", exp_dot_mem);
        $readmemh("sim/iq_feature_cross.mem", exp_cross_mem);
        $readmemb("sim/iq_feature_delta_valid.mem", exp_delta_valid_mem);

        fd = $fopen("outputs/iq_feature/iq_feature_sim.csv", "w");
        $fwrite(fd, "index,mag_sq,expected_mag_sq,delta_valid,expected_delta_valid,dot,expected_dot,cross,expected_cross,ok\n");

        input_idx = 0;
        output_idx = 0;
        error_count = 0;

        repeat (5) @(posedge clk);
        rst_n = 1'b1;

        while (input_idx < SAMPLE_COUNT) begin
            @(posedge clk);
            valid_in <= 1'b1;
            i_in <= i_mem[input_idx];
            q_in <= q_mem[input_idx];
            input_idx = input_idx + 1;
        end

        @(posedge clk);
        valid_in <= 1'b0;
        i_in <= 0;
        q_in <= 0;

        repeat (20) @(posedge clk);

        if (output_idx != SAMPLE_COUNT) begin
            $display("ERROR: output count mismatch. got=%0d expected=%0d", output_idx, SAMPLE_COUNT);
            error_count = error_count + 1;
        end

        $fclose(fd);

        if (error_count == 0) begin
            $display("PASS: iq_feature_extractor");
        end else begin
            $display("FAIL: iq_feature_extractor errors=%0d", error_count);
        end

        $finish;
    end

    always @(posedge clk) begin
        #1;
        if (valid_out) begin
            if ((mag_sq !== exp_mag_sq_mem[output_idx]) ||
                (delta_valid !== exp_delta_valid_mem[output_idx]) ||
                (delta_valid && ((dot !== exp_dot_mem[output_idx]) || (cross !== exp_cross_mem[output_idx])))) begin
                $display("ERROR[%0d]: mag=%0d/%0d dv=%0d/%0d dot=%0d/%0d cross=%0d/%0d",
                    output_idx,
                    mag_sq, exp_mag_sq_mem[output_idx],
                    delta_valid, exp_delta_valid_mem[output_idx],
                    dot, exp_dot_mem[output_idx],
                    cross, exp_cross_mem[output_idx]);
                error_count = error_count + 1;
            end

            $fwrite(fd, "%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d\n",
                output_idx,
                mag_sq, exp_mag_sq_mem[output_idx],
                delta_valid, exp_delta_valid_mem[output_idx],
                dot, exp_dot_mem[output_idx],
                cross, exp_cross_mem[output_idx],
                ((mag_sq === exp_mag_sq_mem[output_idx]) &&
                 (delta_valid === exp_delta_valid_mem[output_idx]) &&
                 (!delta_valid || ((dot === exp_dot_mem[output_idx]) && (cross === exp_cross_mem[output_idx])))));

            output_idx = output_idx + 1;
        end
    end

endmodule
