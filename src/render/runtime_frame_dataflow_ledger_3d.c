#include "render/runtime_frame_dataflow_ledger_3d.h"

#include <stdlib.h>

bool RuntimeFrameDataflowLedger3D_IsEnabled(void) {
    const char* value = getenv("RAY_TRACING_FRAME_DATAFLOW_STATE_LEDGER");
    return value && value[0] && value[0] != '0';
}
