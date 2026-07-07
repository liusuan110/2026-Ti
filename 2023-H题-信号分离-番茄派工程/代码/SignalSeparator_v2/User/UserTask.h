#ifndef __USER_TASK_H__
#define __USER_TASK_H__

#include <stdint.h>
#include "ti_msp_dl_config.h"
#include "Interrupts.h"
#include "Tick.h"
#include "UserADC.h"
#include "DDS.h"
#include "RGBLED.h"
#include "SignalSeparator.h"
#include "Encoder.h"

void UserTask_init(void);
void UserTask_loop(void);

#endif /* #ifndef __USER_TASK_H__ */
