#include "test_preview_transport.h"

#include <string.h>

#include "app/preview_transport.h"
#include "test_support.h"

static PreviewTransport preview_transport_fixture(void) {
  PreviewTransport transport;
  memset(&transport, 0, sizeof(transport));
  assert_true("preview_transport_init",
              PreviewTransportInit(&transport, (TimelineRate){10u, 1u},
                                   (TimelineRange){5, 4u}) ==
                  TIMELINE_STATUS_OK);
  return transport;
}

static void test_preview_transport_defaults_and_pause(void) {
  PreviewTransport transport = preview_transport_fixture();
  TimelineSample sample = {0};
  PreviewTransportDirection direction = PREVIEW_TRANSPORT_DIRECTION_REVERSE;

  assert_true("preview_transport_default_paused", !transport.playing);
  assert_true("preview_transport_default_loop",
              transport.mode == PREVIEW_TRANSPORT_MODE_LOOP);
  assert_true("preview_transport_default_forward",
              transport.direction == PREVIEW_TRANSPORT_DIRECTION_FORWARD);
  assert_true("preview_transport_paused_advance",
              PreviewTransportAdvance(&transport, 0.25) == TIMELINE_STATUS_OK);
  assert_true("preview_transport_paused_sample",
              PreviewTransportCurrentSample(&transport, &sample, &direction) ==
                      TIMELINE_STATUS_OK &&
                  sample.absolute_frame == 5 &&
                  sample.subframe_numerator == 0u &&
                  sample.subframe_denominator == 1u &&
                  direction == PREVIEW_TRANSPORT_DIRECTION_FORWARD);
}

static void test_preview_transport_loop_endpoints_and_direction(void) {
  PreviewTransport transport = preview_transport_fixture();
  TimelineSample sample = {0};
  PreviewTransportDirection direction = PREVIEW_TRANSPORT_DIRECTION_FORWARD;

  assert_true("preview_transport_loop_play",
              PreviewTransportPlay(&transport) == TIMELINE_STATUS_OK);
  assert_true("preview_transport_loop_forward_wrap",
              PreviewTransportAdvance(&transport, 0.4) == TIMELINE_STATUS_OK);
  (void)PreviewTransportCurrentSample(&transport, &sample, &direction);
  assert_true("preview_transport_loop_forward_start",
              sample.absolute_frame == 5 && sample.subframe_numerator == 0u &&
                  direction == PREVIEW_TRANSPORT_DIRECTION_FORWARD);

  assert_true("preview_transport_loop_reverse",
              PreviewTransportSetDirection(
                  &transport, PREVIEW_TRANSPORT_DIRECTION_REVERSE) ==
                  TIMELINE_STATUS_OK);
  assert_true("preview_transport_loop_reverse_wrap",
              PreviewTransportAdvance(&transport, 0.1) == TIMELINE_STATUS_OK);
  (void)PreviewTransportCurrentSample(&transport, &sample, &direction);
  assert_true("preview_transport_loop_reverse_end",
              sample.absolute_frame == 8 && sample.subframe_numerator == 0u &&
                  direction == PREVIEW_TRANSPORT_DIRECTION_REVERSE);
}

static void test_preview_transport_bounce_endpoints(void) {
  PreviewTransport transport = preview_transport_fixture();
  TimelineSample sample = {0};
  PreviewTransportDirection direction = PREVIEW_TRANSPORT_DIRECTION_FORWARD;

  assert_true(
      "preview_transport_bounce_mode",
      PreviewTransportSetMode(&transport, PREVIEW_TRANSPORT_MODE_BOUNCE) ==
          TIMELINE_STATUS_OK);
  assert_true("preview_transport_bounce_play",
              PreviewTransportPlay(&transport) == TIMELINE_STATUS_OK);
  assert_true("preview_transport_bounce_reach_end",
              PreviewTransportAdvance(&transport, 0.3) == TIMELINE_STATUS_OK);
  (void)PreviewTransportCurrentSample(&transport, &sample, &direction);
  assert_true("preview_transport_bounce_end_reverses",
              sample.absolute_frame == 8 && sample.subframe_numerator == 0u &&
                  direction == PREVIEW_TRANSPORT_DIRECTION_REVERSE);

  assert_true("preview_transport_bounce_reverse_mid",
              PreviewTransportAdvance(&transport, 0.15) == TIMELINE_STATUS_OK);
  (void)PreviewTransportCurrentSample(&transport, &sample, &direction);
  assert_true("preview_transport_bounce_reverse_fraction",
              sample.absolute_frame == 6 &&
                  sample.subframe_numerator == 500000u &&
                  sample.subframe_denominator == 1000000u &&
                  direction == PREVIEW_TRANSPORT_DIRECTION_REVERSE);

  assert_true("preview_transport_bounce_reach_start",
              PreviewTransportAdvance(&transport, 0.15) == TIMELINE_STATUS_OK);
  (void)PreviewTransportCurrentSample(&transport, &sample, &direction);
  assert_true("preview_transport_bounce_start_forwards",
              sample.absolute_frame == 5 && sample.subframe_numerator == 0u &&
                  direction == PREVIEW_TRANSPORT_DIRECTION_FORWARD);
}

static void test_preview_transport_exact_seek_and_resume(void) {
  PreviewTransport transport = preview_transport_fixture();
  TimelineSample exact = {6, 1u, 3u};
  TimelineSample sample = {0};
  PreviewTransportDirection direction = PREVIEW_TRANSPORT_DIRECTION_FORWARD;

  assert_true("preview_transport_exact_seek",
              PreviewTransportSeek(&transport, exact,
                                   PREVIEW_TRANSPORT_DIRECTION_REVERSE) ==
                  TIMELINE_STATUS_OK);
  assert_true("preview_transport_seek_preserves_pause", !transport.playing);
  (void)PreviewTransportCurrentSample(&transport, &sample, &direction);
  assert_true("preview_transport_seek_preserves_rational",
              sample.absolute_frame == exact.absolute_frame &&
                  sample.subframe_numerator == exact.subframe_numerator &&
                  sample.subframe_denominator == exact.subframe_denominator &&
                  direction == PREVIEW_TRANSPORT_DIRECTION_REVERSE);

  assert_true("preview_transport_seek_play",
              PreviewTransportPlay(&transport) == TIMELINE_STATUS_OK);
  assert_true("preview_transport_seek_resume_reverse",
              PreviewTransportAdvance(&transport, 1.0 / 30.0) ==
                  TIMELINE_STATUS_OK);
  (void)PreviewTransportCurrentSample(&transport, &sample, &direction);
  assert_true("preview_transport_seek_resume_sample",
              sample.absolute_frame == 6 && sample.subframe_numerator == 0u &&
                  sample.subframe_denominator == 1000000u &&
                  direction == PREVIEW_TRANSPORT_DIRECTION_REVERSE);
}

static void test_preview_transport_rejects_invalid_without_mutation(void) {
  PreviewTransport transport = preview_transport_fixture();
  PreviewTransport before = transport;

  assert_true("preview_transport_invalid_seek",
              PreviewTransportSeek(&transport, (TimelineSample){9, 0u, 1u},
                                   PREVIEW_TRANSPORT_DIRECTION_FORWARD) ==
                  TIMELINE_STATUS_FRAME_OUT_OF_RANGE);
  assert_true("preview_transport_invalid_seek_nonmutation",
              memcmp(&transport, &before, sizeof(transport)) == 0);
  assert_true("preview_transport_invalid_delta",
              PreviewTransportAdvance(&transport, -0.1) ==
                  TIMELINE_STATUS_INVALID_ARGUMENT);
  assert_true("preview_transport_invalid_delta_nonmutation",
              memcmp(&transport, &before, sizeof(transport)) == 0);
}

int run_test_preview_transport_tests(void) {
  test_preview_transport_defaults_and_pause();
  test_preview_transport_loop_endpoints_and_direction();
  test_preview_transport_bounce_endpoints();
  test_preview_transport_exact_seek_and_resume();
  test_preview_transport_rejects_invalid_without_mutation();
  return test_support_failures();
}
