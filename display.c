/*
 * display.c
 * Display page manager
 */

#include "display.h"
#include "JLX12864G.h"
#include "lcd_draw.h"
#include "adc_measure.h"
#include "timer_capture.h"
#include "fft.h"
#include "uart.h"

/* Page titles (top area, page 0~1). */
static const char * const page_titles[PAGE_COUNT] = {
    "  Team Info  ",   /* PAGE_INFO */
    " Uo1/Uo2 F/A ",   /* PAGE_FREQ */
    " Uo3/Uo5 Wave",   /* PAGE_WAVE */
    " Vpp  (U_o4) ",   /* PAGE_VPP  */
    " FFT  (U_o1) "    /* PAGE_FFT  */
};

/* Vpp page sub-mode. */
static uint8_t vpp_sub_mode = 0; /* 0=Vpp, 1=Vrms */

/* Avoid redundant title redraw. */
static PageID last_title_page = PAGE_COUNT;

/* Shared buffers: wave + FFT reuse. */
static int16_t g_buf_a[WAVE_RAW_POINTS];
static int16_t g_buf_b[FFT_N];

/* 清除一行 16 像素高文本带(两页)，用于局部重绘避免整屏闪烁 */
static void clear_text_band(u8 page_start)
{
    u8 p, c;
    for (p = page_start; p < (u8)(page_start + 2U); p++) {
        LCD_setAddr(p, 0);
        for (c = 0; c < 128; c++) {
            LCD_writeData(0x00);
        }
    }
}

/*
 * 在采样数据中估计“每周期样本点数”(spp)：
 * - 使用归一化前的自相关近似(去均值后点积)
 * - 仅搜索有效范围 [3, len/3]，避免过短/过长周期误判
 */
static uint16_t wave_estimate_spp(const int16_t *raw, uint16_t len)
{
    uint16_t lag;
    uint16_t best_lag = 0;
    int32_t sum = 0;
    int16_t mean;
    int32_t best_score = 0;

    if (len < 12) return 0;

    for (lag = 0; lag < len; lag++) {
        sum += raw[lag];
    }
    mean = (int16_t)(sum / (int32_t)len);

    for (lag = 3; lag <= (uint16_t)(len / 3); lag++) {
        uint16_t i;
        int32_t score = 0;
        for (i = 0; i + lag < len; i++) {
            int16_t a = (int16_t)(raw[i] - mean);
            int16_t b = (int16_t)(raw[i + lag] - mean);
            score += (int32_t)a * (int32_t)b;
        }

        if (score > best_score) {
            best_score = score;
            best_lag = lag;
        }
    }

    return best_lag;
}

static void draw_title(PageID page_id)
{
    if (last_title_page == page_id) return;
    LCD_clearPages(0, 1);
    LCD_showGB2312Str(0, 0, (u8 *)page_titles[page_id]);
    last_title_page = page_id;
}

/* Task 5: team info. */
static void page_info(void)
{
    last_title_page = PAGE_COUNT;
    LCD_clear();

    LCD_showGB2312Str(0, 8, "2026F117");
    LCD_showGB2312Str(2, 8, "���հ�");
    LCD_showGB2312Str(4, 8, "��оȻ");
    LCD_showGB2312Str(6, 8, "�����");
}

/* Task 6: frequency page. */
static void page_freq(void)
{
    VppResult result;
    uint8_t page_changed = (last_title_page != PAGE_FREQ);

    draw_title(PAGE_FREQ);
    if (page_changed) {
        LCD_clearPages(2, 7);
    }

    ADC_measureVpp(ADC_CH_UO2, 300, &result);

    clear_text_band(2);
    if (g_freq_ready) {
        LCD_showMeasure(2, 4, "f=", g_freq_hz, 0, "Hz");
        UART_sendStr("--- Uo1/Uo2 Test ---\r\nSignal: Sine & Square\r\nFreq (Uo1): ");
        UART_sendNum(g_freq_hz);
        UART_sendStr(" Hz\r\n");
    } else {
        LCD_showGB2312Str(2, 16, (u8 *)"Measuring f...");
    }

    clear_text_band(5);
    LCD_showMeasure(5, 4, "A=", (uint32_t)result.vpp_mv, 0, "mV");
    UART_sendStr("Amplitude (Uo2): ");
    UART_sendNum(result.vpp_mv);
    UART_sendStr(" mV\r\n\r\n");
}

/* Task 8: Vpp / Vrms page (Uo4). */
static void page_vpp(void)
{
    VppResult result;
    uint8_t page_changed = (last_title_page != PAGE_VPP);

    draw_title(PAGE_VPP);
    if (page_changed) {
        LCD_clearPages(2, 7);
    }

    ADC_measureVpp(ADC_CH_UO4, 500, &result);

    clear_text_band(2);
    clear_text_band(4);
    if (vpp_sub_mode == 0) {
        LCD_showMeasure(2, 4, "Vpp=", (uint32_t)result.vpp_mv, 0, "mV");
        LCD_showMeasure(4, 4, "   =", (uint32_t)result.vpp_mv, 3, "V");
        UART_sendStr("--- Uo4 Test ---\r\nSignal: Cosine\r\nMode: Vpp\r\nVpp: ");
        UART_sendNum(result.vpp_mv);
        UART_sendStr(" mV\r\n\r\n");
    } else {
        LCD_showMeasure(2, 4, "Vrms=", (uint32_t)result.vrms_mv, 0, "mV");
        LCD_showMeasure(4, 4, "    =", (uint32_t)result.vrms_mv, 3, "V");
        UART_sendStr("--- Uo4 Test ---\r\nSignal: Cosine\r\nMode: Vrms\r\nVrms: ");
        UART_sendNum(result.vrms_mv);
        UART_sendStr(" mV\r\n\r\n");
    }

    clear_text_band(6);
    LCD_showGB2312Str(6, 0, (u8 *)"DBL KEY1:Vpp/Vrms");
}

/* Task 7: simplest waveform display (Uo3/Uo5). */
static void page_wave(void)
{
    int16_t *raw = g_buf_a;
    uint16_t i;
    int16_t adc_min = 1023;
    int16_t adc_max = 0;
    int16_t span, pad;
    int16_t map_min, map_max;
    int16_t raw_map_min, raw_map_max;
    uint16_t spp = 0;
    uint16_t display_spp;
    uint16_t seg_start = 0;
    uint16_t seg_len = WAVE_RAW_POINTS;
    uint16_t seg_end;
    uint16_t seg_min = 1023;
    uint16_t seg_max = 0;
    uint16_t idx;
    uint16_t idx_prev, idx_next;
    int32_t val;
    int32_t y_norm;
    u8 y_pixel;
    const u8 y_bottom = 63;
    const u8 usable_h = (u8)(63 - 16 + 1);
    const u8 x_step = 1;
    uint32_t freq_hint = 25000U; /* 20~30kHz 显示默认中心频率 */

    /* Frame-to-frame stabilizers */
    static uint8_t seg_inited = 0;
    static uint16_t last_seg_start = 0;
    static uint16_t last_seg_len = 40;
    static uint8_t scale_inited = 0;
    static int16_t smooth_map_min = 0;
    static int16_t smooth_map_max = 1023;

    draw_title(PAGE_WAVE);

    /* 20~30kHz 正弦波显示:
     * 1) 优先使用捕获频率提示
     * 2) 频率异常时回退到 25kHz
     * 3) 使用自适应采样让窗口稳定覆盖固定周期数
     */
    if (g_freq_period > 0U) {
        freq_hint = SYS_FREQ / g_freq_period;
    } else if (g_freq_hz > 0U) {
        freq_hint = g_freq_hz;
    }
    if (freq_hint < 20000U || freq_hint > 30000U) {
        freq_hint = 25000U;
    }
    ADC_sampleToBufferAdaptive(ADC_CH_UO3_FFT, raw, WAVE_RAW_POINTS, freq_hint);

    /* Global min/max. */
    for (i = 0; i < WAVE_RAW_POINTS; i++) {
        if (raw[i] > adc_max) adc_max = raw[i];
        if (raw[i] < adc_min) adc_min = raw[i];
    }
    if (adc_max <= adc_min) adc_max = adc_min + 1;

    /* 周期估计 + 固定 2 周期显示，减少高频/欠采样下的触发抖动 */
    spp = wave_estimate_spp(raw, WAVE_RAW_POINTS);
    if (spp < 4) {
        if (seg_inited) {
            spp = (uint16_t)(last_seg_len / 2U);
        } else {
            spp = 6;
        }
    }
    display_spp = spp;
    if (display_spp < 4) display_spp = 4;
    if (display_spp > 20U) display_spp = 20U;
    if (display_spp > (WAVE_RAW_POINTS / 2U)) display_spp = (WAVE_RAW_POINTS / 2U);

    seg_len = (uint16_t)(display_spp * 2U);
    if (seg_len > WAVE_RAW_POINTS) seg_len = WAVE_RAW_POINTS;
    seg_start = (uint16_t)((WAVE_RAW_POINTS - seg_len) / 2U);

    /* Fallback to last stable segment if current detection is weak. */
    if (seg_len < 12) {
        if (seg_inited) {
            seg_start = last_seg_start;
            seg_len = last_seg_len;
        } else {
            seg_start = 0;
            seg_len = WAVE_RAW_POINTS;
        }
    }
    if (seg_len > WAVE_RAW_POINTS) seg_len = WAVE_RAW_POINTS;
    if ((uint16_t)(seg_start + seg_len) > WAVE_RAW_POINTS) {
        seg_start = (uint16_t)(WAVE_RAW_POINTS - seg_len);
    }

    /* Smooth segment position/length to reduce horizontal jitter. */
    if (!seg_inited) {
        last_seg_start = seg_start;
        last_seg_len = seg_len;
        seg_inited = 1;
    } else {
        last_seg_start = (uint16_t)(((uint32_t)last_seg_start * 7U + (uint32_t)seg_start * 3U + 5U) / 10U);
        last_seg_len = (uint16_t)(((uint32_t)last_seg_len * 7U + (uint32_t)seg_len * 3U + 5U) / 10U);
        if (last_seg_len < 12) last_seg_len = 12;
        if (last_seg_len > WAVE_RAW_POINTS) last_seg_len = WAVE_RAW_POINTS;
        if ((uint16_t)(last_seg_start + last_seg_len) > WAVE_RAW_POINTS) {
            last_seg_start = (uint16_t)(WAVE_RAW_POINTS - last_seg_len);
        }
        seg_start = last_seg_start;
        seg_len = last_seg_len;
    }
    seg_end = (uint16_t)(seg_start + seg_len - 1);

    /* Vertical mapping based on selected segment + frame smoothing. */
    seg_min = 1023;
    seg_max = 0;
    for (i = seg_start; i <= seg_end; i++) {
        if (raw[i] > seg_max) seg_max = raw[i];
        if (raw[i] < seg_min) seg_min = raw[i];
    }

    span = (int16_t)(seg_max - seg_min);
    if (span < 1) span = 1;
    pad = (int16_t)(span / 8);
    if (pad < 4) pad = 4;

    raw_map_min = (int16_t)(seg_min - pad);
    raw_map_max = (int16_t)(seg_max + pad);
    if (raw_map_min < 0) raw_map_min = 0;
    if (raw_map_max > 1023) raw_map_max = 1023;
    if (raw_map_max <= raw_map_min + 16) raw_map_max = (int16_t)(raw_map_min + 16);
    if (raw_map_max > 1023) {
        raw_map_max = 1023;
        raw_map_min = (int16_t)(raw_map_max - 16);
    }

    if (!scale_inited) {
        smooth_map_min = raw_map_min;
        smooth_map_max = raw_map_max;
        scale_inited = 1;
    } else {
        smooth_map_min = (int16_t)(((int32_t)smooth_map_min * 8 + (int32_t)raw_map_min * 2 + 5) / 10);
        smooth_map_max = (int16_t)(((int32_t)smooth_map_max * 8 + (int32_t)raw_map_max * 2 + 5) / 10);
        if (smooth_map_max <= smooth_map_min + 16) smooth_map_max = (int16_t)(smooth_map_min + 16);
        if (smooth_map_max > 1023) {
            smooth_map_max = 1023;
            smooth_map_min = (int16_t)(smooth_map_max - 16);
        }
        if (smooth_map_min < 0) smooth_map_min = 0;
    }

    map_min = smooth_map_min;
    map_max = smooth_map_max;

    LCD_clearPages(2, 7);

    /* Dot-style waveform: 2-cycle segment stretched to 128 columns. */
    for (i = 0; i < 128; i += x_step) {
        idx = (uint16_t)(seg_start + ((uint32_t)i * (seg_len - 1) / 127U));
        idx_prev = (idx > seg_start) ? (uint16_t)(idx - 1) : idx;
        idx_next = (idx < seg_end) ? (uint16_t)(idx + 1) : idx;

        /* Light local smoothing only for display aesthetics. */
        val = ((int32_t)raw[idx_prev] + ((int32_t)raw[idx] << 1) + (int32_t)raw[idx_next] + 2) / 4;

        y_norm = (val - map_min) * (usable_h - 1) / (map_max - map_min);
        if (y_norm < 0) y_norm = 0;
        if (y_norm > (usable_h - 1)) y_norm = (usable_h - 1);

        y_pixel = (u8)(y_bottom - (u8)y_norm);
        LCD_drawDot((u8)i, y_pixel);
    }
}

/* Task 9: FFT page. */
static void page_fft(void)
{
    int16_t *real_buf = g_buf_a;
    int16_t *imag_buf = g_buf_b;
    uint16_t mag_buf[FFT_N / 2];
    static uint16_t mag_smooth[FFT_N / 2];
    static uint16_t max_smooth = 1;
    static uint8_t fft_inited = 0;
    u8 bar_data[FFT_N / 2];
    uint16_t i;
    uint16_t mag_max_raw = 0;
    uint16_t mag_max_disp;

    draw_title(PAGE_FFT);

    ADC_sampleToBuffer(ADC_CH_UO3_FFT, real_buf, FFT_N);

    for (i = 0; i < FFT_N; i++) {
        imag_buf[i] = 0;
    }

    {
        int32_t sum = 0;
        int16_t avg;
        for (i = 0; i < FFT_N; i++) {
            sum += real_buf[i];
        }
        avg = (int16_t)(sum / FFT_N);
        for (i = 0; i < FFT_N; i++) {
            real_buf[i] -= avg;
        }
    }

    fft32(real_buf, imag_buf);
    fft_magnitude(real_buf, imag_buf, mag_buf);

    if (!fft_inited) {
        for (i = 0; i < FFT_N / 2; i++) {
            mag_smooth[i] = mag_buf[i];
        }
        fft_inited = 1;
    }

    /* 对每个频点做时域平滑: 上升快, 下降慢, 降低“上下抖动” */
    for (i = 1; i < FFT_N / 2; i++) {
        uint16_t cur = mag_buf[i];
        uint16_t prev = mag_smooth[i];
        uint16_t smooth;

        if (cur >= prev) {
            smooth = (uint16_t)(((uint32_t)prev * 1U + (uint32_t)cur * 3U + 2U) / 4U);
        } else {
            smooth = (uint16_t)(((uint32_t)prev * 7U + (uint32_t)cur + 4U) / 8U);
        }
        mag_smooth[i] = smooth;

        if (smooth > mag_max_raw) mag_max_raw = smooth;
    }

    if (mag_max_raw == 0) mag_max_raw = 1;

    /* 峰值归一化基准也做平滑, 减少“最高柱变化导致其他柱联动跳动” */
    if (mag_max_raw > max_smooth) {
        max_smooth = (uint16_t)(((uint32_t)max_smooth * 1U + (uint32_t)mag_max_raw * 3U + 2U) / 4U);
    } else {
        max_smooth = (uint16_t)(((uint32_t)max_smooth * 15U + (uint32_t)mag_max_raw + 8U) / 16U);
    }
    if (max_smooth == 0) max_smooth = 1;
    mag_max_disp = max_smooth;

    for (i = 1; i < FFT_N / 2; i++) {
        uint16_t noise_th = (uint16_t)(mag_max_disp / 24U); /* 约 4% 噪声门限 */
        uint16_t v = mag_smooth[i];
        if (v < noise_th) v = 0;
        bar_data[i] = (u8)((uint32_t)v * 47U / mag_max_disp);
    }
    bar_data[0] = 0;

    LCD_drawBars(&bar_data[1], FFT_N / 2 - 1, 6, 2, 4);

    /* 每隔一段时间通过串口输出最高频带位置信息 */
    {
        static uint8_t fft_uart_div = 0;
        if (++fft_uart_div > 3) {
            fft_uart_div = 0;
            UART_sendStr("--- FFT Uo1 Test ---\r\nMax Mag Band: ");
            UART_sendNum(mag_max_disp);
            UART_sendStr("\r\n\r\n");
        }
    }
}

void Display_toggleSubMode(PageID page_id)
{
    if (page_id == PAGE_VPP) {
        vpp_sub_mode ^= 1;
    }
}

void Display_init(void)
{
    LCD_init();
    LCD_clear();
}

void Display_refresh(PageID page_id)
{
    switch (page_id) {
    case PAGE_INFO:
        page_info();
        break;
    case PAGE_FREQ:
        page_freq();
        break;
    case PAGE_WAVE:
        page_wave();
        break;
    case PAGE_VPP:
        page_vpp();
        break;
    case PAGE_FFT:
        page_fft();
        break;
    default:
        break;
    }
}
