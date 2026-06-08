`timescale 1ns / 1ps

module acx720_acm2108_receiver_loopback_top #(
    parameter integer BUTTON_DEBOUNCE_CYCLES = 1000000
) (
    input wire Clk,
    input wire reset_n,
    input wire key_in,
    input wire [7:0] AD0,
    input wire [7:0] AD1,
    output wire AD0_CLK,
    output wire AD1_CLK,
    output wire [7:0] DA0_Data,
    output wire [7:0] DA1_Data,
    output wire DA0_Clk,
    output wire DA1_Clk,
    output reg [7:0] led
);

    localparam [2:0] MODE_LAST = 3'd5;

    wire mode_press;
    wire [7:0] source_sample;
    wire source_symbol_bit;
    wire receiver_reset_n;
    wire result_valid;
    wire [2:0] signal_kind_code;
    wire mod_freq_valid;
    wire [15:0] mod_freq_hz;
    wire am_ma_valid;
    wire [9:0] am_ma_permille;
    wire fm_param_valid;
    wire [15:0] fm_delta_f_hz;
    wire [7:0] fm_mf_tenths;
    wire keying_param_valid;
    wire [15:0] rc_hz;
    wire fsk_param_valid;
    wire [15:0] fsk_freq_sep_hz;
    wire [7:0] fsk_h_tenths;
    wire [2:0] analog_kind_debug;
    wire [2:0] keying_kind_debug;

    reg [2:0] mode;
    reg [7:0] ad0_sample;
    reg [7:0] ad1_sample;
    reg [2:0] result_kind_hold;
    reg result_seen;
    reg [24:0] heartbeat_count;

    assign AD0_CLK = Clk;
    assign AD1_CLK = Clk;
    assign DA0_Clk = Clk;
    assign DA1_Clk = Clk;
    assign DA0_Data = source_sample;
    assign DA1_Data = source_sample;
    assign receiver_reset_n = reset_n && !mode_press;

    button_press_pulse #(
        .DEBOUNCE_CYCLES(BUTTON_DEBOUNCE_CYCLES)
    ) mode_button_i (
        .clk(Clk),
        .rst_n(reset_n),
        .key_n(key_in),
        .press_pulse(mode_press)
    );

    dac_2023d_signal_source source_i (
        .clk(Clk),
        .rst_n(reset_n),
        .mode(mode),
        .sample_u8(source_sample),
        .symbol_bit_debug(source_symbol_bit)
    );

    receiver_stream_core #(
        .DECIM(200),
        .DECIM_COUNT_BITS(8),
        .BASEBAND_SAMPLE_HZ(250000),
        .CARRIER_PHASE_STEP(32'd171798692),
        .USE_POWER_SCORE(0),
        .FSK_MIN_H_TENTHS(25)
    ) receiver_i (
        .clk(Clk),
        .rst_n(receiver_reset_n),
        .valid_in(1'b1),
        .sample_u8_in(ad0_sample),
        .result_valid(result_valid),
        .signal_kind_code(signal_kind_code),
        .mod_freq_valid(mod_freq_valid),
        .mod_freq_hz(mod_freq_hz),
        .am_ma_valid(am_ma_valid),
        .am_ma_permille(am_ma_permille),
        .fm_param_valid(fm_param_valid),
        .fm_delta_f_hz(fm_delta_f_hz),
        .fm_mf_tenths(fm_mf_tenths),
        .keying_param_valid(keying_param_valid),
        .rc_hz(rc_hz),
        .fsk_param_valid(fsk_param_valid),
        .fsk_freq_sep_hz(fsk_freq_sep_hz),
        .fsk_h_tenths(fsk_h_tenths),
        .analog_kind_debug(analog_kind_debug),
        .keying_kind_debug(keying_kind_debug)
    );

    always @(posedge Clk) begin
        if (!reset_n) begin
            mode <= 3'd0;
            ad0_sample <= 8'h80;
            ad1_sample <= 8'h80;
            result_kind_hold <= 3'd0;
            result_seen <= 1'b0;
            heartbeat_count <= 25'd0;
            led <= 8'd0;
        end else begin
            ad0_sample <= AD0;
            ad1_sample <= AD1;
            heartbeat_count <= heartbeat_count + 1'b1;

            if (mode_press) begin
                mode <= (mode == MODE_LAST) ? 3'd0 : (mode + 1'b1);
                result_kind_hold <= 3'd0;
                result_seen <= 1'b0;
            end

            if (result_valid) begin
                result_kind_hold <= signal_kind_code;
                result_seen <= 1'b1;
            end

            led <= {heartbeat_count[24], result_seen, mode, result_kind_hold};
        end
    end

endmodule
