`timescale 1ns / 1ps

module receiver_nco_frontend #(
    parameter integer SAMPLE_BITS = 8,
    parameter integer AMP_BITS = 12,
    parameter integer PHASE_BITS = 32,
    parameter integer LUT_ADDR_BITS = 10,
    parameter integer DECIM = 80,
    parameter integer DECIM_COUNT_BITS = 7,
    parameter [PHASE_BITS-1:0] CARRIER_PHASE_STEP = 32'd429496730,
    parameter [PHASE_BITS-1:0] REF0_PHASE_STEP = 32'd17179869,
    parameter [PHASE_BITS-1:0] REF1_PHASE_STEP = 32'd34359738,
    parameter [PHASE_BITS-1:0] REF2_PHASE_STEP = 32'd51539608,
    parameter [PHASE_BITS-1:0] REF3_PHASE_STEP = 32'd68719477,
    parameter [PHASE_BITS-1:0] REF4_PHASE_STEP = 32'd85899346,
    parameter COS_MEM_FILE = "rtl/cos_1024x12.mem",
    parameter SIN_MEM_FILE = "rtl/sin_1024x12.mem"
) (
    input wire clk,
    input wire rst_n,
    input wire valid_in,
    input wire signed [SAMPLE_BITS-1:0] sample_in,
    output reg valid_out,
    output reg signed [SAMPLE_BITS-1:0] sample_out,
    output reg signed [AMP_BITS-1:0] carrier_cos,
    output reg signed [AMP_BITS-1:0] carrier_sin,
    output reg signed [AMP_BITS-1:0] ref0_cos,
    output reg signed [AMP_BITS-1:0] ref0_sin,
    output reg signed [AMP_BITS-1:0] ref1_cos,
    output reg signed [AMP_BITS-1:0] ref1_sin,
    output reg signed [AMP_BITS-1:0] ref2_cos,
    output reg signed [AMP_BITS-1:0] ref2_sin,
    output reg signed [AMP_BITS-1:0] ref3_cos,
    output reg signed [AMP_BITS-1:0] ref3_sin,
    output reg signed [AMP_BITS-1:0] ref4_cos,
    output reg signed [AMP_BITS-1:0] ref4_sin,
    output reg [DECIM_COUNT_BITS-1:0] decim_count_debug
);

    localparam integer LUT_SIZE = 1 << LUT_ADDR_BITS;
    localparam integer ADDR_LSB = PHASE_BITS - LUT_ADDR_BITS;
    localparam [DECIM_COUNT_BITS-1:0] DECIM_LAST = DECIM - 1;

    reg [DECIM_COUNT_BITS-1:0] decim_count;
    reg [PHASE_BITS-1:0] carrier_phase;
    reg [PHASE_BITS-1:0] ref0_phase;
    reg [PHASE_BITS-1:0] ref1_phase;
    reg [PHASE_BITS-1:0] ref2_phase;
    reg [PHASE_BITS-1:0] ref3_phase;
    reg [PHASE_BITS-1:0] ref4_phase;
    reg signed [AMP_BITS-1:0] cos_rom [0:LUT_SIZE-1];
    reg signed [AMP_BITS-1:0] sin_rom [0:LUT_SIZE-1];

    wire [LUT_ADDR_BITS-1:0] carrier_addr;
    wire [LUT_ADDR_BITS-1:0] ref0_addr;
    wire [LUT_ADDR_BITS-1:0] ref1_addr;
    wire [LUT_ADDR_BITS-1:0] ref2_addr;
    wire [LUT_ADDR_BITS-1:0] ref3_addr;
    wire [LUT_ADDR_BITS-1:0] ref4_addr;

    assign carrier_addr = carrier_phase[PHASE_BITS-1:ADDR_LSB];
    assign ref0_addr = ref0_phase[PHASE_BITS-1:ADDR_LSB];
    assign ref1_addr = ref1_phase[PHASE_BITS-1:ADDR_LSB];
    assign ref2_addr = ref2_phase[PHASE_BITS-1:ADDR_LSB];
    assign ref3_addr = ref3_phase[PHASE_BITS-1:ADDR_LSB];
    assign ref4_addr = ref4_phase[PHASE_BITS-1:ADDR_LSB];

    initial begin
        $readmemh(COS_MEM_FILE, cos_rom);
        $readmemh(SIN_MEM_FILE, sin_rom);
    end

    always @(posedge clk) begin
        if (!rst_n) begin
            valid_out <= 1'b0;
            sample_out <= {SAMPLE_BITS{1'b0}};
            carrier_cos <= {AMP_BITS{1'b0}};
            carrier_sin <= {AMP_BITS{1'b0}};
            ref0_cos <= {AMP_BITS{1'b0}};
            ref0_sin <= {AMP_BITS{1'b0}};
            ref1_cos <= {AMP_BITS{1'b0}};
            ref1_sin <= {AMP_BITS{1'b0}};
            ref2_cos <= {AMP_BITS{1'b0}};
            ref2_sin <= {AMP_BITS{1'b0}};
            ref3_cos <= {AMP_BITS{1'b0}};
            ref3_sin <= {AMP_BITS{1'b0}};
            ref4_cos <= {AMP_BITS{1'b0}};
            ref4_sin <= {AMP_BITS{1'b0}};
            decim_count_debug <= {DECIM_COUNT_BITS{1'b0}};
            decim_count <= {DECIM_COUNT_BITS{1'b0}};
            carrier_phase <= {PHASE_BITS{1'b0}};
            ref0_phase <= {PHASE_BITS{1'b0}};
            ref1_phase <= {PHASE_BITS{1'b0}};
            ref2_phase <= {PHASE_BITS{1'b0}};
            ref3_phase <= {PHASE_BITS{1'b0}};
            ref4_phase <= {PHASE_BITS{1'b0}};
        end else begin
            valid_out <= valid_in;

            if (valid_in) begin
                sample_out <= sample_in;
                carrier_cos <= cos_rom[carrier_addr];
                carrier_sin <= sin_rom[carrier_addr];
                ref0_cos <= cos_rom[ref0_addr];
                ref0_sin <= sin_rom[ref0_addr];
                ref1_cos <= cos_rom[ref1_addr];
                ref1_sin <= sin_rom[ref1_addr];
                ref2_cos <= cos_rom[ref2_addr];
                ref2_sin <= sin_rom[ref2_addr];
                ref3_cos <= cos_rom[ref3_addr];
                ref3_sin <= sin_rom[ref3_addr];
                ref4_cos <= cos_rom[ref4_addr];
                ref4_sin <= sin_rom[ref4_addr];
                decim_count_debug <= decim_count;

                carrier_phase <= carrier_phase + CARRIER_PHASE_STEP;

                if (decim_count == DECIM_LAST) begin
                    decim_count <= {DECIM_COUNT_BITS{1'b0}};
                    ref0_phase <= ref0_phase + REF0_PHASE_STEP;
                    ref1_phase <= ref1_phase + REF1_PHASE_STEP;
                    ref2_phase <= ref2_phase + REF2_PHASE_STEP;
                    ref3_phase <= ref3_phase + REF3_PHASE_STEP;
                    ref4_phase <= ref4_phase + REF4_PHASE_STEP;
                end else begin
                    decim_count <= decim_count + 1'b1;
                end
            end
        end
    end

endmodule
