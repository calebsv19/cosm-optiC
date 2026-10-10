#ifndef RENDER_RUNTIME_CAUSTIC_PHOTON_RECEIVER_ACCOUNTING_3D_H
#define RENDER_RUNTIME_CAUSTIC_PHOTON_RECEIVER_ACCOUNTING_3D_H
#include "render/runtime_caustic_photon_map_3d.h"
/* Bounded diagnostic storage, independent of image size. Sums are over query
 * calls, never spatially integrated power or unique pixels. Disabled by default. */
enum { RUNTIME_PHOTON_RECEIVER_ACCOUNTING_CAPACITY = 64 };
typedef struct {
    int sceneObjectIndex, materialId;
    uint64_t queryCount, positiveQueryCount, undersampledPositiveQueryCount;
    uint64_t effectiveSampleCountSum, receiverRejectCount, normalRejectCount;
    Vec3 irradianceSum, radianceSum;
    double maxRadiance, minimumSupportRadius, maximumSupportRadius;
} RuntimePhotonReceiverAccountingBucket3D;
typedef struct {
    uint64_t bucketCount, overflowQueryCount;
    RuntimePhotonReceiverAccountingBucket3D buckets[RUNTIME_PHOTON_RECEIVER_ACCOUNTING_CAPACITY];
} RuntimePhotonReceiverAccounting3D;
void RuntimePhotonReceiverAccounting3D_SetEnabled(bool enabled);
void RuntimePhotonReceiverAccounting3D_Reset(void);
void RuntimePhotonReceiverAccounting3D_Record(int object_index, int material_id,
    const RuntimeCausticPhotonMapQueryResult3D* query, Vec3 radiance);
void RuntimePhotonReceiverAccounting3D_Snapshot(RuntimePhotonReceiverAccounting3D* out);
#endif
