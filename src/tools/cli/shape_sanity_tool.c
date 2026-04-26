#include <stdio.h>
#include <stdlib.h>

#include "../../../third_party/codework_shared/shape/tests/shape_sanity.h"
#include "../../../third_party/codework_shared/shape/tests/shape_sanity.c"
#include "../../../third_party/codework_shared/shape/geo/shape_library.c"
#include "../../../third_party/codework_shared/shape/geo/shape_asset.c"
#include "../../../third_party/codework_shared/shape/ShapeLib/shape_core.c"
#include "../../../third_party/codework_shared/shape/ShapeLib/shape_flatten.c"
#include "../../../third_party/codework_shared/shape/ShapeLib/shape_json.c"
#include "../../../third_party/codework_shared/shape/external/cjson/cJSON.c"

int main(int argc, char **argv) {
    const char *dir = NULL;
    if (argc > 1) {
        dir = argv[1];
    } else {
        dir = getenv("SHAPE_ASSET_DIR");
    }
    int rc = shape_sanity_check(dir, stdout);
    if (rc != 0) {
        fprintf(stderr, "shape_sanity_check failed (code %d)\n", rc);
    }
    return rc;
}
