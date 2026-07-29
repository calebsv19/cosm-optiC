#include "test_preview_timeline_inspection.h"

#include <stdio.h>
#include <string.h>

#include "app/preview_timeline_inspection.h"
#include "test_support.h"

static RuntimeSceneLightTimelineDocument preview_timeline_document_fixture(void) {
  RuntimeSceneLightTimelineDocument document = {0};
  TimelineTrack progress = {0};
  TimelineTrack intensity = {0};

  assert_true("preview_marker_document_init",
              TimelineDocumentInit(&document.timeline,
                                   (TimelineRate){24u, 1u},
                                   (TimelineRange){10, 21u}) ==
                  TIMELINE_STATUS_OK);
  assert_true("preview_marker_progress_init",
              TimelineTrackInit(&progress, "light_progress", "light/key",
                                "light.path.progress",
                                TIMELINE_VALUE_SCALAR) == TIMELINE_STATUS_OK);
  assert_true("preview_marker_progress_first",
              TimelineTrackAddKey(&progress, 10, TimelineValueScalar(0.0),
                                  TIMELINE_INTERPOLATION_LINEAR) ==
                  TIMELINE_STATUS_OK);
  assert_true("preview_marker_progress_middle",
              TimelineTrackAddKey(&progress, 20, TimelineValueScalar(0.5),
                                  TIMELINE_INTERPOLATION_CUBIC_BEZIER) ==
                  TIMELINE_STATUS_OK);
  assert_true("preview_marker_progress_last",
              TimelineTrackAddKey(&progress, 30, TimelineValueScalar(1.0),
                                  TIMELINE_INTERPOLATION_STEP) ==
                  TIMELINE_STATUS_OK);
  assert_true("preview_marker_intensity_init",
              TimelineTrackInit(&intensity, "light_intensity", "light/key",
                                "light.intensity", TIMELINE_VALUE_SCALAR) ==
                  TIMELINE_STATUS_OK);
  assert_true("preview_marker_intensity_first",
              TimelineTrackAddKey(&intensity, 15, TimelineValueScalar(2.0),
                                  TIMELINE_INTERPOLATION_STEP) ==
                  TIMELINE_STATUS_OK);
  assert_true("preview_marker_add_progress",
              TimelineDocumentAddTrack(&document.timeline, &progress) ==
                  TIMELINE_STATUS_OK);
  assert_true("preview_marker_add_intensity",
              TimelineDocumentAddTrack(&document.timeline, &intensity) ==
                  TIMELINE_STATUS_OK);
  document.valid = true;
  document.progress_track_index = 0u;
  return document;
}

static void test_preview_marker_projection_is_read_only(void) {
  RuntimeSceneLightTimelineDocument document =
      preview_timeline_document_fixture();
  RuntimeSceneLightTimelineDocument before = document;
  PreviewTimelineInspection inspection = {0};

  assert_true("preview_marker_inspection_init",
              PreviewTimelineInspectionInit(&inspection, &document) ==
                  TIMELINE_STATUS_OK);
  assert_true("preview_marker_count", inspection.marker_count == 4u);
  assert_true("preview_marker_order",
              inspection.markers[0].frame == 10 &&
                  inspection.markers[1].frame == 20 &&
                  inspection.markers[2].frame == 30 &&
                  inspection.markers[3].frame == 15);
  assert_true("preview_marker_document_nonmutation",
              memcmp(&document, &before, sizeof(document)) == 0);
  PreviewTimelineInspectionReset(&inspection);
}

static void test_preview_marker_exact_selection_and_overlap_order(void) {
  RuntimeSceneLightTimelineDocument document =
      preview_timeline_document_fixture();
  PreviewTimelineInspection inspection = {0};
  PreviewWorkspaceRect slider = {100, 200, 400, 18};
  TimelineSample sample = {0};

  (void)PreviewTimelineInspectionInit(&inspection, &document);
  assert_true("preview_marker_start_x",
              PreviewTimelineInspectionMarkerX(&inspection, 0u, slider) ==
                  100);
  assert_true("preview_marker_middle_x",
              PreviewTimelineInspectionMarkerX(&inspection, 1u, slider) ==
                  300);
  assert_true("preview_marker_end_x",
              PreviewTimelineInspectionMarkerX(&inspection, 2u, slider) ==
                  500);
  assert_true("preview_marker_select_nearest",
              PreviewTimelineInspectionSelectAt(&inspection, slider, 304, 209,
                                                7, &sample));
  assert_true("preview_marker_select_exact_sample",
              sample.absolute_frame == 20 &&
                  sample.subframe_numerator == 0u &&
                  sample.subframe_denominator == 1u);
  assert_true("preview_marker_select_stable_track_order",
              inspection.selected_marker_index == 1u);
  assert_true("preview_marker_miss",
              !PreviewTimelineInspectionSelectAt(&inspection, slider, 350, 209,
                                                 7, &sample));
  PreviewTimelineInspectionReset(&inspection);
}

static void test_preview_marker_readout_uses_evaluated_provenance(void) {
  RuntimeSceneLightTimelineDocument document =
      preview_timeline_document_fixture();
  PreviewTimelineInspection inspection = {0};
  RayEvaluatedSceneSnapshot snapshot = {0};
  TimelineSample sample = {0};
  PreviewWorkspaceRect slider = {100, 200, 400, 18};

  (void)PreviewTimelineInspectionInit(&inspection, &document);
  (void)PreviewTimelineInspectionSelectAt(&inspection, slider, 300, 209, 1,
                                          &sample);
  snapshot.valid = true;
  snapshot.light.property_provenance.valid = true;
  snapshot.light.property_provenance.exact_key = true;
  snapshot.light.property_provenance.left_frame = 20;
  snapshot.light.property_provenance.right_frame = 20;
  snapshot.light.property_provenance.alpha = 0.0;
  snprintf(snapshot.light.property_provenance.target_id,
           sizeof(snapshot.light.property_provenance.target_id), "%s",
           "light/key");
  snprintf(snapshot.light.property_provenance.property_id,
           sizeof(snapshot.light.property_provenance.property_id), "%s",
           "light.path.progress");

  PreviewTimelineInspectionUpdateReadout(&inspection, &snapshot);
  assert_true("preview_marker_readout_frame",
              strstr(inspection.frame_line, "frame 20") != NULL);
  assert_true("preview_marker_readout_channel",
              strstr(inspection.channel_line, "light.path.progress") != NULL);
  assert_true("preview_marker_readout_interpolation",
              strstr(inspection.interpolation_line, "cubic_bezier") != NULL);
  assert_true("preview_marker_readout_provenance",
              strstr(inspection.provenance_line, "exact key") != NULL &&
                  strstr(inspection.provenance_line, "20 -> 20") != NULL);
  PreviewTimelineInspectionReset(&inspection);
}

static void test_preview_workspace_inspection_pauses_and_seeks_exactly(void) {
  PreviewWorkspace workspace = {0};
  TimelineSample sample = {20, 0u, 1u};
  TimelineSample current = {0};

  (void)PreviewWorkspaceInit(&workspace, (TimelineRate){24u, 1u},
                             (TimelineRange){10, 21u}, 800, 600);
  assert_true("preview_marker_workspace_starts_playing",
              workspace.transport.playing);
  assert_true("preview_marker_workspace_inspect",
              PreviewWorkspaceInspectSample(&workspace, sample) ==
                  TIMELINE_STATUS_OK);
  (void)PreviewWorkspaceCurrentSample(&workspace, &current, NULL);
  assert_true("preview_marker_workspace_paused_exact",
              !workspace.transport.playing && !workspace.scrubbing &&
                  current.absolute_frame == 20 &&
                  current.subframe_denominator == 1u);
}

int run_test_preview_timeline_inspection_tests(void) {
  test_preview_marker_projection_is_read_only();
  test_preview_marker_exact_selection_and_overlap_order();
  test_preview_marker_readout_uses_evaluated_provenance();
  test_preview_workspace_inspection_pauses_and_seeks_exactly();
  return test_support_failures();
}
