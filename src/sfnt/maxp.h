#pragma once

#include <stdint.h>

#include "parser.h"
#include "pdf/error.h"
#include "sfnt/types.h"

typedef struct {
    SfntFixed version;
    uint16_t num_glyphs;
    uint16_t max_points;
    uint16_t max_contours;
    uint16_t max_component_points;
    uint16_t max_component_contours;
    uint16_t max_zones;
    uint16_t max_twilight_points;
    uint16_t max_storage;
    uint16_t max_function_defs;
    uint16_t max_instruction_defs;
    uint16_t max_stack_elements;
    uint16_t max_size_of_instructions;
    uint16_t max_component_elements;
    uint16_t max_component_depth;
} SfntMaxp;

PdfError* sfnt_parse_maxp(SfntParser* parser, SfntMaxp* maxp);
