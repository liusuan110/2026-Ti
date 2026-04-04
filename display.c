#include "display.h"
#include <msp430g2553.h>
#include "JLX12864G.h"
#include "lcd_draw.h"
#include "adc_measure.h"
#include "timer_capture.h"
#include "fft.h"

/*
 * MSP430G2553 RAM is only 512 bytes.
 * Keep display buffers minimal and avoid large local arrays.
 */
static uint8_t wave_buf[WAVE_RAW_POINTS];
static int16_t fft_buf[FFT_N * 2];
static uint8_t spectrum[FFT_N / 2];

void Display_init(void) {
    LCD_init();
    LCD_clear();
}

void Display_drawHeader(PageID page) {
    LCD_clear();
    switch(page) {
        case PAGE_INFO:      LCD_showGB2312Str(0, 0, (u8*)"1: TEAM INFO   "); break;
        case PAGE_VDC:       LCD_showGB2312Str(0, 0, (u8*)"2: DC VOLTAGE  "); break;
        case PAGE_DECODE:    LCD_showGB2312Str(0, 0, (u8*)"3: SIGNAL DESC "); break;
        case PAGE_FREQ_DUTY: LCD_showGB2312Str(0, 0, (u8*)"4: FREQ & DUTY "); break;
        case PAGE_WAVE:      LCD_showGB2312Str(0, 0, (u8*)"5: OSCILLOSCOPE"); break;
        case PAGE_SPECTRUM:  LCD_showGB2312Str(0, 0, (u8*)"6: SPECTRUM    "); break;
    }
}

void Display_refresh(PageID page) {
    switch(page) {
        case PAGE_INFO: {
            LCD_showGB2312Str(2, 0, (u8*)"TEAM: THE BEST ");
            LCD_showGB2312Str(4, 0, (u8*)"NAME: ZHANG SAN");
            LCD_showGB2312Str(6, 0, (u8*)"NAME: LI SI    ");
            __delay_cycles(1600000);
            break;
        }
        case PAGE_VDC: {
            uint16_t mv = ADC_measureVDC();
            LCD_showMeasure(2, 0, "V=", mv, 3, "V    ");
            __delay_cycles(1600000);
            break;
        }
        case PAGE_DECODE: {
            uint16_t mv = ADC_measureVDC();
            char code = '?';
            if (mv >= 600 && mv <= 1000) code = 'A';
            else if (mv >= 1400 && mv <= 1800) code = 'B';
            else if (mv >= 2200 && mv <= 2600) code = 'C';
            else if (mv >= 3000 && mv <= 3400) code = 'D';

            char str[16] = "KEY: ?   ";
            str[5] = code;
            LCD_showGB2312Str(3, 0, (u8*)str);
            __delay_cycles(1600000);
            break;
        }
        case PAGE_FREQ_DUTY: {
            Capture_poll();
            LCD_showMeasure(2, 0, "F=", g_freq_hz, 0, "Hz     ");
            LCD_showMeasure(4, 0, "D=", g_duty, 0, "%      ");
            __delay_cycles(3200000);
            break;
        }
        case PAGE_WAVE: {
            uint8_t i;
            for (i = 0; i < WAVE_RAW_POINTS; i++) {
                uint16_t adc = ADC_readSingle();
                wave_buf[i] = (uint8_t)(63U - ((adc * 64U) / 1024U));
            }

            LCD_clearPages(2, 7);
            for (i = 0; i < (WAVE_RAW_POINTS - 1U); i++) {
                uint8_t x1 = (uint8_t)((i * 128U) / WAVE_RAW_POINTS);
                uint8_t x2 = (uint8_t)(((i + 1U) * 128U) / WAVE_RAW_POINTS);
                uint8_t y1 = wave_buf[i];
                uint8_t y2 = wave_buf[i + 1U];
                if (y1 > 63) y1 = 63;
                if (y2 > 63) y2 = 63;
                LCD_drawVLine(x1, (y1 < y2 ? y1 : y2), (y1 > y2 ? y1 : y2));
                if (x2 != x1) {
                    LCD_drawVLine(x2, (y1 < y2 ? y1 : y2), (y1 > y2 ? y1 : y2));
                }
            }
            __delay_cycles(1600000);
            break;
        }
        case PAGE_SPECTRUM: {
            uint8_t i;
            int16_t *fft_real = &fft_buf[0];
            int16_t *fft_imag = &fft_buf[FFT_N];

            for (i = 0; i < FFT_N; i++) {
                fft_real[i] = (int16_t)ADC_readSingle();
                fft_imag[i] = 0;
            }
            fft32(fft_real, fft_imag);

            for (i = 0; i < (FFT_N / 2); i++) {
                int16_t r = fft_real[i] < 0 ? -fft_real[i] : fft_real[i];
                int16_t img = fft_imag[i] < 0 ? -fft_imag[i] : fft_imag[i];
                uint16_t mag = r + img;
                spectrum[i] = (mag >> 4) > 48 ? 48 : (mag >> 4);
            }

            LCD_clearPages(2, 7);
            LCD_drawBars(spectrum, (FFT_N / 2), 5, 2, 0);
            __delay_cycles(1600000);
            break;
        }
    }
}

