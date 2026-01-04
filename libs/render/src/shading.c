#include "shading.h"

#include "logger/log.h"

void render_shading(
    PdfShadingDict* shading_dict,
    GeomMat3 ctm,
    Canvas* canvas
) {
    (void)ctm;

    RELEASE_ASSERT(shading_dict);
    RELEASE_ASSERT(canvas);

    switch (shading_dict->shading_type) {
        case 3: {
            break;
        }
        default: {
            LOG_TODO("Render shading type %d", shading_dict->shading_type);
        }
    }
}
