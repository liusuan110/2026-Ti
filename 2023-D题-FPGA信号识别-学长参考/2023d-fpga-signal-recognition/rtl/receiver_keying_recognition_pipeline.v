`timescale 1ns / 1ps

module receiver_keying_recognition_pipeline #(
    parameter integer SAMPLE_BITS = 8,
    parameter integer LO_BITS = 12,
    parameter integer DECIM = 80,
    parameter integer DECIM_COUNT_BITS = 7,
    parameter integer BLOCK_LEN = 1024,
    parameter integer BLOCK_COUNT_BITS = 10,
    parameter integer SCORE_BITS = 10,
    parameter integer FREQ_HZ_BITS = 16,
    parameter integer BASEBAND_SAMPLE_HZ = 250000,
    parameter integer MIN_KEY_SCORE = 4,
    parameter integer MIN_TRANSITIONS = 6,
    parameter integer FSK_MIN_SEP_HZ = 9000,
    parameter integer FSK_MIN_H_TENTHS = 18,
    parameter integer FSK_MAX_H_TENTHS = 55
) (
    input wire clk,
    input wire rst_n,
    input wire valid_in,
    input wire signed [SAMPLE_BITS-1:0] sample_in,
    input wire signed [LO_BITS-1:0] carrier_cos_in,
    input wire signed [LO_BITS-1:0] carrier_sin_in,
    output reg keying_valid,
    output reg [2:0] keying_kind_code,
    output reg [1:0] rc_index,
    output reg [FREQ_HZ_BITS-1:0] rc_hz,
    output reg [FREQ_HZ_BITS-1:0] freq_sep_hz,
    output reg [7:0] h_tenths,
    output reg ask_like,
    output reg [SCORE_BITS-1:0] ask_best_score,
    output reg [SCORE_BITS-1:0] psk_best_score,
    output reg [SCORE_BITS-1:0] fsk_best_score,
    output reg [SCORE_BITS-1:0] ask_transition_count,
    output reg [SCORE_BITS-1:0] psk_transition_count,
    output reg [SCORE_BITS-1:0] fsk_transition_count,
    output reg [1:0] ask_rc_index,
    output reg [1:0] psk_rc_index,
    output reg [1:0] fsk_rc_index
);

    localparam [2:0] KIND_UNKNOWN = 3'd0;
    localparam [2:0] KIND_2ASK = 3'd4;
    localparam [2:0] KIND_2PSK = 3'd5;
    localparam [2:0] KIND_2FSK = 3'd6;

    wire ask_valid_w;
    wire [1:0] ask_rc_index_w;
    wire [FREQ_HZ_BITS-1:0] ask_rc_hz_w;
    wire ask_like_w;
    wire [SCORE_BITS-1:0] ask_score0_w;
    wire [SCORE_BITS-1:0] ask_score1_w;
    wire [SCORE_BITS-1:0] ask_score2_w;
    wire [SCORE_BITS-1:0] ask_transition_count_w;

    wire psk_valid_w;
    wire [1:0] psk_rc_index_w;
    wire [FREQ_HZ_BITS-1:0] psk_rc_hz_w;
    wire [SCORE_BITS-1:0] psk_score0_w;
    wire [SCORE_BITS-1:0] psk_score1_w;
    wire [SCORE_BITS-1:0] psk_score2_w;
    wire [SCORE_BITS-1:0] psk_transition_count_w;

    wire fsk_valid_w;
    wire [1:0] fsk_rc_index_w;
    wire [FREQ_HZ_BITS-1:0] fsk_rc_hz_w;
    wire [FREQ_HZ_BITS-1:0] fsk_freq_sep_hz_w;
    wire [7:0] fsk_h_tenths_w;
    wire [SCORE_BITS-1:0] fsk_score0_w;
    wire [SCORE_BITS-1:0] fsk_score1_w;
    wire [SCORE_BITS-1:0] fsk_score2_w;
    wire [SCORE_BITS-1:0] fsk_transition_count_w;

    reg ask_seen;
    reg psk_seen;
    reg fsk_seen;
    reg [1:0] ask_rc_index_hold;
    reg [FREQ_HZ_BITS-1:0] ask_rc_hz_hold;
    reg ask_like_hold;
    reg [SCORE_BITS-1:0] ask_best_score_hold;
    reg [SCORE_BITS-1:0] ask_transition_count_hold;
    reg [1:0] psk_rc_index_hold;
    reg [FREQ_HZ_BITS-1:0] psk_rc_hz_hold;
    reg [SCORE_BITS-1:0] psk_best_score_hold;
    reg [SCORE_BITS-1:0] psk_transition_count_hold;
    reg [1:0] fsk_rc_index_hold;
    reg [FREQ_HZ_BITS-1:0] fsk_rc_hz_hold;
    reg [FREQ_HZ_BITS-1:0] fsk_freq_sep_hz_hold;
    reg [7:0] fsk_h_tenths_hold;
    reg [SCORE_BITS-1:0] fsk_best_score_hold;
    reg [SCORE_BITS-1:0] fsk_transition_count_hold;

    wire ask_seen_next;
    wire psk_seen_next;
    wire fsk_seen_next;
    wire all_seen_next;
    wire [SCORE_BITS-1:0] ask_best_score_w;
    wire [SCORE_BITS-1:0] psk_best_score_w;
    wire [SCORE_BITS-1:0] fsk_best_score_w;
    wire [1:0] ask_rc_index_next;
    wire [FREQ_HZ_BITS-1:0] ask_rc_hz_next;
    wire ask_like_next;
    wire [SCORE_BITS-1:0] ask_best_next;
    wire [SCORE_BITS-1:0] ask_transition_count_next;
    wire [1:0] psk_rc_index_next;
    wire [FREQ_HZ_BITS-1:0] psk_rc_hz_next;
    wire [SCORE_BITS-1:0] psk_best_next;
    wire [SCORE_BITS-1:0] psk_transition_count_next;
    wire [1:0] fsk_rc_index_next;
    wire [FREQ_HZ_BITS-1:0] fsk_rc_hz_next;
    wire [FREQ_HZ_BITS-1:0] fsk_freq_sep_hz_next;
    wire [7:0] fsk_h_tenths_next;
    wire [SCORE_BITS-1:0] fsk_best_next;
    wire [SCORE_BITS-1:0] fsk_transition_count_next;
    wire ask_active_next;
    wire psk_active_next;
    wire fsk_active_next;
    wire [2:0] selected_kind_next;
    wire [1:0] selected_rc_index_next;
    wire [FREQ_HZ_BITS-1:0] selected_rc_hz_next;
    wire [FREQ_HZ_BITS-1:0] selected_freq_sep_next;
    wire [7:0] selected_h_next;
    wire all_seen_hold;
    wire ask_active_hold;
    wire psk_active_hold;
    wire fsk_active_hold;
    wire [2:0] selected_kind_hold;
    wire [1:0] selected_rc_index_hold;
    wire [FREQ_HZ_BITS-1:0] selected_rc_hz_hold;
    wire [FREQ_HZ_BITS-1:0] selected_freq_sep_hold;
    wire [7:0] selected_h_hold;

    function [SCORE_BITS-1:0] max3_score;
        input [SCORE_BITS-1:0] a;
        input [SCORE_BITS-1:0] b;
        input [SCORE_BITS-1:0] c;
        begin
            if ((a >= b) && (a >= c)) begin
                max3_score = a;
            end else if (b >= c) begin
                max3_score = b;
            end else begin
                max3_score = c;
            end
        end
    endfunction

    assign ask_best_score_w = max3_score(ask_score0_w, ask_score1_w, ask_score2_w);
    assign psk_best_score_w = max3_score(psk_score0_w, psk_score1_w, psk_score2_w);
    assign fsk_best_score_w = max3_score(fsk_score0_w, fsk_score1_w, fsk_score2_w);

    assign ask_seen_next = ask_seen || ask_valid_w;
    assign psk_seen_next = psk_seen || psk_valid_w;
    assign fsk_seen_next = fsk_seen || fsk_valid_w;
    assign all_seen_next = ask_seen_next && psk_seen_next && fsk_seen_next;

    assign ask_rc_index_next = ask_valid_w ? ask_rc_index_w : ask_rc_index_hold;
    assign ask_rc_hz_next = ask_valid_w ? ask_rc_hz_w : ask_rc_hz_hold;
    assign ask_like_next = ask_valid_w ? ask_like_w : ask_like_hold;
    assign ask_best_next = ask_valid_w ? ask_best_score_w : ask_best_score_hold;
    assign ask_transition_count_next = ask_valid_w ? ask_transition_count_w : ask_transition_count_hold;

    assign psk_rc_index_next = psk_valid_w ? psk_rc_index_w : psk_rc_index_hold;
    assign psk_rc_hz_next = psk_valid_w ? psk_rc_hz_w : psk_rc_hz_hold;
    assign psk_best_next = psk_valid_w ? psk_best_score_w : psk_best_score_hold;
    assign psk_transition_count_next = psk_valid_w ? psk_transition_count_w : psk_transition_count_hold;

    assign fsk_rc_index_next = fsk_valid_w ? fsk_rc_index_w : fsk_rc_index_hold;
    assign fsk_rc_hz_next = fsk_valid_w ? fsk_rc_hz_w : fsk_rc_hz_hold;
    assign fsk_freq_sep_hz_next = fsk_valid_w ? fsk_freq_sep_hz_w : fsk_freq_sep_hz_hold;
    assign fsk_h_tenths_next = fsk_valid_w ? fsk_h_tenths_w : fsk_h_tenths_hold;
    assign fsk_best_next = fsk_valid_w ? fsk_best_score_w : fsk_best_score_hold;
    assign fsk_transition_count_next = fsk_valid_w ? fsk_transition_count_w : fsk_transition_count_hold;

    assign ask_active_next = ask_like_next &&
                             (ask_best_next >= MIN_KEY_SCORE[SCORE_BITS-1:0]) &&
                             (ask_transition_count_next >= MIN_TRANSITIONS[SCORE_BITS-1:0]);
    assign psk_active_next = (psk_best_next >= MIN_KEY_SCORE[SCORE_BITS-1:0]) &&
                             (psk_transition_count_next >= MIN_TRANSITIONS[SCORE_BITS-1:0]);
    assign fsk_active_next = (fsk_best_next >= MIN_KEY_SCORE[SCORE_BITS-1:0]) &&
                             (fsk_transition_count_next >= MIN_TRANSITIONS[SCORE_BITS-1:0]) &&
                             (fsk_freq_sep_hz_next >= FSK_MIN_SEP_HZ[FREQ_HZ_BITS-1:0]) &&
                             (fsk_h_tenths_next >= FSK_MIN_H_TENTHS[7:0]) &&
                             (fsk_h_tenths_next <= FSK_MAX_H_TENTHS[7:0]);

    assign selected_kind_next = psk_active_next ? KIND_2PSK :
                                ask_active_next ? KIND_2ASK :
                                fsk_active_next ? KIND_2FSK :
                                KIND_UNKNOWN;
    assign selected_rc_index_next = psk_active_next ? psk_rc_index_next :
                                    ask_active_next ? ask_rc_index_next :
                                    fsk_active_next ? fsk_rc_index_next :
                                    2'd0;
    assign selected_rc_hz_next = psk_active_next ? psk_rc_hz_next :
                                 ask_active_next ? ask_rc_hz_next :
                                 fsk_active_next ? fsk_rc_hz_next :
                                 {FREQ_HZ_BITS{1'b0}};
    assign selected_freq_sep_next = fsk_active_next ? fsk_freq_sep_hz_next : {FREQ_HZ_BITS{1'b0}};
    assign selected_h_next = fsk_active_next ? fsk_h_tenths_next : 8'd0;

    assign all_seen_hold = ask_seen && psk_seen && fsk_seen;
    assign ask_active_hold = ask_like_hold &&
                             (ask_best_score_hold >= MIN_KEY_SCORE[SCORE_BITS-1:0]) &&
                             (ask_transition_count_hold >= MIN_TRANSITIONS[SCORE_BITS-1:0]);
    assign psk_active_hold = (psk_best_score_hold >= MIN_KEY_SCORE[SCORE_BITS-1:0]) &&
                             (psk_transition_count_hold >= MIN_TRANSITIONS[SCORE_BITS-1:0]);
    assign fsk_active_hold = (fsk_best_score_hold >= MIN_KEY_SCORE[SCORE_BITS-1:0]) &&
                             (fsk_transition_count_hold >= MIN_TRANSITIONS[SCORE_BITS-1:0]) &&
                             (fsk_freq_sep_hz_hold >= FSK_MIN_SEP_HZ[FREQ_HZ_BITS-1:0]) &&
                             (fsk_h_tenths_hold >= FSK_MIN_H_TENTHS[7:0]) &&
                             (fsk_h_tenths_hold <= FSK_MAX_H_TENTHS[7:0]);
    assign selected_kind_hold = psk_active_hold ? KIND_2PSK :
                                ask_active_hold ? KIND_2ASK :
                                fsk_active_hold ? KIND_2FSK :
                                KIND_UNKNOWN;
    assign selected_rc_index_hold = psk_active_hold ? psk_rc_index_hold :
                                    ask_active_hold ? ask_rc_index_hold :
                                    fsk_active_hold ? fsk_rc_index_hold :
                                    2'd0;
    assign selected_rc_hz_hold = psk_active_hold ? psk_rc_hz_hold :
                                 ask_active_hold ? ask_rc_hz_hold :
                                 fsk_active_hold ? fsk_rc_hz_hold :
                                 {FREQ_HZ_BITS{1'b0}};
    assign selected_freq_sep_hold = fsk_active_hold ? fsk_freq_sep_hz_hold : {FREQ_HZ_BITS{1'b0}};
    assign selected_h_hold = fsk_active_hold ? fsk_h_tenths_hold : 8'd0;

    receiver_ask_rate_pipeline #(
        .SAMPLE_BITS(SAMPLE_BITS),
        .LO_BITS(LO_BITS),
        .DECIM(DECIM),
        .DECIM_COUNT_BITS(DECIM_COUNT_BITS),
        .BLOCK_LEN(BLOCK_LEN),
        .BLOCK_COUNT_BITS(BLOCK_COUNT_BITS),
        .SCORE_BITS(SCORE_BITS),
        .FREQ_HZ_BITS(FREQ_HZ_BITS),
        .ASK_MINMAX_PERCENT(3)
    ) ask_i (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(valid_in),
        .sample_in(sample_in),
        .carrier_cos_in(carrier_cos_in),
        .carrier_sin_in(carrier_sin_in),
        .ask_rate_valid(ask_valid_w),
        .rc_index(ask_rc_index_w),
        .rc_hz(ask_rc_hz_w),
        .ask_like(ask_like_w),
        .score0(ask_score0_w),
        .score1(ask_score1_w),
        .score2(ask_score2_w),
        .transition_count(ask_transition_count_w),
        .threshold_ready_debug(),
        .bit_valid_debug(),
        .bit_debug()
    );

    receiver_psk_rate_pipeline #(
        .SAMPLE_BITS(SAMPLE_BITS),
        .LO_BITS(LO_BITS),
        .DECIM(DECIM),
        .DECIM_COUNT_BITS(DECIM_COUNT_BITS),
        .BLOCK_LEN(BLOCK_LEN),
        .BLOCK_COUNT_BITS(BLOCK_COUNT_BITS),
        .SCORE_BITS(SCORE_BITS),
        .FREQ_HZ_BITS(FREQ_HZ_BITS)
    ) psk_i (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(valid_in),
        .sample_in(sample_in),
        .carrier_cos_in(carrier_cos_in),
        .carrier_sin_in(carrier_sin_in),
        .psk_rate_valid(psk_valid_w),
        .rc_index(psk_rc_index_w),
        .rc_hz(psk_rc_hz_w),
        .score0(psk_score0_w),
        .score1(psk_score1_w),
        .score2(psk_score2_w),
        .transition_count(psk_transition_count_w),
        .bit_valid_debug(),
        .bit_debug(),
        .phase_flip_event_debug()
    );

    receiver_fsk_rate_h_pipeline #(
        .SAMPLE_BITS(SAMPLE_BITS),
        .LO_BITS(LO_BITS),
        .DECIM(DECIM),
        .DECIM_COUNT_BITS(DECIM_COUNT_BITS),
        .BLOCK_LEN(BLOCK_LEN),
        .BLOCK_COUNT_BITS(BLOCK_COUNT_BITS),
        .SCORE_BITS(SCORE_BITS),
        .FREQ_HZ_BITS(FREQ_HZ_BITS),
        .BASEBAND_SAMPLE_HZ(BASEBAND_SAMPLE_HZ)
    ) fsk_i (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(valid_in),
        .sample_in(sample_in),
        .carrier_cos_in(carrier_cos_in),
        .carrier_sin_in(carrier_sin_in),
        .fsk_result_valid(fsk_valid_w),
        .rc_index(fsk_rc_index_w),
        .rc_hz(fsk_rc_hz_w),
        .freq_sep_hz(fsk_freq_sep_hz_w),
        .h_tenths(fsk_h_tenths_w),
        .score0(fsk_score0_w),
        .score1(fsk_score1_w),
        .score2(fsk_score2_w),
        .transition_count(fsk_transition_count_w),
        .threshold_ready_debug(),
        .bit_valid_debug(),
        .bit_debug(),
        .phase_delta_valid_debug(),
        .phase_delta_debug()
    );

    always @(posedge clk) begin
        if (!rst_n) begin
            keying_valid <= 1'b0;
            keying_kind_code <= KIND_UNKNOWN;
            rc_index <= 2'd0;
            rc_hz <= {FREQ_HZ_BITS{1'b0}};
            freq_sep_hz <= {FREQ_HZ_BITS{1'b0}};
            h_tenths <= 8'd0;
            ask_like <= 1'b0;
            ask_best_score <= {SCORE_BITS{1'b0}};
            psk_best_score <= {SCORE_BITS{1'b0}};
            fsk_best_score <= {SCORE_BITS{1'b0}};
            ask_transition_count <= {SCORE_BITS{1'b0}};
            psk_transition_count <= {SCORE_BITS{1'b0}};
            fsk_transition_count <= {SCORE_BITS{1'b0}};
            ask_rc_index <= 2'd0;
            psk_rc_index <= 2'd0;
            fsk_rc_index <= 2'd0;
            ask_seen <= 1'b0;
            psk_seen <= 1'b0;
            fsk_seen <= 1'b0;
            ask_rc_index_hold <= 2'd0;
            ask_rc_hz_hold <= {FREQ_HZ_BITS{1'b0}};
            ask_like_hold <= 1'b0;
            ask_best_score_hold <= {SCORE_BITS{1'b0}};
            ask_transition_count_hold <= {SCORE_BITS{1'b0}};
            psk_rc_index_hold <= 2'd0;
            psk_rc_hz_hold <= {FREQ_HZ_BITS{1'b0}};
            psk_best_score_hold <= {SCORE_BITS{1'b0}};
            psk_transition_count_hold <= {SCORE_BITS{1'b0}};
            fsk_rc_index_hold <= 2'd0;
            fsk_rc_hz_hold <= {FREQ_HZ_BITS{1'b0}};
            fsk_freq_sep_hz_hold <= {FREQ_HZ_BITS{1'b0}};
            fsk_h_tenths_hold <= 8'd0;
            fsk_best_score_hold <= {SCORE_BITS{1'b0}};
            fsk_transition_count_hold <= {SCORE_BITS{1'b0}};
        end else begin
            keying_valid <= 1'b0;

            if (all_seen_hold) begin
                keying_valid <= 1'b1;
                keying_kind_code <= selected_kind_hold;
                rc_index <= selected_rc_index_hold;
                rc_hz <= selected_rc_hz_hold;
                freq_sep_hz <= selected_freq_sep_hold;
                h_tenths <= selected_h_hold;
                ask_like <= ask_like_hold;
                ask_best_score <= ask_best_score_hold;
                psk_best_score <= psk_best_score_hold;
                fsk_best_score <= fsk_best_score_hold;
                ask_transition_count <= ask_transition_count_hold;
                psk_transition_count <= psk_transition_count_hold;
                fsk_transition_count <= fsk_transition_count_hold;
                ask_rc_index <= ask_rc_index_hold;
                psk_rc_index <= psk_rc_index_hold;
                fsk_rc_index <= fsk_rc_index_hold;
                ask_seen <= 1'b0;
                psk_seen <= 1'b0;
                fsk_seen <= 1'b0;
            end else begin
                ask_seen <= ask_seen_next;
                psk_seen <= psk_seen_next;
                fsk_seen <= fsk_seen_next;

                if (ask_valid_w) begin
                    ask_rc_index_hold <= ask_rc_index_w;
                    ask_rc_hz_hold <= ask_rc_hz_w;
                    ask_like_hold <= ask_like_w;
                    ask_best_score_hold <= ask_best_score_w;
                    ask_transition_count_hold <= ask_transition_count_w;
                end

                if (psk_valid_w) begin
                    psk_rc_index_hold <= psk_rc_index_w;
                    psk_rc_hz_hold <= psk_rc_hz_w;
                    psk_best_score_hold <= psk_best_score_w;
                    psk_transition_count_hold <= psk_transition_count_w;
                end

                if (fsk_valid_w) begin
                    fsk_rc_index_hold <= fsk_rc_index_w;
                    fsk_rc_hz_hold <= fsk_rc_hz_w;
                    fsk_freq_sep_hz_hold <= fsk_freq_sep_hz_w;
                    fsk_h_tenths_hold <= fsk_h_tenths_w;
                    fsk_best_score_hold <= fsk_best_score_w;
                    fsk_transition_count_hold <= fsk_transition_count_w;
                end
            end
        end
    end

endmodule
