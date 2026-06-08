`timescale 1ns / 1ps

module signed_block_minmax #(
    parameter integer DATA_BITS = 24,
    parameter integer BLOCK_LEN = 1024,
    parameter integer COUNT_BITS = 10
) (
    input wire clk,
    input wire rst_n,
    input wire valid_in,
    input wire signed [DATA_BITS-1:0] data_in,
    output reg result_valid,
    output reg signed [DATA_BITS-1:0] min_out,
    output reg signed [DATA_BITS-1:0] max_out
);

    localparam [COUNT_BITS-1:0] BLOCK_LAST = BLOCK_LEN - 1;

    reg [COUNT_BITS-1:0] count;
    reg signed [DATA_BITS-1:0] min_acc;
    reg signed [DATA_BITS-1:0] max_acc;

    wire first_sample;
    wire last_sample;
    wire signed [DATA_BITS-1:0] min_next;
    wire signed [DATA_BITS-1:0] max_next;

    assign first_sample = (count == {COUNT_BITS{1'b0}});
    assign last_sample = (count == BLOCK_LAST);
    assign min_next = (first_sample || (data_in < min_acc)) ? data_in : min_acc;
    assign max_next = (first_sample || (data_in > max_acc)) ? data_in : max_acc;

    always @(posedge clk) begin
        if (!rst_n) begin
            count <= {COUNT_BITS{1'b0}};
            min_acc <= {DATA_BITS{1'b0}};
            max_acc <= {DATA_BITS{1'b0}};
            result_valid <= 1'b0;
            min_out <= {DATA_BITS{1'b0}};
            max_out <= {DATA_BITS{1'b0}};
        end else begin
            result_valid <= 1'b0;

            if (valid_in) begin
                if (last_sample) begin
                    result_valid <= 1'b1;
                    min_out <= min_next;
                    max_out <= max_next;
                    count <= {COUNT_BITS{1'b0}};
                    min_acc <= {DATA_BITS{1'b0}};
                    max_acc <= {DATA_BITS{1'b0}};
                end else begin
                    count <= count + 1'b1;
                    min_acc <= min_next;
                    max_acc <= max_next;
                end
            end
        end
    end

endmodule
