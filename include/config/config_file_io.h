#ifndef CONFIG_FILE_IO_H
#define CONFIG_FILE_IO_H

#include <stdbool.h>
#include <stdio.h>
#include <json-c/json.h>

bool config_io_directory_exists(const char* path);
bool config_io_ensure_directory_exists(const char* path);
bool config_io_ensure_parent_directory_for_file(const char* path);

FILE* config_io_open_read_with_fallback(const char* primary,
                                        const char* fallback,
                                        const char* legacy,
                                        const char** selected_path);

struct json_object* config_io_parse_json_file(FILE* file,
                                              const char* lane_name,
                                              bool warn_on_empty);

#endif
