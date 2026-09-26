#import <AppKit/AppKit.h>

#include "platform/ray_tracing_macos_file_panel.h"
#include <string.h>

RayTracingFolderPickerResult RayTracing_MacOSFilePanelSelect(const char *prompt,
                                                             const char *initial_directory,
                                                             char *out_path,
                                                             size_t out_path_size) {
    if (!out_path || out_path_size == 0) return RAY_TRACING_FOLDER_PICKER_FAILED;
    out_path[0] = '\0';
    @autoreleasepool {
        NSOpenPanel *panel = [NSOpenPanel openPanel];
        [panel setCanChooseFiles:YES];
        [panel setCanChooseDirectories:NO];
        [panel setAllowsMultipleSelection:NO];
        if (prompt && prompt[0]) [panel setMessage:[NSString stringWithUTF8String:prompt]];
        if (initial_directory && initial_directory[0]) {
            NSString *path = [NSString stringWithUTF8String:initial_directory];
            if (path) [panel setDirectoryURL:[NSURL fileURLWithPath:path isDirectory:YES]];
        }
        [[NSApplication sharedApplication] activateIgnoringOtherApps:YES];
        if ([panel runModal] != NSModalResponseOK) return RAY_TRACING_FOLDER_PICKER_CANCELLED;
        const char *path = [[[panel URLs] firstObject] path].UTF8String;
        if (!path || !path[0]) return RAY_TRACING_FOLDER_PICKER_FAILED;
        size_t length = strlen(path);
        if (length >= out_path_size) return RAY_TRACING_FOLDER_PICKER_FAILED;
        memcpy(out_path, path, length + 1);
        return RAY_TRACING_FOLDER_PICKER_SELECTED;
    }
}
