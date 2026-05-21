#include "slcan.h"
#include <string.h>
#include "prj_can.h"
#include "prj_cdc_usb.h"
#include "stm32f0xx_hal.h"
#include "utils_conf.h"

static bool slcan_hex_to_u4(uint8_t c, uint8_t* out) {
    if (c >= '0' && c <= '9') {
        *out = c - '0';
        return true;
    }
    if (c >= 'A' && c <= 'F') {
        *out = c - 'A' + 10;
        return true;
    }
    if (c >= 'a' && c <= 'f') {
        *out = c - 'a' + 10;
        return true;
    }
    return false;
}

// Parse an incoming CAN frame into an outgoing slcan message
size_t slcan_parse_frame(uint8_t* buf, const rxCanBusFrame_t* frame) {
    size_t msg_position = 0;

    for (uint8_t j = 0; j < SLCAN_MTU; j++) {
        buf[j] = '\0';
    }

    // Add character for frame type
    if (frame->Header.RTR == CAN_RTR_DATA) {
        buf[msg_position] = 't';
    } else if (frame->Header.RTR == CAN_RTR_REMOTE) {
        buf[msg_position] = 'r';
    }

    // Assume standard identifier
    uint8_t id_len  = SLCAN_STD_ID_LEN;
    uint32_t can_id = frame->Header.StdId;

    // Check if extended
    if (frame->Header.IDE == CAN_ID_EXT) {
        // Convert first char to upper case for extended frame
        buf[msg_position] -= 32;
        id_len = SLCAN_EXT_ID_LEN;
        can_id = frame->Header.ExtId;
    }
    msg_position++;

    // Add identifier to buffer
    for (uint8_t j = id_len; j > 0; j--) {
        // Add nybble to buffer
        buf[j] = (can_id & 0xF);
        can_id = can_id >> 4;
        msg_position++;
    }

    // Add DLC to buffer
    buf[msg_position++] = frame->Header.DLC;

    if (frame->Header.RTR == CAN_RTR_DATA) {
        for (size_t j = 0; j < frame->Header.DLC; j++) {
            buf[msg_position++] = (frame->Body[j] >> 4);
            buf[msg_position++] = (frame->Body[j] & 0x0F);
        }
    }

    // Convert to ASCII (2nd character to end)
    for (size_t j = 1; j < msg_position; j++) {
        if (buf[j] < 0xA) {
            buf[j] += 0x30;
        } else {
            buf[j] += 0x37;
        }
    }

    // Add CR (slcan EOL)
    buf[msg_position++] = '\r';

    // Return number of bytes in string
    return msg_position;
}

int8_t slcan_parse_str(uint8_t* buf, uint8_t len) {
    if (buf == NULL || len == 0)
        return -1;
    txCanBusFrame_t Frame = {0};
    Frame.Header.IDE      = CAN_ID_STD;
    Frame.Header.RTR      = CAN_RTR_DATA;
    switch (buf[0]) {
        case 'O':
            if (len != 1)
                return -1;
            debug_printf("OPEN command\n");
            return prj_can_bus_enable() ? 0 : -1;
        case 'C':
            if (len != 1)
                return -1;
            debug_printf("CLOSE command\n");
            prj_can_bus_disable();
            return 0;
        case 'S': {
            uint8_t bitrate;
            if (len != 2 || !slcan_hex_to_u4(buf[1], &bitrate))
                return -1;
            debug_printf("SET BITRATE command, value=%d\n", bitrate);
            if (bitrate >= CAN_BITRATE_INVALID)
                return -1;
            return prj_can_bus_set_bitrate((enum can_bitrate) bitrate) ? 0 : -1;
        }
        case 'm':
        case 'M': {
            uint8_t mode;
            if (len != 2 || !slcan_hex_to_u4(buf[1], &mode) || mode > 1)
                return -1;
            return prj_can_bus_set_silent(mode == 1) ? 0 : -1;
        }
        case 'a':
        case 'A': {
            uint8_t enable;
            if (len != 2 || !slcan_hex_to_u4(buf[1], &enable) || enable > 1)
                return -1;
            prj_can_bus_set_auto_retransmit(enable == 1);
            return 0;
        }
        case 'V': {
            if (len != 1)
                return -1;
            // Report firmware version and remote
            // char* fw_id = GIT_VERSION " " GIT_REMOTE "\r"; // TODO
            // CDC_Transmit_FS((uint8_t*)fw_id, strlen(fw_id));
            return 0;
        }
        // Nonstandard!
        case 'E': {
            if (len != 1)
                return -1;
            char errstr[64] = {0};
            prj_usb_cdc_send((uint8_t*) errstr, strlen(errstr));
            return 0;
        }
        case 'T':
            Frame.Header.IDE = CAN_ID_EXT;
            break;
        case 't':
            break;
        case 'R':
            Frame.Header.IDE = CAN_ID_EXT;
            Frame.Header.RTR = CAN_RTR_REMOTE;
            break;
        case 'r':
            Frame.Header.RTR = CAN_RTR_REMOTE;
            break;
        default:
            debug_printf("ERROR: unknown command=%d\n", buf[0]);
            return -1;
    }
    const uint8_t id_len = (Frame.Header.IDE == CAN_ID_EXT) ? SLCAN_EXT_ID_LEN : SLCAN_STD_ID_LEN;
    if (len < (uint8_t) (1 + id_len + 1))
        return -1;
    uint8_t msg_position = 1;
    if (Frame.Header.IDE == CAN_ID_EXT) {
        while (msg_position <= SLCAN_EXT_ID_LEN) {
            uint8_t nibble;
            if (!slcan_hex_to_u4(buf[msg_position], &nibble))
                return -1;
            Frame.Header.ExtId *= 16;
            Frame.Header.ExtId += nibble;
            msg_position++;
        }
    } else {
        while (msg_position <= SLCAN_STD_ID_LEN) {
            uint8_t nibble;
            if (!slcan_hex_to_u4(buf[msg_position], &nibble))
                return -1;
            Frame.Header.StdId *= 16;
            Frame.Header.StdId += nibble;
            msg_position++;
        }
    }
    uint8_t dlc;
    if (!slcan_hex_to_u4(buf[msg_position++], &dlc) || dlc > 8)
        return -1;
    Frame.Header.DLC           = dlc;
    const bool remote          = Frame.Header.RTR == CAN_RTR_REMOTE;
    const uint8_t expected_len = 1 + id_len + 1 + (remote ? 0 : dlc * 2);
    if (len != expected_len)
        return -1;
    if (!remote) {
        for (uint32_t j = 0; j < Frame.Header.DLC; j++) {
            uint8_t hi;
            uint8_t lo;
            if (!slcan_hex_to_u4(buf[msg_position], &hi) || !slcan_hex_to_u4(buf[msg_position + 1], &lo))
                return -1;
            Frame.Body[j] = (hi << 4) | lo;
            msg_position += 2;
        }
    }
    return prj_can_bus_send(&Frame) == HAL_OK ? 0 : -1;
}
