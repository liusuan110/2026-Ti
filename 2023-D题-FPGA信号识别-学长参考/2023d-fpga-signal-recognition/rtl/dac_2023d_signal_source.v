`timescale 1ns / 1ps

module dac_2023d_signal_source #(
    parameter integer AMP_BITS = 12,
    parameter integer PHASE_BITS = 32,
    parameter integer LUT_ADDR_BITS = 10,
    parameter [PHASE_BITS-1:0] CARRIER_PHASE_STEP = 32'd171798692,
    parameter [PHASE_BITS-1:0] AM_MOD_PHASE_STEP = 32'd257698,
    parameter [PHASE_BITS-1:0] FM_MOD_PHASE_STEP = 32'd343597,
    parameter [PHASE_BITS-1:0] SYMBOL_PHASE_STEP = 32'd687195,
    parameter [PHASE_BITS-1:0] FSK_HALF_SEP_PHASE_STEP = 32'd1202591,
    parameter signed [31:0] FM_PHASE_GAIN = 32'sd1162060968,
    parameter signed [12:0] AM_MA_Q12 = 13'sd2253,
    parameter COS_MEM_FILE = "rtl/cos_1024x12.mem",
    parameter SIN_MEM_FILE = "rtl/sin_1024x12.mem"
) (
    input wire clk,
    input wire rst_n,
    input wire [2:0] mode,
    output reg [7:0] sample_u8,
    output reg symbol_bit_debug
);

    localparam integer LUT_SIZE = 1 << LUT_ADDR_BITS;
    localparam integer ADDR_LSB = PHASE_BITS - LUT_ADDR_BITS;
    localparam [2:0] MODE_CW = 3'd0;
    localparam [2:0] MODE_AM = 3'd1;
    localparam [2:0] MODE_FM = 3'd2;
    localparam [2:0] MODE_2ASK = 3'd3;
    localparam [2:0] MODE_2PSK = 3'd4;
    localparam [2:0] MODE_2FSK = 3'd5;

    reg [PHASE_BITS-1:0] carrier_phase;
    reg [PHASE_BITS-1:0] mod_phase;
    reg [PHASE_BITS-1:0] symbol_phase;
    reg [15:0] lfsr;
    reg signed [AMP_BITS-1:0] cos_rom [0:LUT_SIZE-1];
    reg signed [AMP_BITS-1:0] sin_rom [0:LUT_SIZE-1];

    wire [PHASE_BITS-1:0] mod_phase_step;
    wire [PHASE_BITS-1:0] carrier_step;
    wire [PHASE_BITS-1:0] symbol_phase_next;
    wire symbol_tick;
    wire lfsr_feedback;
    wire [LUT_ADDR_BITS-1:0] carrier_addr;
    wire [LUT_ADDR_BITS-1:0] mod_addr;
    wire signed [AMP_BITS-1:0] carrier_cos_w;
    wire signed [AMP_BITS-1:0] mod_sin_w;
    wire signed [43:0] fm_offset_product;
    wire signed [PHASE_BITS-1:0] fm_phase_offset;
    wire [PHASE_BITS-1:0] wave_phase;
    wire [LUT_ADDR_BITS-1:0] wave_addr;
    wire signed [AMP_BITS-1:0] wave_cos_w;
    wire signed [12:0] cw_sample_s;
    wire signed [12:0] carrier_am_s;
    wire signed [25:0] am_term_product;
    wire signed [13:0] envelope_q12;
    wire signed [27:0] am_product;
    wire signed [12:0] am_sample_s;
    wire signed [12:0] ask_high_s;
    wire signed [12:0] ask_low_s;
    wire signed [12:0] psk_sample_s;
    wire signed [12:0] fsk_sample_s;
    reg signed [12:0] selected_sample_s;

    function [7:0] signed_to_u8;
        input signed [12:0] value;
        begin
            if (value < -13'sd128) begin
                signed_to_u8 = 8'd0;
            end else if (value > 13'sd127) begin
                signed_to_u8 = 8'd255;
            end else begin
                signed_to_u8 = value[7:0] + 8'd128;
            end
        end
    endfunction

    assign mod_phase_step = (mode == MODE_FM) ? FM_MOD_PHASE_STEP : AM_MOD_PHASE_STEP;
    assign carrier_step = (mode == MODE_2FSK) ?
                          (symbol_bit_debug ? (CARRIER_PHASE_STEP + FSK_HALF_SEP_PHASE_STEP) :
                                              (CARRIER_PHASE_STEP - FSK_HALF_SEP_PHASE_STEP)) :
                          CARRIER_PHASE_STEP;
    assign symbol_phase_next = symbol_phase + SYMBOL_PHASE_STEP;
    assign symbol_tick = symbol_phase_next < symbol_phase;
    assign lfsr_feedback = lfsr[15] ^ lfsr[13] ^ lfsr[12] ^ lfsr[10];
    assign carrier_addr = carrier_phase[PHASE_BITS-1:ADDR_LSB];
    assign mod_addr = mod_phase[PHASE_BITS-1:ADDR_LSB];
    assign carrier_cos_w = cos_rom[carrier_addr];
    assign mod_sin_w = sin_rom[mod_addr];
    assign fm_offset_product = mod_sin_w * FM_PHASE_GAIN;
    assign fm_phase_offset = fm_offset_product >>> 11;
    assign wave_phase = (mode == MODE_FM) ? (carrier_phase + fm_phase_offset) : carrier_phase;
    assign wave_addr = wave_phase[PHASE_BITS-1:ADDR_LSB];
    assign wave_cos_w = cos_rom[wave_addr];

    assign cw_sample_s = wave_cos_w >>> 4;
    assign carrier_am_s = carrier_cos_w >>> 5;
    assign am_term_product = mod_sin_w * AM_MA_Q12;
    assign envelope_q12 = 14'sd4096 + (am_term_product >>> 11);
    assign am_product = carrier_am_s * envelope_q12;
    assign am_sample_s = am_product >>> 12;
    assign ask_high_s = wave_cos_w >>> 4;
    assign ask_low_s = wave_cos_w >>> 8;
    assign psk_sample_s = symbol_bit_debug ? -(wave_cos_w >>> 4) : (wave_cos_w >>> 4);
    assign fsk_sample_s = wave_cos_w >>> 4;

    initial begin
        $readmemh(COS_MEM_FILE, cos_rom);
        $readmemh(SIN_MEM_FILE, sin_rom);
    end

    always @(*) begin
        case (mode)
            MODE_AM: selected_sample_s = am_sample_s;
            MODE_FM: selected_sample_s = cw_sample_s;
            MODE_2ASK: selected_sample_s = symbol_bit_debug ? ask_high_s : ask_low_s;
            MODE_2PSK: selected_sample_s = psk_sample_s;
            MODE_2FSK: selected_sample_s = fsk_sample_s;
            default: selected_sample_s = cw_sample_s;
        endcase
    end

    always @(posedge clk) begin
        if (!rst_n) begin
            carrier_phase <= {PHASE_BITS{1'b0}};
            mod_phase <= {PHASE_BITS{1'b0}};
            symbol_phase <= {PHASE_BITS{1'b0}};
            lfsr <= 16'hace1;
            symbol_bit_debug <= 1'b1;
            sample_u8 <= 8'h80;
        end else begin
            carrier_phase <= carrier_phase + carrier_step;
            mod_phase <= mod_phase + mod_phase_step;
            symbol_phase <= symbol_phase_next;
            sample_u8 <= signed_to_u8(selected_sample_s);

            if (symbol_tick) begin
                lfsr <= {lfsr[14:0], lfsr_feedback};
                symbol_bit_debug <= lfsr[0] ^ lfsr[3] ^ lfsr[5];
            end
        end
    end

endmodule
