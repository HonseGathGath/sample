/*******************************************************************************
** File: sample_app.h
**
** Purpose:
**   This is the main header file for the SAMPLE application.
**
*******************************************************************************/
#ifndef _SAMPLE_APP_H_
#define _SAMPLE_APP_H_

/*
** Include Files
*/
#include "cfe.h"
#include "sample_device.h"
#include "sample_events.h"
#include "sample_platform_cfg.h"
#include "sample_perfids.h"
#include "sample_msg.h"
#include "sample_msgids.h"
#include "sample_version.h"
#include "hwlib.h"

/* TODO: This is specific to the sample application, remove if using template generator */
#include "mgr_msg.h"
#include "mgr_msgids.h"

/* Internal-Bus IDS project - B1 attack scenario (illegitimate subscription).
 * SAMPLE has no functional reason to read EPS telemetry; this simulates a
 * supply-chain-compromised COTS component quietly reading power-system
 * data it was never designed to see. Also pull in a couple of other HK
 * MIDs that are equally "none of SAMPLE's business" so b1_extra_mids can
 * offer more than one unrelated-MID option (see PHASE1_NOTES.md). */
#include "generic_eps_msgids.h"
#include "generic_adcs_msgids.h"
#include "generic_imu_msgids.h"

/*
** Specified pipe depth - how many messages will be queued in the pipe
*/
#define SAMPLE_PIPE_DEPTH 32

/* B2 payload-mode selector values (sample_params key b2_payload_mode) */
#define SAMPLE_B2_PAYLOAD_COUNTER    0
#define SAMPLE_B2_PAYLOAD_CONSTANT   1
#define SAMPLE_B2_PAYLOAD_RANDOM     2
#define SAMPLE_B2_PAYLOAD_LOWENTROPY 3

/* Up to this many additional "unrelated" MIDs for the B1 variant
 * (b1_extra_mids); kept small and fixed-size, no dynamic allocation. */
#define SAMPLE_B1_MAX_EXTRA_MIDS 4

/*
** Enabled and Disabled Definitions
*/
#define SAMPLE_DEVICE_DISABLED 0
#define SAMPLE_DEVICE_ENABLED  1

/*
** SAMPLE global data structure
** The cFE convention is to put all global app data in a single struct.
** This struct is defined in the `sample_app.h` file with one global instance
** in the `.c` file.
*/
typedef struct
{
    /*
    ** Housekeeping telemetry packet
    ** Each app defines its own packet which contains its OWN telemetry
    */
    SAMPLE_Hk_tlm_t HkTelemetryPkt; /* SAMPLE Housekeeping Telemetry Packet */

    /*
    ** Operational data  - not reported in housekeeping
    */
    CFE_MSG_Message_t *MsgPtr;    /* Pointer to msg received on software bus */
    CFE_SB_PipeId_t    CmdPipe;   /* Pipe Id for HK command pipe */
    uint32             RunStatus; /* App run status for controlling the application state */

    /*
     ** Device data
     ** TODO: Make specific to your application
     */
    SAMPLE_Device_tlm_t DevicePkt; /* Device specific data packet */

    /*
    ** Device protocol
    ** TODO: Make specific to your application
    */
    uart_info_t SampleUart; /* Hardware protocol definition */

    /* B1 attack scenario instrumentation only - not part of upstream SAMPLE */
    uint32 IllegitEpsRxCount;
    bool   B1B2Disabled; /* legacy marker-file result, kept for fallback (see sample_params.c) */

    /* B2 attack scenario only - covert coordination packet (see sample_msg.h) */
    SAMPLE_DiagSync_tlm_t DiagSyncPkt;

    /* Phase 1 simulation-rigor params, loaded once at init from sample_params
     * (see sample_params.c) with the legacy marker file as a fallback when
     * that file is absent. */
    bool   B1Enabled;
    bool   B2Enabled;
    uint16 B1ExtraMids[SAMPLE_B1_MAX_EXTRA_MIDS];
    uint8  B1ExtraMidCount;
    uint32 B1SubscribeDelayS;
    uint32 StartAfterS;
    uint32 DurationS; /* 0 = unset = active for the whole run, matching prior behavior */
    int    B2PayloadMode;
    uint32 B2JitterMs;

    /* Runtime onset/offset state, re-evaluated once per HK cycle (this app
     * has no independent timer task - see PHASE1_NOTES.md for why cycle
     * granularity, not true wall-clock granularity, is what's achievable
     * here without a bigger architectural change). */
    CFE_TIME_SysTime_t BootTime;
    bool               B1Subscribed;
    bool               B2WindowActive;
    uint32             B2SkipCyclesRemaining; /* jitter approximation, see sample_params.c */

} SAMPLE_AppData_t;

/*
** Exported Data
** Extern the global struct in the header for the Unit Test Framework (UTF).
*/
extern SAMPLE_AppData_t SAMPLE_AppData; /* SAMPLE App Data */

/*
**
** Local function prototypes.
**
** Note: Except for the entry point (SAMPLE_AppMain), these
**       functions are not called from any other source module.
*/
void  SAMPLE_AppMain(void);
int32 SAMPLE_AppInit(void);
void  SAMPLE_ProcessCommandPacket(void);
void  SAMPLE_ProcessGroundCommand(void);
void  SAMPLE_ProcessTelemetryRequest(void);
void  SAMPLE_ReportHousekeeping(void);
void  SAMPLE_ReportDeviceTelemetry(void);
void  SAMPLE_ResetCounters(void);
void  SAMPLE_Enable(void);
void  SAMPLE_Disable(void);
void  SAMPLE_Configure(void);
int32 SAMPLE_VerifyCmdLength(CFE_MSG_Message_t *msg, uint16 expected_length);

/* TODO: This is specific to the sample application, remove if using template generator */
void SAMPLE_ProcessMgrHk(void);

#endif /* _SAMPLE_APP_H_ */
