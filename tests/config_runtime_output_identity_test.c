#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "config/core/config_output_identity.h"

int main(void) {
    char reconciled[256] = "unchanged";

    assert(!config_output_identity_reconcile_root(
        "/Users/test/frames/dragon_bunny/v4",
        "/Users/test/frames/dragon_bunny/v4/prev/",
        reconciled,
        sizeof(reconciled)));
    assert(strcmp(reconciled, "unchanged") == 0);

    assert(config_output_identity_reconcile_root(
        "/Users/test/frames/dragon_bunny/v3/qual",
        "/Users/test/frames/dragon_bunny/v4/prev/",
        reconciled,
        sizeof(reconciled)));
    assert(strcmp(reconciled, "/Users/test/frames/dragon_bunny/v4") == 0);

    assert(!config_output_identity_reconcile_root(
        "data/runtime",
        "data/runtime/frames/default",
        reconciled,
        sizeof(reconciled)));

    assert(!config_output_identity_reconcile_root(
        "/Users/test/frames/dragon_bunny/v3",
        "/Users/test/frames/../escape",
        reconciled,
        sizeof(reconciled)));

    puts("config runtime output identity: PASS");
    return 0;
}
