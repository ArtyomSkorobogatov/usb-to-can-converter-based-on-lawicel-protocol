#include "prj_can.h"
#include "circular-buffer.h"
#include "discrete_output.h"
#include "error.h"
#include "prj_cdc_usb.h"
#include "slcan.h"
#include "stm32f0xx_hal.h"
#include "utils_conf.h"

extern DiscreteOutput_t LedRed;
extern DiscreteOutput_t LedBlue;
extern uint8_t cdc_message_buf[SLCAN_MTU];

typedef struct PrjCanBus_ {
    CAN_HandleTypeDef handle;
    CAN_FilterTypeDef filter;
    can_bus_state_t bus_state;
    uint32_t prescaler;
    uint8_t auto_retransmit;
    uint32_t silentMode;
} PrjCanBus_t;

static PrjCanBus_t PrjCanBus;
static rxCanBusFrame_t rxCanBusTmpBuf = {0};
static txCanBusFrame_t txCanBusTmpBuf = {0};

#define RX_CAN_BUS_QUEUE_FRAME_CNT 24
#define TX_CAN_BUS_QUEUE_FRAME_CNT 24
static rxCanBusFrame_t rxCanBusBuffer[RX_CAN_BUS_QUEUE_FRAME_CNT];
static txCanBusFrame_t txCanBusBuffer[TX_CAN_BUS_QUEUE_FRAME_CNT];
static CBuf_t* rxCanBusQueue;
static CBuf_t* txCanBusQueue;

bool prj_can_bus_set_bitrate(const enum can_bitrate bitrate) {
    if (PrjCanBus.bus_state == ON_BUS) {
        return false;
    }
    switch (bitrate) {
        case CAN_BITRATE_10K:
            PrjCanBus.prescaler = 600;
            break;
        case CAN_BITRATE_20K:
            PrjCanBus.prescaler = 300;
            break;
        case CAN_BITRATE_50K:
            PrjCanBus.prescaler = 120;
            break;
        case CAN_BITRATE_100K:
            PrjCanBus.prescaler = 60;
            break;
        case CAN_BITRATE_125K:
            PrjCanBus.prescaler = 48;
            break;
        case CAN_BITRATE_250K:
            PrjCanBus.prescaler = 24;
            break;
        case CAN_BITRATE_500K:
            PrjCanBus.prescaler = 12;
            break;
        case CAN_BITRATE_750K:
            PrjCanBus.prescaler = 8;
            break;
        default:
            PrjCanBus.prescaler = 6;
            break;
    }
    return true;
}

bool prj_can_bus_set_silent(const bool enable) {
    if (PrjCanBus.bus_state == ON_BUS) {
        return false;
    }
    PrjCanBus.silentMode = enable ? CAN_MODE_SILENT : CAN_MODE_NORMAL;
    return true;
}

// Callback for FIFO0 full
void HAL_CAN_RxFifo0FullCallback(CAN_HandleTypeDef* hcan) { error_assert(ERR_CANRXFIFO_OVERFLOW); }

bool prj_can_bus_init(void) {
    rxCanBusQueue = CBUF_INIT(rxCanBusBuffer, true);
    txCanBusQueue = CBUF_INIT(txCanBusBuffer, true);
    if (rxCanBusQueue == NULL || txCanBusQueue == NULL)
        return false;
    GPIO_InitTypeDef GPIO_InitStruct;
    CAN_BUS_CLK_ENA();
    CAN_BUS_PORT_CLK_ENA();
    GPIO_InitStruct.Pin       = CAN_BUS_RX_Pin;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF4_CAN;
    HAL_GPIO_Init(CAN_BUS_RX_GPIO_Port, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = CAN_BUS_TX_Pin;
    HAL_GPIO_Init(CAN_BUS_TX_GPIO_Port, &GPIO_InitStruct);

    PrjCanBus.filter.FilterIdHigh         = 0;
    PrjCanBus.filter.FilterIdLow          = 0;
    PrjCanBus.filter.FilterMaskIdHigh     = 0;
    PrjCanBus.filter.FilterMaskIdLow      = 0;
    PrjCanBus.filter.FilterFIFOAssignment = CAN_RX_FIFO0;
    PrjCanBus.filter.FilterBank           = 0;
    PrjCanBus.filter.FilterMode           = CAN_FILTERMODE_IDMASK;
    PrjCanBus.filter.FilterScale          = CAN_FILTERSCALE_32BIT;
    PrjCanBus.filter.FilterActivation     = ENABLE;

    PrjCanBus.prescaler                   = 48;
    PrjCanBus.auto_retransmit             = ENABLE;
    PrjCanBus.handle.Instance             = CAN;
    PrjCanBus.bus_state                   = OFF_BUS;
    PrjCanBus.silentMode                  = CAN_MODE_NORMAL;

    HAL_NVIC_SetPriority(CEC_CAN_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(CEC_CAN_IRQn);
    return true;
}

bool prj_can_bus_enable(void) {
    if (PrjCanBus.bus_state == ON_BUS)
        return true;

    PrjCanBus.handle.Init.Prescaler            = PrjCanBus.prescaler;
    PrjCanBus.handle.Init.Mode                 = PrjCanBus.silentMode;
    PrjCanBus.handle.Init.SyncJumpWidth        = CAN_SJW_1TQ;
    PrjCanBus.handle.Init.TimeSeg1             = CAN_BS1_4TQ;
    PrjCanBus.handle.Init.TimeSeg2             = CAN_BS2_3TQ;
    PrjCanBus.handle.Init.TimeTriggeredMode    = DISABLE;
    PrjCanBus.handle.Init.AutoBusOff           = ENABLE;
    PrjCanBus.handle.Init.AutoWakeUp           = DISABLE;
    PrjCanBus.handle.Init.AutoRetransmission   = PrjCanBus.auto_retransmit;
    PrjCanBus.handle.Init.ReceiveFifoLocked    = DISABLE;
    PrjCanBus.handle.Init.TransmitFifoPriority = ENABLE;

    if (HAL_CAN_Init(&PrjCanBus.handle) != HAL_OK ||
        HAL_CAN_ConfigFilter(&PrjCanBus.handle, &PrjCanBus.filter) != HAL_OK ||
        HAL_CAN_Start(&PrjCanBus.handle) != HAL_OK ||
        HAL_CAN_ActivateNotification(&PrjCanBus.handle, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK) {
        error_assert(ERR_PERIPHINIT);
        PrjCanBus.bus_state = OFF_BUS;
        discrete_output_reset(&LedBlue);
        discrete_output_set(&LedBlue, false);
        return false;
    }

    PrjCanBus.bus_state = ON_BUS;
    discrete_output_reset(&LedBlue);
    discrete_output_set(&LedBlue, true);
    return true;
}

void prj_can_bus_disable(void) {
    if (PrjCanBus.bus_state == ON_BUS) {
        // Do a bxCAN reset (set RESET bit to 1)
        PrjCanBus.handle.Instance->MCR |= CAN_MCR_RESET;
        PrjCanBus.bus_state = OFF_BUS;
    }
    cb_clear(txCanBusQueue);
    discrete_output_reset(&LedBlue);
    discrete_output_set(&LedBlue, false);
}

void prj_can_bus_run(void) {
    if (!cb_is_empty(rxCanBusQueue)) {
        if (!cb_pop(rxCanBusQueue, &rxCanBusTmpBuf)) {
            debug_printf("ERROR: can queue empty\n");
            return;
        }
        size_t cdc_msg_len = slcan_parse_frame((uint8_t*) &cdc_message_buf, &rxCanBusTmpBuf);
        if (cdc_msg_len)
            prj_usb_cdc_send(cdc_message_buf, cdc_msg_len);
    }
    if (PrjCanBus.bus_state != ON_BUS)
        return;
    if (cb_is_empty(txCanBusQueue) || HAL_CAN_GetTxMailboxesFreeLevel(&PrjCanBus.handle) < 1)
        return;
    if (!cb_pop(txCanBusQueue, &txCanBusTmpBuf)) {
        debug_printf("ERROR: cannot pop outgoing CANBUS frame from queue!\n");
        return;
    }
    uint32_t mailbox_txed = 0;
    uint32_t status =
            HAL_CAN_AddTxMessage(&PrjCanBus.handle, &txCanBusTmpBuf.Header, txCanBusTmpBuf.Body, &mailbox_txed);
    discrete_output_reset(&LedRed);
    discrete_output_meander_start(&LedRed, 100, 0, 1);
    if (status != HAL_OK) {
        error_assert(ERR_CAN_TXFAIL);
    }
}

uint32_t prj_can_bus_send(const txCanBusFrame_t* frame) {
    if (PrjCanBus.bus_state != ON_BUS) {
        debug_printf("ERROR: cannot queue CANBUS TX frame while bus is off\n");
        return HAL_ERROR;
    }
    if (cb_is_full(txCanBusQueue)) {
        debug_printf("ERROR: CANBUS TX queue is full!\n");
        return HAL_ERROR;
    }
    if (!cb_push(txCanBusQueue, (txCanBusFrame_t*) frame)) {
        debug_printf("ERROR: Cannot push CANBUS TX frame into queue!\n");
        return HAL_ERROR;
    }
    return HAL_OK;
}

void prj_can_bus_set_auto_retransmit(const bool enable) {
    if (PrjCanBus.bus_state == ON_BUS) {
        debug_printf("ERROR: cannot set auto-retransmit while bus is on\n");
        return;
    }
    PrjCanBus.auto_retransmit = enable ? ENABLE : DISABLE;
}


CAN_HandleTypeDef* prj_can_bus_get_handle(void) { return &PrjCanBus.handle; }

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef* hcan) {
    rxCanBusFrame_t frame;
    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &frame.Header, frame.Body) == HAL_OK) {
        if (!cb_push(rxCanBusQueue, &frame))
            debug_printf("ERROR: Cannot push CAN RX frame into queue!\n");
    }
}
