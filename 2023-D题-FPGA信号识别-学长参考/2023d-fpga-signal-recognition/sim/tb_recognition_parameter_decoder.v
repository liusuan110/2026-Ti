`timescale 1ns / 1ps

module tb_recognition_parameter_decoder;

    localparam integer CASE_COUNT = 6;
    localparam integer FREQ_HZ_BITS = 16;

    reg clk = 1'b0;
    reg rst_n = 1'b0;
    reg valid_in = 1'b0;
    reg [2:0] kind_code = 0;
    reg [2:0] selected_freq_index = 0;
    reg [2:0] am_best_index = 0;
    reg [2:0] fm_best_index = 0;

    wire param_valid;
    wire is_cw;
    wire is_am_like;
    wire is_fm_like;
    wire is_psk_like;
    wire mod_freq_valid;
    wire [FREQ_HZ_BITS-1:0] mod_freq_hz;
    wire [FREQ_HZ_BITS-1:0] am_freq_hz;
    wire [FREQ_HZ_BITS-1:0] fm_freq_hz;

    reg [2:0] kind_mem [0:CASE_COUNT-1];
    reg [2:0] selected_mem [0:CASE_COUNT-1];
    reg [2:0] am_best_mem [0:CASE_COUNT-1];
    reg [2:0] fm_best_mem [0:CASE_COUNT-1];
    reg exp_is_cw_mem [0:CASE_COUNT-1];
    reg exp_is_am_like_mem [0:CASE_COUNT-1];
    reg exp_is_fm_like_mem [0:CASE_COUNT-1];
    reg exp_is_psk_like_mem [0:CASE_COUNT-1];
    reg exp_mod_freq_valid_mem [0:CASE_COUNT-1];
    reg [FREQ_HZ_BITS-1:0] exp_mod_freq_hz_mem [0:CASE_COUNT-1];
    reg [FREQ_HZ_BITS-1:0] exp_am_freq_hz_mem [0:CASE_COUNT-1];
    reg [FREQ_HZ_BITS-1:0] exp_fm_freq_hz_mem [0:CASE_COUNT-1];

    integer case_idx;
    integer fd;
    integer error_count;
    reg case_ok;

    recognition_parameter_decoder dut (
        .clk(clk),
        .rst_n(rst_n),
        .valid_in(valid_in),
        .kind_code(kind_code),
        .selected_freq_index(selected_freq_index),
        .am_best_index(am_best_index),
        .fm_best_index(fm_best_index),
        .param_valid(param_valid),
        .is_cw(is_cw),
        .is_am_like(is_am_like),
        .is_fm_like(is_fm_like),
        .is_psk_like(is_psk_like),
        .mod_freq_valid(mod_freq_valid),
        .mod_freq_hz(mod_freq_hz),
        .am_freq_hz(am_freq_hz),
        .fm_freq_hz(fm_freq_hz)
    );

    always #5 clk = ~clk;

    initial begin
        $readmemh("sim/recognition_param_kind.mem", kind_mem);
        $readmemh("sim/recognition_param_selected.mem", selected_mem);
        $readmemh("sim/recognition_param_am_best.mem", am_best_mem);
        $readmemh("sim/recognition_param_fm_best.mem", fm_best_mem);
        $readmemb("sim/recognition_param_exp_is_cw.mem", exp_is_cw_mem);
        $readmemb("sim/recognition_param_exp_is_am_like.mem", exp_is_am_like_mem);
        $readmemb("sim/recognition_param_exp_is_fm_like.mem", exp_is_fm_like_mem);
        $readmemb("sim/recognition_param_exp_is_psk_like.mem", exp_is_psk_like_mem);
        $readmemb("sim/recognition_param_exp_mod_freq_valid.mem", exp_mod_freq_valid_mem);
        $readmemh("sim/recognition_param_exp_mod_freq_hz.mem", exp_mod_freq_hz_mem);
        $readmemh("sim/recognition_param_exp_am_freq_hz.mem", exp_am_freq_hz_mem);
        $readmemh("sim/recognition_param_exp_fm_freq_hz.mem", exp_fm_freq_hz_mem);

        fd = $fopen("outputs/recognition_parameter_decoder/recognition_parameter_decoder_sim.csv", "w");
        $fwrite(fd, "case,kind,selected,mod_freq_hz,expected_mod_freq_hz,am_freq_hz,expected_am_freq_hz,fm_freq_hz,expected_fm_freq_hz,ok\n");

        error_count = 0;
        repeat (4) @(posedge clk);
        rst_n = 1'b1;
        repeat (2) @(posedge clk);

        for (case_idx = 0; case_idx < CASE_COUNT; case_idx = case_idx + 1) begin
            @(negedge clk);
            valid_in = 1'b1;
            kind_code = kind_mem[case_idx];
            selected_freq_index = selected_mem[case_idx];
            am_best_index = am_best_mem[case_idx];
            fm_best_index = fm_best_mem[case_idx];
            @(posedge clk);
            #1;

            case_ok = param_valid &&
                      (is_cw === exp_is_cw_mem[case_idx]) &&
                      (is_am_like === exp_is_am_like_mem[case_idx]) &&
                      (is_fm_like === exp_is_fm_like_mem[case_idx]) &&
                      (is_psk_like === exp_is_psk_like_mem[case_idx]) &&
                      (mod_freq_valid === exp_mod_freq_valid_mem[case_idx]) &&
                      (mod_freq_hz === exp_mod_freq_hz_mem[case_idx]) &&
                      (am_freq_hz === exp_am_freq_hz_mem[case_idx]) &&
                      (fm_freq_hz === exp_fm_freq_hz_mem[case_idx]);

            if (!case_ok) begin
                $display("ERROR[%0d]: recognition parameter mismatch", case_idx);
                error_count = error_count + 1;
            end

            $fwrite(fd, "%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d,%0d\n",
                case_idx,
                kind_code,
                selected_freq_index,
                mod_freq_hz,
                exp_mod_freq_hz_mem[case_idx],
                am_freq_hz,
                exp_am_freq_hz_mem[case_idx],
                fm_freq_hz,
                exp_fm_freq_hz_mem[case_idx],
                case_ok);
        end

        @(negedge clk);
        valid_in = 1'b0;
        kind_code = 0;
        selected_freq_index = 0;
        am_best_index = 0;
        fm_best_index = 0;
        repeat (4) @(posedge clk);

        $fclose(fd);

        if (error_count == 0) begin
            $display("PASS: recognition_parameter_decoder");
        end else begin
            $display("FAIL: recognition_parameter_decoder errors=%0d", error_count);
        end

        $finish;
    end

endmodule
