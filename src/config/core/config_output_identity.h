#ifndef CONFIG_OUTPUT_IDENTITY_H
#define CONFIG_OUTPUT_IDENTITY_H

#include <stdbool.h>
#include <stddef.h>

bool config_output_identity_reconcile_root(const char *output_root,
                                           const char *frame_directory,
                                           char *reconciled_root,
                                           size_t reconciled_root_size);

#endif
