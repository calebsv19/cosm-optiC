#include "settings_loader.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


#include "../../external/cJSON.h"  // You’ll need to include this in your project

TimeScopeSettings ts_settings = {
    .hud_enabled = true,
    .log_enabled = true,
    .event_tagging_enabled = false,

    .timer_buffer_size = 128,

    .log_filepath = "timing.json",
    .log_format = "json",

    .render_mode = "throttled",       // default behavior: throttle render rate
    .render_threshold = 0.033f        // ~30 FPS (in seconds)
};


void save_settings_to_file(const char* path) {
    FILE* f = fopen(path, "w");
    if (!f) return;

    fprintf(f, "{\n");
    fprintf(f, "  \"hud_enabled\": %s,\n", ts_settings.hud_enabled ? "true" : "false");
    fprintf(f, "  \"log_enabled\": %s,\n", ts_settings.log_enabled ? "true" : "false");
    fprintf(f, "  \"event_tagging_enabled\": %s,\n", ts_settings.event_tagging_enabled ? "true" : "false");
    fprintf(f, "  \"timer_buffer_size\": %d,\n", ts_settings.timer_buffer_size);
    fprintf(f, "  \"log_filepath\": \"%s\",\n", ts_settings.log_filepath);
    fprintf(f, "  \"log_format\": \"%s\",\n", ts_settings.log_format);
    fprintf(f, "  \"render_mode\": \"%s\",\n", ts_settings.render_mode);
    fprintf(f, "  \"render_threshold\": %.3f,\n", ts_settings.render_threshold);

    // NEW HUD FIELDS
    fprintf(f, "  \"hud_position\": \"%s\",\n", ts_settings.hud_position);
    fprintf(f, "  \"hud_offset_x\": %d,\n", ts_settings.hud_offset_x);
    fprintf(f, "  \"hud_offset_y\": %d\n", ts_settings.hud_offset_y);

    fprintf(f, "}\n");

    fclose(f);
}


bool ts_load_settings(const char* filepath) {
    FILE* f = fopen(filepath, "r");
    if (!f) {
        fprintf(stderr, "[TimeScope] Failed to open settings file: %s\n", filepath);
        return false;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* buffer = (char*)malloc(size + 1);
    fread(buffer, 1, size, f);
    buffer[size] = '\0';
    fclose(f);


     cJSON* root = cJSON_Parse(buffer);
     if (!root) {
	    const char* error_ptr = cJSON_GetErrorPtr();
	    fprintf(stderr, "[TimeScope] Failed to parse JSON settings.\n");
	    if (error_ptr) {
	        fprintf(stderr, "  Error before: %.20s\n", error_ptr);
	    }
	
	    fprintf(stderr, "[TimeScope] Full contents:\n%s\n", buffer);
	    free(buffer);
	    return false;
    }
    free(buffer);


    cJSON* val;
    if ((val = cJSON_GetObjectItem(root, "hud_enabled"))) {
        ts_settings.hud_enabled = cJSON_IsTrue(val);
    }
    if ((val = cJSON_GetObjectItem(root, "log_enabled"))) {
        ts_settings.log_enabled = cJSON_IsTrue(val);
    }
    if ((val = cJSON_GetObjectItem(root, "event_tagging_enabled"))) {
        ts_settings.event_tagging_enabled = cJSON_IsTrue(val);
    }
    if ((val = cJSON_GetObjectItem(root, "timer_buffer_size"))) {
        ts_settings.timer_buffer_size = val->valueint;
    }
    if ((val = cJSON_GetObjectItem(root, "log_filepath")) && cJSON_IsString(val)) {
        strncpy(ts_settings.log_filepath, val->valuestring, sizeof(ts_settings.log_filepath));
    }
    if ((val = cJSON_GetObjectItem(root, "log_format")) && cJSON_IsString(val)) {
        strncpy(ts_settings.log_format, val->valuestring, sizeof(ts_settings.log_format));
    }
    if ((val = cJSON_GetObjectItem(root, "render_mode")) && cJSON_IsString(val)) {
        strncpy(ts_settings.render_mode, val->valuestring, sizeof(ts_settings.render_mode));
    }
    if ((val = cJSON_GetObjectItem(root, "render_threshold"))) {
        ts_settings.render_threshold = (float)val->valuedouble;
    }

    // NEW HUD POSITION CONFIG
    if ((val = cJSON_GetObjectItem(root, "hud_position")) && cJSON_IsString(val)) {
        strncpy(ts_settings.hud_position, val->valuestring, sizeof(ts_settings.hud_position));
    } else {
        strncpy(ts_settings.hud_position, "top-left", sizeof(ts_settings.hud_position));
    }

    if ((val = cJSON_GetObjectItem(root, "hud_offset_x"))) {
        ts_settings.hud_offset_x = val->valueint;
    } else {
        ts_settings.hud_offset_x = 10;
    }

    if ((val = cJSON_GetObjectItem(root, "hud_offset_y"))) {
        ts_settings.hud_offset_y = val->valueint;
    } else {
        ts_settings.hud_offset_y = 10;
    }


    printf("[TimeScope] HUD Position: %s (%d, %d)\n",
	    ts_settings.hud_position,
	    ts_settings.hud_offset_x,
	    ts_settings.hud_offset_y);


    cJSON_Delete(root);
    return true;
}



