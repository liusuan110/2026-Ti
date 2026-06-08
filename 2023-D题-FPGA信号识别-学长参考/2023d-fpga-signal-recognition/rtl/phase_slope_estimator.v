`timescale 1ns / 1ps

module phase_slope_estimator #(
    parameter integer PHASE_BITS = 40,
    parameter integer WINDOW_SHIFT = 13,
    parameter integer COUNT_BITS = 14
) (
    input wire clk,
    input wire rst_n,
    input wire valid_in,
    input wire signed [PHASE_BITS-1:0] phase_unwrapped_in,
    output reg estimate_valid,
    output reg signed [PHASE_BITS-1:0] first_phase_debug,
    output reg signed [PHASE_BITS-1:0] last_phase_debug,
    output reg signed [PHASE_BITS:0] delta_phase_out,
    output reg signed [PHASE_BITS-1:0] slope_per_sample_out,
    output reg [COUNT_BITS-1:0] window_sample_count_debug
);

    localparam [COUNT_BITS-1:0] WINDOW_LAST = (1 << WINDOW_SHIFT);

    reg [COUNT_BITS-1:0] sample_count;
    reg signed [PHASE_BITS-1:0] first_phase;

    wire signed [PHASE_BITS:0] phase_ext;
    wire signed [PHASE_BITS:0] first_ext;
    wire signed [PHASE_BITS:0] delta_now;

    assign phase_ext = {phase_unwrapped_in[PHASE_BITS-1], phase_unwrapped_in};
    assign first_ext = {first_phase[PHASE_BITS-1], first_phase};
    assign delta_now = phase_ext - first_ext;

    always @(posedge clk) begin
        if (!rst_n) begin
            sample_count <= {COUNT_BITS{1'b0}};
            first_phase <= {PHASE_BITS{1'b0}};
            estimate_valid <= 1'b0;
            first_phase_debug <= {PHASE_BITS{1'b0}};
            last_phase_debug <= {PHASE_BITS{1'b0}};
            delta_phase_out <= {(PHASE_BITS+1){1'b0}};
            slope_per_sample_out <= {PHASE_BITS{1'b0}};
            window_sample_count_debug <= {COUNT_BITS{1'b0}};
        end else begin
            estimate_valid <= 1'b0;

            if (valid_in) begin
                if (sample_count == {COUNT_BITS{1'b0}}) begin
                    first_phase <= phase_unwrapped_in;
                    first_phase_debug <= phase_unwrapped_in;
                    sample_count <= {{(COUNT_BITS-1){1'b0}}, 1'b1};
                end else if (sample_count == WINDOW_LAST) begin
                    last_phase_debug <= phase_unwrapped_in;
                    delta_phase_out <= delta_now;
                    slope_per_sample_out <= delta_now >>> WINDOW_SHIFT;
                    window_sample_count_debug <= WINDOW_LAST + 1'b1;
                    estimate_valid <= 1'b1;
                    sample_count <= {COUNT_BITS{1'b0}};
                end else begin
                    sample_count <= sample_count + 1'b1;
                end
            end
        end
    end

endmodule
