#include "shading.h"

#include <math.h>
#include <string.h>

#include "arena/arena.h"
#include "canvas/canvas.h"
#include "color/rgb.h"
#include "err/error.h"
#include "geom/mat3.h"
#include "geom/rect.h"
#include "geom/vec2.h"
#include "geom/vec3.h"
#include "logger/log.h"
#include "pdf/function.h"
#include "pdf/object.h"
#include "pdf/shading.h"
#include "pdf/types.h"

static double clamp01(double value) {
    if (value < 0.0) {
        return 0.0;
    }
    if (value > 1.0) {
        return 1.0;
    }
    return value;
}

static Error* eval_shading_function(
    PdfFunctionVec* functions,
    PdfReal input,
    Arena* arena,
    PdfObjectVec* io,
    PdfObjectVec* scratch_outputs
) {
    RELEASE_ASSERT(functions);
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(io);
    RELEASE_ASSERT(scratch_outputs);

    size_t function_count = pdf_function_vec_len(functions);
    if (function_count == 0) {
        return ERROR(PDF_ERR_INCORRECT_TYPE);
    }

    if (function_count == 1) {
        PdfFunction function;
        RELEASE_ASSERT(pdf_function_vec_get(functions, 0, &function));

        pdf_object_vec_clear(io);
        pdf_object_vec_push(
            io,
            (PdfObject) {.type = PDF_OBJECT_TYPE_REAL, .data.real = input}
        );
        TRY(pdf_run_function(&function, arena, io));

        if (pdf_object_vec_len(io) == 0) {
            return ERROR(PDF_ERR_INCORRECT_TYPE);
        }
        return NULL;
    }

    pdf_object_vec_clear(scratch_outputs);

    for (size_t idx = 0; idx < function_count; idx++) {
        PdfFunction function;
        RELEASE_ASSERT(pdf_function_vec_get(functions, idx, &function));

        pdf_object_vec_clear(io);
        pdf_object_vec_push(
            io,
            (PdfObject) {.type = PDF_OBJECT_TYPE_REAL, .data.real = input}
        );
        TRY(pdf_run_function(&function, arena, io));

        if (pdf_object_vec_len(io) != 1) {
            return ERROR(PDF_ERR_INCORRECT_TYPE);
        }

        PdfObject output;
        RELEASE_ASSERT(pdf_object_vec_get(io, 0, &output));
        pdf_object_vec_push(scratch_outputs, output);
    }

    pdf_object_vec_clear(io);
    for (size_t idx = 0; idx < function_count; idx++) {
        PdfObject output;
        RELEASE_ASSERT(pdf_object_vec_get(scratch_outputs, idx, &output));
        pdf_object_vec_push(io, output);
    }

    return NULL;
}

static bool solve_radial_t(
    GeomVec2 point,
    GeomVec2 c0,
    PdfReal r0,
    GeomVec2 c1,
    PdfReal r1,
    const PdfBoolean extend[2],
    PdfReal* out_t
) {
    RELEASE_ASSERT(out_t);

    const double eps = 1e-9;

    double dc_x = c1.x - c0.x;
    double dc_y = c1.y - c0.y;
    double dr = r1 - r0;
    double c0p_x = c0.x - point.x;
    double c0p_y = c0.y - point.y;

    double a = dc_x * dc_x + dc_y * dc_y - dr * dr;
    double b = 2.0 * (c0p_x * dc_x + c0p_y * dc_y - r0 * dr);
    double c = c0p_x * c0p_x + c0p_y * c0p_y - r0 * r0;

    PdfReal roots[2];
    size_t n_roots = 0;

    if (fabs(a) < eps) {
        if (fabs(b) < eps) {
            return false;
        }
        roots[0] = -c / b;
        n_roots = 1;
    } else {
        double disc = b * b - 4.0 * a * c;
        if (disc < -eps) {
            return false;
        }

        if (disc < 0.0) {
            disc = 0.0;
        }

        double sqrt_disc = sqrt(disc);
        double inv_den = 1.0 / (2.0 * a);
        roots[0] = (-b - sqrt_disc) * inv_den;
        roots[1] = (-b + sqrt_disc) * inv_den;
        n_roots = 2;
    }

    bool has_inside = false;
    PdfReal inside_t = 0.0;
    for (size_t idx = 0; idx < n_roots; idx++) {
        PdfReal t = roots[idx];
        if (t >= -eps && t <= 1.0 + eps) {
            PdfReal clamped_t = fmin(1.0, fmax(0.0, t));
            if (!has_inside || clamped_t > inside_t) {
                has_inside = true;
                inside_t = clamped_t;
            }
        }
    }

    if (has_inside) {
        *out_t = inside_t;
        return true;
    }

    // No boundary crossing inside [0, 1]. If the point is in the end circle,
    // it is inside every circle over the blend interval, so the latest paint
    // sample is t=1.
    double end_value = a + b + c;
    if (end_value <= eps) {
        *out_t = 1.0;
        return true;
    }

    bool has_extended = false;
    PdfReal extended_t = 0.0;
    double extended_dist = 0.0;
    for (size_t idx = 0; idx < n_roots; idx++) {
        PdfReal t = roots[idx];
        bool eligible = false;
        double dist = 0.0;

        if (t < 0.0 && extend[0]) {
            eligible = true;
            dist = -t;
        } else if (t > 1.0 && extend[1]) {
            eligible = true;
            dist = t - 1.0;
        }

        if (!eligible) {
            continue;
        }

        if (!has_extended || dist < extended_dist) {
            has_extended = true;
            extended_t = t;
            extended_dist = dist;
        }
    }

    if (has_extended) {
        *out_t = extended_t;
        return true;
    }

    return false;
}

static Error* shading_component_or_default(
    const PdfObjectVec* components,
    size_t idx,
    PdfReal default_value,
    PdfReal* out
) {
    RELEASE_ASSERT(components);
    RELEASE_ASSERT(out);

    if (idx >= pdf_object_vec_len(components)) {
        *out = default_value;
        return NULL;
    }

    PdfObject component;
    RELEASE_ASSERT(pdf_object_vec_get(components, idx, &component));

    PdfNumber number;
    TRY(pdf_deserde_number(&component, &number, NULL));
    *out = pdf_number_as_real(number);
    return NULL;
}

static Error* shading_components_to_rgb(
    const PdfShadingDict* shading_dict,
    const PdfObjectVec* components,
    GeomVec3* out_rgb
) {
    RELEASE_ASSERT(shading_dict);
    RELEASE_ASSERT(components);
    RELEASE_ASSERT(out_rgb);

    size_t n_components = pdf_object_vec_len(components);

    switch (shading_dict->color_space.family) {
        case PDF_COLOR_SPACE_DEVICE_GRAY: {
            PdfReal gray = 0.0;
            TRY(shading_component_or_default(components, 0, 0.0, &gray));
            gray = clamp01(gray);
            *out_rgb = geom_vec3_new(gray, gray, gray);
            return NULL;
        }
        case PDF_COLOR_SPACE_DEVICE_RGB:
        case PDF_COLOR_SPACE_CAL_RGB: {
            PdfReal r = 0.0;
            PdfReal g = 0.0;
            PdfReal b = 0.0;
            TRY(shading_component_or_default(components, 0, 0.0, &r));
            TRY(shading_component_or_default(components, 1, 0.0, &g));
            TRY(shading_component_or_default(components, 2, 0.0, &b));
            *out_rgb = geom_vec3_new(clamp01(r), clamp01(g), clamp01(b));
            return NULL;
        }
        case PDF_COLOR_SPACE_DEVICE_CMYK: {
            PdfReal c = 0.0;
            PdfReal m = 0.0;
            PdfReal y = 0.0;
            PdfReal k = 0.0;
            TRY(shading_component_or_default(components, 0, 0.0, &c));
            TRY(shading_component_or_default(components, 1, 0.0, &m));
            TRY(shading_component_or_default(components, 2, 0.0, &y));
            TRY(shading_component_or_default(components, 3, 0.0, &k));
            c = clamp01(c);
            m = clamp01(m);
            y = clamp01(y);
            k = clamp01(k);
            *out_rgb = geom_vec3_new(
                (1.0 - c) * (1.0 - k),
                (1.0 - m) * (1.0 - k),
                (1.0 - y) * (1.0 - k)
            );
            return NULL;
        }
        case PDF_COLOR_SPACE_DEVICE_N: {
            PdfReal tint = 0.0;
            TRY(shading_component_or_default(components, 0, 0.0, &tint));
            tint = clamp01(tint);

            PdfReal c = 0.0;
            PdfReal m = 0.0;
            PdfReal y = 0.0;
            PdfReal k = 0.0;

            PdfName colorant = NULL;
            PdfNameVec* names = shading_dict->color_space.params.device_n.names;
            if (names != NULL && pdf_name_vec_len(names) > 0) {
                RELEASE_ASSERT(pdf_name_vec_get(names, 0, &colorant));
            }

            if (colorant != NULL && strcmp(colorant, "Cyan") == 0) {
                c = tint;
            } else if (colorant != NULL && strcmp(colorant, "Magenta") == 0) {
                m = tint;
            } else if (colorant != NULL && strcmp(colorant, "Yellow") == 0) {
                y = tint;
            } else if (colorant != NULL && strcmp(colorant, "Black") == 0) {
                k = tint;
            } else {
                PdfReal gray = 1.0 - tint;
                *out_rgb = geom_vec3_new(gray, gray, gray);
                return NULL;
            }

            *out_rgb = geom_vec3_new(
                (1.0 - c) * (1.0 - k),
                (1.0 - m) * (1.0 - k),
                (1.0 - y) * (1.0 - k)
            );
            return NULL;
        }
        default: {
            if (n_components >= 3) {
                PdfReal r = 0.0;
                PdfReal g = 0.0;
                PdfReal b = 0.0;
                TRY(shading_component_or_default(components, 0, 0.0, &r));
                TRY(shading_component_or_default(components, 1, 0.0, &g));
                TRY(shading_component_or_default(components, 2, 0.0, &b));
                *out_rgb = geom_vec3_new(clamp01(r), clamp01(g), clamp01(b));
                return NULL;
            }
            if (n_components >= 1) {
                PdfReal gray = 0.0;
                TRY(shading_component_or_default(components, 0, 0.0, &gray));
                gray = clamp01(gray);
                *out_rgb = geom_vec3_new(gray, gray, gray);
                return NULL;
            }
            *out_rgb = geom_vec3_new(0.0, 0.0, 0.0);
            return NULL;
        }
    }
}

void render_shading(
    PdfShadingDict* shading_dict,
    Arena* arena,
    GeomMat3 ctm,
    Canvas* canvas
) {
    RELEASE_ASSERT(shading_dict);
    RELEASE_ASSERT(arena);
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
            if ((radial.extend[0] || radial.extend[1])
                && shading_dict->bbox.is_some) {
                bbox = pdf_rectangle_to_geom(shading_dict->bbox.value);
            } else if (shading_dict->bbox.is_some) {
                bbox = geom_rect_intersection(
                    bbox,
                    pdf_rectangle_to_geom(shading_dict->bbox.value)
                );
            }

            bbox = geom_rect_transform(bbox, ctm);
            if (canvas_is_raster(canvas)) {
                bbox = geom_rect_round(bbox);
            }

            if (!geom_rect_positive(bbox)) {
                break;
            }

            GeomMat3 inv_ctm = geom_mat3_inverse(ctm);
            PdfReal domain_min = pdf_number_as_real(radial.domain[0]);
            PdfReal domain_max = pdf_number_as_real(radial.domain[1]);
            Arena* local_arena = arena_new(1024);
            PdfObjectVec* function_io = pdf_object_vec_new(local_arena);
            PdfObjectVec* function_outputs = pdf_object_vec_new(local_arena);
            Error* render_error = NULL;
            bool abort_render = false;

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

                    GeomVec2 canvas_point = geom_vec2_new(canvas_x, canvas_y);
                    GeomVec2 shading_point =
                        geom_vec2_transform(canvas_point, inv_ctm);

                    PdfReal t_geom = 0.0;
                    if (!solve_radial_t(
                            shading_point,
                            p0,
                            r0,
                            p1,
                            r1,
                            radial.extend,
                            &t_geom
                        )) {
                        continue;
                    }

                    PdfReal t_input =
                        domain_min + t_geom * (domain_max - domain_min);

                    Error* function_error = eval_shading_function(
                        radial.function,
                        t_input,
                        local_arena,
                        function_io,
                        function_outputs
                    );
                    if (function_error) {
                        render_error = function_error;
                        abort_render = true;
                        break;
                    }

                    size_t n_components = pdf_object_vec_len(function_io);
                    if (n_components == 0) {
                        render_error = ERROR(
                            PDF_ERR_INCORRECT_TYPE,
                            "Shading function returned zero output components"
                        );
                        abort_render = true;
                        break;
                    }

                    GeomVec3 rgb;
                    Error* color_error = shading_components_to_rgb(
                        shading_dict,
                        function_io,
                        &rgb
                    );
                    if (color_error) {
                        render_error = color_error;
                        abort_render = true;
                        break;
                    }

                    canvas_draw_pixel(
                        canvas,
                        canvas_point,
                        rgba_new(
                            clamp01(rgb.x),
                            clamp01(rgb.y),
                            clamp01(rgb.z),
                            1.0
                        )
                    );
                }

                if (abort_render) {
                    break;
                }
            }
            arena_free(local_arena);
            if (render_error != NULL) {
                error_print(render_error);
                error_free(render_error);
                return;
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
