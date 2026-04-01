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

/* Freq page sub-mode */
static uint8_t freq_sub_mode = 0; /* 0=Freq+Amp, 1=Amp only/Freq only */

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
    clear_text_band(4);
    clear_text_band(6);
    
    if (freq_sub_mode == 0) {
        if (g_freq_ready) {
            /* 显示为 kHz，保留一位小数，即除以 100 并设置 decimal=1 */
            LCD_showMeasure(2, 4, "f=", g_freq_hz / 100, 1, "kHz");
        } else {
            LCD_showGB2312Str(2, 16, (u8 *)"Measuring f...");
        }
        LCD_showGB2312Str(6, 0, (u8 *)"DBL KEY1: freq/amp");
    } else {
        LCD_showMeasure(4, 4, "A=", (uint32_t)result.vpp_mv, 0, "mV");
        LCD_showGB2312Str(6, 0, (u8 *)"DBL KEY1: amp/freq");
    }
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
    } else {
        LCD_showMeasure(2, 4, "Vrms=", (uint32_t)result.vrms_mv, 0, "mV");
        LCD_showMeasure(4, 4, "    =", (uint32_t)result.vrms_mv, 3, "V");
    }

    clear_text_band(6);
    LCD_showGB2312Str(6, 0, (u8 *)"DBL KEY1:Vpp/Vrms");
}

/* Task 7: simplest waveform display (Uo3/Uo5). */
static void page_wave(void)
{
    uint8_t page_changed = (last_title_page != PAGE_WAVE);

    draw_title(PAGE_WAVE);

    if (page_changed) {
        LCD_clearPages(2, 7);
    }

    clear_text_band(3);
    clear_text_band(5);
    LCD_showGB2312Str(3, 8, (u8 *)"Wave Disabled");
    LCD_showGB2312Str(5, 0, (u8 *)"Keep this page only");
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
}

void Display_toggleSubMode(PageID page_id)
{
    if (page_id == PAGE_VPP) {
        vpp_sub_mode ^= 1;
    } else if (page_id == PAGE_FREQ) {
        freq_sub_mode ^= 1;
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
