#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "arena/arena.h"
#include "pdf_error/error.h"
#include "postscript/tokenizer.h"

typedef enum {
    POSTSCRIPT_OBJECT_BOOLEAN,
    POSTSCRIPT_OBJECT_FONT_ID,
    POSTSCRIPT_OBJECT_INTEGER,
    POSTSCRIPT_OBJECT_MARK,
    POSTSCRIPT_OBJECT_NAME,
    POSTSCRIPT_OBJECT_NULL,
    POSTSCRIPT_OBJECT_OPERATOR,
    POSTSCRIPT_OBJECT_REAL,
    POSTSCRIPT_OBJECT_ARRAY,
    POSTSCRIPT_OBJECT_CUSTOM_PROC, /// Builtin or custom procedure
    POSTSCRIPT_OBJECT_DICT,
    POSTSCRIPT_OBJECT_FILE,
    POSTSCRIPT_OBJECT_GSTATE,
    POSTSCRIPT_OBJECT_PACKEDARRAY,
    POSTSCRIPT_OBJECT_SAVE,
    POSTSCRIPT_OBJECT_STRING
} PostscriptObjectType;

typedef enum {
    /// All operations defined for that object are allowed. However, packed
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
typedef PdfError* (*PostscriptCustomProcedure)(
    PostscriptInterpreter* interpreter
);

typedef struct {
    PostscriptObjectType type;
    union {
        bool boolean;
        int32_t integer;
        char* name;
        double real;
        PostscriptObjectList* array;
        PostscriptObjectList* dict;
        PostscriptCustomProcedure custom_proc;
        PostscriptString string;
    } data;

    bool literal;
    PostscriptAccess access;
} PostscriptObject;

PdfError* postscript_object_execute(
    PostscriptInterpreter* interpreter,
    const PostscriptObject* object
);
bool postscript_object_eq(const PostscriptObject* a, const PostscriptObject* b);
char* postscript_object_fmt(Arena* arena, const PostscriptObject* object);

#define DLINKED_NAME PostscriptObjectList
#define DLINKED_LOWERCASE_NAME postscript_object_list
#define DLINKED_TYPE PostscriptObject
#include "arena/dlinked_decl.h"
