#ifndef RAY_TRACING_SEQUENCE_RECOVERY_H
#define RAY_TRACING_SEQUENCE_RECOVERY_H
#include "app/agent_render_request.h"
/* Validate every retained sequence-frame receipt before skipping its image. */
bool ray_tracing_sequence_recovery_validate(const RayTracingAgentRenderRequest *,
                                            int, int, char *, size_t);
#endif
