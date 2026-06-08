`timescale 1ns / 1ps

module fm_delta_to_hz #(
    parameter integer ANGLE_BITS = 24,
    parameter integer ABS_BITS = ANGLE_BITS + 1,
    parameter integer FREQ_HZ_BITS = 16,
    parameter integer BASEBAND_SAMPLE_HZ = 250000,
    parameter integer TWO_PI_Q4_20 = 6588398,
    parameter integer SCALE_FRAC_BITS = 32,
    parameter integer FREQ_SCALE_Q32 = 162974645,
    parameter integer PRODUCT_BITS = ABS_BITS + 32
) (
    input wire [ABS_BITS-1:0] max_abs_phase_delta,
    output wire [FREQ_HZ_BITS-1:0] delta_f_hz
);

    wire [31:0] freq_scale_w;
    wire [PRODUCT_BITS-1:0] scaled;
    wire [PRODUCT_BITS:0] rounded;
    wire [31:0] delta_f_calc;

    assign freq_scale_w = FREQ_SCALE_Q32;
    assign scaled = max_abs_phase_delta * freq_scale_w;
    assign rounded = {1'b0, scaled} + {{(PRODUCT_BITS-31){1'b0}}, 1'b1, 31'd0};
    assign delta_f_calc = rounded >> SCALE_FRAC_BITS;
    assign delta_f_hz = delta_f_calc[FREQ_HZ_BITS-1:0];

endmodule
