`timescale 1ns / 1ps

module button_press_pulse #(
    parameter integer DEBOUNCE_CYCLES = 1000000
) (
    input wire clk,
    input wire rst_n,
    input wire key_n,
    output reg press_pulse
);

    localparam integer COUNT_BITS = 20;

    reg key_meta;
    reg key_sync;
    reg stable_key;
    reg [COUNT_BITS-1:0] debounce_count;

    wire key_changed;
    wire count_done;

    assign key_changed = key_sync ^ stable_key;
    assign count_done = debounce_count == (DEBOUNCE_CYCLES - 1);

    always @(posedge clk) begin
        if (!rst_n) begin
            key_meta <= 1'b1;
            key_sync <= 1'b1;
            stable_key <= 1'b1;
            debounce_count <= {COUNT_BITS{1'b0}};
            press_pulse <= 1'b0;
        end else begin
            key_meta <= key_n;
            key_sync <= key_meta;
            press_pulse <= 1'b0;

            if (key_changed) begin
                if (count_done) begin
                    stable_key <= key_sync;
                    debounce_count <= {COUNT_BITS{1'b0}};
                    if (!key_sync) begin
                        press_pulse <= 1'b1;
                    end
                end else begin
                    debounce_count <= debounce_count + 1'b1;
                end
            end else begin
                debounce_count <= {COUNT_BITS{1'b0}};
            end
        end
    end

endmodule
