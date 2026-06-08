`timescale 1ns / 1ps

module recognition_fsm_skeleton (
    input wire clk,
    input wire rst_n,
    input wire start,

    input wire am_result_valid,
    input wire am_is_am,
    input wire am_params_valid,
    input wire [9:0] am_ma_permille,
    input wire [15:0] am_f_hz,

    input wire fm_cw_result_valid,
    input wire fm_is_fm,
    input wire fm_params_valid,
    input wire [15:0] fm_f_hz,
    input wire [15:0] fm_mf_permille,
    input wire [31:0] fm_delta_f_hz,

    output reg collect_enable,
    output reg am_branch_enable,
    output reg fm_branch_enable,
    output reg result_valid,
    output reg [1:0] modulation_kind,
    output reg [15:0] result_f_hz,
    output reg [15:0] result_param_permille,
    output reg [31:0] result_delta_f_hz,
    output reg [3:0] state_debug
);

    localparam [3:0] S_IDLE = 4'd0;
    localparam [3:0] S_COLLECT = 4'd1;
    localparam [3:0] S_WAIT_AM_DECISION = 4'd2;
    localparam [3:0] S_WAIT_AM_PARAMS = 4'd3;
    localparam [3:0] S_WAIT_FM_CW_DECISION = 4'd4;
    localparam [3:0] S_WAIT_FM_PARAMS = 4'd5;
    localparam [3:0] S_OUTPUT = 4'd6;

    localparam [1:0] KIND_CW = 2'd0;
    localparam [1:0] KIND_AM = 2'd1;
    localparam [1:0] KIND_FM = 2'd2;

    reg [3:0] state;
    reg [3:0] next_state;

    always @(*) begin
        next_state = state;

        case (state)
            S_IDLE: begin
                if (start) begin
                    next_state = S_COLLECT;
                end
            end

            S_COLLECT: begin
                next_state = S_WAIT_AM_DECISION;
            end

            S_WAIT_AM_DECISION: begin
                if (am_result_valid) begin
                    if (am_is_am) begin
                        next_state = S_WAIT_AM_PARAMS;
                    end else begin
                        next_state = S_WAIT_FM_CW_DECISION;
                    end
                end
            end

            S_WAIT_AM_PARAMS: begin
                if (am_params_valid) begin
                    next_state = S_OUTPUT;
                end
            end

            S_WAIT_FM_CW_DECISION: begin
                if (fm_cw_result_valid) begin
                    if (fm_is_fm) begin
                        next_state = S_WAIT_FM_PARAMS;
                    end else begin
                        next_state = S_OUTPUT;
                    end
                end
            end

            S_WAIT_FM_PARAMS: begin
                if (fm_params_valid) begin
                    next_state = S_OUTPUT;
                end
            end

            S_OUTPUT: begin
                if (!start) begin
                    next_state = S_IDLE;
                end
            end

            default: begin
                next_state = S_IDLE;
            end
        endcase
    end

    always @(posedge clk) begin
        if (!rst_n) begin
            state <= S_IDLE;
            collect_enable <= 1'b0;
            am_branch_enable <= 1'b0;
            fm_branch_enable <= 1'b0;
            result_valid <= 1'b0;
            modulation_kind <= KIND_CW;
            result_f_hz <= 16'd0;
            result_param_permille <= 16'd0;
            result_delta_f_hz <= 32'd0;
            state_debug <= S_IDLE;
        end else begin
            state <= next_state;
            state_debug <= next_state;

            collect_enable <= (next_state == S_COLLECT);
            am_branch_enable <= (next_state == S_WAIT_AM_DECISION) || (next_state == S_WAIT_AM_PARAMS);
            fm_branch_enable <= (next_state == S_WAIT_FM_CW_DECISION) || (next_state == S_WAIT_FM_PARAMS);
            result_valid <= (next_state == S_OUTPUT);

            if (state == S_WAIT_AM_PARAMS && am_params_valid) begin
                modulation_kind <= KIND_AM;
                result_f_hz <= am_f_hz;
                result_param_permille <= {6'd0, am_ma_permille};
                result_delta_f_hz <= 32'd0;
            end

            if (state == S_WAIT_FM_CW_DECISION && fm_cw_result_valid && !fm_is_fm) begin
                modulation_kind <= KIND_CW;
                result_f_hz <= 16'd0;
                result_param_permille <= 16'd0;
                result_delta_f_hz <= 32'd0;
            end

            if (state == S_WAIT_FM_PARAMS && fm_params_valid) begin
                modulation_kind <= KIND_FM;
                result_f_hz <= fm_f_hz;
                result_param_permille <= fm_mf_permille;
                result_delta_f_hz <= fm_delta_f_hz;
            end
        end
    end

endmodule
