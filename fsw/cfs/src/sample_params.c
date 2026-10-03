/*******************************************************************************
** File: sample_params.c
**
** Purpose: Phase 1 simulation-rigor parameterization for B1 (illegitimate
** subscription) and B2 (covert coordination). See PHASE1_NOTES.md for the
** full design rationale; summary here:
**
** - One plain-text "sample_params" file (key=value, one per line, '#'
**   comments), read once at init from the FSW exe's cwd - the same
**   location the legacy sample_disable_b1b2 marker file already used.
**   This lets a host-side capture script vary B1/B2's behavior between
**   reps without a rebuild, same spirit as the existing marker files.
** - Backward compatible: if sample_params is absent, the legacy marker
**   file's presence/absence still sets B1/B2 enabled/disabled exactly as
**   before. If sample_params IS present, it is authoritative for every
**   key it sets (the marker is only consulted as a legacy default).
** - Onset/offset timing (start_after_s/duration_s) is evaluated once per
**   HK cycle, not on a dedicated timer: SAMPLE is a message-driven app
**   with no independent periodic task of its own, and its only recurring
**   "wake up" is handling SAMPLE_REQ_HK_MID (routed here by SCH at a
**   roughly 0.2-5Hz rate per evidence/phase1/SUMMARY.md's measured app
**   rates). That means onset/offset transitions land on HK-cycle
**   boundaries, not exact wall-clock seconds - a real, documented
**   limitation, not a bug.
*******************************************************************************/
#include "sample_app.h"
#include "sample_params.h"
#include "sample_events.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define SAMPLE_PARAMS_FILE "sample_params"

/* Isolation toggle for Experiment 2 (per-technique captures): checked once
 * at init from a marker file in the FSW exe cwd, so B1/B2 can be captured
 * alone or excluded from a B3/B4-only run without a rebuild. Absence (the
 * default) means B1/B2 active, as originally shipped. Consulted as a
 * fallback default only - sample_params, if present, is authoritative. */
#define SAMPLE_DISABLE_B1B2_MARKER "sample_disable_b1b2"

/* Reserved range for ephemeral/external MID values a host-side script might
 * poke in via a future b1_extra_mids entry that isn't one of this mission's
 * real declared MIDs - not enforced here (SAMPLE has no visibility into the
 * full mission MID table bus_mon uses), just documented so a script author
 * knows which range is safe to pick an out-of-band test value from without
 * checking this file: 0x09F0-0x09FF (see PHASE1_NOTES.md; the mission's
 * highest in-use 0x09xx telemetry MID at time of writing is 0x0993). */

static uint32 SAMPLE_ParseElapsedCeiling(CFE_TIME_SysTime_t now, CFE_TIME_SysTime_t boot)
{
    if (now.Seconds <= boot.Seconds)
    {
        return 0;
    }
    return now.Seconds - boot.Seconds;
}

/* Trim leading/trailing whitespace in place, return pointer to trimmed start. */
static char *SAMPLE_Trim(char *s)
{
    char *end;
    while (*s == ' ' || *s == '\t')
    {
        s++;
    }
    if (*s == '\0')
    {
        return s;
    }
    end = s + strlen(s) - 1;
    while (end > s && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r'))
    {
        *end = '\0';
        end--;
    }
    return s;
}

static void SAMPLE_ParseExtraMids(const char *value)
{
    char       buf[128];
    char      *tok;
    char      *saveptr;
    SAMPLE_AppData.B1ExtraMidCount = 0;

    strncpy(buf, value, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    tok = strtok_r(buf, ",", &saveptr);
    while (tok != NULL && SAMPLE_AppData.B1ExtraMidCount < SAMPLE_B1_MAX_EXTRA_MIDS)
    {
        char         *trimmed = SAMPLE_Trim(tok);
        unsigned long parsed  = strtoul(trimmed, NULL, 16);
        SAMPLE_AppData.B1ExtraMids[SAMPLE_AppData.B1ExtraMidCount] = (uint16)parsed;
        SAMPLE_AppData.B1ExtraMidCount++;
        tok = strtok_r(NULL, ",", &saveptr);
    }
}

static void SAMPLE_ApplyParamLine(char *line)
{
    char *eq;
    char *key;
    char *value;

    line = SAMPLE_Trim(line);
    if (line[0] == '\0' || line[0] == '#')
    {
        return;
    }
    eq = strchr(line, '=');
    if (eq == NULL)
    {
        return;
    }
    *eq  = '\0';
    key   = SAMPLE_Trim(line);
    value = SAMPLE_Trim(eq + 1);

    if (strcmp(key, "b1_enabled") == 0)
    {
        SAMPLE_AppData.B1Enabled = (atoi(value) != 0);
    }
    else if (strcmp(key, "b2_enabled") == 0)
    {
        SAMPLE_AppData.B2Enabled = (atoi(value) != 0);
    }
    else if (strcmp(key, "b1_extra_mids") == 0)
    {
        SAMPLE_ParseExtraMids(value);
    }
    else if (strcmp(key, "b1_subscribe_delay_s") == 0)
    {
        SAMPLE_AppData.B1SubscribeDelayS = (uint32)atoi(value);
    }
    else if (strcmp(key, "start_after_s") == 0)
    {
        SAMPLE_AppData.StartAfterS = (uint32)atoi(value);
    }
    else if (strcmp(key, "duration_s") == 0)
    {
        SAMPLE_AppData.DurationS = (uint32)atoi(value);
    }
    else if (strcmp(key, "b2_jitter_ms") == 0)
    {
        SAMPLE_AppData.B2JitterMs = (uint32)atoi(value);
    }
    else if (strcmp(key, "b2_payload_mode") == 0)
    {
        if (strcmp(value, "constant") == 0)
        {
            SAMPLE_AppData.B2PayloadMode = SAMPLE_B2_PAYLOAD_CONSTANT;
        }
        else if (strcmp(value, "random") == 0)
        {
            SAMPLE_AppData.B2PayloadMode = SAMPLE_B2_PAYLOAD_RANDOM;
        }
        else if (strcmp(value, "lowentropy") == 0)
        {
            SAMPLE_AppData.B2PayloadMode = SAMPLE_B2_PAYLOAD_LOWENTROPY;
        }
        else
        {
            SAMPLE_AppData.B2PayloadMode = SAMPLE_B2_PAYLOAD_COUNTER; /* "counter" or unrecognized */
        }
    }
    /* Unrecognized keys are ignored, not an error - forward compatible with
     * future params this file doesn't know about yet. */
}

void SAMPLE_LoadParams(void)
{
    FILE  *fp;
    char   line[256];
    bool   markerPresent = (access(SAMPLE_DISABLE_B1B2_MARKER, F_OK) == 0);

    /* Shipped defaults */
    SAMPLE_AppData.B1Enabled           = true;
    SAMPLE_AppData.B2Enabled           = true;
    SAMPLE_AppData.B1ExtraMidCount     = 1;
    SAMPLE_AppData.B1ExtraMids[0]      = GENERIC_EPS_HK_TLM_MID;
    SAMPLE_AppData.B1SubscribeDelayS   = 0;
    SAMPLE_AppData.StartAfterS         = 0;
    SAMPLE_AppData.DurationS           = 0;
    SAMPLE_AppData.B2PayloadMode       = SAMPLE_B2_PAYLOAD_COUNTER;
    SAMPLE_AppData.B2JitterMs          = 0;
    SAMPLE_AppData.B1Subscribed        = false;
    SAMPLE_AppData.B2WindowActive      = false;
    SAMPLE_AppData.B2SkipCyclesRemaining = 0;

    /* Legacy marker as a fallback default - only matters if sample_params
     * doesn't override b1_enabled/b2_enabled explicitly below. */
    SAMPLE_AppData.B1B2Disabled = markerPresent;
    if (markerPresent)
    {
        SAMPLE_AppData.B1Enabled = false;
        SAMPLE_AppData.B2Enabled = false;
    }

    fp = fopen(SAMPLE_PARAMS_FILE, "r");
    if (fp != NULL)
    {
        while (fgets(line, sizeof(line), fp) != NULL)
        {
            SAMPLE_ApplyParamLine(line);
        }
        fclose(fp);
    }

    /* Only matters for b2_payload_mode=random; harmless otherwise. Seeded
     * from wall clock + pid, so B2's random payload is NOT reproducible
     * run-to-run - a real limitation, distinct from this pipeline's
     * separate (and still true) finding that attack TIMING has no RNG
     * seed anywhere. Documented in PHASE1_NOTES.md. */
    srand((unsigned int)(time(NULL) ^ getpid()));
}

bool SAMPLE_IsB1ExtraMid(uint16 mid)
{
    uint8 i;
    for (i = 0; i < SAMPLE_AppData.B1ExtraMidCount; i++)
    {
        if (SAMPLE_AppData.B1ExtraMids[i] == mid)
        {
            return true;
        }
    }
    return false;
}

static void SAMPLE_B1Subscribe(void)
{
    uint8  i;
    int32  status;
    for (i = 0; i < SAMPLE_AppData.B1ExtraMidCount; i++)
    {
        status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(SAMPLE_AppData.B1ExtraMids[i]), SAMPLE_AppData.CmdPipe);
        if (status != CFE_SUCCESS)
        {
            CFE_EVS_SendEvent(SAMPLE_SUB_REQ_HK_ERR_EID, CFE_EVS_EventType_ERROR,
                              "Error Subscribing to B1 extra MID, MID=0x%04X, RC=0x%08X",
                              SAMPLE_AppData.B1ExtraMids[i], (unsigned int)status);
        }
    }
}

static void SAMPLE_B1Unsubscribe(void)
{
    uint8 i;
    for (i = 0; i < SAMPLE_AppData.B1ExtraMidCount; i++)
    {
        CFE_SB_Unsubscribe(CFE_SB_ValueToMsgId(SAMPLE_AppData.B1ExtraMids[i]), SAMPLE_AppData.CmdPipe);
    }
}

void SAMPLE_UpdateB1B2Activation(void)
{
    CFE_TIME_SysTime_t now        = CFE_TIME_GetTime();
    uint32              elapsed_s  = SAMPLE_ParseElapsedCeiling(now, SAMPLE_AppData.BootTime);
    uint32              b1_delay   = SAMPLE_AppData.StartAfterS;
    bool                b1_active;
    bool                b2_active;

    if (SAMPLE_AppData.B1SubscribeDelayS > b1_delay)
    {
        b1_delay = SAMPLE_AppData.B1SubscribeDelayS;
    }

    b1_active = SAMPLE_AppData.B1Enabled && (elapsed_s >= b1_delay) &&
                (SAMPLE_AppData.DurationS == 0 || elapsed_s < b1_delay + SAMPLE_AppData.DurationS);
    b2_active = SAMPLE_AppData.B2Enabled && (elapsed_s >= SAMPLE_AppData.StartAfterS) &&
                (SAMPLE_AppData.DurationS == 0 ||
                 elapsed_s < SAMPLE_AppData.StartAfterS + SAMPLE_AppData.DurationS);

    if (b1_active && !SAMPLE_AppData.B1Subscribed)
    {
        SAMPLE_B1Subscribe();
        SAMPLE_AppData.B1Subscribed = true;
        CFE_EVS_SendEvent(SAMPLE_B1_ONSET_EID, CFE_EVS_EventType_INFORMATION,
                          "SAMPLE: [B1] subscription onset, elapsed=%us", (unsigned int)elapsed_s);
    }
    else if (!b1_active && SAMPLE_AppData.B1Subscribed)
    {
        SAMPLE_B1Unsubscribe();
        SAMPLE_AppData.B1Subscribed = false;
        CFE_EVS_SendEvent(SAMPLE_B1_OFFSET_EID, CFE_EVS_EventType_INFORMATION,
                          "SAMPLE: [B1] subscription offset, elapsed=%us", (unsigned int)elapsed_s);
    }

    if (b2_active != SAMPLE_AppData.B2WindowActive)
    {
        CFE_EVS_SendEvent(b2_active ? SAMPLE_B2_ONSET_EID : SAMPLE_B2_OFFSET_EID, CFE_EVS_EventType_INFORMATION,
                          "SAMPLE: [B2] publish window %s, elapsed=%us", b2_active ? "onset" : "offset",
                          (unsigned int)elapsed_s);
        SAMPLE_AppData.B2WindowActive = b2_active;
    }
}
