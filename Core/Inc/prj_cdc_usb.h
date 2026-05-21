#ifndef PRJ_CDC_USB_H
#define PRJ_CDC_USB_H

#include "main.h"

bool prj_usb_cdc_init(void);
bool prj_usb_cdc_set_buffers(void);
bool prj_usb_cdc_reset_buffers(void);
void prj_usb_cdc_run(void);
bool prj_usb_cdc_send(const uint8_t* buf, uint32_t len);
bool prj_usb_cdc_on_incoming_message(uint8_t* buf, const uint32_t* len);

#endif // PRJ_CDC_USB_H
