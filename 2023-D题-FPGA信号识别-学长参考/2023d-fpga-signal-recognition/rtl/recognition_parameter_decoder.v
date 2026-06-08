`timescale 1ns / 1ps

module recognition_parameter_decoder #(
    parameter integer FREQ_HZ_BITS = 16,
    parameter [FREQ_HZ_BITS-1:0] FREQ0_HZ = 16'd1000,
    parameter [FREQ_HZ_BITS-1:0] FREQ1_HZ = 16'd2000,
    parameter [FREQ_HZ_BITS-1:0] FREQ2_HZ = 16'd3000,
    parameter [FREQ_HZ_BITS-1:0] FREQ3_HZ = 16'd4000,
    parameter [FREQ_HZ_BITS-1:0] FREQ4_HZ = 16'd5000
) (
    input wire clk,
    input wire rst_n,
    input wire valid_in,
    input wire [2:0] kind_code,
    input wire [2:0] selected_freq_index,
    input wire [2:0] am_best_index,
    input wire [2:0] fm_best_index,
    output reg param_valid,
    output reg is_cw,
    output reg is_am_like,
    output reg is_fm_like,
    output reg is_psk_like,
    output reg mod_freq_valid,
    output reg [FREQ_HZ_BITS-1:0] mod_freq_hz,
    output reg [FREQ_HZ_BITS-1:0] am_freq_hz,
    output reg [FREQ_HZ_BITS-1:0] fm_freq_hz
);

    localparam [2:0] KIND_CW = 3'd0;
    localparam [2:0] KIND_AM_LIKE = 3'd1;
    localparam [2:0] KIND_FM_LIKE = 3'd2;
    localparam [2:0] KIND_PSK_LIKE = 3'd3;

    function [FREQ_HZ_BITS-1:0] index_to_freq_hz;
        input [2:0] index;
        begin
            case (index)
                3'd0: index_to_freq_hz = FREQ0_HZ;
                3'd1: index_to_freq_hz = FREQ1_HZ;
                3'd2: index_to_freq_hz = FREQ2_HZ;
                3'd3: index_to_freq_hz = FREQ3_HZ;
                3'd4: index_to_freq_hz = FREQ4_HZ;
                default: index_to_freq_hz = {FREQ_HZ_BITS{1'b0}};
            endcase
        end
    endfunction

    wire [FREQ_HZ_BITS-1:0] selected_freq_hz_w;
    wire [FREQ_HZ_BITS-1:0] am_freq_hz_w;
    wire [FREQ_HZ_BITS-1:0] fm_freq_hz_w;
    wire freq_valid_w;

    assign selected_freq_hz_w = index_to_freq_hz(selected_freq_index);
    assign am_freq_hz_w = index_to_freq_hz(am_best_index);
    assign fm_freq_hz_w = index_to_freq_hz(fm_best_index);
    assign freq_valid_w = (kind_code == KIND_AM_LIKE) || (kind_code == KIND_FM_LIKE);

    always @(posedge clk) begin
        if (!rst_n) begin
            param_valid <= 1'b0;
            is_cw <= 1'b0;
            is_am_like <= 1'b0;
            is_fm_like <= 1'b0;
            is_psk_like <= 1'b0;
            mod_freq_valid <= 1'b0;
            mod_freq_hz <= {FREQ_HZ_BITS{1'b0}};
            am_freq_hz <= {FREQ_HZ_BITS{1'b0}};
            fm_freq_hz <= {FREQ_HZ_BITS{1'b0}};
        end else begin
            param_valid <= valid_in;

            if (valid_in) begin
                is_cw <= (kind_code == KIND_CW);
                is_am_like <= (kind_code == KIND_AM_LIKE);
                is_fm_like <= (kind_code == KIND_FM_LIKE);
                is_psk_like <= (kind_code == KIND_PSK_LIKE);
                mod_freq_valid <= freq_valid_w;
                mod_freq_hz <= freq_valid_w ? selected_freq_hz_w : {FREQ_HZ_BITS{1'b0}};
                am_freq_hz <= am_freq_hz_w;
                fm_freq_hz <= fm_freq_hz_w;
            end
        end
    end

endmodule
