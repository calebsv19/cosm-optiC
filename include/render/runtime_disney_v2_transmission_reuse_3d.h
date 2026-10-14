#pragma once
/* One-call cache: every input direction and sampling context is identical.
 * Never shared across hits, pixels, frames, scenes, or renderer threads. */
typedef struct RuntimeDisneyV2TransmissionTraceReuse {
    bool valid, contributed, receiver_found;
    RuntimeDirectLight3DResult direct;
    HitInfo3D hit;
    Ray3D ray;
    double throughput_r, throughput_g, throughput_b;
    int depth;
    RuntimeRenderTraceCostTransmissionTermination3D termination;
} RuntimeDisneyV2TransmissionTraceReuse;
