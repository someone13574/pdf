#include "pdf/color_space.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

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

GeomVec3 pdf_map_color(
    PdfReal* components,
    size_t n_components,
    PdfColorSpace color_space
) {
    RELEASE_ASSERT(components);

    switch (color_space.family) {
        case PDF_COLOR_SPACE_DEVICE_GRAY: {
            RELEASE_ASSERT(n_components == 1);
            return geom_vec3_new(components[0], components[0], components[0]);
        }
        case PDF_COLOR_SPACE_DEVICE_RGB: {
            RELEASE_ASSERT(n_components == 3);
            return geom_vec3_new(components[0], components[1], components[2]);
        }
        case PDF_COLOR_SPACE_DEVICE_CMYK: {
            RELEASE_ASSERT(n_components == 4);
            double c = components[0];
            double m = components[1];
            double y = components[2];
            double k = components[3];

            // TODO: ICC profiles?
            GeomVec3 linear = geom_vec3_new(
                (1.0 - c) * (1.0 - k),
                (1.0 - m) * (1.0 - k),
                (1.0 - y) * (1.0 - k)
            );

            return linear_srgb_to_nonlinear(
                linear,
                geom_vec3_new(1.0, 1.0, 1.0),
                geom_vec3_new(0.0, 0.0, 0.0)
            );
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

            GeomVec3 xyz = geom_vec3_new(
                matrix.mat[0][0] * pow(components[0], gamma.x)
                    + matrix.mat[1][0] * pow(components[1], gamma.y)
                    + matrix.mat[2][0] * pow(components[2], gamma.z),
                matrix.mat[0][1] * pow(components[0], gamma.x)
                    + matrix.mat[1][1] * pow(components[1], gamma.y)
                    + matrix.mat[2][1] * pow(components[2], gamma.z),
                matrix.mat[0][2] * pow(components[0], gamma.x)
                    + matrix.mat[1][2] * pow(components[1], gamma.y)
                    + matrix.mat[2][2] * pow(components[2], gamma.z)
            );

            return cie_xyz_to_srgb(xyz, params.whitepoint, blackpoint);
        }
        default: {
            LOG_TODO("Unimplemented color space %d", color_space.family);
        }
    }

    return geom_vec3_new(0.0, 0.0, 0.0);
}
