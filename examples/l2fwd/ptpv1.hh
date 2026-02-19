#pragma once

#include <rte_common.h>

#define PTP_SUBDOMAIN_NAME_LENGTH 16

struct __rte_packed ptpv1_header {
        uint16_t versionPTP;
        uint16_t versionNetwork;
        uint8_t subdomain[PTP_SUBDOMAIN_NAME_LENGTH];
        uint8_t messageType;
        uint8_t sourceCommunicationTechnology;
        uint8_t sourceUuid[6];
        uint16_t sourcePortId;
        uint16_t sequenceId;
        uint8_t control;
        uint8_t flags[2];
        uint8_t reserved[4];
};

enum ptpv1_control_field {
    PTP_SYNC_MESSAGE,
    PTP_DELAY_REQ_MESSAGE,
    PTP_FOLLOWUP_MESSAGE,
    PTP_DELAY_RESP_MESSAGE,
    PTP_MANAGEMENT_MESSAGE
};

#define PTP_SOURCE_UUID_PRT_FMT "%02X:%02X:%02X:%02X:%02X:%02X"

#define PTP_SOURCE_UUID_BYTES(src_uuid) \
    (src_uuid[0]), (src_uuid[1]), (src_uuid[2]), (src_uuid[3]), (src_uuid[4]), (src_uuid[5])

#define PTP_CONTROL_FIELD_STRING(ctrl)               \
    (ctrl == PTP_SYNC_MESSAGE         ? "Sync"       \
     : ctrl == PTP_DELAY_REQ_MESSAGE  ? "Delay_Req"  \
     : ctrl == PTP_FOLLOWUP_MESSAGE   ? "Follow_Up"  \
     : ctrl == PTP_DELAY_RESP_MESSAGE ? "Delay_Resp" \
     : ctrl == PTP_MANAGEMENT_MESSAGE ? "Management" \
                                      : "Reserved")
