#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "arena/arena.h"
#include "pdf_error/error.h"
#include "postscript/tokenizer.h"

#define POSTSCRIPT_OBJECT_TYPES                                                \
    X(BOOLEAN)                                                                 \
    X(FONT_ID)                                                                 \
    X(INTEGER)                                                                 \
    X(MARK)                                                                    \
    X(NAME)                                                                    \
    X(NULL)                                                                    \
    X(REAL)                                                                    \
    X(ARRAY)                                                                   \
    X(OPERATOR)                                                                \
    X(DICT)                                                                    \
    X(FILE)                                                                    \
    X(GSTATE)                                                                  \
    X(PACKEDARRAY)                                                             \
    X(SAVE)                                                                    \
    X(STRING)                                                                  \
    X(SINK)

#define X(name) POSTSCRIPT_OBJECT_##name,
typedef enum { POSTSCRIPT_OBJECT_TYPES } PostscriptObjectType;
#undef X

extern const char* postscript_object_name_lookup[];

typedef enum {
    /// All operators defined for that object are allowed. However, packed
    /// array objects always have read-only (or even more restricted) access.
    POSTSCRIPT_ACCESS_UNLIMITED,

    /// An object with read-only access may not have its value written, but may
    /// still be read or executed.
    POSTSCRIPT_ACCESS_READ_ONLY,

    /// An object with execute-only access may not have its value either read or
    /// written, but may still be executed by the PostScript interpreter.
    POSTSCRIPT_ACCESS_EXECUTE_ONLY,

    /// An object with no access may not be operated on in any way by a
    /// PostScript program. Such objects are not of any direct use to
    /// PostScript programs, but serve internal purposes that are not documented
    /// in this book.
    POSTSCRIPT_ACCESS_NONE
} PostscriptAccess;

typedef struct PostscriptInterpreter PostscriptInterpreter;
typedef struct PostscriptObjectList PostscriptObjectList;
typedef PdfError* (*PostscriptOperator)(PostscriptInterpreter* interpreter);

/// A buffer for collecting literal objects.
typedef struct {
    PostscriptObjectList* list;

    /// Restricts the final size of the buffer.
    enum { POSTSCRIPT_SINK_ARRAY, POSTSCRIPT_SINK_CUSTOM } type;

    /// If this sink is a `POSTSCRIPT_SINK_CUSTOM`, then set the name here so it
    /// can be type-checked when consumed.
    char* sink_name;
} PostscriptObjectSink;

typedef struct {
    PostscriptObjectType type;
    union {
        bool boolean;
        int32_t integer;
        char* name;
        double real;
        PostscriptObjectList* array;
        PostscriptObjectList* dict;
        PostscriptOperator operator;
        PostscriptString string;
        PostscriptObjectSink sink;
    } data;

    bool literal;
    PostscriptAccess access;
} PostscriptObject;

PdfError* postscript_object_execute(
    PostscriptInterpreter* interpreter,
    const PostscriptObject* object
);
bool postscript_object_eq(const PostscriptObject* a, const PostscriptObject* b);
const char* postscript_object_fmt(Arena* arena, const PostscriptObject* object);

#define DLINKED_NAME PostscriptObjectList
#define DLINKED_LOWERCASE_NAME postscript_object_list
#define DLINKED_TYPE PostscriptObject
#include "arena/dlinked_decl.h"
