#include "test_preview_workspace.h"

#include <math.h>

#include "app/preview_workspace.h"
#include "test_support.h"

static PreviewWorkspace preview_workspace_fixture(void) {
  PreviewWorkspace workspace = {0};
  assert_true("preview_workspace_init",
              PreviewWorkspaceInit(&workspace, (TimelineRate){10u, 1u},
                                   (TimelineRange){5, 4u}, 800,
                                   600) == TIMELINE_STATUS_OK);
  return workspace;
}

static void preview_workspace_click(PreviewWorkspace *workspace,
                                    PreviewWorkspaceRect rect) {
  int x = rect.x + rect.w / 2;
  int y = rect.y + rect.h / 2;
  assert_true("preview_workspace_click_down",
              PreviewWorkspacePointerDown(workspace, x, y) ==
                  TIMELINE_STATUS_OK);
  assert_true("preview_workspace_click_up",
              PreviewWorkspacePointerUp(workspace, x, y) == TIMELINE_STATUS_OK);
}

static void test_preview_workspace_defaults_and_controls(void) {
  PreviewWorkspace workspace = preview_workspace_fixture();

  assert_true("preview_workspace_default_playing", workspace.transport.playing);
  assert_true("preview_workspace_default_loop",
              workspace.transport.mode == PREVIEW_TRANSPORT_MODE_LOOP);
  assert_true("preview_workspace_layout_bottom",
              workspace.layout.panel.y == 512 &&
                  workspace.layout.panel.h == 88);

  preview_workspace_click(&workspace, workspace.layout.play_pause_button);
  assert_true("preview_workspace_pause_control", !workspace.transport.playing);
  preview_workspace_click(&workspace, workspace.layout.play_pause_button);
  assert_true("preview_workspace_play_control", workspace.transport.playing);

  preview_workspace_click(&workspace, workspace.layout.bounce_button);
  assert_true("preview_workspace_bounce_control",
              workspace.transport.mode == PREVIEW_TRANSPORT_MODE_BOUNCE);
  preview_workspace_click(&workspace, workspace.layout.loop_button);
  assert_true("preview_workspace_loop_control",
              workspace.transport.mode == PREVIEW_TRANSPORT_MODE_LOOP);
}

static void test_preview_workspace_exact_scrub_and_resume(void) {
  PreviewWorkspace workspace = preview_workspace_fixture();
  PreviewWorkspaceRect track = workspace.layout.slider_track;
  TimelineSample sample = {0};

  assert_true("preview_workspace_scrub_down",
              PreviewWorkspacePointerDown(&workspace, track.x + track.w / 3,
                                          track.y + track.h / 2) ==
                  TIMELINE_STATUS_OK);
  assert_true("preview_workspace_scrub_pauses",
              workspace.scrubbing && !workspace.transport.playing &&
                  workspace.resume_after_scrub);
  (void)PreviewWorkspaceCurrentSample(&workspace, &sample, NULL);
  assert_true("preview_workspace_scrub_exact_frame",
              sample.absolute_frame == 6 && sample.subframe_numerator == 0u &&
                  sample.subframe_denominator == 1u);

  assert_true("preview_workspace_scrub_motion",
              PreviewWorkspacePointerMotion(&workspace, track.x + track.w,
                                            track.y + track.h / 2) ==
                  TIMELINE_STATUS_OK);
  (void)PreviewWorkspaceCurrentSample(&workspace, &sample, NULL);
  assert_true("preview_workspace_scrub_end_exact",
              sample.absolute_frame == 8 && sample.subframe_numerator == 0u &&
                  sample.subframe_denominator == 1u);

  assert_true("preview_workspace_scrub_up",
              PreviewWorkspacePointerUp(&workspace, track.x + track.w,
                                        track.y + track.h / 2) ==
                  TIMELINE_STATUS_OK);
  assert_true("preview_workspace_scrub_resumes",
              !workspace.scrubbing && workspace.transport.playing);
}

static void test_preview_workspace_paused_scrub_and_endpoints(void) {
  PreviewWorkspace workspace = preview_workspace_fixture();
  PreviewWorkspaceRect track = workspace.layout.slider_track;
  TimelineSample sample = {0};

  (void)PreviewWorkspaceTogglePlayback(&workspace);
  assert_true("preview_workspace_paused_before_scrub",
              !workspace.transport.playing);
  (void)PreviewWorkspacePointerDown(&workspace, track.x + track.w,
                                    track.y + track.h / 2);
  (void)PreviewWorkspacePointerUp(&workspace, track.x + track.w,
                                  track.y + track.h / 2);
  (void)PreviewWorkspaceCurrentSample(&workspace, &sample, NULL);
  assert_true("preview_workspace_paused_scrub_end",
              !workspace.transport.playing && sample.absolute_frame == 8 &&
                  fabs(PreviewWorkspaceNormalizedPosition(&workspace) - 1.0) <
                      1e-12);

  (void)PreviewWorkspacePointerDown(&workspace, track.x, track.y + track.h / 2);
  (void)PreviewWorkspacePointerUp(&workspace, track.x, track.y + track.h / 2);
  (void)PreviewWorkspaceCurrentSample(&workspace, &sample, NULL);
  assert_true("preview_workspace_paused_scrub_start",
              !workspace.transport.playing && sample.absolute_frame == 5 &&
                  PreviewWorkspaceNormalizedPosition(&workspace) == 0.0);
}

static void test_preview_workspace_frame_step_and_resize(void) {
  PreviewWorkspace workspace = preview_workspace_fixture();
  TimelineSample sample = {0};

  assert_true("preview_workspace_step_forward",
              PreviewWorkspaceSeekFrameDelta(&workspace, 1) ==
                  TIMELINE_STATUS_OK);
  (void)PreviewWorkspaceCurrentSample(&workspace, &sample, NULL);
  assert_true("preview_workspace_step_exact",
              sample.absolute_frame == 6 && sample.subframe_denominator == 1u);
  assert_true("preview_workspace_step_clamp",
              PreviewWorkspaceSeekFrameDelta(&workspace, 99) ==
                  TIMELINE_STATUS_OK);
  (void)PreviewWorkspaceCurrentSample(&workspace, &sample, NULL);
  assert_true("preview_workspace_step_clamped_end", sample.absolute_frame == 8);

  assert_true("preview_workspace_resize",
              PreviewWorkspaceResize(&workspace, 1024, 768) ==
                  TIMELINE_STATUS_OK);
  assert_true("preview_workspace_resize_layout",
              workspace.layout.panel.y == 680 &&
                  workspace.layout.panel.w == 1024 &&
                  workspace.layout.slider_track.w > 400);
}

int run_test_preview_workspace_tests(void) {
  test_preview_workspace_defaults_and_controls();
  test_preview_workspace_exact_scrub_and_resume();
  test_preview_workspace_paused_scrub_and_endpoints();
  test_preview_workspace_frame_step_and_resize();
  return test_support_failures();
}
