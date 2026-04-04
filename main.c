#include <msp430g2553.h>
#include <stdint.h>
#include "Clock.h"
#include "config.h"
#include "key.h"
#include "adc_measure.h"
#include "timer_capture.h"
#include "display.h"

uint8_t current_page = PAGE_INFO;

void main(void) {
    WDTCTL = WDTPW + WDTHOLD; 
    Clock_init(); 
    Key_init();
    ADC_init();
    Capture_init();
    Display_init(); 
    
    __enable_interrupt();

    uint8_t page_changed = 1;

    while(1) {
        if (current_page >= PAGE_COUNT) {
            current_page = PAGE_INFO;
            page_changed = 1;
        }

        // ��ȡ�̰��¼�������ҳ����ת (0 -> 1 -> 2 -> ... -> 5 -> 0)
        if (Key_scan() == KEY_1_SHORT) {
            current_page = (current_page + 1) % PAGE_COUNT;
            page_changed = 1;
        }

        if (page_changed) {
            page_changed = 0;
            // �л�ҳ��ʱ������ϴ����ӡ�±�ͷ
            Display_drawHeader(current_page);
            
            // ������ҳ�洦���ײ㶨ʱ������/�ָ�������޶�ʡ���������
            if (current_page == PAGE_FREQ_DUTY) {
                Capture_start(); // ��Ҫ��Ƶ�ʲſ��ж�
            } else {
                Capture_stop();  // ����ʱ�������ж�
            }
        }

        // ����ˢ������������
        Display_refresh(current_page);
    }
}

