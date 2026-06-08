`timescale 1ns / 1ps

module fm_mf_from_delta #(
    parameter integer FREQ_HZ_BITS = 16,
    parameter integer MF_BITS = 8
) (
    input wire [FREQ_HZ_BITS-1:0] delta_f_hz,
    input wire [2:0] freq_index,
    output wire [MF_BITS-1:0] mf_tenths,
    output wire [FREQ_HZ_BITS-1:0] mod_freq_hz
);

    wire [31:0] mf_num;
    wire [31:0] mf_calc;

    assign mod_freq_hz = (freq_index == 3'd0) ? 16'd1000 :
                         (freq_index == 3'd1) ? 16'd2000 :
                         (freq_index == 3'd2) ? 16'd3000 :
                         (freq_index == 3'd3) ? 16'd4000 :
                         (freq_index == 3'd4) ? 16'd5000 :
                         16'd1;

    assign mf_num = delta_f_hz * 4'd10 + (mod_freq_hz >> 1);
    assign mf_calc = mf_num / mod_freq_hz;
    assign mf_tenths = mf_calc[MF_BITS-1:0];

endmodule
