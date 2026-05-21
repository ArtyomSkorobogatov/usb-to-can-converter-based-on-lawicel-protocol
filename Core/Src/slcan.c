//
// slcan: Parse incoming and generate outgoing slcan messages
//

#include "slcan.h"
#include <string.h>
#include "prj_can.h"
#include "prj_cdc_usb.h"
#include "stm32f0xx_hal.h"
#include "utils_conf.h"

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

    // Add data bytes
    for (uint8_t j = 0; j < frame->Header.DLC; j++) {
        buf[msg_position++] = (frame->Body[j] >> 4);
        buf[msg_position++] = (frame->Body[j] & 0x0F);
    }

    // Convert to ASCII (2nd character to end)
    for (uint8_t j = 1; j < msg_position; j++) {
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


// Parse an incoming slcan command from the USB CDC port
int8_t slcan_parse_str(uint8_t* buf, uint8_t len) {
    // CAN_TxHeaderTypeDef frame_header;
    txCanBusFrame_t Frame = {0};
    // Default to standard ID unless otherwise specified
    Frame.Header.IDE   = CAN_ID_STD;
    Frame.Header.StdId = 0;
    Frame.Header.ExtId = 0;


    // Convert from ASCII (2nd character to end)
    for (uint8_t i = 1; i < len; i++) {
        // Lowercase letters
        if (buf[i] >= 'a')
            buf[i] = buf[i] - 'a' + 10;
        // Uppercase letters
        else if (buf[i] >= 'A')
            buf[i] = buf[i] - 'A' + 10;
        // Numbers
        else
            buf[i] = buf[i] - '0';
    }

    switch (buf[0]) {
        case 'O':
            debug_printf("OPEN command\n");
            prj_can_bus_enable();
            return 0;
        case 'C':
            debug_printf("CLOSE command\n");
            prj_can_bus_disable();
            return 0;
        case 'S':
            debug_printf("SET BITRATE command, value=%d\n", buf[1]);
            if (buf[1] >= CAN_BITRATE_INVALID) {
                return -1;
            }
            prj_can_bus_set_bitrate(buf[1]);
            return 0;
        case 'm':
        case 'M':
            // Set mode command
            if (buf[1] == 1) {
                // Mode 1: silent
                prj_can_bus_set_silent(1);
            } else {
                // Default to normal mode
                prj_can_bus_set_silent(0);
            }
            return 0;
        case 'a':
        case 'A':
            if (buf[1] == 1) {
                prj_can_bus_set_auto_retransmit(true);
            } else {
                prj_can_bus_set_auto_retransmit(false);
            }
            return 0;
        case 'V': {
            // Report firmware version and remote
            // char* fw_id = GIT_VERSION " " GIT_REMOTE "\r"; // TODO
            // CDC_Transmit_FS((uint8_t*)fw_id, strlen(fw_id));
            return 0;
        }
        // Nonstandard!
        case 'E': {
            // Report error register
            char errstr[64] = {0};
            prj_usb_cdc_send((uint8_t*) errstr, strlen(errstr));
            return 0;
        }
        case 'T':
            Frame.Header.IDE = CAN_ID_EXT;
            break;
        case 't':
            Frame.Header.RTR = CAN_RTR_DATA;
            break;
        case 'R':
            Frame.Header.IDE = CAN_ID_EXT;
            break;
        case 'r':
            // Transmit remote frame command
            Frame.Header.RTR = CAN_RTR_REMOTE;
            break;
        default:
            debug_printf("ERROR: unknown command=%d\n", buf[0]);
            return -1;
    }


    // Save CAN ID depending on ID type
    uint8_t msg_position = 1;
    if (Frame.Header.IDE == CAN_ID_EXT) {
        while (msg_position <= SLCAN_EXT_ID_LEN) {
            Frame.Header.ExtId *= 16;
            Frame.Header.ExtId += buf[msg_position++];
        }
    } else {
        while (msg_position <= SLCAN_STD_ID_LEN) {
            Frame.Header.StdId *= 16;
            Frame.Header.StdId += buf[msg_position++];
        }
    }


    // Attempt to parse DLC and check sanity
    Frame.Header.DLC = buf[msg_position++];
    if (Frame.Header.DLC > 8) {
        return -1;
    }

    // Copy frame data to buffer
    for (uint8_t j = 0; j < Frame.Header.DLC; j++) {
        Frame.Body[j] = (buf[msg_position] << 4) + buf[msg_position + 1];
        msg_position += 2;
    }
    prj_can_bus_send(&Frame);
    return 0;
}
