`timescale 1ns / 1ps

module receiver_psk_rate_pipeline #(
    parameter integer SAMPLE_BITS = 8,
    parameter integer LO_BITS = 12,
    parameter integer DECIM = 80,
    parameter integer DECIM_COUNT_BITS = 7,
    parameter integer DDC_PRODUCT_BITS = SAMPLE_BITS + LO_BITS + 1,
    parameter integer IQ_BITS = DDC_PRODUCT_BITS + DECIM_COUNT_BITS,
    parameter integer BLOCK_LEN = 1024,
    parameter integer BLOCK_COUNT_BITS = 10,
    parameter integer FEATURE_PRODUCT_BITS = 2 * IQ_BITS,
    parameter integer MAG_SQ_BITS = FEATURE_PRODUCT_BITS + 1,
    parameter integer DOT_BITS = FEATURE_PRODUCT_BITS + 1,
    parameter integer SCORE_BITS = 10,
    parameter integer FREQ_HZ_BITS = 16,
    parameter integer COOLDOWN_SAMPLES = 8,
    parameter integer COOLDOWN_BITS = 4
) (
    input wire clk,
    input wire rst_n,
    input wire valid_in,
    input wire signed [SAMPLE_BITS-1:0] sample_in,
    input wire signed [LO_BITS-1:0] carrier_cos_in,
    input wire signed [LO_BITS-1:0] carrier_sin_in,
    output wire psk_rate_valid,
    output wire [1:0] rc_index,
    output wire [FREQ_HZ_BITS-1:0] rc_hz,
    output wire [SCORE_BITS-1:0] score0,
    output wire [SCORE_BITS-1:0] score1,
    output wire [SCORE_BITS-1:0] score2,
    output wire [SCORE_BITS-1:0] transition_count,
    output wire bit_valid_debug,
    output wire bit_debug,
    output wire phase_flip_event_debug
);

    wire ddc_valid;
    wire signed [IQ_BITS-1:0] i_sum;
    wire signed [IQ_BITS-1:0] q_sum;
    wire feature_valid;
    wire delta_valid;
    wire dot_negative;
    wire cooldown_clear;
    wire phase_flip_event;
    wire bit_next;

    reg bit_state;
    reg [COOLDOWN_BITS-1:0] cooldown;

    assign cooldown_clear = (cooldown == {COOLDOWN_BITS{1'b0}});
    assign phase_flip_event = feature_valid && delta_valid && dot_negative && cooldown_clear;
    assign bit_next = phase_flip_event ? ~bit_state : bit_state;
    assign bit_valid_debug = feature_valid;
    assign bit_debug = bit_next;
    assign phase_flip_event_debug = phase_flip_event;
    assign rc_hz = (rc_index == 2'd0) ? 16'd6000 :
                   (rc_index == 2'd1) ? 16'd8000 :
                   (rc_index == 2'd2) ? 16'd10000 :
                   16'd0;

    ddc_boxcar_decimator #(
        .SAMPLE_BITS(SAMPLE_BITS),
        .LO_BITS(LO_BITS),
        .DECIM(DECIM),
        .COUNT_BITS(DECIM_COUNT_BITS),
        .PRODUCT_BITS(DDC_PRODUCT_BITS),
        .ACC_BITS(IQ_BITS)
    ) ddc_i (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(valid_in),
        .sample_in(sample_in),
        .cos_in(carrier_cos_in),
        .sin_in(carrier_sin_in),
        .valid_out(ddc_valid),
        .i_sum(i_sum),
        .q_sum(q_sum)
    );

    iq_dot_sign_extractor #(
        .IQ_BITS(IQ_BITS),
        .PRODUCT_BITS(FEATURE_PRODUCT_BITS),
        .DOT_BITS(DOT_BITS)
    ) feature_i (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(ddc_valid),
        .i_in(i_sum),
        .q_in(q_sum),
        .valid_out(feature_valid),
        .delta_valid(delta_valid),
        .dot_negative(dot_negative)
    );

    transition_rate_estimator #(
        .BLOCK_LEN(BLOCK_LEN),
        .COUNT_BITS(BLOCK_COUNT_BITS),
        .SCORE_BITS(SCORE_BITS)
    ) rate_i (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(feature_valid),
        .bit_in(bit_next),
        .result_valid(psk_rate_valid),
        .best_index(rc_index),
        .score0(score0),
        .score1(score1),
        .score2(score2),
        .transition_count(transition_count)
    );

    always @(posedge clk) begin
        if (!rst_n) begin
            bit_state <= 1'b0;
            cooldown <= {COOLDOWN_BITS{1'b0}};
        end else if (feature_valid) begin
            bit_state <= bit_next;

            if (phase_flip_event) begin
                cooldown <= COOLDOWN_SAMPLES[COOLDOWN_BITS-1:0];
            end else if (!cooldown_clear) begin
                cooldown <= cooldown - 1'b1;
            end
        end
    end

endmodule
