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

    draw_title(PAGE_FREQ);
    LCD_clearPages(2, 7);

    ADC_measureVpp(ADC_CH_UO2, 300, &result);

    if (g_freq_ready) {
        LCD_showMeasure(2, 4, "f=", g_freq_hz, 0, "Hz");
    } else {
        LCD_showGB2312Str(2, 16, (u8 *)"Measuring f...");
    }

    LCD_showMeasure(5, 4, "A=", (uint32_t)result.vpp_mv, 0, "mV");
}

/* Task 8: Vpp / Vrms page (Uo4). */
static void page_vpp(void)
{
    VppResult result;

    draw_title(PAGE_VPP);
    LCD_clearPages(2, 7);

    ADC_measureVpp(ADC_CH_UO4, 500, &result);

    if (vpp_sub_mode == 0) {
        LCD_showMeasure(2, 4, "Vpp=", (uint32_t)result.vpp_mv, 0, "mV");
        LCD_showMeasure(4, 4, "   =", (uint32_t)result.vpp_mv, 3, "V");
    } else {
        LCD_showMeasure(2, 4, "Vrms=", (uint32_t)result.vrms_mv, 0, "mV");
        LCD_showMeasure(4, 4, "    =", (uint32_t)result.vrms_mv, 3, "V");
    }

    LCD_showGB2312Str(6, 0, (u8 *)"DBL KEY1:Vpp/Vrms");
}

/* Task 7: simplest waveform display (Uo3/Uo5). */
static void page_wave(void)
{
    int16_t *raw = g_buf_a;
    uint16_t i;
    int16_t adc_min = 1023;
    int16_t adc_max = 0;
    int16_t midpoint;
    int16_t trig_hyst;
    int16_t low_th, high_th;
    int16_t span, pad;
    int16_t map_min, map_max;
    int16_t raw_map_min, raw_map_max;
    uint16_t c1 = WAVE_RAW_POINTS;
    uint16_t c2 = WAVE_RAW_POINTS;
    uint16_t c3 = WAVE_RAW_POINTS;
    uint16_t spp = 0;
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
    uint8_t armed;

    /* Frame-to-frame stabilizers */
    static uint8_t seg_inited = 0;
    static uint16_t last_seg_start = 0;
    static uint16_t last_seg_len = 40;
    static uint8_t scale_inited = 0;
    static int16_t smooth_map_min = 0;
    static int16_t smooth_map_max = 1023;

    draw_title(PAGE_WAVE);

    /* Fixed-rate sampling for 5kHz input. */
    ADC_sampleToBuffer(ADC_CH_UO3_FFT, raw, WAVE_RAW_POINTS);

    /* Global min/max. */
    for (i = 0; i < WAVE_RAW_POINTS; i++) {
        if (raw[i] > adc_max) adc_max = raw[i];
        if (raw[i] < adc_min) adc_min = raw[i];
    }
    if (adc_max <= adc_min) adc_max = adc_min + 1;

    /* Keep only 2 cycles on screen: Schmitt-triggered rising crossings. */
    span = (int16_t)(adc_max - adc_min);
    if (span < 1) span = 1;
    midpoint = (int16_t)((adc_min + adc_max) / 2);
    trig_hyst = (int16_t)(span / 16);   /* ~6.25% hysteresis */
    if (trig_hyst < 3) trig_hyst = 3;
    low_th = (int16_t)(midpoint - trig_hyst);
    high_th = (int16_t)(midpoint + trig_hyst);

    armed = 0;
    for (i = 0; i < WAVE_RAW_POINTS; i++) {
        if (!armed) {
            if (raw[i] <= low_th) armed = 1;
        } else if (raw[i] >= high_th) {
            c1 = i;
            break;
        }
    }

    if (c1 + 2 < WAVE_RAW_POINTS) {
        armed = 0;
        for (i = (uint16_t)(c1 + 2); i < WAVE_RAW_POINTS; i++) {
            if (!armed) {
                if (raw[i] <= low_th) armed = 1;
            } else if (raw[i] >= high_th) {
                c2 = i;
                break;
            }
        }
    }

    if (c2 + 2 < WAVE_RAW_POINTS) {
        armed = 0;
        for (i = (uint16_t)(c2 + 2); i < WAVE_RAW_POINTS; i++) {
            if (!armed) {
                if (raw[i] <= low_th) armed = 1;
            } else if (raw[i] >= high_th) {
                c3 = i;
                break;
            }
        }
    }

    if (c3 < WAVE_RAW_POINTS && c3 > c1) {
        seg_start = c1;
        seg_len = (uint16_t)(c3 - c1);       /* exactly 2 cycles */
    } else if (c2 < WAVE_RAW_POINTS && c2 > c1) {
        spp = (uint16_t)(c2 - c1);
        if (spp >= 6 && (uint16_t)(c1 + spp * 2) <= WAVE_RAW_POINTS) {
            seg_start = c1;
            seg_len = (uint16_t)(spp * 2);   /* fallback: estimated 2 cycles */
        }
    }

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
    u8 bar_data[FFT_N / 2];
    uint16_t i;
    uint16_t mag_max = 0;

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

    for (i = 1; i < FFT_N / 2; i++) {
        if (mag_buf[i] > mag_max) mag_max = mag_buf[i];
    }
    if (mag_max == 0) mag_max = 1;

    bar_data[0] = 0;
    for (i = 1; i < FFT_N / 2; i++) {
        bar_data[i] = (u8)((uint32_t)mag_buf[i] * 47 / mag_max);
    }

    LCD_drawBars(&bar_data[1], FFT_N / 2 - 1, 6, 2, 4);
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
