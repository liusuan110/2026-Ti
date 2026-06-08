`timescale 1ns / 1ps

module receiver_full_scope_pipeline #(
    parameter integer SAMPLE_BITS = 8,
    parameter integer LO_BITS = 12,
    parameter integer REF_BITS = 12,
    parameter integer DECIM = 80,
    parameter integer DECIM_COUNT_BITS = 7,
    parameter integer FREQ_HZ_BITS = 16,
    parameter integer BASEBAND_SAMPLE_HZ = 250000,
    parameter integer USE_POWER_SCORE = 1,
    parameter integer FSK_MIN_H_TENTHS = 18
) (
    input wire clk,
    input wire rst_n,
    input wire valid_in,
    input wire signed [SAMPLE_BITS-1:0] sample_in,
    input wire signed [LO_BITS-1:0] carrier_cos_in,
    input wire signed [LO_BITS-1:0] carrier_sin_in,
    input wire signed [REF_BITS-1:0] ref0_cos,
    input wire signed [REF_BITS-1:0] ref0_sin,
    input wire signed [REF_BITS-1:0] ref1_cos,
    input wire signed [REF_BITS-1:0] ref1_sin,
    input wire signed [REF_BITS-1:0] ref2_cos,
    input wire signed [REF_BITS-1:0] ref2_sin,
    input wire signed [REF_BITS-1:0] ref3_cos,
    input wire signed [REF_BITS-1:0] ref3_sin,
    input wire signed [REF_BITS-1:0] ref4_cos,
    input wire signed [REF_BITS-1:0] ref4_sin,
    output reg result_valid,
    output reg [2:0] signal_kind_code,
    output reg mod_freq_valid,
    output reg [FREQ_HZ_BITS-1:0] mod_freq_hz,
    output reg am_ma_valid,
    output reg [9:0] am_ma_permille,
    output reg fm_param_valid,
    output reg [FREQ_HZ_BITS-1:0] fm_delta_f_hz,
    output reg [7:0] fm_mf_tenths,
    output reg keying_param_valid,
    output reg [FREQ_HZ_BITS-1:0] rc_hz,
    output reg fsk_param_valid,
    output reg [FREQ_HZ_BITS-1:0] fsk_freq_sep_hz,
    output reg [7:0] fsk_h_tenths,
    output reg [2:0] analog_kind_debug,
    output reg [2:0] keying_kind_debug
);

    localparam [2:0] KIND_CW = 3'd0;
    localparam [2:0] KIND_AM = 3'd1;
    localparam [2:0] KIND_FM = 3'd2;
    localparam [2:0] KIND_2ASK = 3'd4;
    localparam [2:0] KIND_2PSK = 3'd5;
    localparam [2:0] KIND_2FSK = 3'd6;

    wire analog_valid_w;
    wire [2:0] analog_kind_w;
    wire [2:0] analog_selected_freq_index_w;
    wire [2:0] analog_coarse_flags_w;
    wire [2:0] analog_am_best_index_w;
    wire [2:0] analog_fm_best_index_w;
    wire analog_is_cw_w;
    wire analog_is_am_w;
    wire analog_is_fm_w;
    wire analog_is_psk_like_w;
    wire analog_mod_freq_valid_w;
    wire [FREQ_HZ_BITS-1:0] analog_mod_freq_hz_w;
    wire [FREQ_HZ_BITS-1:0] analog_am_freq_hz_w;
    wire [FREQ_HZ_BITS-1:0] analog_fm_freq_hz_w;
    wire analog_am_ma_valid_w;
    wire [9:0] analog_am_ma_permille_w;
    wire analog_fm_param_valid_w;
    wire [FREQ_HZ_BITS-1:0] analog_fm_delta_f_hz_w;
    wire [7:0] analog_fm_mf_tenths_w;

    wire keying_valid_w;
    wire [2:0] keying_kind_w;
    wire [1:0] keying_rc_index_w;
    wire [FREQ_HZ_BITS-1:0] keying_rc_hz_w;
    wire [FREQ_HZ_BITS-1:0] keying_freq_sep_hz_w;
    wire [7:0] keying_h_tenths_w;
    wire keying_ask_like_w;
    wire [9:0] keying_ask_best_score_w;
    wire [9:0] keying_psk_best_score_w;
    wire [9:0] keying_fsk_best_score_w;
    wire [9:0] keying_ask_transition_count_w;
    wire [9:0] keying_psk_transition_count_w;
    wire [9:0] keying_fsk_transition_count_w;
    wire [1:0] keying_ask_rc_index_w;
    wire [1:0] keying_psk_rc_index_w;
    wire [1:0] keying_fsk_rc_index_w;

    reg analog_seen;
    reg keying_seen;
    reg [2:0] analog_kind_hold;
    reg analog_mod_freq_valid_hold;
    reg [FREQ_HZ_BITS-1:0] analog_mod_freq_hz_hold;
    reg analog_am_ma_valid_hold;
    reg [9:0] analog_am_ma_permille_hold;
    reg analog_fm_param_valid_hold;
    reg [FREQ_HZ_BITS-1:0] analog_fm_delta_f_hz_hold;
    reg [7:0] analog_fm_mf_tenths_hold;
    reg [2:0] keying_kind_hold;
    reg [FREQ_HZ_BITS-1:0] keying_rc_hz_hold;
    reg [FREQ_HZ_BITS-1:0] keying_freq_sep_hz_hold;
    reg [7:0] keying_h_tenths_hold;

    wire analog_seen_next;
    wire keying_seen_next;
    wire all_seen_next;
    wire [2:0] analog_kind_next;
    wire analog_mod_freq_valid_next;
    wire [FREQ_HZ_BITS-1:0] analog_mod_freq_hz_next;
    wire analog_am_ma_valid_next;
    wire [9:0] analog_am_ma_permille_next;
    wire analog_fm_param_valid_next;
    wire [FREQ_HZ_BITS-1:0] analog_fm_delta_f_hz_next;
    wire [7:0] analog_fm_mf_tenths_next;
    wire [2:0] keying_kind_next;
    wire [FREQ_HZ_BITS-1:0] keying_rc_hz_next;
    wire [FREQ_HZ_BITS-1:0] keying_freq_sep_hz_next;
    wire [7:0] keying_h_tenths_next;
    wire use_keying_next;
    wire selected_is_am_next;
    wire selected_is_fm_next;
    wire selected_is_keying_next;
    wire selected_is_fsk_next;

    assign analog_seen_next = analog_seen || analog_valid_w;
    assign keying_seen_next = keying_seen || keying_valid_w;
    assign all_seen_next = analog_seen_next && keying_seen_next;

    assign analog_kind_next = analog_valid_w ? analog_kind_w : analog_kind_hold;
    assign analog_mod_freq_valid_next = analog_valid_w ? analog_mod_freq_valid_w : analog_mod_freq_valid_hold;
    assign analog_mod_freq_hz_next = analog_valid_w ? analog_mod_freq_hz_w : analog_mod_freq_hz_hold;
    assign analog_am_ma_valid_next = analog_valid_w ? analog_am_ma_valid_w : analog_am_ma_valid_hold;
    assign analog_am_ma_permille_next = analog_valid_w ? analog_am_ma_permille_w : analog_am_ma_permille_hold;
    assign analog_fm_param_valid_next = analog_valid_w ? analog_fm_param_valid_w : analog_fm_param_valid_hold;
    assign analog_fm_delta_f_hz_next = analog_valid_w ? analog_fm_delta_f_hz_w : analog_fm_delta_f_hz_hold;
    assign analog_fm_mf_tenths_next = analog_valid_w ? analog_fm_mf_tenths_w : analog_fm_mf_tenths_hold;
    assign keying_kind_next = keying_valid_w ? keying_kind_w : keying_kind_hold;
    assign keying_rc_hz_next = keying_valid_w ? keying_rc_hz_w : keying_rc_hz_hold;
    assign keying_freq_sep_hz_next = keying_valid_w ? keying_freq_sep_hz_w : keying_freq_sep_hz_hold;
    assign keying_h_tenths_next = keying_valid_w ? keying_h_tenths_w : keying_h_tenths_hold;

    assign use_keying_next = (keying_kind_next == KIND_2ASK) ||
                             (keying_kind_next == KIND_2PSK) ||
                             (keying_kind_next == KIND_2FSK);
    assign selected_is_am_next = (!use_keying_next) && (analog_kind_next == KIND_AM);
    assign selected_is_fm_next = (!use_keying_next) && (analog_kind_next == KIND_FM);
    assign selected_is_keying_next = use_keying_next;
    assign selected_is_fsk_next = use_keying_next && (keying_kind_next == KIND_2FSK);

    receiver_recognition_with_params #(
        .SAMPLE_BITS(SAMPLE_BITS),
        .LO_BITS(LO_BITS),
        .REF_BITS(REF_BITS),
        .DECIM(DECIM),
        .DECIM_COUNT_BITS(DECIM_COUNT_BITS),
        .FREQ_HZ_BITS(FREQ_HZ_BITS),
        .BASEBAND_SAMPLE_HZ(BASEBAND_SAMPLE_HZ),
        .USE_POWER_SCORE(USE_POWER_SCORE)
    ) analog_i (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(valid_in),
        .sample_in(sample_in),
        .carrier_cos_in(carrier_cos_in),
        .carrier_sin_in(carrier_sin_in),
        .ref0_cos(ref0_cos),
        .ref0_sin(ref0_sin),
        .ref1_cos(ref1_cos),
        .ref1_sin(ref1_sin),
        .ref2_cos(ref2_cos),
        .ref2_sin(ref2_sin),
        .ref3_cos(ref3_cos),
        .ref3_sin(ref3_sin),
        .ref4_cos(ref4_cos),
        .ref4_sin(ref4_sin),
        .param_valid(analog_valid_w),
        .kind_code(analog_kind_w),
        .selected_freq_index(analog_selected_freq_index_w),
        .coarse_flags(analog_coarse_flags_w),
        .am_best_index(analog_am_best_index_w),
        .fm_best_index(analog_fm_best_index_w),
        .is_cw(analog_is_cw_w),
        .is_am_like(analog_is_am_w),
        .is_fm_like(analog_is_fm_w),
        .is_psk_like(analog_is_psk_like_w),
        .mod_freq_valid(analog_mod_freq_valid_w),
        .mod_freq_hz(analog_mod_freq_hz_w),
        .am_freq_hz(analog_am_freq_hz_w),
        .fm_freq_hz(analog_fm_freq_hz_w),
        .am_ma_valid(analog_am_ma_valid_w),
        .am_ma_permille(analog_am_ma_permille_w),
        .fm_param_valid(analog_fm_param_valid_w),
        .fm_delta_f_hz(analog_fm_delta_f_hz_w),
        .fm_mf_tenths(analog_fm_mf_tenths_w),
        .recognition_valid_debug()
    );

    receiver_keying_recognition_pipeline #(
        .SAMPLE_BITS(SAMPLE_BITS),
        .LO_BITS(LO_BITS),
        .DECIM(DECIM),
        .DECIM_COUNT_BITS(DECIM_COUNT_BITS),
        .FREQ_HZ_BITS(FREQ_HZ_BITS),
        .BASEBAND_SAMPLE_HZ(BASEBAND_SAMPLE_HZ),
        .FSK_MIN_H_TENTHS(FSK_MIN_H_TENTHS)
    ) keying_i (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(valid_in),
        .sample_in(sample_in),
        .carrier_cos_in(carrier_cos_in),
        .carrier_sin_in(carrier_sin_in),
        .keying_valid(keying_valid_w),
        .keying_kind_code(keying_kind_w),
        .rc_index(keying_rc_index_w),
        .rc_hz(keying_rc_hz_w),
        .freq_sep_hz(keying_freq_sep_hz_w),
        .h_tenths(keying_h_tenths_w),
        .ask_like(keying_ask_like_w),
        .ask_best_score(keying_ask_best_score_w),
        .psk_best_score(keying_psk_best_score_w),
        .fsk_best_score(keying_fsk_best_score_w),
        .ask_transition_count(keying_ask_transition_count_w),
        .psk_transition_count(keying_psk_transition_count_w),
        .fsk_transition_count(keying_fsk_transition_count_w),
        .ask_rc_index(keying_ask_rc_index_w),
        .psk_rc_index(keying_psk_rc_index_w),
        .fsk_rc_index(keying_fsk_rc_index_w)
    );

    always @(posedge clk) begin
        if (!rst_n) begin
            result_valid <= 1'b0;
            signal_kind_code <= KIND_CW;
            mod_freq_valid <= 1'b0;
            mod_freq_hz <= {FREQ_HZ_BITS{1'b0}};
            am_ma_valid <= 1'b0;
            am_ma_permille <= 10'd0;
            fm_param_valid <= 1'b0;
            fm_delta_f_hz <= {FREQ_HZ_BITS{1'b0}};
            fm_mf_tenths <= 8'd0;
            keying_param_valid <= 1'b0;
            rc_hz <= {FREQ_HZ_BITS{1'b0}};
            fsk_param_valid <= 1'b0;
            fsk_freq_sep_hz <= {FREQ_HZ_BITS{1'b0}};
            fsk_h_tenths <= 8'd0;
            analog_kind_debug <= KIND_CW;
            keying_kind_debug <= KIND_CW;
            analog_seen <= 1'b0;
            keying_seen <= 1'b0;
            analog_kind_hold <= KIND_CW;
            analog_mod_freq_valid_hold <= 1'b0;
            analog_mod_freq_hz_hold <= {FREQ_HZ_BITS{1'b0}};
            analog_am_ma_valid_hold <= 1'b0;
            analog_am_ma_permille_hold <= 10'd0;
            analog_fm_param_valid_hold <= 1'b0;
            analog_fm_delta_f_hz_hold <= {FREQ_HZ_BITS{1'b0}};
            analog_fm_mf_tenths_hold <= 8'd0;
            keying_kind_hold <= KIND_CW;
            keying_rc_hz_hold <= {FREQ_HZ_BITS{1'b0}};
            keying_freq_sep_hz_hold <= {FREQ_HZ_BITS{1'b0}};
            keying_h_tenths_hold <= 8'd0;
        end else begin
            result_valid <= 1'b0;

            if (all_seen_next) begin
                result_valid <= 1'b1;
                signal_kind_code <= use_keying_next ? keying_kind_next : analog_kind_next;
                mod_freq_valid <= (!use_keying_next) && analog_mod_freq_valid_next;
                mod_freq_hz <= (!use_keying_next) ? analog_mod_freq_hz_next : {FREQ_HZ_BITS{1'b0}};
                am_ma_valid <= selected_is_am_next && analog_am_ma_valid_next;
                am_ma_permille <= selected_is_am_next ? analog_am_ma_permille_next : 10'd0;
                fm_param_valid <= selected_is_fm_next && analog_fm_param_valid_next;
                fm_delta_f_hz <= selected_is_fm_next ? analog_fm_delta_f_hz_next : {FREQ_HZ_BITS{1'b0}};
                fm_mf_tenths <= selected_is_fm_next ? analog_fm_mf_tenths_next : 8'd0;
                keying_param_valid <= selected_is_keying_next;
                rc_hz <= selected_is_keying_next ? keying_rc_hz_next : {FREQ_HZ_BITS{1'b0}};
                fsk_param_valid <= selected_is_fsk_next;
                fsk_freq_sep_hz <= selected_is_fsk_next ? keying_freq_sep_hz_next : {FREQ_HZ_BITS{1'b0}};
                fsk_h_tenths <= selected_is_fsk_next ? keying_h_tenths_next : 8'd0;
                analog_kind_debug <= analog_kind_next;
                keying_kind_debug <= keying_kind_next;
                analog_seen <= 1'b0;
                keying_seen <= 1'b0;
            end else begin
                analog_seen <= analog_seen_next;
                keying_seen <= keying_seen_next;

                if (analog_valid_w) begin
                    analog_kind_hold <= analog_kind_w;
                    analog_mod_freq_valid_hold <= analog_mod_freq_valid_w;
                    analog_mod_freq_hz_hold <= analog_mod_freq_hz_w;
                    analog_am_ma_valid_hold <= analog_am_ma_valid_w;
                    analog_am_ma_permille_hold <= analog_am_ma_permille_w;
                    analog_fm_param_valid_hold <= analog_fm_param_valid_w;
                    analog_fm_delta_f_hz_hold <= analog_fm_delta_f_hz_w;
                    analog_fm_mf_tenths_hold <= analog_fm_mf_tenths_w;
                end

                if (keying_valid_w) begin
                    keying_kind_hold <= keying_kind_w;
                    keying_rc_hz_hold <= keying_rc_hz_w;
                    keying_freq_sep_hz_hold <= keying_freq_sep_hz_w;
                    keying_h_tenths_hold <= keying_h_tenths_w;
                end
            end
        end
    end

endmodule
