#include "shading.h"

#include <math.h>

#include "canvas/canvas.h"
#include "geom/mat3.h"
#include "geom/rect.h"
#include "geom/vec2.h"
#include "logger/log.h"
#include "pdf/shading.h"
#include "pdf/types.h"

void render_shading(
    PdfShadingDict* shading_dict,
    GeomMat3 ctm,
    Canvas* canvas
) {
    (void)ctm;

    RELEASE_ASSERT(shading_dict);
    RELEASE_ASSERT(canvas);

    switch (shading_dict->shading_type) {
        case 2: {
            break;
        }
        case 3: {
            PdfShadingDictType3 radial = shading_dict->data.type3;

            GeomVec2 p0 = geom_vec2_new(
                pdf_number_as_real(radial.coords[0]),
                pdf_number_as_real(radial.coords[1])
            );
            PdfReal r0 = pdf_number_as_real(radial.coords[2]);
            GeomRect b0 = geom_rect_new_centered(p0, geom_vec2_new(r0, r0));

            GeomVec2 p1 = geom_vec2_new(
                pdf_number_as_real(radial.coords[3]),
                pdf_number_as_real(radial.coords[4])
            );
            PdfReal r1 = pdf_number_as_real(radial.coords[5]);
            GeomRect b1 = geom_rect_new_centered(p1, geom_vec2_new(r1, r1));

            GeomRect bbox = geom_rect_union(b0, b1);
            if (shading_dict->bbox.is_some) {
                bbox = geom_rect_intersection(
                    bbox,
                    pdf_rectangle_to_geom(shading_dict->bbox.value)
                );
            }

            bbox = geom_rect_transform(bbox, ctm);
            if (canvas_is_raster(canvas)) {
                bbox = geom_rect_round(bbox);
            }

            // GeomMat3 inv_ctm = geom_mat3_inverse(ctm);

            double step = canvas_raster_res(canvas);
            size_t n_x = (size_t)ceil((bbox.max.x - bbox.min.x) / step);
            size_t n_y = (size_t)ceil((bbox.max.y - bbox.min.y) / step);

            for (size_t idx_x = 0; idx_x < n_x; idx_x++) {
                double canvas_x = bbox.min.x + (double)idx_x * step;
                if (!(canvas_x < bbox.max.x)) {
                    break;
                }

                for (size_t idx_y = 0; idx_y < n_y; idx_y++) {
                    double canvas_y = bbox.min.y + (double)idx_y * step;
                    if (!(canvas_y < bbox.max.y)) {
                        break;
                    }

                    canvas_draw_pixel(
                        canvas,
                        geom_vec2_new(canvas_x, canvas_y),
                        0xff00ffff
                    );
                }
            }

            break;
        }
        case 7: {
            break;
        }
        default: {
            LOG_TODO("Render shading type %d", shading_dict->shading_type);
        }
    }
}
