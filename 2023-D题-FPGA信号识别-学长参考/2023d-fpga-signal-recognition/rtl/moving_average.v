`timescale 1ns / 1ps

module moving_average #(
    parameter integer DATA_BITS = 21,
    parameter integer WINDOW_LEN = 16,
    parameter integer SHIFT_BITS = 4,
    parameter integer ACC_BITS = DATA_BITS + SHIFT_BITS
) (
    input wire clk,
    input wire rst_n,
    input wire valid_in,
    input wire signed [DATA_BITS-1:0] data_in,
    output reg valid_out,
    output reg signed [ACC_BITS-1:0] sum_out,
    output reg signed [DATA_BITS-1:0] avg_out
);

    reg signed [DATA_BITS-1:0] delay_line [0:WINDOW_LEN-1];
    reg signed [ACC_BITS-1:0] sum_acc;
    reg [15:0] valid_count;

    wire signed [ACC_BITS-1:0] data_ext;
    wire signed [ACC_BITS-1:0] oldest_ext;
    wire signed [ACC_BITS-1:0] next_sum;

    integer i;

    assign data_ext = {{(ACC_BITS-DATA_BITS){data_in[DATA_BITS-1]}}, data_in};
    assign oldest_ext = {{(ACC_BITS-DATA_BITS){delay_line[WINDOW_LEN-1][DATA_BITS-1]}}, delay_line[WINDOW_LEN-1]};
    assign next_sum = sum_acc + data_ext - oldest_ext;

    always @(posedge clk) begin
        if (!rst_n) begin
            for (i = 0; i < WINDOW_LEN; i = i + 1) begin
                delay_line[i] <= {DATA_BITS{1'b0}};
            end
            sum_acc <= {ACC_BITS{1'b0}};
            sum_out <= {ACC_BITS{1'b0}};
            avg_out <= {DATA_BITS{1'b0}};
            valid_count <= 16'd0;
            valid_out <= 1'b0;
        end else begin
            valid_out <= 1'b0;

            if (valid_in) begin
                for (i = WINDOW_LEN-1; i > 0; i = i - 1) begin
                    delay_line[i] <= delay_line[i-1];
                end
                delay_line[0] <= data_in;

                sum_acc <= next_sum;
                sum_out <= next_sum;
                avg_out <= next_sum >>> SHIFT_BITS;

                if (valid_count < WINDOW_LEN) begin
                    valid_count <= valid_count + 16'd1;
                end
                valid_out <= (valid_count >= WINDOW_LEN-1);
            end
        end
    end

endmodule
