#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "arena/arena.h"
#include "pdf_error/error.h"
#include "postscript/tokenizer.h"

#define PS_OBJECT_TYPES                                                        \
    X(BOOLEAN)                                                                 \
    X(FONT_ID)                                                                 \
    X(INTEGER)                                                                 \
    X(MARK)                                                                    \
    X(NAME)                                                                    \
    X(NULL)                                                                    \
    X(REAL)                                                                    \
    X(ARRAY)                                                                   \
    X(PROC)                                                                    \
    X(OPERATOR)                                                                \
    X(DICT)                                                                    \
    X(FILE)                                                                    \
    X(GSTATE)                                                                  \
    X(PACKEDARRAY)                                                             \
    X(SAVE)                                                                    \
    X(STRING)                                                                  \
    X(SINK)

#define X(name) PS_OBJECT_##name,
typedef enum { PS_OBJECT_TYPES } PSObjectType;
#undef X

extern const char* ps_object_name_lookup[];

typedef enum {
    /// All operators defined for that object are allowed. However, packed
    /// array objects always have read-only (or even more restricted) access.
    PS_ACCESS_UNLIMITED,

    /// An object with read-only access may not have its value written, but may
    /// still be read or executed.
    PS_ACCESS_READ_ONLY,

    /// An object with execute-only access may not have its value either read or
    /// written, but may still be executed by the PostScript interpreter.
    PS_ACCESS_EXECUTE_ONLY,

    /// An object with no access may not be operated on in any way by a
    /// PostScript program. Such objects are not of any direct use to
    /// PostScript programs, but serve internal purposes that are not documented
    /// in this book.
    PS_ACCESS_NONE
} PSAccess;

typedef struct PSInterpreter PSInterpreter;
typedef struct PSObjectList PSObjectList;
typedef PdfError* (*PSOperator)(PSInterpreter* interpreter);

/// A buffer for collecting literal objects.
typedef struct {
    PSObjectList* list;

    /// Restricts the final size of the buffer.
    enum { PS_SINK_ARRAY, PS_SINK_PROC, PS_SINK_DICT, PS_SINK_CUSTOM } type;

    /// If this sink is a `POSTSCRIPT_SINK_CUSTOM`, then set the name here so it
    /// can be type-checked when consumed.
    char* sink_name;
} PSObjectSink;

typedef struct {
    PSObjectType type;
    union {
        bool boolean;
        int32_t integer;
        char* name;
        double real;
        PSObjectList* array;
        PSObjectList* proc;
        PSObjectList* dict;
        PSOperator operator;
        PSString string;
        PSObjectSink sink;
    } data;

    bool literal;
    PSAccess access;
} PSObject;

PdfError* ps_object_execute(PSInterpreter* interpreter, const PSObject* object);
bool ps_object_eq(const PSObject* a, const PSObject* b);
const char* ps_object_fmt(Arena* arena, const PSObject* object);

#define DLINKED_NAME PSObjectList
#define DLINKED_LOWERCASE_NAME ps_object_list
#define DLINKED_TYPE PSObject
#include "arena/dlinked_decl.h"
