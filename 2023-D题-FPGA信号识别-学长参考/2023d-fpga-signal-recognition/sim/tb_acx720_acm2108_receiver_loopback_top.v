`timescale 1ns / 1ps

module tb_acx720_acm2108_receiver_loopback_top;

    reg Clk = 1'b0;
    reg reset_n = 1'b0;
    reg key_in = 1'b1;
    wire [7:0] AD0;
    wire [7:0] AD1;
    wire AD0_CLK;
    wire AD1_CLK;
    wire [7:0] DA0_Data;
    wire [7:0] DA1_Data;
    wire DA0_Clk;
    wire DA1_Clk;
    wire [7:0] led;

    integer error_count = 0;

    assign AD0 = DA0_Data;
    assign AD1 = DA1_Data;

    acx720_acm2108_receiver_loopback_top #(
        .BUTTON_DEBOUNCE_CYCLES(4)
    ) dut (
        .Clk(Clk),
        .reset_n(reset_n),
        .key_in(key_in),
        .AD0(AD0),
        .AD1(AD1),
        .AD0_CLK(AD0_CLK),
        .AD1_CLK(AD1_CLK),
        .DA0_Data(DA0_Data),
        .DA1_Data(DA1_Data),
        .DA0_Clk(DA0_Clk),
        .DA1_Clk(DA1_Clk),
        .led(led)
    );

    always #10 Clk = ~Clk;

    task press_key;
        begin
            @(negedge Clk);
            key_in = 1'b0;
            repeat (8) @(negedge Clk);
            key_in = 1'b1;
            repeat (12) @(negedge Clk);
        end
    endtask

    task wait_and_check;
        input [2:0] expected_mode;
        input [2:0] expected_kind;
        integer idx;
        reg seen;
        reg seen_expected;
        reg [2:0] last_kind;
        integer valid_count;
        begin
            seen = 1'b0;
            seen_expected = 1'b0;
            last_kind = 3'd0;
            valid_count = 0;

            for (idx = 0; idx < 1200000; idx = idx + 1) begin
                @(posedge Clk);
                if (dut.result_valid) begin
                    seen = 1'b1;
                    valid_count = valid_count + 1;
                    last_kind = dut.signal_kind_code;
                    if (dut.mode !== expected_mode) begin
                        $display("ERROR: mode mismatch expected=%0d got=%0d", expected_mode, dut.mode);
                        error_count = error_count + 1;
                    end
                    if (dut.signal_kind_code === expected_kind) begin
                        seen_expected = 1'b1;
                        idx = 1200000;
                    end
                end
            end

            if (!seen) begin
                $display("ERROR: no result for mode=%0d", expected_mode);
                error_count = error_count + 1;
            end else if (!seen_expected) begin
                $display("ERROR: no expected kind for mode=%0d expected=%0d last=%0d valid_count=%0d analog=%0d keying=%0d",
                    expected_mode,
                    expected_kind,
                    last_kind,
                    valid_count,
                    dut.analog_kind_debug,
                    dut.keying_kind_debug);
                error_count = error_count + 1;
            end
        end
    endtask

    initial begin
        repeat (8) @(posedge Clk);
        reset_n = 1'b1;
        repeat (8) @(posedge Clk);

        wait_and_check(3'd0, 3'd0);
        press_key();
        wait_and_check(3'd1, 3'd1);
        press_key();
        wait_and_check(3'd2, 3'd2);
        press_key();
        wait_and_check(3'd3, 3'd4);
        press_key();
        wait_and_check(3'd4, 3'd5);
        press_key();
        wait_and_check(3'd5, 3'd6);

        if (error_count == 0) begin
            $display("PASS: acx720_acm2108_receiver_loopback_top");
        end else begin
            $display("FAIL: acx720_acm2108_receiver_loopback_top errors=%0d", error_count);
        end

        $finish;
    end

endmodule
