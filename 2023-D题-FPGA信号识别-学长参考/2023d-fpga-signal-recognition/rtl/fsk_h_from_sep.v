`timescale 1ns / 1ps

module fsk_h_from_sep #(
    parameter integer FREQ_HZ_BITS = 16,
    parameter integer H_BITS = 8,
    parameter integer SCALE_FRAC_BITS = 24,
    parameter integer H_SCALE_RC6K_Q24 = 27962,
    parameter integer H_SCALE_RC8K_Q24 = 20972,
    parameter integer H_SCALE_RC10K_Q24 = 16777,
    parameter integer PRODUCT_BITS = FREQ_HZ_BITS + 16
) (
    input wire [FREQ_HZ_BITS-1:0] freq_sep_hz,
    input wire [1:0] rc_index,
    output wire [FREQ_HZ_BITS-1:0] rc_hz,
    output wire [H_BITS-1:0] h_tenths
);

    wire [15:0] h_scale;
    wire [PRODUCT_BITS-1:0] h_scaled;
    wire [PRODUCT_BITS:0] h_rounded;
    wire [31:0] h_calc;

    assign rc_hz = (rc_index == 2'd0) ? 16'd6000 :
                   (rc_index == 2'd1) ? 16'd8000 :
                   (rc_index == 2'd2) ? 16'd10000 :
                   16'd1;

    assign h_scale = (rc_index == 2'd0) ? H_SCALE_RC6K_Q24[15:0] :
                     (rc_index == 2'd1) ? H_SCALE_RC8K_Q24[15:0] :
                     (rc_index == 2'd2) ? H_SCALE_RC10K_Q24[15:0] :
                     16'd0;
    assign h_scaled = freq_sep_hz * h_scale;
    assign h_rounded = {1'b0, h_scaled} + {{(PRODUCT_BITS-23){1'b0}}, 1'b1, 23'd0};
    assign h_calc = h_rounded >> SCALE_FRAC_BITS;
    assign h_tenths = h_calc[H_BITS-1:0];

endmodule
