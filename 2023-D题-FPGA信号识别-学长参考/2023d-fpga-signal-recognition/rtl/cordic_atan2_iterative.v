`timescale 1ns / 1ps

module cordic_atan2_iterative #(
    parameter integer DATA_BITS = 21,
    parameter integer XY_BITS = DATA_BITS + 4,
    parameter integer ANGLE_BITS = 24,
    parameter integer ITERATIONS = 20
) (
    input wire clk,
    input wire rst_n,
    input wire valid_in,
    input wire signed [DATA_BITS-1:0] i_in,
    input wire signed [DATA_BITS-1:0] q_in,
    output reg input_ready,
    output reg busy,
    output reg valid_out,
    output reg signed [ANGLE_BITS-1:0] phase_out,
    output reg [5:0] iter_debug
);

    localparam [5:0] ITER_LAST = ITERATIONS - 1;
    localparam signed [ANGLE_BITS-1:0] PI_Q4_20 = 24'sd3294199;

    reg [5:0] iter;
    reg signed [XY_BITS-1:0] x_reg;
    reg signed [XY_BITS-1:0] y_reg;
    reg signed [ANGLE_BITS-1:0] z_reg;

    wire signed [XY_BITS-1:0] i_ext;
    wire signed [XY_BITS-1:0] q_ext;
    wire signed [XY_BITS-1:0] x_shift;
    wire signed [XY_BITS-1:0] y_shift;

    wire signed [XY_BITS-1:0] x_next_pos_y;
    wire signed [XY_BITS-1:0] y_next_pos_y;
    wire signed [ANGLE_BITS-1:0] z_next_pos_y;
    wire signed [XY_BITS-1:0] x_next_neg_y;
    wire signed [XY_BITS-1:0] y_next_neg_y;
    wire signed [ANGLE_BITS-1:0] z_next_neg_y;

    wire signed [XY_BITS-1:0] x_next;
    wire signed [XY_BITS-1:0] y_next;
    wire signed [ANGLE_BITS-1:0] z_next;

    assign i_ext = {{(XY_BITS-DATA_BITS){i_in[DATA_BITS-1]}}, i_in};
    assign q_ext = {{(XY_BITS-DATA_BITS){q_in[DATA_BITS-1]}}, q_in};

    assign x_shift = x_reg >>> iter;
    assign y_shift = y_reg >>> iter;

    assign x_next_pos_y = x_reg + y_shift;
    assign y_next_pos_y = y_reg - x_shift;
    assign z_next_pos_y = z_reg + atan_table(iter);

    assign x_next_neg_y = x_reg - y_shift;
    assign y_next_neg_y = y_reg + x_shift;
    assign z_next_neg_y = z_reg - atan_table(iter);

    assign x_next = (y_reg > 0) ? x_next_pos_y : x_next_neg_y;
    assign y_next = (y_reg > 0) ? y_next_pos_y : y_next_neg_y;
    assign z_next = (y_reg > 0) ? z_next_pos_y : z_next_neg_y;

    function signed [ANGLE_BITS-1:0] atan_table;
        input [5:0] index;
        begin
            case (index)
                6'd0: atan_table = 24'sd823550;
                6'd1: atan_table = 24'sd486170;
                6'd2: atan_table = 24'sd256879;
                6'd3: atan_table = 24'sd130396;
                6'd4: atan_table = 24'sd65451;
                6'd5: atan_table = 24'sd32757;
                6'd6: atan_table = 24'sd16383;
                6'd7: atan_table = 24'sd8192;
                6'd8: atan_table = 24'sd4096;
                6'd9: atan_table = 24'sd2048;
                6'd10: atan_table = 24'sd1024;
                6'd11: atan_table = 24'sd512;
                6'd12: atan_table = 24'sd256;
                6'd13: atan_table = 24'sd128;
                6'd14: atan_table = 24'sd64;
                6'd15: atan_table = 24'sd32;
                6'd16: atan_table = 24'sd16;
                6'd17: atan_table = 24'sd8;
                6'd18: atan_table = 24'sd4;
                6'd19: atan_table = 24'sd2;
                default: atan_table = {ANGLE_BITS{1'b0}};
            endcase
        end
    endfunction

    always @(posedge clk) begin
        if (!rst_n) begin
            iter <= 6'd0;
            x_reg <= {XY_BITS{1'b0}};
            y_reg <= {XY_BITS{1'b0}};
            z_reg <= {ANGLE_BITS{1'b0}};
            input_ready <= 1'b1;
            busy <= 1'b0;
            valid_out <= 1'b0;
            phase_out <= {ANGLE_BITS{1'b0}};
            iter_debug <= 6'd0;
        end else begin
            valid_out <= 1'b0;

            if (!busy) begin
                input_ready <= 1'b1;

                if (valid_in) begin
                    input_ready <= 1'b0;
                    busy <= 1'b1;
                    iter <= 6'd0;
                    iter_debug <= 6'd0;

                    if (i_ext < 0) begin
                        x_reg <= -i_ext;
                        y_reg <= -q_ext;
                        z_reg <= (q_ext >= 0) ? PI_Q4_20 : -PI_Q4_20;
                    end else begin
                        x_reg <= i_ext;
                        y_reg <= q_ext;
                        z_reg <= {ANGLE_BITS{1'b0}};
                    end
                end
            end else begin
                input_ready <= 1'b0;

                if (y_reg == 0) begin
                    phase_out <= z_reg;
                    valid_out <= 1'b1;
                    busy <= 1'b0;
                    input_ready <= 1'b1;
                end else if (iter == ITER_LAST) begin
                    phase_out <= z_next;
                    valid_out <= 1'b1;
                    busy <= 1'b0;
                    input_ready <= 1'b1;
                    x_reg <= x_next;
                    y_reg <= y_next;
                    z_reg <= z_next;
                end else begin
                    x_reg <= x_next;
                    y_reg <= y_next;
                    z_reg <= z_next;
                    iter <= iter + 1'b1;
                    iter_debug <= iter + 1'b1;
                end
            end
        end
    end

endmodule
