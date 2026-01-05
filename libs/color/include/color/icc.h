#pragma once

#include <stdint.h>

#include "err/error.h"
#include "parse_ctx/ctx.h"

typedef struct {
    uint16_t year;
    uint16_t month;
    uint16_t day_of_month;
    uint16_t hour;
    uint16_t minute;
    uint16_t second;
} ICCDateTime;

Error* icc_parse_date_time(ParseCtx* ctx, ICCDateTime* out);

typedef uint32_t ICCS15Fixed16Number;

typedef struct {
    ICCS15Fixed16Number x;
    ICCS15Fixed16Number y;
    ICCS15Fixed16Number z;
} ICCXYZNumber;

Error* icc_parse_xyz_number(ParseCtx* ctx, ICCXYZNumber* out);

typedef struct {
    uint32_t size;
    uint32_t preferred_cmm;
    uint32_t version;
    uint32_t class;
    uint32_t color_space;
    uint32_t pcs;
    ICCDateTime date;
    uint32_t signature;
    uint32_t platform_signature;
    uint32_t flags;
    uint32_t device_manufacturer;
    uint32_t device_model;
    uint64_t device_attributes;
    uint32_t rendering_intent;
    ICCXYZNumber illuminant;
    uint32_t creator_signature;
    uint64_t id_low;
    uint64_t id_high;
} ICCProfileHeader;

Error* icc_parse_header(ParseCtx* ctx, ICCProfileHeader* out);

typedef struct {
    ParseCtx file_ctx;
    ParseCtx table_ctx;
    uint32_t tag_count;
} ICCTagTable;

Error* icc_tag_table_new(ParseCtx* file_ctx, ICCTagTable* out);
Error*
icc_tag_table_lookup(ICCTagTable table, uint32_t tag_signature, ParseCtx* out);
