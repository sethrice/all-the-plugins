#include "seader_i.h"

#define TAG "SeaderCCID"

bool hasSAM = false;
const uint8_t SAM_ATR[] =
    {0x3b, 0x95, 0x96, 0x80, 0xb1, 0xfe, 0x55, 0x1f, 0xc7, 0x47, 0x72, 0x61, 0x63, 0x65, 0x13};
const uint8_t SAM_ATR2[] = {0x3b, 0x90, 0x96, 0x91, 0x81, 0xb1, 0xfe, 0x55, 0x1f, 0xc7, 0xd4};

bool powered[2] = {false, false};
uint8_t sam_slot = 0;
uint8_t sequence[2] = {0, 0};
uint8_t retries = 3;

uint8_t getSequence(uint8_t slot) {
    if(sequence[slot] > 254) {
        sequence[slot] = 0;
    }
    return sequence[slot]++;
}

void seader_ccid_IccPowerOn(SeaderUartBridge* seader_uart, uint8_t slot) {
    if(powered[slot]) {
        return;
    }
    powered[slot] = true;

    FURI_LOG_D(TAG, "Sending Power On (%d)", slot);
    memset(seader_uart->tx_buf, 0, SEADER_UART_RX_BUF_SIZE);
    seader_uart->tx_buf[0] = SYNC;
    seader_uart->tx_buf[1] = CTRL;
    seader_uart->tx_buf[2 + 0] = CCID_MESSAGE_TYPE_PC_to_RDR_IccPowerOn;

    seader_uart->tx_buf[2 + 5] = slot;
    seader_uart->tx_buf[2 + 6] = getSequence(slot);
    seader_uart->tx_buf[2 + 7] = 2; //power

    seader_uart->tx_len = seader_add_lrc(seader_uart->tx_buf, 2 + 10);
    furi_thread_flags_set(furi_thread_get_id(seader_uart->tx_thread), WorkerEvtSamRx);
}

void seader_ccid_check_for_sam(SeaderUartBridge* seader_uart) {
    hasSAM = false; // If someone is calling this, reset sam state
    powered[0] = false;
    powered[1] = false;
    retries = 3;
    seader_ccid_GetSlotStatus(seader_uart, 0);
}

void seader_ccid_GetSlotStatus(SeaderUartBridge* seader_uart, uint8_t slot) {
    FURI_LOG_D(TAG, "seader_ccid_GetSlotStatus(%d)", slot);
    memset(seader_uart->tx_buf, 0, SEADER_UART_RX_BUF_SIZE);
    seader_uart->tx_buf[0] = SYNC;
    seader_uart->tx_buf[1] = CTRL;
    seader_uart->tx_buf[2 + 0] = CCID_MESSAGE_TYPE_PC_to_RDR_GetSlotStatus;
    seader_uart->tx_buf[2 + 5] = slot;
    seader_uart->tx_buf[2 + 6] = getSequence(slot);

    seader_uart->tx_len = seader_add_lrc(seader_uart->tx_buf, 2 + 10);
    furi_thread_flags_set(furi_thread_get_id(seader_uart->tx_thread), WorkerEvtSamRx);
}

void seader_ccid_SetParameters(Seader* seader, uint8_t slot, uint8_t* atr, size_t atr_len) {
    SeaderWorker* seader_worker = seader->worker;
    SeaderUartBridge* seader_uart = seader_worker->uart;
    UNUSED(atr_len);
    FURI_LOG_D(TAG, "seader_ccid_SetParameters(%d)", slot);

    uint8_t payloadLen = 0;
    if(seader_uart->T == 0) {
        payloadLen = 5;
    } else if(atr[4] == 0xB1 && seader_uart->T == 1) {
        payloadLen = 7;
    }
    memset(seader_uart->tx_buf, 0, SEADER_UART_RX_BUF_SIZE);
    seader_uart->tx_buf[0] = SYNC;
    seader_uart->tx_buf[1] = CTRL;
    seader_uart->tx_buf[2 + 0] = CCID_MESSAGE_TYPE_PC_to_RDR_SetParameters;
    seader_uart->tx_buf[2 + 1] = payloadLen;
    seader_uart->tx_buf[2 + 5] = slot;
    seader_uart->tx_buf[2 + 6] = getSequence(slot);
    seader_uart->tx_buf[2 + 7] = seader_uart->T;
    seader_uart->tx_buf[2 + 8] = 0;
    seader_uart->tx_buf[2 + 9] = 0;

    if(seader_uart->T == 0) {
        // I'm leaving this here for completeness, but it was actually causing ICC_MUTE on the first apdu.
        seader_uart->tx_buf[2 + 10] = 0x96; //atr[2]; //bmFindexDindex
        seader_uart->tx_buf[2 + 11] = 0x00; //bmTCCKST1
        seader_uart->tx_buf[2 + 12] = 0x00; //bGuardTimeT0
        seader_uart->tx_buf[2 + 13] = 0x0a; //bWaitingIntegerT0
        seader_uart->tx_buf[2 + 14] = 0x00; //bClockStop
    } else if(seader_uart->T == 1) {
        seader_uart->tx_buf[2 + 10] = atr[2]; //bmFindexDindex
        seader_uart->tx_buf[2 + 11] = 0x10; //bmTCCKST1
        seader_uart->tx_buf[2 + 12] = 0xfe; //bGuardTimeT1
        seader_uart->tx_buf[2 + 13] = atr[6]; //bWaitingIntegerT1
        seader_uart->tx_buf[2 + 14] = atr[8]; //bClockStop
        seader_uart->tx_buf[2 + 15] = atr[5]; //bIFSC
        seader_uart->tx_buf[2 + 16] = 0x00; //bNadValue
    }

    seader_uart->tx_len = seader_add_lrc(seader_uart->tx_buf, 2 + 10 + payloadLen);
    furi_thread_flags_set(furi_thread_get_id(seader_uart->tx_thread), WorkerEvtSamRx);
}

void seader_ccid_GetParameters(SeaderUartBridge* seader_uart) {
    memset(seader_uart->tx_buf, 0, SEADER_UART_RX_BUF_SIZE);
    seader_uart->tx_buf[0] = SYNC;
    seader_uart->tx_buf[1] = CTRL;
    seader_uart->tx_buf[2 + 0] = CCID_MESSAGE_TYPE_PC_to_RDR_GetParameters;
    seader_uart->tx_buf[2 + 1] = 0;
    seader_uart->tx_buf[2 + 5] = sam_slot;
    seader_uart->tx_buf[2 + 6] = getSequence(sam_slot);
    seader_uart->tx_buf[2 + 7] = 0;
    seader_uart->tx_buf[2 + 8] = 0;
    seader_uart->tx_buf[2 + 9] = 0;

    seader_uart->tx_len = seader_add_lrc(seader_uart->tx_buf, 2 + 10);

    furi_thread_flags_set(furi_thread_get_id(seader_uart->tx_thread), WorkerEvtSamRx);
}

void seader_ccid_XfrBlock(SeaderUartBridge* seader_uart, uint8_t* data, size_t len) {
    seader_ccid_XfrBlockToSlot(seader_uart, sam_slot, data, len);
}

void seader_ccid_XfrBlockToSlot(
    SeaderUartBridge* seader_uart,
    uint8_t slot,
    uint8_t* data,
    size_t len) {
    memset(seader_uart->tx_buf, 0, SEADER_UART_RX_BUF_SIZE);
    seader_uart->tx_buf[0] = SYNC;
    seader_uart->tx_buf[1] = CTRL;
    seader_uart->tx_buf[2 + 0] = CCID_MESSAGE_TYPE_PC_to_RDR_XfrBlock;
    seader_uart->tx_buf[2 + 1] = (len >> 0) & 0xff;
    seader_uart->tx_buf[2 + 2] = (len >> 8) & 0xff;
    seader_uart->tx_buf[2 + 5] = slot;
    seader_uart->tx_buf[2 + 6] = getSequence(slot);
    seader_uart->tx_buf[2 + 7] = 5;
    seader_uart->tx_buf[2 + 8] = 0;
    seader_uart->tx_buf[2 + 9] = 0;

    uint8_t header_len = 2 + 10;
    memcpy(seader_uart->tx_buf + header_len, data, len);
    seader_uart->tx_len = header_len + len;
    seader_uart->tx_len = seader_add_lrc(seader_uart->tx_buf, seader_uart->tx_len);

    char display[SEADER_UART_RX_BUF_SIZE * 2 + 1] = {0};
    for(uint8_t i = 0; i < seader_uart->tx_len; i++) {
        snprintf(display + (i * 2), sizeof(display), "%02x", seader_uart->tx_buf[i]);
    }
    FURI_LOG_D(TAG, "seader_ccid_XfrBlockToSlot(%d) %d: %s", slot, seader_uart->tx_len, display);

    furi_thread_flags_set(furi_thread_get_id(seader_uart->tx_thread), WorkerEvtSamRx);
}

size_t seader_ccid_process(Seader* seader, uint8_t* cmd, size_t cmd_len) {
    SeaderWorker* seader_worker = seader->worker;
    SeaderUartBridge* seader_uart = seader_worker->uart;
    CCID_Message message;
    message.consumed = 0;

    char display[SEADER_UART_RX_BUF_SIZE * 2 + 1] = {0};
    for(uint8_t i = 0; i < cmd_len; i++) {
        snprintf(display + (i * 2), sizeof(display), "%02x", cmd[i]);
    }
    FURI_LOG_D(TAG, "seader_ccid_process %d: %s", cmd_len, display);

    if(cmd_len == 2) {
        if(cmd[0] == CCID_MESSAGE_TYPE_RDR_to_PC_NotifySlotChange) {
            switch(cmd[1] & SLOT_0_MASK) {
            case 0:
            case 1:
                // No change, no-op
                break;
            case CARD_IN_1:
                FURI_LOG_D(TAG, "Card Inserted (0)");
                if(hasSAM && sam_slot == 0) {
                    break;
                }
                sequence[0] = 0;
                seader_ccid_IccPowerOn(seader_uart, 0);
                break;
            case CARD_OUT_1:
                FURI_LOG_D(TAG, "Card Removed (0)");
                if(hasSAM && sam_slot == 0) {
                    powered[0] = false;
                    hasSAM = false;
                    retries = 3;
                    if(seader_worker->callback) {
                        seader_worker->callback(
                            SeaderWorkerEventSamMissing, seader_worker->context);
                    }
                }
                break;
            };

            switch(cmd[1] & SLOT_1_MASK) {
            case 0:
            case 1:
                // No change, no-op
                break;
            case CARD_IN_2:
                FURI_LOG_D(TAG, "Card Inserted (1)");
                if(hasSAM && sam_slot == 1) {
                    break;
                }
                sequence[1] = 0;
                seader_ccid_IccPowerOn(seader_uart, 1);
                break;
            case CARD_OUT_2:
                FURI_LOG_D(TAG, "Card Removed (1)");
                if(hasSAM && sam_slot == 1) {
                    powered[1] = false;
                    hasSAM = false;
                    retries = 3;
                    if(seader_worker->callback) {
                        seader_worker->callback(
                            SeaderWorkerEventSamMissing, seader_worker->context);
                    }
                }
                break;
            };

            return 2;
        }
    }

    while(cmd_len >= 3 && cmd[0] == SYNC && cmd[1] == NAK) {
        // 031516
        FURI_LOG_W(TAG, "NAK");
        cmd += 3;
        cmd_len -= 3;
        message.consumed += 3;
    }

    while(cmd_len > 2 && (cmd[0] != SYNC || cmd[1] != CTRL)) {
        FURI_LOG_W(TAG, "invalid start: %02x", cmd[0]);
        cmd += 1;
        cmd_len -= 1;
        message.consumed += 1;
    }

    if(cmd_len > 12 && cmd[0] == SYNC && cmd[1] == CTRL) {
        uint8_t* ccid = cmd + 2;
        message.bMessageType = ccid[0];
        message.dwLength = *((uint32_t*)(ccid + 1));
        message.bSlot = ccid[5];
        message.bSeq = ccid[6];
        message.bStatus = ccid[7];
        message.bError = ccid[8];
        message.payload = ccid + 10;

        memset(display, 0, sizeof(display));
        for(uint8_t i = 0; i < message.dwLength; i++) {
            snprintf(display + (i * 2), sizeof(display), "%02x", message.payload[i]);
        }

        if(cmd_len < 2 + 10 + message.dwLength + 1) {
            // Incomplete
            return message.consumed;
        }
        message.consumed += 2 + 10 + message.dwLength + 1;

        if(seader_validate_lrc(cmd, 2 + 10 + message.dwLength + 1) == false) {
            FURI_LOG_W(
                TAG,
                "Invalid LRC.  Recv: %02x vs Calc: %02x",
                cmd[2 + 10 + message.dwLength + 1],
                seader_calc_lrc(cmd, 2 + 10 + message.dwLength));
            // TODO: Should I respond with an error?
            return message.consumed;
        }

        /*
        if(message.dwLength == 0) {
            FURI_LOG_D(
                TAG,
                "CCID [%d|%d] type: %02x, status: %02x, error: %02x",
                message.bSlot,
                message.bSeq,
                message.bMessageType,
                message.bStatus,
                message.bError);
        } else {
            FURI_LOG_D(
                TAG,
                "CCID [%d|%d] %ld: %s",
                message.bSlot,
                message.bSeq,
                message.dwLength,
                display);
        }
        */

        //0306 81 00000000 0000 0200 01 87
        //0306 81 00000000 0000 0100 01 84
        if(message.bMessageType == CCID_MESSAGE_TYPE_RDR_to_PC_SlotStatus) {
            uint8_t status = (message.bStatus & BMICCSTATUS_MASK);
            if(status == 0 || status == 1) {
                seader_ccid_IccPowerOn(seader_uart, message.bSlot);
                return message.consumed;
            } else if(status == 2) {
                FURI_LOG_W(TAG, "No ICC is present [retries %d]", retries);
                if(retries-- > 1 && hasSAM == false) {
                    furi_delay_ms(100);
                    seader_ccid_GetSlotStatus(seader_uart, retries % 2);
                } else {
                    if(seader_worker->callback) {
                        seader_worker->callback(
                            SeaderWorkerEventSamMissing, seader_worker->context);
                    }
                }
                return message.consumed;
            }
        }

        //0306 80 00000000 0001 42fe 00 38
        if(message.bStatus == 0x41 && message.bError == 0xfe) {
            FURI_LOG_W(TAG, "card probably upside down");
            hasSAM = false;
            if(seader_worker->callback) {
                seader_worker->callback(SeaderWorkerEventSamMissing, seader_worker->context);
            }
            return message.consumed;
        }
        if(message.bStatus == 0x42 && message.bError == 0xfe) {
            FURI_LOG_W(TAG, "No card");
            if(seader_worker->callback) {
                seader_worker->callback(SeaderWorkerEventSamMissing, seader_worker->context);
            }
            return message.consumed;
        }
        if(message.bError != 0) {
            FURI_LOG_W(TAG, "CCID error %02x", message.bError);
            message.consumed = cmd_len;
            if(seader_worker->callback) {
                seader_worker->callback(SeaderWorkerEventSamMissing, seader_worker->context);
            }
            return message.consumed;
        }

        if(message.bMessageType == CCID_MESSAGE_TYPE_RDR_to_PC_Parameters) {
            FURI_LOG_D(TAG, "Got Parameters");
            if(seader_uart->T == 1) {
                seader_t_1_set_IFSD(seader);
            } else {
                seader_worker_send_version(seader);
                if(seader_worker->callback) {
                    seader_worker->callback(SeaderWorkerEventSamPresent, seader_worker->context);
                }
            }
        } else if(message.bMessageType == CCID_MESSAGE_TYPE_RDR_to_PC_DataBlock) {
            if(hasSAM) {
                if(message.bSlot == sam_slot) {
                    if(seader_uart->T == 0) {
                        seader_worker_process_sam_message(
                            seader, message.payload, message.dwLength);
                    } else if(seader_uart->T == 1) {
                        seader_recv_t1(seader, &message);
                    }
                } else {
                    FURI_LOG_D(TAG, "Discarding message on non-sam slot");
                }
            } else {
                if(memcmp(SAM_ATR, message.payload, sizeof(SAM_ATR)) == 0) {
                    FURI_LOG_I(TAG, "SAM ATR!");
                    hasSAM = true;
                    sam_slot = message.bSlot;
                    if(seader_uart->T == 0) {
                        seader_ccid_GetParameters(seader_uart);
                    } else if(seader_uart->T == 1) {
                        seader_ccid_SetParameters(
                            seader, sam_slot, message.payload, message.dwLength);
                    }
                } else if(memcmp(SAM_ATR2, message.payload, sizeof(SAM_ATR2)) == 0) {
                    FURI_LOG_I(TAG, "SAM ATR2!");
                    hasSAM = true;
                    sam_slot = message.bSlot;
                    // I don't have an ATR2 to test with
                    seader_ccid_GetParameters(seader_uart);
                } else {
                    FURI_LOG_W(TAG, "Unknown ATR");
                    if(seader_worker->callback) {
                        seader_worker->callback(SeaderWorkerEventSamWrong, seader_worker->context);
                    }
                }
            }
        } else {
            FURI_LOG_W(TAG, "Unhandled CCID message type %02x", message.bMessageType);
        }
    }

    return message.consumed;
}
