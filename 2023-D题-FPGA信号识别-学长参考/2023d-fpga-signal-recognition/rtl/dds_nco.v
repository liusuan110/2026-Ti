`timescale 1ns / 1ps

module dds_nco #(
    parameter integer PHASE_BITS = 32,
    parameter integer LUT_ADDR_BITS = 10,
    parameter integer AMP_BITS = 12,
    parameter [PHASE_BITS-1:0] PHASE_STEP = 32'd429496730,
    parameter COS_MEM_FILE = "rtl/cos_1024x12.mem",
    parameter SIN_MEM_FILE = "rtl/sin_1024x12.mem"
) (
    input wire clk,
    input wire rst_n,
    input wire valid_in,
    output reg valid_out,
    output reg signed [AMP_BITS-1:0] cos_out,
    output reg signed [AMP_BITS-1:0] sin_out,
    output reg [PHASE_BITS-1:0] phase_out
);

    localparam integer LUT_SIZE = 1 << LUT_ADDR_BITS;
    localparam integer ADDR_LSB = PHASE_BITS - LUT_ADDR_BITS;

    reg [PHASE_BITS-1:0] phase_acc;
    reg signed [AMP_BITS-1:0] cos_rom [0:LUT_SIZE-1];
    reg signed [AMP_BITS-1:0] sin_rom [0:LUT_SIZE-1];

    wire [LUT_ADDR_BITS-1:0] lut_addr;
    assign lut_addr = phase_acc[PHASE_BITS-1:ADDR_LSB];

    initial begin
        $readmemh(COS_MEM_FILE, cos_rom);
        $readmemh(SIN_MEM_FILE, sin_rom);
    end

    always @(posedge clk) begin
        if (!rst_n) begin
            phase_acc <= {PHASE_BITS{1'b0}};
            phase_out <= {PHASE_BITS{1'b0}};
            cos_out <= {AMP_BITS{1'b0}};
            sin_out <= {AMP_BITS{1'b0}};
            valid_out <= 1'b0;
        end else begin
            valid_out <= valid_in;

            if (valid_in) begin
                phase_out <= phase_acc;
                cos_out <= cos_rom[lut_addr];
                sin_out <= sin_rom[lut_addr];
                phase_acc <= phase_acc + PHASE_STEP;
            end
        end
    end

endmodule
