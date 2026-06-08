`timescale 1ns / 1ps

module cordic_atan2_pipeline #(
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
    output wire valid_out,
    output wire signed [ANGLE_BITS-1:0] phase_out
);

    localparam signed [ANGLE_BITS-1:0] PI_Q4_20 = 24'sd3294199;

    reg valid_pipe [0:ITERATIONS];
    reg signed [XY_BITS-1:0] x_pipe [0:ITERATIONS];
    reg signed [XY_BITS-1:0] y_pipe [0:ITERATIONS];
    reg signed [ANGLE_BITS-1:0] z_pipe [0:ITERATIONS];

    wire signed [XY_BITS-1:0] i_ext;
    wire signed [XY_BITS-1:0] q_ext;

    integer k;

    assign i_ext = {{(XY_BITS-DATA_BITS){i_in[DATA_BITS-1]}}, i_in};
    assign q_ext = {{(XY_BITS-DATA_BITS){q_in[DATA_BITS-1]}}, q_in};

    assign valid_out = valid_pipe[ITERATIONS];
    assign phase_out = z_pipe[ITERATIONS];

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
            for (k = 0; k <= ITERATIONS; k = k + 1) begin
                valid_pipe[k] <= 1'b0;
                x_pipe[k] <= {XY_BITS{1'b0}};
                y_pipe[k] <= {XY_BITS{1'b0}};
                z_pipe[k] <= {ANGLE_BITS{1'b0}};
            end
        end else begin
            valid_pipe[0] <= valid_in;

            if (i_ext < 0) begin
                x_pipe[0] <= -i_ext;
                y_pipe[0] <= -q_ext;
                z_pipe[0] <= (q_ext >= 0) ? PI_Q4_20 : -PI_Q4_20;
            end else begin
                x_pipe[0] <= i_ext;
                y_pipe[0] <= q_ext;
                z_pipe[0] <= {ANGLE_BITS{1'b0}};
            end

            for (k = 0; k < ITERATIONS; k = k + 1) begin
                valid_pipe[k + 1] <= valid_pipe[k];

                if (y_pipe[k] == 0) begin
                    x_pipe[k + 1] <= x_pipe[k];
                    y_pipe[k + 1] <= y_pipe[k];
                    z_pipe[k + 1] <= z_pipe[k];
                end else if (y_pipe[k] > 0) begin
                    x_pipe[k + 1] <= x_pipe[k] + (y_pipe[k] >>> k);
                    y_pipe[k + 1] <= y_pipe[k] - (x_pipe[k] >>> k);
                    z_pipe[k + 1] <= z_pipe[k] + atan_table(k[5:0]);
                end else begin
                    x_pipe[k + 1] <= x_pipe[k] - (y_pipe[k] >>> k);
                    y_pipe[k + 1] <= y_pipe[k] + (x_pipe[k] >>> k);
                    z_pipe[k + 1] <= z_pipe[k] - atan_table(k[5:0]);
                end
            end
        end
    end

endmodule
