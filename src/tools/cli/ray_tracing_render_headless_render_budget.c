#include "tools/ray_tracing_render_headless_internal.h"

const RuntimeNative3DResourceBudget *ray_tracing_headless_request_resource_budget(
    const RayTracingAgentRenderRequest *request,
    RuntimeNative3DResourceBudget *out_budget) {
    if (!request || !out_budget || !request->has_resource_budget) {
        return NULL;
    }
    out_budget->cpuPercent = request->resource_cpu_percent;
    out_budget->maxWorkerThreads = request->resource_max_workers;
    out_budget->reserveCpuCount = request->resource_reserve_cpu_count;
    return out_budget;
}
