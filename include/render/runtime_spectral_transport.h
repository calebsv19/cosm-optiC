#ifndef RENDER_RUNTIME_SPECTRAL_TRANSPORT_H
#define RENDER_RUNTIME_SPECTRAL_TRANSPORT_H
#include "render/runtime_spectral_surface_path.h"
#include <json-c/json.h>

/* One route resolver and execution owner for normal V3. Geometry and job
 * lifetime stay with the native prepared frame/scheduler. */
typedef enum {RUNTIME_SPECTRAL_UNRESOLVED,RUNTIME_SPECTRAL_SURFACE_PATH,RUNTIME_SPECTRAL_PHOTON_MAP,
    RUNTIME_SPECTRAL_TWO_SURFACE_CALIBRATION,RUNTIME_SPECTRAL_PENCIL_CALIBRATION,RUNTIME_SPECTRAL_LEGACY_DIRECT} RuntimeSpectralTransportRoute;
RuntimeSpectralTransportRoute RuntimeSpectralTransport_Route(const RuntimeSpectralPencil *);
const char *RuntimeSpectralTransport_RouteLabel(RuntimeSpectralTransportRoute);
typedef enum {RUNTIME_SPECTRAL_FINAL,RUNTIME_SPECTRAL_PREVIEW} RuntimeSpectralTransportPurpose;
bool RuntimeSpectralTransport_Prepare(RuntimeNative3DPreparedFrame *,const RuntimeSpectralPencil *,
    const char *,RuntimeSpectralTransportPurpose,char *,size_t);
const char *RuntimeSpectralTransport_StatusLabel(RuntimeSpectralExecutionStatus);
void RuntimeSpectralTransport_Finish(RuntimeNative3DPreparedFrame *,bool,const RuntimeNative3DTileSchedulerCancelToken *);
bool RuntimeSpectralTransport_ConfigureOutput(RuntimeNative3DPreparedFrame *,const char *,bool);
bool RuntimeSpectralTransport_IsSurfacePath(const RuntimeNative3DPreparedFrame *);
bool RuntimeSpectralTransport_Render(RuntimeNative3DPreparedFrame *,uint8_t *,
    const RuntimeNative3DTileSchedulerControl *,RuntimeNative3DTileSchedulerProgressCallback,void *,
    RuntimeNative3DRenderStats *);
/* terminal=false reads immutable settings only; safe while a job is running.
 * terminal=true requires joined/exclusive execution. Caller owns the result. */
typedef struct {
    bool valid,terminal,preview;
    RuntimeSpectralExecutionStatus execution_status;
    int requested_samples,width,height,frame_index;
    RuntimeCameraProjector3D projector;
    double camera_zoom;
    RuntimeSpectralPencil profile;
    RuntimeSpectralReadout readout;
    char diagnostic[256];
} RuntimeSpectralTransportSnapshot;
/* Capture settings during preparation, or results only after joined/exclusive execution.
 * The snapshot owns its values and can outlive the prepared frame. */
bool RuntimeSpectralTransport_Capture(const RuntimeNative3DPreparedFrame *,bool,RuntimeSpectralTransportSnapshot *);
json_object *RuntimeSpectralTransportSnapshot_JSON(const RuntimeSpectralTransportSnapshot *,bool detailed);
json_object *RuntimeSpectralTransport_JSON(const RuntimeNative3DPreparedFrame *,bool terminal);
json_object *RuntimeSpectralSurfacePath_JSON(const RuntimeSpectralPencil *,const RuntimeSpectralReadout *,
    const RuntimeScene3D *,bool,const char *,const char *);
#endif
