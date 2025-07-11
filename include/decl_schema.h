#include "pdf_doc.h"
#include "pdf_object.h"
#include "pdf_result.h"

// Check for parameters
#ifndef SCHEMA_NAME
#error "SCHEMA_NAME not defined"
#define SCHEMA_NAME
#endif

#ifndef SCHEMA_LOWERCASE_NAME
#error "SCHEMA_LOWERCASE_NAME not defined"
#define SCHEMA_LOWERCASE_NAME
#endif

#ifndef SCHEMA_FIELDS
#error "SCHEMA_FIELDS not defined"
#define SCHEMA_FIELDS
#endif

#define PASTE_INNER(a, b) a##b
#define PASTE(a, b) PASTE_INNER(a, b)

// Define names
#define REF_NAME PASTE(SCHEMA_NAME, Ref)
#define OP_REF_NAME PASTE(SCHEMA_NAME, OpRef)

#define NEW_FN PASTE(SCHEMA_LOWERCASE_NAME, _new)
#define RESOLVE_FN PASTE(SCHEMA_LOWERCASE_NAME, _resolve)

// Ref structs
typedef struct SCHEMA_NAME SCHEMA_NAME;

typedef struct REF_NAME {
    SCHEMA_NAME* (*resolve)(
        struct REF_NAME* ref,
        PdfDocument* doc,
        PdfResult* result
    );
    PdfObjectRef ref_object;
    SCHEMA_NAME* cached;
} REF_NAME;

typedef struct OP_REF_NAME {
    bool has_value;
    REF_NAME value;
} OP_REF_NAME;

// Main struct
#define X(type, name) type name;

struct SCHEMA_NAME {
    SCHEMA_FIELDS

    PdfObject* raw_dict;
};

#undef X

// Functions
SCHEMA_NAME* NEW_FN(PdfObject* object, Arena* arena, PdfResult* result);
SCHEMA_NAME* RESOLVE_FN(REF_NAME* ref, PdfDocument* doc, PdfResult* result);

// Undefs
#undef SCHEMA_NAME
#undef SCHEMA_LOWERCASE_NAME
#undef SCHEMA_FIELDS
#undef PASTE
#undef PASTE_INNER
#undef REF_NAME
#undef OP_REF_NAME
#undef NEW_FN
#undef RESOLVE_FN
#undef OP_RESOLVE_FN
