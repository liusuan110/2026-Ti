#ifndef __DISPLAY_H__
#define __DISPLAY_H__

#include "config.h"

void Display_init(void);
void Display_drawHeader(PageID page);
void Display_refresh(PageID page);

#endif 
