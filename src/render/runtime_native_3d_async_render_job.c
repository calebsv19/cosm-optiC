#include "render/runtime_native_3d_async_render_job.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

struct RuntimeNative3DAsyncRenderJob {
    pthread_mutex_t mutex;
    pthread_t thread;
    bool mutexReady;
    bool threadStarted;
    bool published;
    volatile bool cancelRequested;
    RuntimeNative3DAsyncRenderJobStatus status;
    RuntimeNative3DRenderRequestSnapshot snapshot;
    RuntimeNative3DAsyncRenderJobRunFn runFn;
    void* userData;
    RuntimeNative3DAsyncRenderJobResult result;
};

static void runtime_native_3d_async_render_job_result_init(
    RuntimeNative3DAsyncRenderJobResult* result,
    const RuntimeNative3DRenderRequestSnapshot* snapshot,
    uint64_t generation) {
    if (!result) return;
    memset(result, 0, sizeof(*result));
    result->valid = true;
    result->generation = generation;
    if (snapshot) {
        result->snapshot = *snapshot;
    }
}

static void* runtime_native_3d_async_render_job_worker(void* user_data) {
    RuntimeNative3DAsyncRenderJob* job = (RuntimeNative3DAsyncRenderJob*)user_data;
    RuntimeNative3DAsyncRenderJobResult result;
    bool ok = false;
    bool cancel_requested = false;

    if (!job) return NULL;
    runtime_native_3d_async_render_job_result_init(&result,
                                                   &job->snapshot,
                                                   job->snapshot.generation);
    ok = job->runFn(&job->snapshot,
                    &job->snapshot.cancelToken,
                    job->userData,
                    &result);
    cancel_requested = job->cancelRequested || result.cancelRequested;
    result.cancelRequested = cancel_requested;
    result.canceled = result.canceled || cancel_requested;
    result.succeeded = ok && !result.canceled;
    result.generation = job->snapshot.generation;
    result.snapshot = job->snapshot;

    pthread_mutex_lock(&job->mutex);
    job->result = result;
    if (result.canceled) {
        job->status = RUNTIME_NATIVE_3D_ASYNC_RENDER_JOB_CANCELED;
    } else if (result.succeeded) {
        job->status = RUNTIME_NATIVE_3D_ASYNC_RENDER_JOB_COMPLETED;
    } else {
        job->status = RUNTIME_NATIVE_3D_ASYNC_RENDER_JOB_FAILED;
    }
    pthread_mutex_unlock(&job->mutex);
    return NULL;
}

RuntimeNative3DAsyncRenderJob* RuntimeNative3DAsyncRenderJob_Create(void) {
    RuntimeNative3DAsyncRenderJob* job =
        (RuntimeNative3DAsyncRenderJob*)calloc(1u, sizeof(*job));
    if (!job) return NULL;
    if (pthread_mutex_init(&job->mutex, NULL) != 0) {
        free(job);
        return NULL;
    }
    job->mutexReady = true;
    job->status = RUNTIME_NATIVE_3D_ASYNC_RENDER_JOB_IDLE;
    return job;
}

void RuntimeNative3DAsyncRenderJob_Destroy(RuntimeNative3DAsyncRenderJob* job) {
    if (!job) return;
    RuntimeNative3DAsyncRenderJob_ShutdownCancelFirst(job);
    if (job->mutexReady) {
        pthread_mutex_destroy(&job->mutex);
    }
    free(job);
}

bool RuntimeNative3DAsyncRenderJob_Start(
    RuntimeNative3DAsyncRenderJob* job,
    const RuntimeNative3DAsyncRenderJobStartDesc* desc) {
    RuntimeNative3DRenderRequestSnapshot snapshot;

    if (!job || !desc || !desc->run_fn || desc->generation == 0u ||
        !desc->snapshot.valid) {
        return false;
    }

    pthread_mutex_lock(&job->mutex);
    if (job->threadStarted ||
        job->status == RUNTIME_NATIVE_3D_ASYNC_RENDER_JOB_RUNNING) {
        pthread_mutex_unlock(&job->mutex);
        return false;
    }

    snapshot = desc->snapshot;
    snapshot.generationBound = true;
    snapshot.generation = desc->generation;
    snapshot.cancelTokenBound = true;
    snapshot.cancelToken.cancelRequested = &job->cancelRequested;
    snapshot.cancelToken.generation = desc->generation;
    snapshot.cancelGeneration = desc->generation;

    job->cancelRequested = false;
    job->published = false;
    job->snapshot = snapshot;
    job->runFn = desc->run_fn;
    job->userData = desc->user_data;
    runtime_native_3d_async_render_job_result_init(&job->result,
                                                   &job->snapshot,
                                                   desc->generation);
    job->status = RUNTIME_NATIVE_3D_ASYNC_RENDER_JOB_RUNNING;
    if (pthread_create(&job->thread,
                       NULL,
                       runtime_native_3d_async_render_job_worker,
                       job) != 0) {
        job->status = RUNTIME_NATIVE_3D_ASYNC_RENDER_JOB_FAILED;
        job->runFn = NULL;
        job->userData = NULL;
        pthread_mutex_unlock(&job->mutex);
        return false;
    }
    job->threadStarted = true;
    pthread_mutex_unlock(&job->mutex);
    return true;
}

void RuntimeNative3DAsyncRenderJob_RequestCancel(RuntimeNative3DAsyncRenderJob* job) {
    if (!job) return;
    pthread_mutex_lock(&job->mutex);
    job->cancelRequested = true;
    pthread_mutex_unlock(&job->mutex);
}

bool RuntimeNative3DAsyncRenderJob_Join(RuntimeNative3DAsyncRenderJob* job) {
    bool should_join = false;
    if (!job) return false;

    pthread_mutex_lock(&job->mutex);
    should_join = job->threadStarted;
    pthread_mutex_unlock(&job->mutex);

    if (should_join) {
        if (pthread_join(job->thread, NULL) != 0) {
            return false;
        }
        pthread_mutex_lock(&job->mutex);
        job->threadStarted = false;
        job->runFn = NULL;
        job->userData = NULL;
        pthread_mutex_unlock(&job->mutex);
    }
    return true;
}

bool RuntimeNative3DAsyncRenderJob_ShutdownCancelFirst(
    RuntimeNative3DAsyncRenderJob* job) {
    if (!job) return false;
    RuntimeNative3DAsyncRenderJob_RequestCancel(job);
    return RuntimeNative3DAsyncRenderJob_Join(job);
}

RuntimeNative3DAsyncRenderJobStatus RuntimeNative3DAsyncRenderJob_GetStatus(
    RuntimeNative3DAsyncRenderJob* job) {
    RuntimeNative3DAsyncRenderJobStatus status =
        RUNTIME_NATIVE_3D_ASYNC_RENDER_JOB_FAILED;
    if (!job) return status;
    pthread_mutex_lock(&job->mutex);
    status = job->status;
    pthread_mutex_unlock(&job->mutex);
    return status;
}

RuntimeNative3DAsyncRenderPublishStatus RuntimeNative3DAsyncRenderJob_TryPublish(
    RuntimeNative3DAsyncRenderJob* job,
    uint64_t current_generation,
    RuntimeNative3DAsyncRenderJobResult* out_result) {
    RuntimeNative3DAsyncRenderJobResult result;
    RuntimeNative3DAsyncRenderPublishStatus publish_status =
        RUNTIME_NATIVE_3D_ASYNC_RENDER_PUBLISH_NOT_READY;

    if (!job) {
        return RUNTIME_NATIVE_3D_ASYNC_RENDER_PUBLISH_FAILED;
    }

    pthread_mutex_lock(&job->mutex);
    if (job->published) {
        publish_status = RUNTIME_NATIVE_3D_ASYNC_RENDER_PUBLISH_ALREADY_PUBLISHED;
    } else if (job->status == RUNTIME_NATIVE_3D_ASYNC_RENDER_JOB_RUNNING ||
               job->status == RUNTIME_NATIVE_3D_ASYNC_RENDER_JOB_IDLE) {
        publish_status = RUNTIME_NATIVE_3D_ASYNC_RENDER_PUBLISH_NOT_READY;
    } else {
        result = job->result;
        if (job->status == RUNTIME_NATIVE_3D_ASYNC_RENDER_JOB_CANCELED) {
            result.canceled = true;
            publish_status = RUNTIME_NATIVE_3D_ASYNC_RENDER_PUBLISH_CANCELED;
        } else if (job->status == RUNTIME_NATIVE_3D_ASYNC_RENDER_JOB_FAILED) {
            publish_status = RUNTIME_NATIVE_3D_ASYNC_RENDER_PUBLISH_FAILED;
        } else if (current_generation != job->snapshot.generation) {
            result.staleGeneration = true;
            publish_status = RUNTIME_NATIVE_3D_ASYNC_RENDER_PUBLISH_STALE_GENERATION;
        } else {
            result.published = true;
            job->published = true;
            publish_status = RUNTIME_NATIVE_3D_ASYNC_RENDER_PUBLISH_PUBLISHED;
        }
        if (out_result) {
            *out_result = result;
        }
    }
    pthread_mutex_unlock(&job->mutex);
    return publish_status;
}
