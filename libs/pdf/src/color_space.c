#include "pdf/color_space.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "color/icc.h"
#include "color/icc_cache.h"
#include "color/icc_color.h"
#include "err/error.h"
#include "geom/mat3.h"
#include "geom/vec3.h"
#include "logger/log.h"
#include "pdf/deserde.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf/types.h"

static Error* deserde_cal_rgb_params(
    const PdfObject* object,
    PdfCalRGBParams* target_ptr,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(resolver);

    PdfFieldDescriptor fields[] = {
        pdf_geom_vec3_field("WhitePoint", &target_ptr->whitepoint),
        pdf_geom_vec3_optional_field("BlackPoint", &target_ptr->blackpoint),
        pdf_geom_vec3_optional_field("Gamma", &target_ptr->gamma),
        pdf_geom_mat3_optional_field("Matrix", &target_ptr->matrix)
    };

    TRY(pdf_deserde_fields(
        object,
        fields,
        sizeof(fields) / sizeof(PdfFieldDescriptor),
        false,
        resolver,
        "CalRGB"
    ));
    return NULL;
}

Error* pdf_deserde_color_space(
    const PdfObject* object,
    PdfColorSpace* target_ptr,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(resolver);

    PdfObject resolved;
    TRY(pdf_resolve_object(resolver, object, &resolved, true));

    PdfName family = NULL;
    if (resolved.type == PDF_OBJECT_TYPE_NAME) {
        family = resolved.data.name;
    } else if (resolved.type == PDF_OBJECT_TYPE_ARRAY) {
        PdfObject first;
        bool has_first =
            pdf_object_vec_get(resolved.data.array.elements, 0, &first);

        if (!has_first || first.type != PDF_OBJECT_TYPE_NAME) {
            return ERROR(
                PDF_ERR_INCORRECT_TYPE,
                "First element of color space array must be a name"
            );
        }

        family = first.data.name;
    } else {
        return ERROR(
            PDF_ERR_INCORRECT_TYPE,
            "Color space must be a name or array"
        );
    }

    if (strcmp(family, "DeviceGray") == 0) {
        target_ptr->family = PDF_COLOR_SPACE_DEVICE_GRAY;
    } else if (strcmp(family, "DeviceRGB") == 0) {
        target_ptr->family = PDF_COLOR_SPACE_DEVICE_RGB;
    } else if (strcmp(family, "DeviceCMYK") == 0) {
        target_ptr->family = PDF_COLOR_SPACE_DEVICE_CMYK;
    } else if (strcmp(family, "CalGray") == 0) {
        target_ptr->family = PDF_COLOR_SPACE_CAL_GRAY;
    } else if (strcmp(family, "CalRGB") == 0) {
        target_ptr->family = PDF_COLOR_SPACE_CAL_RGB;
    } else if (strcmp(family, "Lab") == 0) {
        target_ptr->family = PDF_COLOR_SPACE_LAB;
    } else if (strcmp(family, "ICCBased") == 0) {
        target_ptr->family = PDF_COLOR_SPACE_ICC_BASED;
    } else if (strcmp(family, "Indexed") == 0) {
        target_ptr->family = PDF_COLOR_SPACE_INDEXED;
    } else if (strcmp(family, "Pattern") == 0) {
        target_ptr->family = PDF_COLOR_SPACE_PATTERN;
    } else if (strcmp(family, "Separation") == 0) {
        target_ptr->family = PDF_COLOR_SPACE_SEPARATION;
    } else if (strcmp(family, "DeviceN") == 0) {
        target_ptr->family = PDF_COLOR_SPACE_DEVICE_N;
    } else {
        return ERROR(
            PDF_ERR_INVALID_SUBTYPE,
            "Unknown color space `%s`",
            family
        );
    }

    switch (target_ptr->family) {
        case PDF_COLOR_SPACE_DEVICE_CMYK: {
            break;
        }
        case PDF_COLOR_SPACE_CAL_RGB: {
            PdfObject second;
            bool has_second =
                pdf_object_vec_get(resolved.data.array.elements, 1, &second);

            if (!has_second) {
                return ERROR(
                    PDF_ERR_INCORRECT_TYPE,
                    "First element of color space array must be a name"
                );
            }

            TRY(deserde_cal_rgb_params(
                &second,
                &target_ptr->params.cal_rgb,
                resolver
            ));

            break;
        }
        case PDF_COLOR_SPACE_ICC_BASED: {
            LOG_WARN(PDF, "TODO: ICC Color spaces");
            break;
        }
        case PDF_COLOR_SPACE_DEVICE_N: {
            if (resolved.type != PDF_OBJECT_TYPE_ARRAY) {
                return ERROR(
                    PDF_ERR_INCORRECT_TYPE,
                    "DeviceN color spaces must be arrays"
                );
            }

            size_t num_elements =
                pdf_object_vec_len(resolved.data.array.elements);
            if (num_elements != 4 && num_elements != 5) {
                return ERROR(
                    PDF_ERR_INCORRECT_TYPE,
                    "DeviceN color spaces must have exactly 4 or 5 elements, "
                    "found %zu",
                    num_elements
                );
            }

            PdfObject names_object;
            RELEASE_ASSERT(pdf_object_vec_get(
                resolved.data.array.elements,
                1,
                &names_object
            ));
            PdfArray names_array;
            TRY(pdf_deserde_array(&names_object, &names_array, resolver));
            if (pdf_object_vec_len(names_array.elements) == 0) {
                return ERROR(
                    PDF_ERR_INCORRECT_TYPE,
                    "DeviceN color space names array must contain at least one "
                    "colorant name"
                );
            }

            target_ptr->params.device_n.names =
                pdf_name_vec_new(pdf_resolver_arena(resolver));
            for (size_t idx = 0; idx < pdf_object_vec_len(names_array.elements);
                 idx++) {
                PdfObject name_object;
                RELEASE_ASSERT(
                    pdf_object_vec_get(names_array.elements, idx, &name_object)
                );

                PdfName name;
                TRY(pdf_deserde_name(&name_object, &name, resolver));
                pdf_name_vec_push(target_ptr->params.device_n.names, name);
            }

            PdfObject alternate_object;
            RELEASE_ASSERT(pdf_object_vec_get(
                resolved.data.array.elements,
                2,
                &alternate_object
            ));

            PdfObject alternate_resolved;
            TRY(pdf_resolve_object(
                resolver,
                &alternate_object,
                &alternate_resolved,
                true
            ));

            if (alternate_resolved.type == PDF_OBJECT_TYPE_NAME) {
                target_ptr->params.device_n.alternate_space =
                    alternate_resolved.data.name;
            } else if (alternate_resolved.type == PDF_OBJECT_TYPE_ARRAY) {
                PdfObject first;
                if (!pdf_object_vec_get(
                        alternate_resolved.data.array.elements,
                        0,
                        &first
                    )) {
                    return ERROR(
                        PDF_ERR_INCORRECT_TYPE,
                        "DeviceN alternate color space array must contain a "
                        "base color space name"
                    );
                }

                PdfObject first_resolved;
                TRY(
                    pdf_resolve_object(resolver, &first, &first_resolved, true)
                );

                if (first_resolved.type != PDF_OBJECT_TYPE_NAME) {
                    return ERROR(
                        PDF_ERR_INCORRECT_TYPE,
                        "DeviceN alternate color space array must start with "
                        "a color space name"
                    );
                }

                target_ptr->params.device_n.alternate_space =
                    first_resolved.data.name;
            } else {
                return ERROR(
                    PDF_ERR_INCORRECT_TYPE,
                    "DeviceN alternate color space must be a name or array"
                );
            }

            PdfObject tint_transform_object;
            RELEASE_ASSERT(pdf_object_vec_get(
                resolved.data.array.elements,
                3,
                &tint_transform_object
            ));

            PdfObject tint_transform_resolved;
            TRY(pdf_resolve_object(
                resolver,
                &tint_transform_object,
                &tint_transform_resolved,
                true
            ));
            if (tint_transform_resolved.type != PDF_OBJECT_TYPE_DICT
                && tint_transform_resolved.type != PDF_OBJECT_TYPE_STREAM) {
                return ERROR(
                    PDF_ERR_INCORRECT_TYPE,
                    "DeviceN TintTransform must be a function dictionary or "
                    "stream"
                );
            }
            target_ptr->params.device_n.tint_transform =
                tint_transform_resolved;

            if (num_elements == 5) {
                PdfObject attributes_object;
                RELEASE_ASSERT(pdf_object_vec_get(
                    resolved.data.array.elements,
                    4,
                    &attributes_object
                ));
                TRY(pdf_deserde_dict(
                    &attributes_object,
                    &target_ptr->params.device_n.attributes.value,
                    resolver
                ));
                target_ptr->params.device_n.attributes.is_some = true;
            } else {
                target_ptr->params.device_n.attributes =
                    (PdfDictOptional) {.is_some = false};
            }
            break;
        }
        default: {
            printf(
                "%s\n",
                pdf_fmt_object(pdf_resolver_arena(resolver), &resolved)
            );

            LOG_TODO("Unimplemented color space family %s", family);
        }
    }

    return NULL;
}

PDF_IMPL_FIELD(PdfColorSpace, color_space)

static GeomVec3 linear_srgb_to_nonlinear(
    GeomVec3 linear,
    GeomVec3 whitepoint,
    GeomVec3 blackpoint
) {
    GeomVec3 non_linear = geom_vec3_new(
        linear.x <= 0.00304 ? (linear.x * 12.92)
                            : (1.055 * pow(linear.x, 1.0 / 2.4) - 0.055),
        linear.y <= 0.00304 ? (linear.y * 12.92)
                            : (1.055 * pow(linear.y, 1.0 / 2.4) - 0.055),
        linear.z <= 0.00304 ? (linear.z * 12.92)
                            : (1.055 * pow(linear.z, 1.0 / 2.4) - 0.055)
    );

    return geom_vec3_new(
        (whitepoint.x - blackpoint.x) * non_linear.x + blackpoint.x,
        (whitepoint.y - blackpoint.y) * non_linear.y + blackpoint.y,
        (whitepoint.z - blackpoint.z) * non_linear.z + blackpoint.z
    );
}

/// Converts CIE xyz color to non-linear sRGB:
/// https://www.w3.org/Graphics/Color/sRGB.html
static GeomVec3
cie_xyz_to_srgb(GeomVec3 xyz, GeomVec3 whitepoint, GeomVec3 blackpoint) {
    GeomVec3 linear = geom_vec3_transform(
        xyz,
        geom_mat3_new(
            3.2410,
            -1.5374,
            -0.4986,
            -0.9692,
            1.8760,
            0.0416,
            0.0556,
            -0.2040,
            1.0570
        )
    );

    return linear_srgb_to_nonlinear(linear, whitepoint, blackpoint);
}

Error* pdf_map_color(
    double* components,
    size_t n_components,
    PdfColorSpace color_space,
    IccProfileCache* icc_cache,
    GeomVec3* srgb_out
) {
    RELEASE_ASSERT(components);
    RELEASE_ASSERT(icc_cache);
    RELEASE_ASSERT(srgb_out);

    switch (color_space.family) {
        case PDF_COLOR_SPACE_DEVICE_GRAY: {
            RELEASE_ASSERT(n_components == 1);
            *srgb_out =
                geom_vec3_new(components[0], components[0], components[0]);
            return NULL;
        }
        case PDF_COLOR_SPACE_DEVICE_RGB: {
            RELEASE_ASSERT(n_components == 3);
            *srgb_out =
                geom_vec3_new(components[0], components[1], components[2]);
            return NULL;
        }
        case PDF_COLOR_SPACE_DEVICE_CMYK: {
            RELEASE_ASSERT(n_components == 4);
            double c = components[0];
            double m = components[1];
            double y = components[2];
            double k = components[3];

            IccProfile *swop_profile, *srgb_profile;
            TRY(icc_profile_cache_get(
                icc_cache,
                "assets/icc-profiles/CGATS001Compat-v2-micro.icc",
                &swop_profile
            ));
            TRY(icc_profile_cache_get(
                icc_cache,
                "assets/icc-profiles/sRGB_v4_ICC_preference.icc",
                &srgb_profile
            ));

            IccColor dst;
            TRY(icc_device_to_device(
                swop_profile,
                srgb_profile,
                ICC_INTENT_PERCEPTUAL,
                (IccColor) {
                    .color_space = ICC_COLOR_SPACE_CMYK,
                    .channels = {c, m, y, k}
            },
                &dst
            ));

            RELEASE_ASSERT(dst.color_space == ICC_COLOR_SPACE_RGB);
            *srgb_out = geom_vec3_new(
                dst.channels[0],
                dst.channels[1],
                dst.channels[2]
            );
            return NULL;
        }
        case PDF_COLOR_SPACE_CAL_RGB: {
            RELEASE_ASSERT(n_components == 3);

            PdfCalRGBParams params = color_space.params.cal_rgb;
            GeomVec3 blackpoint = params.blackpoint.is_some
                                    ? params.blackpoint.value
                                    : geom_vec3_new(0.0, 0.0, 0.0);
            GeomVec3 gamma = params.gamma.is_some
                               ? params.gamma.value
                               : geom_vec3_new(0.0, 0.0, 0.0);
            GeomMat3 matrix = params.matrix.is_some ? params.matrix.value
                                                    : geom_mat3_identity();
            GeomVec3 pow_rgb = geom_vec3_new(
                pow(components[0], gamma.x),
                pow(components[1], gamma.y),
                pow(components[2], gamma.z)
            );

            GeomVec3 xyz = geom_vec3_transform(pow_rgb, matrix);

            *srgb_out = cie_xyz_to_srgb(xyz, params.whitepoint, blackpoint);
            return NULL;
        }
        default: {
            LOG_TODO("Unimplemented color space %d", color_space.family);
        }
    }
}
