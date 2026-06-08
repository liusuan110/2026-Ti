`timescale 1ns / 1ps

module tb_ddc_boxcar_decimator;

    localparam integer SAMPLE_BITS = 8;
    localparam integer LO_BITS = 12;
    localparam integer DECIM = 80;
    localparam integer COUNT_BITS = 7;
    localparam integer PRODUCT_BITS = SAMPLE_BITS + LO_BITS + 1;
    localparam integer ACC_BITS = PRODUCT_BITS + COUNT_BITS;
    localparam integer SAMPLE_COUNT = 1600;
    localparam integer OUT_COUNT = SAMPLE_COUNT / DECIM;

    reg clk = 1'b0;
    reg rst_n = 1'b0;
    reg valid_in = 1'b0;
    reg signed [SAMPLE_BITS-1:0] sample_in = 0;
    reg signed [LO_BITS-1:0] cos_in = 0;
    reg signed [LO_BITS-1:0] sin_in = 0;

    wire valid_out;
    wire signed [ACC_BITS-1:0] i_sum;
    wire signed [ACC_BITS-1:0] q_sum;

    reg signed [SAMPLE_BITS-1:0] sample_mem [0:SAMPLE_COUNT-1];
    reg signed [LO_BITS-1:0] cos_mem [0:SAMPLE_COUNT-1];
    reg signed [LO_BITS-1:0] sin_mem [0:SAMPLE_COUNT-1];
    reg signed [ACC_BITS-1:0] exp_i_mem [0:OUT_COUNT-1];
    reg signed [ACC_BITS-1:0] exp_q_mem [0:OUT_COUNT-1];

    integer sample_idx;
    integer out_idx;
    integer error_count;
    integer fd;

    ddc_boxcar_decimator #(
        .SAMPLE_BITS(SAMPLE_BITS),
        .LO_BITS(LO_BITS),
        .DECIM(DECIM),
        .COUNT_BITS(COUNT_BITS),
        .PRODUCT_BITS(PRODUCT_BITS),
        .ACC_BITS(ACC_BITS)
    ) dut (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(valid_in),
        .sample_in(sample_in),
        .cos_in(cos_in),
        .sin_in(sin_in),
        .valid_out(valid_out),
        .i_sum(i_sum),
        .q_sum(q_sum)
    );

    always #5 clk = ~clk;

    initial begin
        $readmemh("sim/ddc_boxcar_sample.mem", sample_mem);
        $readmemh("sim/ddc_boxcar_cos.mem", cos_mem);
        $readmemh("sim/ddc_boxcar_sin.mem", sin_mem);
        $readmemh("sim/ddc_boxcar_exp_i.mem", exp_i_mem);
        $readmemh("sim/ddc_boxcar_exp_q.mem", exp_q_mem);

        fd = $fopen("outputs/ddc_boxcar/ddc_boxcar_sim.csv", "w");
        $fwrite(fd, "index,i_sum,q_sum,expected_i,expected_q,ok\n");

        sample_idx = 0;
        out_idx = 0;
        error_count = 0;

        repeat (5) @(posedge clk);
        rst_n = 1'b1;

        while (sample_idx < SAMPLE_COUNT) begin
            @(posedge clk);
            valid_in <= 1'b1;
            sample_in <= sample_mem[sample_idx];
            cos_in <= cos_mem[sample_idx];
            sin_in <= sin_mem[sample_idx];
            sample_idx = sample_idx + 1;
        end

        @(posedge clk);
        valid_in <= 1'b0;
        sample_in <= 0;
        cos_in <= 0;
        sin_in <= 0;

        repeat (20) @(posedge clk);

        if (out_idx != OUT_COUNT) begin
            $display("ERROR: output count mismatch. got=%0d expected=%0d", out_idx, OUT_COUNT);
            error_count = error_count + 1;
        end

        $fclose(fd);

        if (error_count == 0) begin
            $display("PASS: ddc_boxcar_decimator");
        end else begin
            $display("FAIL: ddc_boxcar_decimator errors=%0d", error_count);
        end

        $finish;
    end

    always @(posedge clk) begin
        #1;
        if (valid_out) begin
            if ((i_sum !== exp_i_mem[out_idx]) || (q_sum !== exp_q_mem[out_idx])) begin
                $display("ERROR[%0d]: got i=%0d q=%0d expected i=%0d q=%0d",
                    out_idx, i_sum, q_sum, exp_i_mem[out_idx], exp_q_mem[out_idx]);
                error_count = error_count + 1;
            end

            $fwrite(fd, "%0d,%0d,%0d,%0d,%0d,%0d\n",
                out_idx, i_sum, q_sum, exp_i_mem[out_idx], exp_q_mem[out_idx],
                ((i_sum === exp_i_mem[out_idx]) && (q_sum === exp_q_mem[out_idx])));

            out_idx = out_idx + 1;
        end
    end

endmodule
