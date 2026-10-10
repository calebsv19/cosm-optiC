#include "render/runtime_caustic_photon_receiver_accounting_3d.h"
#include <math.h>
#include <stdatomic.h>
#include <string.h>
static RuntimePhotonReceiverAccounting3D g_accounting;
static bool g_enabled;
static atomic_flag g_lock = ATOMIC_FLAG_INIT;
static void lock(void) {
    while (atomic_flag_test_and_set_explicit(&g_lock, memory_order_acquire)) {}
}
static void unlock(void) { atomic_flag_clear_explicit(&g_lock, memory_order_release); }
/* Enable/bind/reset occur before worker dispatch; snapshots after workers join. */
void RuntimePhotonReceiverAccounting3D_SetEnabled(bool enabled) { g_enabled = enabled; }
void RuntimePhotonReceiverAccounting3D_Reset(void) {
    lock(); memset(&g_accounting, 0, sizeof(g_accounting)); unlock();
}
void RuntimePhotonReceiverAccounting3D_Record(int object_index, int material_id,
    const RuntimeCausticPhotonMapQueryResult3D* query, Vec3 radiance) {
    RuntimePhotonReceiverAccountingBucket3D* bucket = NULL;
    if (!g_enabled || !query) return;
    lock();
    for (uint64_t i = 0; i < g_accounting.bucketCount; ++i) {
        if (g_accounting.buckets[i].sceneObjectIndex == object_index &&
            g_accounting.buckets[i].materialId == material_id) {
            bucket = &g_accounting.buckets[i]; break;
        }
    }
    if (!bucket && g_accounting.bucketCount < RUNTIME_PHOTON_RECEIVER_ACCOUNTING_CAPACITY) {
        bucket = &g_accounting.buckets[g_accounting.bucketCount++];
        bucket->sceneObjectIndex = object_index; bucket->materialId = material_id;
    }
    if (!bucket) { g_accounting.overflowQueryCount++; unlock(); return; }
    bucket->queryCount++;
    if (query->hit) {
        bucket->positiveQueryCount++;
        if (query->undersampled) bucket->undersampledPositiveQueryCount++;
    }
    bucket->effectiveSampleCountSum += query->effectiveSampleCount;
    bucket->receiverRejectCount += query->receiverRejectCount;
    bucket->normalRejectCount += query->normalRejectCount;
    bucket->irradianceSum = vec3_add(bucket->irradianceSum, query->physicalFlux);
    bucket->radianceSum = vec3_add(bucket->radianceSum, radiance);
    bucket->maxRadiance = fmax(bucket->maxRadiance, fmax(radiance.x, fmax(radiance.y, radiance.z)));
    if (bucket->queryCount == 1 || query->supportRadius < bucket->minimumSupportRadius)
        bucket->minimumSupportRadius = query->supportRadius;
    bucket->maximumSupportRadius = fmax(bucket->maximumSupportRadius, query->supportRadius);
    unlock();
}
void RuntimePhotonReceiverAccounting3D_Snapshot(RuntimePhotonReceiverAccounting3D* out) {
    if (!out) return;
    lock(); *out = g_accounting; unlock();
}
