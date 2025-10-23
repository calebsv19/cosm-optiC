#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include "app/animation.h"  // Include settings reference
#include "config/config_manager.h"

int RenumberFrames(const char *frameDir) {
    char command[512];
    snprintf(command, sizeof(command),
             "cd %s && ls frame_*.bmp | awk 'BEGIN{count=0} {printf \"mv %%s frame_%%04d.bmp\\n\", $1, count++;}' | bash",
		frameDir);
    return system(command);
}

int MakeVideo(const char *outputFile) {
    printf("Checking frames directory: %s\n", animSettings.frameDir);

    struct stat st;
    if (stat(animSettings.frameDir, &st) != 0) {
        fprintf(stderr, "Error: Frames directory '%s' does not exist.\n", animSettings.frameDir);
        return 1;
    }

    printf("Ensuring frames are sequentially numbered...\n");
    if (RenumberFrames(animSettings.frameDir) != 0) {
        fprintf(stderr, "Error: Failed to rename frames.\n");
        return 1;
    }

    printf("Generating video at resolution: %dx%d, FPS: %d\n",
           sceneSettings.windowWidth, sceneSettings.windowHeight, animSettings.fps);

    // Execute FFmpeg command
    char command[512];
    snprintf(command, sizeof(command),
             "ffmpeg -start_number 0 -framerate %d -i %s/frame_%%04d.bmp -s %dx%d -c:v libx264 -pix_fmt yuv420p %s",
             animSettings.fps, animSettings.frameDir,
             sceneSettings.windowWidth, sceneSettings.windowHeight,  // Ensure correct output resolution
             outputFile);

    printf("Executing command: %s\n", command);
    int ret = system(command);
    if (ret != 0) {
        fprintf(stderr, "FFmpeg command failed with return code %d\n", ret);
        return ret;
    }

    printf("✅ Video successfully created: %s\n", outputFile);
    return 0;
}

