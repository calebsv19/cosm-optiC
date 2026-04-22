#ifndef CONFIG_RUNTIME_PATHS_H
#define CONFIG_RUNTIME_PATHS_H

#include <stdbool.h>
#include <stddef.h>

void config_runtime_paths_normalize_frame_dir(void);
void config_runtime_paths_normalize_data_roots(void);
bool config_runtime_paths_validate_root(char *target,
                                        size_t target_size,
                                        const char *default_path,
                                        const char *label,
                                        bool is_output_root,
                                        bool create_if_missing);

#endif
