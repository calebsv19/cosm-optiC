#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "app/ray_tracing_durable_io.h"
#include "app/ray_tracing_frame_recovery.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int failures = 0;

static void expect_true(const char *label, bool value) {
    if (value) return;
    fprintf(stderr, "FAIL: %s\n", label);
    failures += 1;
}

static void write_le16(unsigned char *bytes, unsigned int value) {
    bytes[0] = (unsigned char)(value & 0xffu);
    bytes[1] = (unsigned char)((value >> 8u) & 0xffu);
}

static void write_le32(unsigned char *bytes, unsigned int value) {
    bytes[0] = (unsigned char)(value & 0xffu);
    bytes[1] = (unsigned char)((value >> 8u) & 0xffu);
    bytes[2] = (unsigned char)((value >> 16u) & 0xffu);
    bytes[3] = (unsigned char)((value >> 24u) & 0xffu);
}

static bool write_test_bmp(const char *path) {
    unsigned char bmp[70];
    RayTracingDurableOutput output;
    memset(bmp, 0, sizeof(bmp));
    bmp[0] = 'B';
    bmp[1] = 'M';
    write_le32(&bmp[2], sizeof(bmp));
    write_le32(&bmp[10], 54u);
    write_le32(&bmp[14], 40u);
    write_le32(&bmp[18], 2u);
    write_le32(&bmp[22], 2u);
    write_le16(&bmp[26], 1u);
    write_le16(&bmp[28], 24u);
    write_le32(&bmp[34], 16u);
    for (size_t index = 54u; index < sizeof(bmp); ++index) {
        bmp[index] = (unsigned char)index;
    }
    if (!ray_tracing_durable_output_begin(&output, path)) return false;
    if (fwrite(bmp, 1u, sizeof(bmp), output.stream) != sizeof(bmp)) {
        ray_tracing_durable_output_abort(&output);
        return false;
    }
    return ray_tracing_durable_output_commit(&output);
}

static bool read_text(const char *path, char *out, size_t out_size) {
    FILE *file = NULL;
    size_t count = 0u;
    if (!path || !out || out_size == 0u) return false;
    file = fopen(path, "rb");
    if (!file) return false;
    count = fread(out, 1u, out_size - 1u, file);
    out[count] = '\0';
    return fclose(file) == 0;
}

int main(void) {
    char root_template[] = "/tmp/ray_tracing_durable_frame_recovery_XXXXXX";
    char *root = mkdtemp(root_template);
    char output_root[PATH_MAX];
    char frames_root[PATH_MAX];
    char frame0[PATH_MAX];
    char frame1[PATH_MAX];
    char frame2[PATH_MAX];
    char state_path[PATH_MAX];
    char text[32];
    RayTracingDurableOutput output;
    RayTracingFrameRecoveryScan scan;

    expect_true("temporary root created", root != NULL);
    if (!root) return 1;
    snprintf(output_root, sizeof(output_root), "%s/output", root);
    snprintf(frames_root, sizeof(frames_root), "%s/frames", output_root);
    snprintf(frame0, sizeof(frame0), "%s/frame_0000.bmp", frames_root);
    snprintf(frame1, sizeof(frame1), "%s/frame_0001.bmp", frames_root);
    snprintf(frame2, sizeof(frame2), "%s/frame_0002.bmp", frames_root);
    snprintf(state_path, sizeof(state_path), "%s/state.json", root);
    expect_true("output directory created", mkdir(output_root, 0700) == 0);
    expect_true("frames directory created", mkdir(frames_root, 0700) == 0);

    expect_true("frame zero durable write", write_test_bmp(frame0));
    expect_true("frame zero validates", ray_tracing_durable_validate_bmp(frame0, 2, 2));
    expect_true("frame two durable write", write_test_bmp(frame2));
    expect_true("noncontiguous scan succeeds",
                ray_tracing_frame_recovery_scan(output_root, 0, 3, 2, 2, &scan));
    expect_true("noncontiguous scan keeps one-frame prefix", scan.durable_prefix_count == 1);
    expect_true("noncontiguous frame detected", scan.first_noncontiguous_frame == 2);

    {
        FILE *corrupt = fopen(frame1, "wb");
        expect_true("corrupt frame opened", corrupt != NULL);
        if (corrupt) {
            expect_true("corrupt frame written", fputs("BM", corrupt) >= 0);
            expect_true("corrupt frame closed", fclose(corrupt) == 0);
        }
    }
    expect_true("invalid scan succeeds",
                ray_tracing_frame_recovery_scan(output_root, 0, 3, 2, 2, &scan));
    expect_true("invalid frame detected", scan.first_invalid_frame == 1);
    expect_true("invalid frame not accepted", !ray_tracing_durable_validate_bmp(frame1, 2, 2));

    expect_true("frame one replaced durably", write_test_bmp(frame1));
    expect_true("contiguous scan succeeds",
                ray_tracing_frame_recovery_scan(output_root, 0, 3, 2, 2, &scan));
    expect_true("all three frames accepted", scan.durable_prefix_count == 3);
    expect_true("resume advances after frame two", scan.resume_from_frame == 3);

    expect_true("state output begins", ray_tracing_durable_output_begin(&output, state_path));
    expect_true("state old value written", fputs("old\n", output.stream) >= 0);
    expect_true("state old value committed", ray_tracing_durable_output_commit(&output));
    expect_true("replacement output begins", ray_tracing_durable_output_begin(&output, state_path));
    expect_true("replacement value written", fputs("new\n", output.stream) >= 0);
    ray_tracing_durable_output_abort(&output);
    expect_true("aborted replacement preserves target", read_text(state_path, text, sizeof(text)));
    expect_true("aborted replacement retained old value", strcmp(text, "old\n") == 0);

    expect_true("committed replacement begins", ray_tracing_durable_output_begin(&output, state_path));
    expect_true("committed replacement written", fputs("new\n", output.stream) >= 0);
    expect_true("committed replacement completes", ray_tracing_durable_output_commit(&output));
    expect_true("committed replacement readable", read_text(state_path, text, sizeof(text)));
    expect_true("committed replacement visible", strcmp(text, "new\n") == 0);

    (void)unlink(frame0);
    (void)unlink(frame1);
    (void)unlink(frame2);
    (void)unlink(state_path);
    (void)rmdir(frames_root);
    (void)rmdir(output_root);
    (void)rmdir(root);

    if (failures != 0) {
        fprintf(stderr, "%d durable frame recovery assertion(s) failed\n", failures);
        return 1;
    }
    printf("ray tracing durable frame recovery contract passed\n");
    return 0;
}
