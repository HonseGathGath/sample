/*******************************************************************************
** File: sample_params.h
**
** Purpose: Phase 1 simulation-rigor parameterization for SAMPLE's B1/B2
** attack scenarios - load a plain-text params file at init, and evaluate
** onset/offset state once per HK cycle. See sample_params.c for the file
** format and PHASE1_NOTES.md for the design rationale and known limits.
*******************************************************************************/
#ifndef SAMPLE_PARAMS_H
#define SAMPLE_PARAMS_H

/* Reads "sample_params" from the FSW exe's cwd (same location as the legacy
 * sample_disable_b1b2 marker file) into SAMPLE_AppData's param fields.
 * Call once, early in SAMPLE_AppInit, before any B1/B2 setup. */
void SAMPLE_LoadParams(void);

/* Re-evaluates whether B1's subscription and B2's publish window should be
 * active right now, given elapsed time since boot, and performs any
 * subscribe/unsubscribe transition plus an onset/offset event. Call once at
 * the end of AppInit (handles the "active from t=0" case) and once per HK
 * cycle thereafter (handles delayed onset / timed offset). */
void SAMPLE_UpdateB1B2Activation(void);

/* Returns true if `mid` is one of B1's currently-configured extra MIDs
 * (used by the command-packet switch to count illegitimate-subscription
 * traffic for MIDs other than the default GENERIC_EPS_HK_TLM_MID). */
bool SAMPLE_IsB1ExtraMid(uint16 mid);

#endif /* SAMPLE_PARAMS_H */
