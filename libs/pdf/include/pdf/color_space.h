#pragma once

#include "geom/vec3.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf_error/error.h"

typedef struct {
    /// (Required) An array of three numbers [ XW YW ZW ] specifying the
    /// tristimulus value, in the CIE 1931 XYZ space, of the diffuse white
    /// point; see below for further discussion. The numbers XW and ZW shall be
    /// positive, and YW shall be equal to 1.0.
    GeomVec3 whitepoint;

    /// (Optional) An array of three numbers [ XB YB ZB] specifying the
    /// tristimulus value, in the CIE 1931 XYZ space, of the diffuse black
    /// point; see below for further discussion. All three of these numbers
    /// shall be non-negative. Default value: [ 0.0 0.0 0.0 ].
    PdfGeomVec3Optional blackpoint;

    /// (Optional) An array of three numbers [ GR GG GB ] specifying the gamma
    /// for the red, green, and blue (A, B, and C) components of the colour
    /// space. Default value: [ 1.0 1.0 1.0 ].
    PdfGeomVec3Optional gamma;

    /// (Optional) An array of nine numbers [ XA YA ZA XB YB ZB XC YC ZC]
    /// specifying the linear interpretation of the decoded A, B, and C
    /// components of the colour space with respect to the final XYZ
    /// representation. Default value: the identity matrix [ 1 0 0 0 1 0 0 0 1
    /// ].
    PdfGeomMat3Optional matrix;
} PdfCalRGBParams;

typedef struct {
    PdfNameVec* names;
    PdfName alternate_space;
    PdfObject* tilt_transform;
    PdfDictOptional attributes;
} PdfDeviceNParams;

typedef enum {
    PDF_COLOR_SPACE_DEVICE_GRAY,
    PDF_COLOR_SPACE_DEVICE_RGB,
    PDF_COLOR_SPACE_DEVICE_CMYK,
    PDF_COLOR_SPACE_CAL_GRAY,
    PDF_COLOR_SPACE_CAL_RGB,
    PDF_COLOR_SPACE_LAB,
    PDF_COLOR_SPACE_ICC_BASED,
    PDF_COLOR_SPACE_INDEXED,
    PDF_COLOR_SPACE_PATTERN,
    PDF_COLOR_SPACE_SEPARATION,
    PDF_COLOR_SPACE_DEVICE_N
} PdfColorSpaceFamily;

typedef union {
    PdfCalRGBParams cal_rgb;
} PdfColorSpaceParams;

typedef struct {
    PdfColorSpaceFamily family;
    PdfColorSpaceParams params;
} PdfColorSpace;

PdfError* pdf_deserialize_color_space(
    const PdfObject* object,
    PdfColorSpace* target_ptr,
    PdfResolver* resolver
);

DESERDE_DECL_TRAMPOLINE(pdf_deserialize_color_space_trampoline)

GeomVec3 pdf_map_color(
    double* components,
    size_t n_components,
    PdfColorSpace color_space
);
