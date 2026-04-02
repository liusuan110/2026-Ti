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
    int16_t *wave_buf = g_buf_a;
    uint16_t max_points = WAVE_RAW_POINTS;
    uint16_t screen_w = 128;
    uint16_t i;
    int32_t sum = 0;
    int16_t dc_mean;
    uint16_t trigger_idx = 0;
    uint32_t freq_hint = 25000U;
    uint32_t sample_interval = 90U;
    uint16_t spp_from_timing = 0;
    int16_t adc_min = 1023;
    int16_t adc_max = 0;
    int16_t adc_min2 = 1023;
    int16_t adc_max2 = 0;
    int16_t span;
    int16_t threshold_high;
    int16_t threshold_low;

    static uint8_t scale_inited = 0;
    static int16_t smooth_map_min = 0;
    static int16_t smooth_map_max = 1023;

    draw_title(PAGE_WAVE);

    if (g_freq_hz > 100000U) {
        LCD_clearPages(2, 7);
        LCD_showGB2312Str(4, 16, (u8*)" 信号频率过高 ");
        LCD_showGB2312Str(5, 16, (u8*)" 波形显示已关闭");
        return;
    }

    if (g_freq_period > 0U) {
        freq_hint = SYS_FREQ / g_freq_period;
    } else if (g_freq_hz > 0U) {
        freq_hint = g_freq_hz;
    }
    if (freq_hint < 20000U || freq_hint > 30000U) {
        freq_hint = 25000U;
    }

    sample_interval = ADC_sampleToBufferAdaptive(ADC_CH_WAVE_VIEW, wave_buf, max_points, freq_hint);
    if (sample_interval == 0U) sample_interval = 90U;

    if (freq_hint > 0U) {
        uint32_t denom = freq_hint * sample_interval;
        if (denom > 0U) {
            spp_from_timing = (uint16_t)(SYS_FREQ / denom);
        }
    }
    if (spp_from_timing < 6U) spp_from_timing = 6U;
    if (spp_from_timing > (max_points / 2U)) spp_from_timing = (uint16_t)(max_points / 2U);

    /* 轻量一阶平滑，抑制高频毛刺导致的散点感 */
    for (i = 1; i < max_points; i++) {
        wave_buf[i] = (int16_t)(((int32_t)wave_buf[i - 1] * 3 + (int32_t)wave_buf[i] + 2) / 4);
    }

    for (i = 0; i < max_points; i++) {
        int16_t v = wave_buf[i];
        sum += v;
        if (v <= adc_min) {
            adc_min2 = adc_min;
            adc_min = v;
        } else if (v < adc_min2) {
            adc_min2 = v;
        }

        if (v >= adc_max) {
            adc_max2 = adc_max;
            adc_max = v;
        } else if (v > adc_max2) {
            adc_max2 = v;
        }
    }

    /* 抗毛刺：优先用“次极值”作为显示量程基准，避免单点尖峰拉坏波形 */
    if (adc_max2 > adc_min2) {
        adc_min = adc_min2;
        adc_max = adc_max2;
    }

    dc_mean = (int16_t)(sum / (int32_t)max_points);

    span = (int16_t)(adc_max - adc_min);
    if (span < 6) {
        LCD_clearPages(2, 7);
        LCD_showGB2312Str(4, 16, (u8*)" 信号幅度过小 ");
        LCD_showGB2312Str(5, 16, (u8*)" 或通道未接入 ");
        return;
    }

    {
        int16_t hyst = (int16_t)(span / 10);
        if (hyst < 2) hyst = 2;
        threshold_high = (int16_t)(dc_mean + hyst);
        threshold_low = (int16_t)(dc_mean - hyst);
    }

    for (i = 1; i < (max_points / 3); i++) {
        if (wave_buf[i - 1] <= threshold_low && wave_buf[i] >= threshold_high) {
            trigger_idx = i;
            break;
        }
    }

    LCD_clearPages(2, 7);

    {
        uint16_t valid_points = (uint16_t)(max_points - trigger_idx);
        uint16_t trigger_idx_2 = 0;
        uint16_t trigger_idx_3 = 0;
        uint16_t spp = 0;
        int16_t pad;
        int16_t raw_map_min;
        int16_t raw_map_max;

        if (valid_points < 4U) {
            valid_points = max_points;
            trigger_idx = 0;
        }

        /* 估计 SPP（每周期采样点数），将显示窗口锁定到约 2.5 周期 */
        for (i = (uint16_t)(trigger_idx + 2U); i < (uint16_t)(max_points - 1U); i++) {
            if (wave_buf[i - 1] <= threshold_low && wave_buf[i] >= threshold_high) {
                trigger_idx_2 = i;
                break;
            }
        }
        if (trigger_idx_2 > trigger_idx) {
            for (i = (uint16_t)(trigger_idx_2 + 2U); i < (uint16_t)(max_points - 1U); i++) {
                if (wave_buf[i - 1] <= threshold_low && wave_buf[i] >= threshold_high) {
                    trigger_idx_3 = i;
                    break;
                }
            }
        }

        if (trigger_idx_3 > trigger_idx_2 && trigger_idx_2 > trigger_idx) {
            spp = (uint16_t)((trigger_idx_3 - trigger_idx) / 2U);
        } else if (trigger_idx_2 > trigger_idx) {
            spp = (uint16_t)(trigger_idx_2 - trigger_idx);
        }

        if (spp == 0U) {
            spp = spp_from_timing;
        } else {
            uint16_t spp_min = (uint16_t)((uint32_t)spp_from_timing * 7U / 10U);
            uint16_t spp_max = (uint16_t)((uint32_t)spp_from_timing * 13U / 10U);
            if (spp_min < 6U) spp_min = 6U;
            if (spp_max < spp_min) spp_max = spp_min;
            if (spp < spp_min || spp > spp_max) {
                spp = spp_from_timing;
            }
        }

        if (spp >= 6U && spp <= (max_points / 2U)) {
            uint16_t target_points = (uint16_t)(((uint32_t)spp * 5U) / 2U); /* 约 2.5 周期 */
            uint16_t min_points = (uint16_t)(spp * 2U);                      /* 至少 2 周期 */
            uint16_t max_points_3cyc = (uint16_t)(spp * 3U);                 /* 至多 3 周期 */

            if (target_points < min_points) target_points = min_points;
            if (target_points > max_points_3cyc) target_points = max_points_3cyc;
            if (target_points > valid_points) target_points = valid_points;
            if (target_points >= 8U) valid_points = target_points;
        }

        pad = (int16_t)(span / 8);
        if (pad < 4) pad = 4;

        raw_map_min = (int16_t)(adc_min - pad);
        raw_map_max = (int16_t)(adc_max + pad);
        if (raw_map_min < 0) raw_map_min = 0;
        if (raw_map_max > 1023) raw_map_max = 1023;
        if (raw_map_max <= raw_map_min) raw_map_max = (int16_t)(raw_map_min + 1);

        if (!scale_inited) {
            smooth_map_min = raw_map_min;
            smooth_map_max = raw_map_max;
            scale_inited = 1;
        } else {
            smooth_map_min = (int16_t)(((int32_t)smooth_map_min * 7 + (int32_t)raw_map_min * 3 + 5) / 10);
            smooth_map_max = (int16_t)(((int32_t)smooth_map_max * 7 + (int32_t)raw_map_max * 3 + 5) / 10);
        }
        if (smooth_map_max <= smooth_map_min) smooth_map_max = (int16_t)(smooth_map_min + 1);

        for (i = 1; i < screen_w; i++) {
            uint32_t float_idx = ((uint32_t)i * (uint32_t)(valid_points - 1U) * 256U) / (uint32_t)(screen_w - 1U);
            uint16_t idx_int = (uint16_t)(float_idx >> 8) + trigger_idx;
            uint16_t idx_frac = (uint16_t)(float_idx & 0xFFU);

            uint32_t float_idx_prev = ((uint32_t)(i - 1U) * (uint32_t)(valid_points - 1U) * 256U) / (uint32_t)(screen_w - 1U);
            uint16_t idx_int_prev = (uint16_t)(float_idx_prev >> 8) + trigger_idx;
            uint16_t idx_frac_prev = (uint16_t)(float_idx_prev & 0xFFU);

            int16_t v0, v1, vp0, vp1;
            int16_t val_curr;
            int16_t val_prev;
            int32_t norm_curr;
            int32_t norm_prev;
            uint8_t y_curr;
            uint8_t y_prev;

            if (idx_int >= (max_points - 1U)) idx_int = (uint16_t)(max_points - 1U);
            if (idx_int_prev >= (max_points - 1U)) idx_int_prev = (uint16_t)(max_points - 1U);

            v0 = wave_buf[idx_int];
            v1 = (idx_int + 1U < max_points) ? wave_buf[idx_int + 1U] : v0;
            vp0 = wave_buf[idx_int_prev];
            vp1 = (idx_int_prev + 1U < max_points) ? wave_buf[idx_int_prev + 1U] : vp0;

            val_curr = (int16_t)(v0 + (((int32_t)(v1 - v0) * idx_frac) >> 8));
            val_prev = (int16_t)(vp0 + (((int32_t)(vp1 - vp0) * idx_frac_prev) >> 8));

            norm_curr = ((int32_t)val_curr - (int32_t)smooth_map_min) * 47 / ((int32_t)smooth_map_max - (int32_t)smooth_map_min);
            norm_prev = ((int32_t)val_prev - (int32_t)smooth_map_min) * 47 / ((int32_t)smooth_map_max - (int32_t)smooth_map_min);

            if (norm_curr < 0) norm_curr = 0;
            if (norm_curr > 47) norm_curr = 47;
            if (norm_prev < 0) norm_prev = 0;
            if (norm_prev > 47) norm_prev = 47;

            y_curr = (uint8_t)(63 - norm_curr);
            y_prev = (uint8_t)(63 - norm_prev);

            if (y_curr < 16U) y_curr = 16U;
            if (y_curr > 63U) y_curr = 63U;
            if (y_prev < 16U) y_prev = 16U;
            if (y_prev > 63U) y_prev = 63U;

            if (y_curr == y_prev) {
                LCD_drawDot((u8)i, y_curr);
            } else if (y_curr > y_prev) {
                uint8_t y;
                for (y = y_prev; y <= y_curr; y++) {
                    LCD_drawDot((u8)i, y);
                }
            } else {
                uint8_t y;
                for (y = y_curr; y <= y_prev; y++) {
                    LCD_drawDot((u8)i, y);
                }
            }
        }
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
