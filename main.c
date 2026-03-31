/*
 * main.c
 * =============================================
 * 2026 Electronic Info Cup - Multi-Signal Generator
 * MSP430G2553 + JLX12864G LCD
 * =============================================
 *
 * State machine:
 *   KEY1 short press -> next page (INFO -> FREQ -> WAVE -> VPP -> FFT -> ...)
 *   KEY1 double press (only in VPP page) -> Vpp/Vrms switch
 *
 * Each page configures its peripherals on entry and stops them on exit.
 */

#include <stdint.h>
#include <msp430g2553.h>
#include "Clock.h"
#include "GPIO.h"
#include "config.h"
#include "key.h"
#include "adc_measure.h"
#include "timer_capture.h"
#include "display.h"

/* ============ Global State ============ */
static PageID cur_page  = PAGE_INFO;
static PageID prev_page = PAGE_COUNT; /* invalid initially, force first refresh */

#define DBL_CLICK_WINDOW_TICKS 15 /* 15 * 20ms = 300ms */

/* Forward declarations for static page callbacks used by page_next() */
static void page_leave(PageID page_id);
static void page_enter(PageID page_id);

static void page_next(void)
{
    page_leave(cur_page);

    cur_page = (PageID)((uint8_t)cur_page + 1);
    if (cur_page >= PAGE_COUNT) {
        cur_page = PAGE_INFO;
    }

    page_enter(cur_page);
}

/* ============ Page enter/leave callbacks ============ */

/* Leave current page: stop peripherals used by this page */
static void page_leave(PageID page_id)
{
    switch (page_id) {
    case PAGE_FREQ:
        Capture_stopFreq();
        break;
    default:
        /* VPP/WAVE/FFT pages use ADC single-shot, no stop needed */
        break;
    }
    Capture_stopAll(); /* Safety: ensure Timer is stopped */
}

/* Enter new page: start corresponding peripherals */
static void page_enter(PageID page_id)
{
    switch (page_id) {
    case PAGE_FREQ:
        Capture_startFreq();
        break;
    default:
        /* ADC pages configure on-demand inside Display_refresh */
        break;
    }
}

/* ============ Main Function ============ */

int main(void)
{
    uint8_t key_event;
    uint8_t need_refresh = 1;
    uint8_t refresh_tick = 0;
    uint8_t wait_second_click = 0;
    uint8_t second_click_ticks = 0;

    /* --- System Init --- */
    WDTCTL = WDTPW | WDTHOLD;  /* Stop watchdog */
    Clock_init();                /* DCO 16MHz */

    /* --- Peripheral Init --- */
    Key_init();                  /* Button P1.3 */
    ADC_init();                  /* ADC pins */
    Capture_init();              /* Timer_A0 capture pins */
    Display_init();              /* LCD init + clear */

    /* --- Enable global interrupts --- */
    __bis_SR_register(GIE);

    /* --- Show initial page --- */
    page_enter(cur_page);
    Display_refresh(cur_page);
    prev_page = cur_page;

    /* ============ Main Loop ============ */
    while (1) {
        /* Key event from interrupt */
        key_event = Key_getEvent();

        if (key_event == KEY_1_SHORT) {
            if (cur_page != PAGE_INFO) {
                if (wait_second_click) {
                    /* 双击: 切换同页的子模式 */
                    Display_toggleSubMode(cur_page);
                    wait_second_click = 0;
                    second_click_ticks = 0;
                    need_refresh = 1;
                } else {
                    /* 等待第二次点击 */
                    wait_second_click = 1;
                    second_click_ticks = 0;
                }
            } else {
                /* 单击直接翻页 */
                page_next();
                need_refresh = 1;
            }
        }

        /* 双击超时 */
        if (wait_second_click) {
            second_click_ticks++;
            if (second_click_ticks >= DBL_CLICK_WINDOW_TICKS) {
                wait_second_click = 0;
                second_click_ticks = 0;
                page_next();
                need_refresh = 1;
            }
        }
        /* Unified tick-based refresh with rate limiting */
        refresh_tick++;

        if (need_refresh) {
            Display_refresh(cur_page);
            need_refresh = 0;
            refresh_tick = 0;
        } else {
            uint8_t do_refresh = 0;

            switch (cur_page) {
            case PAGE_FREQ:
                if (g_freq_ready && refresh_tick >= 10) { /* ~200ms */
                    do_refresh = 1;
                }
                break;
            case PAGE_VPP:
            case PAGE_WAVE:
            case PAGE_FFT:
                if (refresh_tick >= 10) { /* ~200ms */
                    do_refresh = 1;
                }
                break;
            default: /* PAGE_INFO: no auto-refresh */
                break;
            }

            if (do_refresh) {
                Display_refresh(cur_page);
                refresh_tick = 0;
            }
        }

        /* Keep a light scheduler tick; key debounce is done in ISR */
        delay(20);
    }
}