/*
Name: 
ble_reception.c
* 
Authorship:
EECS 452 W26 - Audio Spatialization
Written by Seohyeon Choi
*
Description:

app_main the runs the UART and BLE initialization processes.
Actual UART and BLE implementation is in ble_reception.c
*/
#include "ble_reception.h"

void
app_main(void)
{
    uart_init();
    
    ble_reception_init();


}