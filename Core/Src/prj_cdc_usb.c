#include "prj_cdc_usb.h"
#include "circular-buffer.h"
#include "error.h"
#include "slcan.h"
#include "usbd_cdc.h"
#include "utils.h"
#include "utils_conf.h"

extern USBD_HandleTypeDef hUsbDeviceFS;

#define TX_BUF_SIZE 64 // Linear TX buf size
#define RX_BUF_SIZE CDC_DATA_FS_MAX_PACKET_SIZE // Size of RX buffer item

#define RX_FRAME_CNT 16 // Total 1024 bytes for incoming buffer

uint8_t* activeBuffer = NULL;

typedef struct rxFrame_ {
    uint8_t* buf;
    uint32_t len;
} rxFrame_t;

StaticMemPool_t rxMemoryPool;
bool rxBufUsed[RX_FRAME_CNT];
uint8_t rxBuf[RX_BUF_SIZE * RX_FRAME_CNT];
static rxFrame_t rxMemPoolPtrBuf[RX_FRAME_CNT];
static CBuf_t* rxUsbCdcQueue;

static uint8_t slcan_str[SLCAN_MTU];
static uint8_t slcan_str_index = 0;

// #define TX_CAN_BUS_QUEUE_FRAME_CNT 24
// static txCanBusFrame_t txCanBusBuffer[TX_CAN_BUS_QUEUE_FRAME_CNT];
// static CBuf_t* txCanBusQueue;

bool prj_usb_cdc_init_buffers(void) {
    rxUsbCdcQueue = CBUF_INIT(rxMemPoolPtrBuf, true);
    if (rxUsbCdcQueue == NULL)
        return false;
    if (!static_mem_pool_init(&rxMemoryPool, rxBuf, rxBufUsed, sizeof(rxBuf), RX_BUF_SIZE))
        return false;
    return true;
}

bool prj_usb_cdc_set_buffers(void) {
    uint8_t* buf = static_mem_pool_alloc(&rxMemoryPool);
    if (buf == NULL)
        return false;
    activeBuffer = buf;
    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, buf);
    if (USBD_CDC_ReceivePacket(&hUsbDeviceFS) != USBD_OK) {
        static_mem_pool_free(&rxMemoryPool, buf);
        activeBuffer = NULL;
        return false;
    }
    return true;
}

bool prj_usb_cdc_reset_buffers(void) {
    if (activeBuffer)
        static_mem_pool_free(&rxMemoryPool, activeBuffer);
    activeBuffer = NULL;
    while (!cb_is_empty(rxUsbCdcQueue)) {
        rxFrame_t Frame;
        if (!cb_pop(rxUsbCdcQueue, &Frame))
            break;
        static_mem_pool_free(&rxMemoryPool, Frame.buf);
    }
    slcan_str_index = 0;
    return true;
}

void prj_usb_cdc_run(void) {
    if (cb_is_empty(rxUsbCdcQueue))
        return;
    rxFrame_t Frame;
    if (!cb_pop(rxUsbCdcQueue, &Frame)) {
        debug_printf("ERROR: cannot pop RX USB frame!\n");
        return;
    }
    for (uint32_t i = 0; i < Frame.len; i++) {
        if (Frame.buf[i] == '\r') {
            int8_t result   = slcan_parse_str(slcan_str, slcan_str_index);
            slcan_str_index = 0;
        } else {
            // Check for overflow of buffer
            if (slcan_str_index >= SLCAN_MTU) {
                // TODO: Return here and discard this CDC buffer?
                slcan_str_index = 0;
            }
            slcan_str[slcan_str_index++] = Frame.buf[i];
        }
    }
    static_mem_pool_free(&rxMemoryPool, Frame.buf);
}

// IRQ context
bool prj_usb_cdc_on_incoming_message(uint8_t* buf, uint32_t* len) {
    uint8_t* next = static_mem_pool_alloc(&rxMemoryPool);
    if (next == NULL) {
        error_assert(ERR_FULLBUF_USBRX);
        // Listen again on the same buffer. Old data will be overwritten.
        activeBuffer = buf;
        USBD_CDC_SetRxBuffer(&hUsbDeviceFS, buf);
        USBD_CDC_ReceivePacket(&hUsbDeviceFS);
        return false;
    }
    rxFrame_t Frame;
    Frame.buf = buf;
    Frame.len = *len;
    if (!cb_push(rxUsbCdcQueue, &Frame)) {
        static_mem_pool_free(&rxMemoryPool, buf);
        debug_printf("ERROR: cannot push RX USB frame!\n");
        error_assert(ERR_FULLBUF_USBRX);
    }
    activeBuffer = next;
    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, next);
    USBD_CDC_ReceivePacket(&hUsbDeviceFS);
    return true;
}
