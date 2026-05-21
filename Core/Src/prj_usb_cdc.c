#include "circular-buffer.h"
#include "error.h"
#include "prj_usb_cdc.h"
#include "slcan.h"
#include "usbd_cdc.h"
#include "utils.h"
#include "utils_conf.h"

#ifdef DEBUG
extern Statistics_t Statistics;
#endif

#ifdef DEBUG
#define INCREASE_STAT_INCOMING_QUEUE_OVERFLOW() INCREASE_STATISTIC_CNT(Statistics.usbCdcIncomingQueueOverflow)
#else
#define INCREASE_STAT_INCOMING_QUEUE_OVERFLOW() __NOP()
#endif

#ifdef DEBUG
#define INCREASE_STAT_OUTGOING_QUEUE_OVERFLOW() INCREASE_STATISTIC_CNT(Statistics.usbCdcOutgoingQueueOverflow)
#else
#define INCREASE_STAT_OUTGOING_QUEUE_OVERFLOW() __NOP()
#endif

extern USBD_HandleTypeDef hUsbDeviceFS;

typedef struct rxFrame_ {
    uint8_t* buf;
    uint32_t len;
} Frame_t;

#define RX_BUF_SIZE CDC_DATA_FS_MAX_PACKET_SIZE
#define TX_BUF_SIZE CDC_DATA_FS_MAX_PACKET_SIZE

#define RX_FRAME_CNT 16 // Total 1024 bytes for incoming buffer
#define TX_FRAME_CNT 16 // Total 1024 bytes for outgoing buffer

typedef struct PrjUsbCdc_ {
    struct {
        uint8_t* activeBuffer;
        struct {
            StaticMemPool_t Handle;
            bool poolUsed[RX_FRAME_CNT];
            uint8_t pool[RX_FRAME_CNT * RX_BUF_SIZE];
        } MemPool;
        struct {
            CBuf_t* Handle;
            Frame_t buf[RX_FRAME_CNT];
        } Queue;
    } Rx;
    struct {
        uint8_t* activeBuffer;
        struct {
            StaticMemPool_t Handle;
            bool poolUsed[TX_FRAME_CNT];
            uint8_t pool[TX_FRAME_CNT * TX_BUF_SIZE];
        } MemPool;
        struct {
            CBuf_t* Handle;
            Frame_t buf[TX_FRAME_CNT];
        } Queue;
    } Tx;
} PrjUsbCdc_t;

static PrjUsbCdc_t PrjUsbCdc = {0};

static uint8_t slcan_str[SLCAN_MTU];
static uint8_t slcan_str_index = 0;

bool prj_usb_cdc_init(void) {
    PrjUsbCdc.Rx.Queue.Handle = CBUF_INIT(PrjUsbCdc.Rx.Queue.buf, true);
    PrjUsbCdc.Tx.Queue.Handle = CBUF_INIT(PrjUsbCdc.Tx.Queue.buf, true);
    if (PrjUsbCdc.Rx.Queue.Handle == NULL || PrjUsbCdc.Tx.Queue.Handle == NULL)
        return false;
    // clang-format off
    if (!static_mem_pool_init(
        &PrjUsbCdc.Rx.MemPool.Handle,
        PrjUsbCdc.Rx.MemPool.pool,
        PrjUsbCdc.Rx.MemPool.poolUsed,
        sizeof(PrjUsbCdc.Rx.MemPool.pool),
        RX_BUF_SIZE
        ))
        return false;
    if (!static_mem_pool_init(
        &PrjUsbCdc.Tx.MemPool.Handle,
        PrjUsbCdc.Tx.MemPool.pool,
        PrjUsbCdc.Tx.MemPool.poolUsed,
        sizeof(PrjUsbCdc.Tx.MemPool.pool),
        TX_BUF_SIZE
        ))
        return false;
    // clang-format on
    return true;
}

static bool usb_cdc_tx_ready(void) {
    if (hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED)
        return false;
    USBD_CDC_HandleTypeDef* hcdc = (USBD_CDC_HandleTypeDef*) hUsbDeviceFS.pClassData;
    if (hcdc == NULL)
        return false;
    return hcdc->TxState == 0;
}

void prj_usb_cdc_run(void) {
    if (PrjUsbCdc.Tx.activeBuffer != NULL && usb_cdc_tx_ready()) {
        static_mem_pool_free(&PrjUsbCdc.Tx.MemPool.Handle, PrjUsbCdc.Tx.activeBuffer);
        PrjUsbCdc.Tx.activeBuffer = NULL;
    }
    if (!cb_is_empty(PrjUsbCdc.Tx.Queue.Handle) && usb_cdc_tx_ready()) {
        Frame_t Frame;
        if (cb_pop(PrjUsbCdc.Tx.Queue.Handle, &Frame)) {
            USBD_CDC_SetTxBuffer(&hUsbDeviceFS, Frame.buf, Frame.len);
            if (USBD_CDC_TransmitPacket(&hUsbDeviceFS) == USBD_OK) {
                PrjUsbCdc.Tx.activeBuffer = Frame.buf;
            } else {
                static_mem_pool_free(&PrjUsbCdc.Tx.MemPool.Handle, Frame.buf);
            }
        }
    }
    if (cb_is_empty(PrjUsbCdc.Rx.Queue.Handle))
        return;
    Frame_t Frame;
    if (!cb_pop(PrjUsbCdc.Rx.Queue.Handle, &Frame)) {
        debug_printf("ERROR: cannot pop RX USB frame!\n");
        return;
    }
    for (uint32_t i = 0; i < Frame.len; i++) {
        if (Frame.buf[i] == '\r') {
            slcan_parse_str(slcan_str, slcan_str_index);
            slcan_str_index = 0;
        } else {
            if (slcan_str_index >= SLCAN_MTU) {
                slcan_str_index = 0;
            }
            slcan_str[slcan_str_index++] = Frame.buf[i];
        }
    }
    static_mem_pool_free(&PrjUsbCdc.Rx.MemPool.Handle, Frame.buf);
}

bool prj_usb_cdc_send(const uint8_t* buf, const uint32_t len) {
    if (len > TX_BUF_SIZE)
        return false;
    uint8_t* pool = static_mem_pool_alloc(&PrjUsbCdc.Tx.MemPool.Handle);
    if (pool == NULL)
        return false;
    memcpy(pool, buf, len);
    Frame_t Frame;
    Frame.buf = pool;
    Frame.len = len;
    if (!cb_push(PrjUsbCdc.Tx.Queue.Handle, &Frame)) {
        static_mem_pool_free(&PrjUsbCdc.Tx.MemPool.Handle, pool);
        INCREASE_STAT_OUTGOING_QUEUE_OVERFLOW();
        return false;
    }
    return true;
}

// IRQ context
bool prj_usb_cdc_set_buffers(void) {
    USBD_CDC_SetTxBuffer(&hUsbDeviceFS, NULL, 0);
    uint8_t* buf = static_mem_pool_alloc(&PrjUsbCdc.Rx.MemPool.Handle);
    if (buf == NULL)
        return false;
    PrjUsbCdc.Rx.activeBuffer = buf;
    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, buf);
    if (USBD_CDC_ReceivePacket(&hUsbDeviceFS) != USBD_OK) {
        static_mem_pool_free(&PrjUsbCdc.Rx.MemPool.Handle, buf);
        PrjUsbCdc.Rx.activeBuffer = NULL;
        return false;
    }
    return true;
}

bool prj_usb_cdc_reset_buffers(void) {
    if (PrjUsbCdc.Rx.activeBuffer)
        static_mem_pool_free(&PrjUsbCdc.Rx.MemPool.Handle, PrjUsbCdc.Rx.activeBuffer);
    PrjUsbCdc.Rx.activeBuffer = NULL;
    if (PrjUsbCdc.Tx.activeBuffer)
        static_mem_pool_free(&PrjUsbCdc.Tx.MemPool.Handle, PrjUsbCdc.Tx.activeBuffer);
    PrjUsbCdc.Tx.activeBuffer = NULL;
    while (!cb_is_empty(PrjUsbCdc.Rx.Queue.Handle)) {
        Frame_t Frame;
        if (!cb_pop(PrjUsbCdc.Rx.Queue.Handle, &Frame))
            break;
        static_mem_pool_free(&PrjUsbCdc.Rx.MemPool.Handle, Frame.buf);
    }
    while (!cb_is_empty(PrjUsbCdc.Tx.Queue.Handle)) {
        Frame_t Frame;
        if (!cb_pop(PrjUsbCdc.Tx.Queue.Handle, &Frame))
            break;
        static_mem_pool_free(&PrjUsbCdc.Tx.MemPool.Handle, Frame.buf);
    }
    slcan_str_index = 0;
    return true;
}

bool prj_usb_cdc_on_incoming_message(uint8_t* buf, const uint32_t* len) {
    uint8_t* next = static_mem_pool_alloc(&PrjUsbCdc.Rx.MemPool.Handle);
    if (next == NULL) {
        error_assert(ERR_FULLBUF_USBRX);
        PrjUsbCdc.Rx.activeBuffer = buf;
        USBD_CDC_SetRxBuffer(&hUsbDeviceFS, buf);
        USBD_CDC_ReceivePacket(&hUsbDeviceFS);
        return false;
    }
    Frame_t Frame;
    Frame.buf = buf;
    Frame.len = *len;
    if (!cb_push(PrjUsbCdc.Rx.Queue.Handle, &Frame)) {
        static_mem_pool_free(&PrjUsbCdc.Rx.MemPool.Handle, buf);
        INCREASE_STAT_INCOMING_QUEUE_OVERFLOW();
        error_assert(ERR_FULLBUF_USBRX);
    }
    PrjUsbCdc.Rx.activeBuffer = next;
    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, next);
    USBD_CDC_ReceivePacket(&hUsbDeviceFS);
    return true;
}
