#ifndef PRJ_CAN_H
#define PRJ_CAN_H

#include "main.h"

enum can_bitrate {
    CAN_BITRATE_10K = 0,
    CAN_BITRATE_20K,
    CAN_BITRATE_50K,
    CAN_BITRATE_100K,
    CAN_BITRATE_125K,
    CAN_BITRATE_250K,
    CAN_BITRATE_500K,
    CAN_BITRATE_750K,
    CAN_BITRATE_1000K,

	CAN_BITRATE_INVALID,
};

typedef enum can_bus_state {
    OFF_BUS = 0,
    ON_BUS = 1,
} can_bus_state_t;


typedef struct rxCanBusFrame_ {
	CAN_RxHeaderTypeDef Header;
	uint8_t Body[8];
} rxCanBusFrame_t;

typedef struct txCanBusFrame_ {
	CAN_TxHeaderTypeDef Header;
	uint8_t Body[8];
} txCanBusFrame_t;

bool prj_can_bus_init(void);
void prj_can_bus_run(void);
bool prj_can_bus_enable(void);
void prj_can_bus_disable(void);
CAN_HandleTypeDef* prj_can_bus_get_handle(void);
void prj_can_bus_set_auto_retransmit(bool enable);
bool prj_can_bus_set_silent(bool enable);
uint32_t prj_can_bus_send(const txCanBusFrame_t* frame);
bool prj_can_bus_set_bitrate(enum can_bitrate bitrate);

#endif // PRJ_CAN_H
