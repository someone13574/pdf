#include "object.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena/arena.h"
#include "ctx.h"
#include "log.h"
#include "pdf/object.h"
#include "pdf/result.h"

#define DVEC_NAME PdfObjectVec
#define DVEC_LOWERCASE_NAME pdf_object_vec
#define DVEC_TYPE PdfObject*
#include "arena/dvec_impl.h"

#define DVEC_NAME PdfDictEntryVec
#define DVEC_LOWERCASE_NAME pdf_dict_entry_vec
#define DVEC_TYPE PdfDictEntry
#include "arena/dvec_impl.h"

PdfResult pdf_parse_true(PdfCtx* ctx, PdfObject* object);
PdfResult pdf_parse_false(PdfCtx* ctx, PdfObject* object);
PdfResult pdf_parse_null(PdfCtx* ctx, PdfObject* object);
PdfResult pdf_parse_number(PdfCtx* ctx, PdfObject* object);
PdfResult
pdf_parse_string_literal(Arena* arena, PdfCtx* ctx, PdfObject* object);
PdfResult pdf_parse_name(Arena* arena, PdfCtx* ctx, PdfObject* object);
PdfResult pdf_parse_array(Arena* arena, PdfCtx* ctx, PdfObject* object);
PdfResult pdf_parse_dict(
    Arena* arena,
    PdfCtx* ctx,
    PdfObject* object,
    bool in_indirect_obj
);
PdfResult pdf_parse_indirect(
    Arena* arena,
    PdfCtx* ctx,
    PdfObject* object,
    bool* number_fallback
);
PdfResult pdf_parse_stream(
    Arena* arena,
    PdfCtx* ctx,
    char** stream_bytes,
    PdfDictEntryVec* stream_dict
);

PdfResult pdf_parse_object(
    Arena* arena,
    PdfCtx* ctx,
    PdfObject* object,
    bool in_indirect_obj
) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(object);

    LOG_INFO_G("object", "Parsing object at offset %zu", pdf_ctx_offset(ctx));

    char peeked, peeked_next;
    PDF_PROPAGATE(pdf_ctx_peek(ctx, &peeked));

    PdfResult peeked_next_result = pdf_ctx_peek_next(ctx, &peeked_next);
    if (peeked == '<' && peeked_next_result != PDF_OK) {
        return peeked_next_result;
    }

    if (peeked == 't') {
        return pdf_parse_true(ctx, object);
    } else if (peeked == 'f') {
        return pdf_parse_false(ctx, object);
    } else if (peeked == '.' || peeked == '+' || peeked == '-') {
        return pdf_parse_number(ctx, object);
    } else if (isdigit((unsigned char)peeked)) {
        size_t restore_offset = pdf_ctx_offset(ctx);
        bool number_fallback = true;
        PdfResult result =
            pdf_parse_indirect(arena, ctx, object, &number_fallback);

        if (result == PDF_OK || !number_fallback) {
            return result;
        }

        LOG_DEBUG_G(
            "object",
            "Failed to parse indirect object/reference. Attempting to parse number."
        );
        PDF_PROPAGATE(pdf_ctx_seek(ctx, restore_offset));

        return pdf_parse_number(ctx, object);
    } else if (peeked == '(') {
        return pdf_parse_string_literal(arena, ctx, object);
    } else if (peeked == '<' && peeked_next != '<') {
        LOG_TODO("Hexadecimal strings");
    } else if (peeked == '/') {
        return pdf_parse_name(arena, ctx, object);
    } else if (peeked == '[') {
        return pdf_parse_array(arena, ctx, object);
    } else if (peeked == '<' && peeked_next == '<') {
        return pdf_parse_dict(arena, ctx, object, in_indirect_obj);
    } else if (peeked == 'n') {
        return pdf_parse_null(ctx, object);
    }

    return PDF_ERR_INVALID_OBJECT;
}

PdfResult
pdf_parse_operand_object(Arena* arena, PdfCtx* ctx, PdfObject* object) {
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(object);

    LOG_DEBUG_G(
        "object",
        "Parsing operand object at offset %zu in stream ctx",
        pdf_ctx_offset(ctx)
    );

    char peeked, peeked_next;
    PDF_PROPAGATE(pdf_ctx_peek(ctx, &peeked));

    PdfResult peeked_next_result = pdf_ctx_peek_next(ctx, &peeked_next);
    if (peeked == '<' && peeked_next_result != PDF_OK) {
        return peeked_next_result;
    }

    if (peeked == 't') {
        return pdf_parse_true(ctx, object);
    } else if (peeked == 'f') {
        return pdf_parse_false(ctx, object);
    } else if (peeked == '.' || peeked == '+' || peeked == '-' || isdigit((unsigned char)peeked)) {
        return pdf_parse_number(ctx, object);
    } else if (peeked == '(') {
        return pdf_parse_string_literal(arena, ctx, object);
    } else if (peeked == '<' && peeked_next != '<') {
        LOG_TODO("Hexadecimal strings");
    } else if (peeked == '/') {
        return pdf_parse_name(arena, ctx, object);
    } else if (peeked == '[') {
        return pdf_parse_array(arena, ctx, object);
    } else if (peeked == '<' && peeked_next == '<') {
        return pdf_parse_dict(arena, ctx, object, false);
    } else if (peeked == 'n') {
        return pdf_parse_null(ctx, object);
    }

    return PDF_ERR_INVALID_OBJECT;
}

PdfResult pdf_parse_true(PdfCtx* ctx, PdfObject* object) {
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(object);

    PDF_PROPAGATE(pdf_ctx_expect(ctx, "true"));
    PDF_PROPAGATE(pdf_ctx_require_char_type(ctx, true, &is_pdf_non_regular));

    object->type = PDF_OBJECT_TYPE_BOOLEAN;
    object->data.boolean = true;

    return PDF_OK;
}

PdfResult pdf_parse_false(PdfCtx* ctx, PdfObject* object) {
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(object);

    PDF_PROPAGATE(pdf_ctx_expect(ctx, "false"));
    PDF_PROPAGATE(pdf_ctx_require_char_type(ctx, true, &is_pdf_non_regular));

    object->type = PDF_OBJECT_TYPE_BOOLEAN;
    object->data.boolean = false;

    return PDF_OK;
}

PdfResult pdf_parse_null(PdfCtx* ctx, PdfObject* object) {
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(object);

    PDF_PROPAGATE(pdf_ctx_expect(ctx, "null"));
    PDF_PROPAGATE(pdf_ctx_require_char_type(ctx, true, &is_pdf_non_regular));

    object->type = PDF_OBJECT_TYPE_NULL;

    return PDF_OK;
}

PdfResult pdf_parse_number(PdfCtx* ctx, PdfObject* object) {
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(object);

    LOG_TRACE_G("object", "Parsing number at offset %zu", pdf_ctx_offset(ctx));

    // Parse sign
    char sign_char;
    int32_t sign = 1;
    PDF_PROPAGATE(pdf_ctx_peek(ctx, &sign_char));
    if (sign_char == '+' || sign_char == '-') {
        PDF_PROPAGATE(pdf_ctx_peek_and_advance(ctx, NULL));

        if (sign_char == '-') {
            sign = -1;
        }
    }

    // Parse leading digits
    char digit_char;
    int64_t leading_acc = 0;
    bool has_leading = false;

    while (pdf_ctx_peek(ctx, &digit_char) == PDF_OK && isdigit(digit_char)) {
        has_leading = true;
        int64_t digit = digit_char - '0';

        if (leading_acc <= (INT64_MAX - digit) / 10) {
            leading_acc *= 10;
            leading_acc += digit;
        } else {
            leading_acc = INT64_MAX;
        }

        PDF_PROPAGATE(pdf_ctx_peek_and_advance(ctx, NULL));
    }

    // Parse decimal point
    char decimal_char;
    PdfResult decimal_peek_result = pdf_ctx_peek(ctx, &decimal_char);

    if ((decimal_peek_result == PDF_OK && decimal_char != '.')
        || decimal_peek_result == PDF_ERR_CTX_EOF) {
        LOG_TRACE_G("object", "Number is integer");
        PDF_PROPAGATE(pdf_ctx_require_char_type(ctx, true, &is_pdf_non_regular)
        );

        if (!has_leading) {
            return PDF_ERR_INVALID_NUMBER;
        }

        leading_acc *= sign;
        if (leading_acc < (int64_t)INT32_MIN
            || leading_acc > (int64_t)INT32_MAX) {
            return PDF_ERR_NUMBER_LIMIT;
        }

        object->type = PDF_OBJECT_TYPE_INTEGER;
        object->data.integer = (int32_t)leading_acc;

        return PDF_OK;
    } else if (decimal_peek_result != PDF_OK) {
        return decimal_peek_result;
    } else {
        PDF_PROPAGATE(pdf_ctx_peek_and_advance(ctx, NULL));
    }

    LOG_TRACE_G("object", "Number is real");

    // Parse trailing digits
    double trailing_acc = 0.0;
    double trailing_weight = 0.1;
    bool has_trailing = false;

    while (pdf_ctx_peek(ctx, &digit_char) == PDF_OK && isdigit(digit_char)) {
        has_trailing = true;

        trailing_acc += (double)(digit_char - '0') * trailing_weight;
        trailing_weight *= 0.1;

        PDF_PROPAGATE(pdf_ctx_peek_and_advance(ctx, NULL));
    }

    PDF_PROPAGATE(pdf_ctx_require_char_type(ctx, true, &is_pdf_non_regular));

    double value = ((double)leading_acc + trailing_acc) * (double)sign;
    const double PDF_FLOAT_MAX = 3.403e38;

    if (value > PDF_FLOAT_MAX || value < -PDF_FLOAT_MAX) {
        return PDF_ERR_NUMBER_LIMIT;
    }

    if (!has_leading && !has_trailing) {
        return PDF_ERR_INVALID_NUMBER;
    }

    object->type = PDF_OBJECT_TYPE_REAL;
    object->data.real = value;

    return PDF_OK;
}

PdfResult
pdf_parse_string_literal(Arena* arena, PdfCtx* ctx, PdfObject* object) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(object);

    PDF_PROPAGATE(pdf_ctx_expect(ctx, "("));

    // Find length upper bound
    char peeked_char;
    int escape = 0;
    int open_parenthesis = 1;
    unsigned long length = 0;
    size_t start_offset = pdf_ctx_offset(ctx);

    while (open_parenthesis > 0
           && pdf_ctx_peek_and_advance(ctx, &peeked_char) == PDF_OK) {
        length++;

        if (peeked_char == '(' && escape == 0) {
            open_parenthesis++;
        } else if (peeked_char == ')' && escape == 0) {
            open_parenthesis--;
        }

        if (peeked_char == '\\' && escape == 0) {
            escape = 1;
        } else {
            escape = 0;
        }
    }

    if (open_parenthesis != 0) {
        return PDF_ERR_UNBALANCED_STR;
    }

    PDF_PROPAGATE(pdf_ctx_require_char_type(ctx, true, &is_pdf_non_regular));

    // Parse
    char* raw;
    PDF_PROPAGATE(pdf_ctx_borrow_substr(
        ctx,
        start_offset,
        pdf_ctx_offset(ctx) - start_offset - 1,
        &raw
    ));

    char* parsed = arena_alloc(arena, sizeof(char) * (length + 1));

    escape = 0;
    size_t write_offset = 0;

    for (size_t read_offset = 0; read_offset < length; read_offset++) {
        char read_char = raw[read_offset];

        switch (escape) {
            case 0: {
                if (read_char == '\\') {
                    escape = 1;
                    break;
                }

                parsed[write_offset++] = read_char;
                break;
            }
            case 1: {
                switch (read_char) {
                    case 'n': {
                        parsed[write_offset++] = '\n';
                        escape = 0;
                        break;
                    }
                    case 'r': {
                        parsed[write_offset++] = '\r';
                        escape = 0;
                        break;
                    }
                    case 't': {
                        parsed[write_offset++] = '\t';
                        escape = 0;
                        break;
                    }
                    case 'b': {
                        parsed[write_offset++] = '\b';
                        escape = 0;
                        break;
                    }
                    case 'f': {
                        parsed[write_offset++] = '\f';
                        escape = 0;
                        break;
                    }
                    case '(': {
                        parsed[write_offset++] = '(';
                        escape = 0;
                        break;
                    }
                    case ')': {
                        parsed[write_offset++] = ')';
                        escape = 0;
                        break;
                    }
                    case '\\': {
                        parsed[write_offset++] = '\\';
                        escape = 0;
                        break;
                    }
                    default: {
                        LOG_TODO("Octal escape codes and split lines");
                    }
                }
                break;
            }
            default: {
                LOG_PANIC("Unreachable");
            }
        }
    }

    PDF_PROPAGATE(pdf_ctx_release_substr(ctx));
    parsed[write_offset] = '\0';

    object->type = PDF_OBJECT_TYPE_STRING;
    object->data.string = parsed;

    return PDF_OK;
}

bool char_to_hex(char c, int* out) {
    if (c >= '0' && c <= '9') {
        *out = c - '0';
        return true;
    } else if (c >= 'A' && c <= 'F') {
        *out = c - 'A' + 10;
        return true;
    } else if (c >= 'a' && c <= 'f') {
        *out = c - 'a' + 10;
        return true;
    } else {
        return false;
    }
}

PdfResult pdf_parse_name(Arena* arena, PdfCtx* ctx, PdfObject* object) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(object);

    PDF_PROPAGATE(pdf_ctx_expect(ctx, "/"));

    // Find max length
    size_t start_offset = pdf_ctx_offset(ctx);
    size_t length = 0;
    char peeked;

    while (pdf_ctx_peek_and_advance(ctx, &peeked) == PDF_OK) {
        if (!is_pdf_regular(peeked)) {
            break;
        }

        if (peeked < '!' || peeked > '~') {
            return PDF_ERR_NAME_UNESCAPED_CHAR;
        }

        length++;
    }

    LOG_DEBUG("Length %zu", length);
    PDF_PROPAGATE(pdf_ctx_seek(ctx, start_offset + length));
    PDF_PROPAGATE(pdf_ctx_require_char_type(ctx, true, &is_pdf_non_regular));

    // Parse name
    char* name = arena_alloc(arena, sizeof(char) * (length + 1));
    char* raw;
    PDF_PROPAGATE(pdf_ctx_borrow_substr(ctx, start_offset, length, &raw));

    size_t write_offset = 0;
    int escape = 0;
    int hex_code = 0;

    for (size_t read_offset = 0; read_offset < length; read_offset++) {
        char c = raw[read_offset];

        switch (escape) {
            case 0: {
                if (c == '#') {
                    escape = 1;
                    continue;
                }

                name[write_offset++] = c;
                break;
            }
            case 1: {
                int value;
                if (!char_to_hex(c, &value)) {
                    return PDF_ERR_NAME_BAD_CHAR_CODE;
                }

                hex_code = value << 4;
                escape = 2;
                break;
            }
            case 2: {
                int value;
                if (!char_to_hex(c, &value)) {
                    return PDF_ERR_NAME_BAD_CHAR_CODE;
                }

                hex_code |= value;
                name[write_offset++] = (char)hex_code;
                escape = 0;
                break;
            }
            default: {
                LOG_PANIC("Unreachable");
            }
        }
    }

    if (escape != 0) {
        return PDF_ERR_NAME_BAD_CHAR_CODE;
    }

    PDF_PROPAGATE(pdf_ctx_release_substr(ctx));
    name[write_offset] = '\0';

    object->type = PDF_OBJECT_TYPE_NAME;
    object->data.name = name;

    return PDF_OK;
}

PdfResult pdf_parse_array(Arena* arena, PdfCtx* ctx, PdfObject* object) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(object);

    PDF_PROPAGATE(pdf_ctx_expect(ctx, "["));
    PDF_PROPAGATE(pdf_ctx_consume_whitespace(ctx));

    PdfObjectVec* elements = pdf_object_vec_new(arena);

    char peeked;
    while (pdf_ctx_peek(ctx, &peeked) == PDF_OK && peeked != ']') {
        PdfObject element;
        PDF_PROPAGATE(pdf_parse_object(arena, ctx, &element, false));
        PDF_PROPAGATE(pdf_ctx_require_char_type(ctx, false, is_pdf_non_regular)
        );
        PDF_PROPAGATE(pdf_ctx_consume_whitespace(ctx));

        PdfObject* allocated = arena_alloc(arena, sizeof(PdfObject));
        *allocated = element;
        pdf_object_vec_push(elements, allocated);
    }

    PDF_PROPAGATE(pdf_ctx_expect(ctx, "]"));
    PDF_PROPAGATE(pdf_ctx_require_char_type(ctx, true, is_pdf_non_regular));

    object->type = PDF_OBJECT_TYPE_ARRAY;
    object->data.array.elements = elements;

    return PDF_OK;
}

PdfResult pdf_parse_dict(
    Arena* arena,
    PdfCtx* ctx,
    PdfObject* object,
    bool in_indirect_obj
) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(object);

    PDF_PROPAGATE(pdf_ctx_expect(ctx, "<<"));
    PDF_PROPAGATE(pdf_ctx_consume_whitespace(ctx));

    PdfDictEntryVec* entries = pdf_dict_entry_vec_new(arena);

    char peeked;
    while (pdf_ctx_peek(ctx, &peeked) == PDF_OK && peeked != '>') {
        PdfObject* key = arena_alloc(arena, sizeof(PdfObject));
        PDF_PROPAGATE(pdf_parse_name(arena, ctx, key));
        PDF_PROPAGATE(pdf_ctx_require_char_type(ctx, false, is_pdf_non_regular)
        );
        PDF_PROPAGATE(pdf_ctx_consume_whitespace(ctx));

        PdfObject* value = arena_alloc(arena, sizeof(PdfObject));
        PDF_PROPAGATE(pdf_parse_object(arena, ctx, value, false));
        PDF_PROPAGATE(pdf_ctx_require_char_type(ctx, false, is_pdf_non_regular)
        );
        PDF_PROPAGATE(pdf_ctx_consume_whitespace(ctx));

        pdf_dict_entry_vec_push(
            entries,
            (PdfDictEntry) {.key = key, .value = value}
        );
    }

    PDF_PROPAGATE(pdf_ctx_expect(ctx, ">>"));
    PDF_PROPAGATE(pdf_ctx_require_char_type(ctx, true, is_pdf_non_regular));

    object->type = PDF_OBJECT_TYPE_DICT;
    object->data.dict.entries = entries;

    // Attempt to parse stream
    if (in_indirect_obj) {
        size_t restore_offset = pdf_ctx_offset(ctx);

        PdfResult consume_result = pdf_ctx_consume_whitespace(ctx);
        if (consume_result != PDF_OK) {
            // Not a stream
            pdf_ctx_seek(ctx, restore_offset);
            return PDF_OK;
        }

        char* stream_bytes;
        if (pdf_parse_stream(arena, ctx, &stream_bytes, entries) != PDF_OK
            || !stream_bytes) {
            // Not a stream
            pdf_ctx_seek(ctx, restore_offset);
            return PDF_OK;
        }

        // Type is a stream
        PDF_PROPAGATE(pdf_ctx_require_char_type(ctx, true, is_pdf_non_regular));

        PdfObject* stream_dict = arena_alloc(arena, sizeof(PdfObject));
        *stream_dict = *object;

        object->type = PDF_OBJECT_TYPE_STREAM;
        object->data.stream.stream_dict = stream_dict;
        object->data.stream.stream_bytes = stream_bytes;

        return PDF_OK;
    }

    return PDF_OK;
}

PdfResult pdf_parse_stream(
    Arena* arena,
    PdfCtx* ctx,
    char** stream_body,
    PdfDictEntryVec* stream_dict
) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(stream_body);
    RELEASE_ASSERT(stream_dict);

    // Parse start
    PDF_PROPAGATE(pdf_ctx_expect(ctx, "stream"));

    char peeked_newline;
    PDF_PROPAGATE(pdf_ctx_peek(ctx, &peeked_newline));
    if (peeked_newline == '\n') {
        PDF_PROPAGATE(pdf_ctx_shift(ctx, 1));
    } else if (peeked_newline == '\r') {
        PDF_PROPAGATE(pdf_ctx_expect(ctx, "\r\n"));
    } else {
        return PDF_ERR_CTX_EXPECT;
    }

    // Get length
    int32_t length = -1;
    for (size_t entry_idx = 0; entry_idx < pdf_dict_entry_vec_len(stream_dict);
         entry_idx++) {
        PdfDictEntry entry;
        RELEASE_ASSERT(pdf_dict_entry_vec_get(stream_dict, entry_idx, &entry));
        if (!entry.key || !entry.value
            || entry.value->type != PDF_OBJECT_TYPE_INTEGER
            || strcmp("Length", entry.key->data.name) != 0) {
            continue;
        }

        length = entry.value->data.integer;
    }

    if (length < 0) {
        return PDF_ERR_STREAM_INVALID_LENGTH;
    }

    // Copy stream body
    char* borrowed;
    PDF_PROPAGATE(pdf_ctx_borrow_substr(
        ctx,
        pdf_ctx_offset(ctx),
        (size_t)length,
        &borrowed
    ));

    *stream_body = arena_alloc(arena, sizeof(char) * (size_t)(length + 1));
    strncpy(*stream_body, borrowed, (size_t)length);
    (*stream_body)[length] = '\0';

    PDF_PROPAGATE(pdf_ctx_release_substr(ctx));

    // Parse end
    PDF_PROPAGATE(pdf_ctx_shift(ctx, length));
    if (pdf_ctx_expect(ctx, "\nendstream") != PDF_OK
        && pdf_ctx_expect(ctx, "\r\nendstream") != PDF_OK
        && pdf_ctx_expect(ctx, "\rendstream") != PDF_OK) {
        return PDF_ERR_CTX_EXPECT;
    }

    return PDF_OK;
}

PdfResult pdf_parse_indirect(
    Arena* arena,
    PdfCtx* ctx,
    PdfObject* object,
    bool* number_fallback
) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(number_fallback);

    LOG_DEBUG_G("object", "Parsing indirect object or reference");

    // Parse object id
    uint64_t object_id;
    uint32_t int_length;
    PDF_PROPAGATE(pdf_ctx_parse_int(ctx, NULL, &object_id, &int_length));
    if (int_length == 0) {
        return PDF_ERR_CTX_EXPECT;
    }

    PDF_PROPAGATE(pdf_ctx_expect(ctx, " "));

    // Parse generation
    uint64_t generation;
    PDF_PROPAGATE(pdf_ctx_parse_int(ctx, NULL, &generation, &int_length));
    if (int_length == 0) {
        return PDF_ERR_CTX_EXPECT;
    }

    PDF_PROPAGATE(pdf_ctx_expect(ctx, " "));

    // Determine if indirect object or reference
    char peeked;
    PDF_PROPAGATE(pdf_ctx_peek(ctx, &peeked));

    if (peeked == 'R') {
        PDF_PROPAGATE(pdf_ctx_peek_and_advance(ctx, NULL));

        LOG_DEBUG_G("object", "Parsed indirect reference");

        object->type = PDF_OBJECT_TYPE_INDIRECT_REF;
        object->data.indirect_ref.object_id = object_id;
        object->data.indirect_ref.generation = generation;

        return PDF_OK;
    } else {
        LOG_DEBUG_G("object", "Parsed indirect object");

        PDF_PROPAGATE(pdf_ctx_expect(ctx, "obj"));
        PDF_PROPAGATE(pdf_ctx_require_char_type(ctx, false, &is_pdf_non_regular)
        );
        PDF_PROPAGATE(pdf_ctx_consume_whitespace(ctx));
        *number_fallback = false;

        PdfObject* inner = arena_alloc(arena, sizeof(PdfObject));
        PDF_PROPAGATE(pdf_parse_object(arena, ctx, inner, true));

        PDF_PROPAGATE(pdf_ctx_require_char_type(ctx, false, &is_pdf_non_regular)
        );
        PDF_PROPAGATE(pdf_ctx_consume_whitespace(ctx));
        PDF_PROPAGATE(pdf_ctx_expect(ctx, "endobj"));
        PDF_PROPAGATE(pdf_ctx_require_char_type(ctx, true, &is_pdf_non_regular)
        );

        object->type = PDF_OBJECT_TYPE_INDIRECT_OBJECT;
        object->data.indirect_object.object = inner;
        object->data.indirect_object.object_id = object_id;
        object->data.indirect_object.generation = generation;

        return PDF_OK;
    }
}

static char* format_alloc(Arena* arena, const char* fmt, ...)
    __attribute__((format(printf, 2, 3)));

static char* format_alloc(Arena* arena, const char* fmt, ...) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(fmt);

    va_list args;
    va_start(args, fmt);
    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(NULL, 0, fmt, args_copy);
    char* buffer = arena_alloc(arena, (size_t)needed + 1);
    vsprintf(buffer, fmt, args);
    va_end(args);

    return buffer;
}

char* pdf_fmt_object_indented(
    Arena* arena,
    PdfObject* object,
    int indent,
    bool* force_indent_parent
) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(object);

    switch (object->type) {
        case PDF_OBJECT_TYPE_BOOLEAN: {
            if (object->data.boolean) {
                return format_alloc(arena, "true");
            } else {
                return format_alloc(arena, "false");
            }
        }
        case PDF_OBJECT_TYPE_INTEGER: {
            return format_alloc(arena, "%d", object->data.integer);
        }
        case PDF_OBJECT_TYPE_REAL: {
            return format_alloc(arena, "%g", object->data.real);
        }
        case PDF_OBJECT_TYPE_STRING: {
            return format_alloc(arena, "(%s)", object->data.string);
        }
        case PDF_OBJECT_TYPE_NAME: {
            return format_alloc(arena, "/%s", object->data.name);
        }
        case PDF_OBJECT_TYPE_ARRAY: {
            PdfObjectVec* items = object->data.array.elements;
            size_t num_items = pdf_object_vec_len(items);
            if (num_items == 0) {
                return format_alloc(arena, "[]");
            }

            int length = indent;
            char** item_strs = arena_alloc(arena, sizeof(char*) * num_items);
            bool array_contains_indirect = false;
            for (size_t idx = 0; idx < num_items; idx++) {
                PdfObject* element;
                RELEASE_ASSERT(pdf_object_vec_get(items, idx, &element));

                item_strs[idx] = pdf_fmt_object_indented(
                    arena,
                    element,
                    indent + 2,
                    &array_contains_indirect
                );
                length += (int)strlen(item_strs[idx]);
            }

            if (length > 80 || array_contains_indirect) {
                char* buffer = format_alloc(arena, "[");
                for (size_t idx = 0; idx < num_items; idx++) {
                    char* new_buffer = format_alloc(
                        arena,
                        "%s\n  %*s%s",
                        buffer,
                        indent,
                        "",
                        item_strs[idx]
                    );
                    if (new_buffer) {
                        buffer = new_buffer;
                    } else {
                        LOG_ERROR("Format failed");
                        return NULL;
                    }
                }

                return format_alloc(arena, "%s\n%*s]", buffer, indent, "");
            } else {
                char* buffer = format_alloc(arena, "[");
                for (size_t idx = 0; idx < num_items; idx++) {
                    buffer =
                        format_alloc(arena, "%s %s", buffer, item_strs[idx]);
                }

                return format_alloc(arena, "%s ]", buffer);
            }
        }
        case PDF_OBJECT_TYPE_DICT: {
            PdfDictEntryVec* entries = object->data.dict.entries;

            if (pdf_dict_entry_vec_len(entries) == 0) {
                return format_alloc(arena, "<< >>");
            } else {
                char* buffer = format_alloc(arena, "<<");
                for (size_t idx = 0; idx < pdf_dict_entry_vec_len(entries);
                     idx++) {
                    PdfDictEntry entry;
                    RELEASE_ASSERT(pdf_dict_entry_vec_get(entries, idx, &entry)
                    );
                    char* key_text = pdf_fmt_object_indented(
                        arena,
                        entry.key,
                        indent + 2,
                        NULL
                    );
                    char* value_text = pdf_fmt_object_indented(
                        arena,
                        entry.value,
                        indent + (int)strlen(key_text) + 3,
                        NULL
                    );

                    buffer = format_alloc(
                        arena,
                        "%s\n  %*s%s %s",
                        buffer,
                        indent,
                        "",
                        key_text,
                        value_text
                    );
                }

                return format_alloc(arena, "%s\n%*s>>", buffer, indent, "");
            }
        }
        case PDF_OBJECT_TYPE_STREAM: {
            return format_alloc(
                arena,
                "%s\n%*sstream\n%*s...\n%*sendstream",
                pdf_fmt_object_indented(
                    arena,
                    object->data.stream.stream_dict,
                    indent,
                    NULL
                ),
                indent,
                "",
                indent,
                "",
                indent,
                ""
            );
        }
        case PDF_OBJECT_TYPE_INDIRECT_OBJECT: {
            if (force_indent_parent) {
                *force_indent_parent = true;
            }

            return format_alloc(
                arena,
                "%zu %zu obj\n%*s%s\n%*sendobj",
                object->data.indirect_object.object_id,
                object->data.indirect_object.generation,
                indent + 2,
                "",
                pdf_fmt_object_indented(
                    arena,
                    object->data.indirect_object.object,
                    indent + 2,
                    NULL
                ),
                indent,
                ""
            );
        }
        case PDF_OBJECT_TYPE_INDIRECT_REF: {
            if (force_indent_parent) {
                *force_indent_parent = true;
            }
            return format_alloc(
                arena,
                "%zu %zu R",
                object->data.indirect_ref.object_id,
                object->data.indirect_ref.generation
            );
        }
        case PDF_OBJECT_TYPE_NULL: {
            return format_alloc(arena, "null");
        }
    }
}

char* pdf_fmt_object(Arena* arena, PdfObject* object) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(object);

    Arena* temp_arena = arena_new(512);
    char* formatted = pdf_fmt_object_indented(temp_arena, object, 0, NULL);

    unsigned long len = strlen(formatted);
    char* allocated = arena_alloc(arena, sizeof(char) * (len + 1));
    strncpy(allocated, formatted, (size_t)len + 1);
    arena_free(temp_arena);

    return allocated;
}

#ifdef TEST
#include <string.h>

#include "test.h"

#define SETUP_VALID_PARSE_OBJECT_WITH_OFFSET(                                  \
    buf,                                                                       \
    object_type,                                                               \
    expected_offset                                                            \
)                                                                              \
    Arena* arena = arena_new(128);                                             \
    char buffer[] = buf;                                                       \
    PdfCtx* ctx = pdf_ctx_new(arena, buffer, strlen(buffer));                  \
    PdfObject object;                                                          \
    PdfResult result = pdf_parse_object(arena, ctx, &object, false);           \
    TEST_ASSERT_EQ((PdfResult)PDF_OK, result, "Result was not ok");            \
    TEST_ASSERT_EQ(                                                            \
        (PdfObjectType)(object_type),                                          \
        object.type,                                                           \
        "Parsed object has wrong type"                                         \
    );                                                                         \
    TEST_ASSERT_EQ(                                                            \
        (size_t)(expected_offset),                                             \
        pdf_ctx_offset(ctx),                                                   \
        "Incorrect offset after parsing"                                       \
    );

#define SETUP_VALID_PARSE_OBJECT(buf, object_type)                             \
    SETUP_VALID_PARSE_OBJECT_WITH_OFFSET(buf, object_type, strlen(buf))

#define SETUP_INVALID_PARSE_OBJECT(buf, err)                                   \
    Arena* arena = arena_new(128);                                             \
    char buffer[] = buf;                                                       \
    PdfCtx* ctx = pdf_ctx_new(arena, buffer, strlen(buffer));                  \
                                                                               \
    PdfObject object;                                                          \
    PdfResult result = pdf_parse_object(arena, ctx, &object, false);           \
    TEST_ASSERT_EQ((PdfResult)(err), result, "Incorrect error type");

TEST_FUNC(test_object_bool_true) {
    SETUP_VALID_PARSE_OBJECT("true", PDF_OBJECT_TYPE_BOOLEAN);
    TEST_ASSERT(object.data.boolean);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_bool_false) {
    SETUP_VALID_PARSE_OBJECT("false", PDF_OBJECT_TYPE_BOOLEAN);
    TEST_ASSERT(!object.data.boolean);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_null) {
    SETUP_VALID_PARSE_OBJECT("null", PDF_OBJECT_TYPE_NULL);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_int_123) {
    SETUP_VALID_PARSE_OBJECT("123", PDF_OBJECT_TYPE_INTEGER);
    TEST_ASSERT_EQ((int32_t)123, object.data.integer);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_int_plus_17) {
    SETUP_VALID_PARSE_OBJECT("+17", PDF_OBJECT_TYPE_INTEGER);
    TEST_ASSERT_EQ((int32_t)17, object.data.integer);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_int_minus_98) {
    SETUP_VALID_PARSE_OBJECT("-98", PDF_OBJECT_TYPE_INTEGER);
    TEST_ASSERT_EQ((int32_t)-98, object.data.integer);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_int_0) {
    SETUP_VALID_PARSE_OBJECT("0", PDF_OBJECT_TYPE_INTEGER);
    TEST_ASSERT_EQ((int32_t)0, object.data.integer);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_int_with_whitespace) {
    SETUP_VALID_PARSE_OBJECT_WITH_OFFSET("-98 ", PDF_OBJECT_TYPE_INTEGER, 3);
    TEST_ASSERT_EQ((int32_t)-98, object.data.integer);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_int_large) {
    SETUP_VALID_PARSE_OBJECT("2147483647", PDF_OBJECT_TYPE_INTEGER);
    TEST_ASSERT_EQ((int32_t)2147483647, object.data.integer);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_int_neg_large) {
    SETUP_VALID_PARSE_OBJECT("-2147483648", PDF_OBJECT_TYPE_INTEGER);
    TEST_ASSERT_EQ((int32_t)-2147483648, object.data.integer);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_int_overflow) {
    SETUP_INVALID_PARSE_OBJECT("2147483648", PDF_ERR_NUMBER_LIMIT);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_int_neg_overflow) {
    SETUP_INVALID_PARSE_OBJECT("-2147483649", PDF_ERR_NUMBER_LIMIT);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_int_plus_no_digits) {
    SETUP_INVALID_PARSE_OBJECT("+", PDF_ERR_INVALID_NUMBER);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_int_minus_no_digits) {
    SETUP_INVALID_PARSE_OBJECT("-", PDF_ERR_INVALID_NUMBER);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_real_34_5) {
    SETUP_VALID_PARSE_OBJECT("34.5", PDF_OBJECT_TYPE_REAL);
    TEST_ASSERT_EQ_EPS((double)34.5, object.data.real, 1e-5L);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_real_minus_3_62) {
    SETUP_VALID_PARSE_OBJECT("-3.62", PDF_OBJECT_TYPE_REAL);
    TEST_ASSERT_EQ_EPS((double)-3.62, object.data.real, 1e-5L);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_real_plus_123_6) {
    SETUP_VALID_PARSE_OBJECT("123.6", PDF_OBJECT_TYPE_REAL);
    TEST_ASSERT_EQ_EPS((double)123.6, object.data.real, 1e-5L);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_real_4_) {
    SETUP_VALID_PARSE_OBJECT("4.", PDF_OBJECT_TYPE_REAL);
    TEST_ASSERT_EQ_EPS((double)4.0, object.data.real, 1e-5L);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_real_minus_dot_002) {
    SETUP_VALID_PARSE_OBJECT("-.002", PDF_OBJECT_TYPE_REAL);
    TEST_ASSERT_EQ_EPS((double)-0.002, object.data.real, 1e-5L);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_real_0_0) {
    SETUP_VALID_PARSE_OBJECT("0.0", PDF_OBJECT_TYPE_REAL);
    TEST_ASSERT_EQ_EPS((double)0.0, object.data.real, 1e-5L);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_real_no_digits) {
    SETUP_INVALID_PARSE_OBJECT(".", PDF_ERR_INVALID_NUMBER);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_real_trailing_whitespace) {
    SETUP_VALID_PARSE_OBJECT_WITH_OFFSET("123.6 ", PDF_OBJECT_TYPE_REAL, 5);
    TEST_ASSERT_EQ_EPS((double)123.6, object.data.real, 1e-5L);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_literal_str) {
    SETUP_VALID_PARSE_OBJECT("(This is a string)", PDF_OBJECT_TYPE_STRING);
    TEST_ASSERT_EQ("This is a string", object.data.string);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_literal_str_newline) {
    SETUP_VALID_PARSE_OBJECT(
        "(Strings may contain newlines\nand such.)",
        PDF_OBJECT_TYPE_STRING
    );
    TEST_ASSERT_EQ(
        "Strings may contain newlines\nand such.",
        object.data.string
    );
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_literal_str_parenthesis) {
    SETUP_VALID_PARSE_OBJECT(
        "(Strings may contain balanced parentheses () and special characters (*!&^% and so on).)",
        PDF_OBJECT_TYPE_STRING
    );
    TEST_ASSERT_EQ(
        "Strings may contain balanced parentheses () and special characters (*!&^% and so on).",
        object.data.string
    );
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_literal_str_empty) {
    SETUP_VALID_PARSE_OBJECT("()", PDF_OBJECT_TYPE_STRING);
    TEST_ASSERT_EQ("", object.data.string);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_literal_str_escapes) {
    SETUP_VALID_PARSE_OBJECT(
        "(This is a line\\nThis is a newline\\r\\nThis is another line with tabs (\\t), backspaces(\\b), form feeds (\\f), unbalanced parenthesis \\(\\(\\(\\), and backslashes \\\\\\\\)",
        PDF_OBJECT_TYPE_STRING
    );
    TEST_ASSERT_EQ(
        "This is a line\nThis is a newline\r\nThis is another line with tabs (\t), backspaces(\b), form feeds (\f), unbalanced parenthesis (((), and backslashes \\\\",
        object.data.string
    );
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_literal_str_unbalanced) {
    SETUP_INVALID_PARSE_OBJECT("(", PDF_ERR_UNBALANCED_STR);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_name) {
    SETUP_VALID_PARSE_OBJECT("/Name1", PDF_OBJECT_TYPE_NAME);
    TEST_ASSERT_EQ("Name1", object.data.name);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_name_various_chars) {
    SETUP_VALID_PARSE_OBJECT(
        "/A;Name_With-Various***Characters?",
        PDF_OBJECT_TYPE_NAME
    );
    TEST_ASSERT_EQ("A;Name_With-Various***Characters?", object.data.name);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_name_dollars) {
    SETUP_VALID_PARSE_OBJECT("/$$", PDF_OBJECT_TYPE_NAME);
    TEST_ASSERT_EQ("$$", object.data.name);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_name_escape) {
    SETUP_VALID_PARSE_OBJECT(
        "/lime#20Green",
        PDF_OBJECT_TYPE_NAME
    ); // PDF 32000-1:2008 has a typo for this example
    TEST_ASSERT_EQ("lime Green", object.data.name);
    TEST_ASSERT_NE("Lime Green", object.data.name);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_name_terminated) {
    SETUP_VALID_PARSE_OBJECT_WITH_OFFSET(
        "/paired#28#29parentheses/",
        PDF_OBJECT_TYPE_NAME,
        24
    );
    TEST_ASSERT_EQ("paired()parentheses", object.data.name);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_name_whitespace_terminated) {
    SETUP_VALID_PARSE_OBJECT_WITH_OFFSET("/A#42 ", PDF_OBJECT_TYPE_NAME, 5);
    TEST_ASSERT_EQ("AB", object.data.name);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_name_invalid) {
    SETUP_INVALID_PARSE_OBJECT(
        "/The_Key_of_F#_Minor",
        PDF_ERR_NAME_BAD_CHAR_CODE
    );
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_array) {
    SETUP_VALID_PARSE_OBJECT(
        "[549 3.14 false (Ralph)/SomeName]",
        PDF_OBJECT_TYPE_ARRAY
    );

    PdfObject* element0;
    TEST_ASSERT(pdf_object_vec_get(object.data.array.elements, 0, &element0));
    TEST_ASSERT(element0);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_INTEGER, element0->type);
    TEST_ASSERT_EQ((int32_t)549, element0->data.integer);

    PdfObject* element1;
    TEST_ASSERT(pdf_object_vec_get(object.data.array.elements, 1, &element1));
    TEST_ASSERT(element1);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_REAL, element1->type);
    TEST_ASSERT_EQ(3.14, element1->data.real);

    PdfObject* element2;
    TEST_ASSERT(pdf_object_vec_get(object.data.array.elements, 2, &element2));
    TEST_ASSERT(element2);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_BOOLEAN, element2->type);
    TEST_ASSERT_EQ(false, element2->data.boolean);

    PdfObject* element3;
    TEST_ASSERT(pdf_object_vec_get(object.data.array.elements, 3, &element3));
    TEST_ASSERT(element3);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_STRING, element3->type);
    TEST_ASSERT_EQ("Ralph", element3->data.string);

    PdfObject* element4;
    TEST_ASSERT(pdf_object_vec_get(object.data.array.elements, 4, &element4));
    TEST_ASSERT(element4);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_NAME, element4->type);
    TEST_ASSERT_EQ("SomeName", element4->data.name);

    PdfObject* element5;
    TEST_ASSERT(!pdf_object_vec_get(object.data.array.elements, 5, &element5));

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_array_whitespace) {
    SETUP_VALID_PARSE_OBJECT(
        "[   549   3.14 false (Ralph)/SomeName  ]",
        PDF_OBJECT_TYPE_ARRAY
    );

    PdfObject* element0;
    TEST_ASSERT(pdf_object_vec_get(object.data.array.elements, 0, &element0));
    TEST_ASSERT(element0);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_INTEGER, element0->type);
    TEST_ASSERT_EQ((int32_t)549, element0->data.integer);

    PdfObject* element1;
    TEST_ASSERT(pdf_object_vec_get(object.data.array.elements, 1, &element1));
    TEST_ASSERT(element1);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_REAL, element1->type);
    TEST_ASSERT_EQ(3.14, element1->data.real);

    PdfObject* element2;
    TEST_ASSERT(pdf_object_vec_get(object.data.array.elements, 2, &element2));
    TEST_ASSERT(element2);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_BOOLEAN, element2->type);
    TEST_ASSERT_EQ(false, element2->data.boolean);

    PdfObject* element3;
    TEST_ASSERT(pdf_object_vec_get(object.data.array.elements, 3, &element3));
    TEST_ASSERT(element3);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_STRING, element3->type);
    TEST_ASSERT_EQ("Ralph", element3->data.string);

    PdfObject* element4;
    TEST_ASSERT(pdf_object_vec_get(object.data.array.elements, 4, &element4));
    TEST_ASSERT(element4);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_NAME, element4->type);
    TEST_ASSERT_EQ("SomeName", element4->data.name);

    PdfObject* element5;
    TEST_ASSERT(!pdf_object_vec_get(object.data.array.elements, 5, &element5));
    TEST_ASSERT(!element5);

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_array_nested) {
    SETUP_VALID_PARSE_OBJECT("[[1 2][3 4]]", PDF_OBJECT_TYPE_ARRAY);

    PdfObject* element0;
    TEST_ASSERT(pdf_object_vec_get(object.data.array.elements, 0, &element0));
    TEST_ASSERT(element0);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_ARRAY, element0->type);

    PdfObject* element00;
    TEST_ASSERT(pdf_object_vec_get(element0->data.array.elements, 0, &element00)
    );
    TEST_ASSERT(element00);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_INTEGER, element00->type);
    TEST_ASSERT_EQ((int32_t)1, element00->data.integer);

    PdfObject* element01;
    TEST_ASSERT(pdf_object_vec_get(element0->data.array.elements, 1, &element01)
    );
    TEST_ASSERT(element01);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_INTEGER, element01->type);
    TEST_ASSERT_EQ((int32_t)2, element01->data.integer);

    PdfObject* element1;
    TEST_ASSERT(pdf_object_vec_get(object.data.array.elements, 1, &element1));
    TEST_ASSERT(element1);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_ARRAY, element1->type);

    PdfObject* element10;
    TEST_ASSERT(pdf_object_vec_get(element1->data.array.elements, 0, &element10)
    );
    TEST_ASSERT(element10);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_INTEGER, element10->type);
    TEST_ASSERT_EQ((int32_t)3, element10->data.integer);

    PdfObject* element11;
    TEST_ASSERT(pdf_object_vec_get(element1->data.array.elements, 1, &element11)
    );
    TEST_ASSERT(element11);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_INTEGER, element11->type);
    TEST_ASSERT_EQ((int32_t)4, element11->data.integer);

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_array_empty) {
    SETUP_VALID_PARSE_OBJECT("[]", PDF_OBJECT_TYPE_ARRAY);
    TEST_ASSERT_EQ((size_t)0, pdf_object_vec_len(object.data.array.elements));

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_array_offset) {
    SETUP_VALID_PARSE_OBJECT_WITH_OFFSET("[]  ", PDF_OBJECT_TYPE_ARRAY, 2);
    TEST_ASSERT_EQ((size_t)0, pdf_object_vec_len(object.data.array.elements));

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_array_unterminated) {
    SETUP_INVALID_PARSE_OBJECT(
        "[549 3.14 false (Ralph)/SomeName",
        PDF_ERR_CTX_EOF
    );
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_dict) {
    SETUP_VALID_PARSE_OBJECT(
        "<< /Type /Example\n"
        "/Subtype /DictionaryExample\n"
        "/Version 0.01\n"
        "/IntegerItem 12/StringItem (a string)\n"
        "/Subdictionary << /Item1 0.4\n"
        "                  /Item2 true\n"
        "                  /LastItem (not!)"
        "                  /VeryLastItem (OK)\n"
        "               >>\n"
        ">>",
        PDF_OBJECT_TYPE_DICT
    );

    PdfDictEntry entry0;
    TEST_ASSERT(pdf_dict_entry_vec_get(object.data.dict.entries, 0, &entry0));
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_NAME, entry0.key->type);
    TEST_ASSERT_EQ("Type", entry0.key->data.name);
    TEST_ASSERT(entry0.value);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_NAME, entry0.value->type);
    TEST_ASSERT_EQ("Example", entry0.value->data.name);

    PdfDictEntry entry1;
    TEST_ASSERT(pdf_dict_entry_vec_get(object.data.dict.entries, 1, &entry1));
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_NAME, entry1.key->type);
    TEST_ASSERT_EQ("Subtype", entry1.key->data.name);
    TEST_ASSERT(entry1.value);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_NAME, entry1.value->type);
    TEST_ASSERT_EQ("DictionaryExample", entry1.value->data.name);

    PdfDictEntry entry2;
    TEST_ASSERT(pdf_dict_entry_vec_get(object.data.dict.entries, 2, &entry2));
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_NAME, entry2.key->type);
    TEST_ASSERT_EQ("Version", entry2.key->data.name);
    TEST_ASSERT(entry2.value);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_REAL, entry2.value->type);
    TEST_ASSERT_EQ(0.01, entry2.value->data.real);

    PdfDictEntry entry3;
    TEST_ASSERT(pdf_dict_entry_vec_get(object.data.dict.entries, 3, &entry3));
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_NAME, entry3.key->type);
    TEST_ASSERT_EQ("IntegerItem", entry3.key->data.name);
    TEST_ASSERT(entry3.value);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_INTEGER, entry3.value->type);
    TEST_ASSERT_EQ((int32_t)12, entry3.value->data.integer);

    PdfDictEntry entry4;
    TEST_ASSERT(pdf_dict_entry_vec_get(object.data.dict.entries, 4, &entry4));
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_NAME, entry4.key->type);
    TEST_ASSERT_EQ("StringItem", entry4.key->data.name);
    TEST_ASSERT(entry4.value);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_STRING, entry4.value->type);
    TEST_ASSERT_EQ("a string", entry4.value->data.string);

    PdfDictEntry entry5;
    TEST_ASSERT(pdf_dict_entry_vec_get(object.data.dict.entries, 5, &entry5));
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_NAME, entry5.key->type);
    TEST_ASSERT_EQ("Subdictionary", entry5.key->data.name);
    TEST_ASSERT(entry5.value);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_DICT, entry5.value->type);

    PdfDictEntryVec* subdict = entry5.value->data.dict.entries;

    PdfDictEntry sub0;
    TEST_ASSERT(pdf_dict_entry_vec_get(subdict, 0, &sub0));
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_NAME, sub0.key->type);
    TEST_ASSERT_EQ("Item1", sub0.key->data.name);
    TEST_ASSERT(sub0.value);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_REAL, sub0.value->type);
    TEST_ASSERT_EQ(0.4, sub0.value->data.real);

    PdfDictEntry sub1;
    TEST_ASSERT(pdf_dict_entry_vec_get(subdict, 1, &sub1));
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_NAME, sub1.key->type);
    TEST_ASSERT_EQ("Item2", sub1.key->data.name);
    TEST_ASSERT(sub1.value);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_BOOLEAN, sub1.value->type);
    TEST_ASSERT_EQ(true, sub1.value->data.boolean);

    PdfDictEntry sub2;
    TEST_ASSERT(pdf_dict_entry_vec_get(subdict, 2, &sub2));
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_NAME, sub2.key->type);
    TEST_ASSERT_EQ("LastItem", sub2.key->data.name);
    TEST_ASSERT(sub2.value);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_STRING, sub2.value->type);
    TEST_ASSERT_EQ("not!", sub2.value->data.string);

    PdfDictEntry sub3;
    TEST_ASSERT(pdf_dict_entry_vec_get(subdict, 3, &sub3));
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_NAME, sub3.key->type);
    TEST_ASSERT_EQ("VeryLastItem", sub3.key->data.name);
    TEST_ASSERT(sub3.value);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_STRING, sub3.value->type);
    TEST_ASSERT_EQ("OK", sub3.value->data.string);

    TEST_ASSERT_EQ((size_t)4, pdf_dict_entry_vec_len(subdict));
    TEST_ASSERT_EQ((size_t)6, pdf_dict_entry_vec_len(object.data.dict.entries));

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_dict_empty) {
    SETUP_VALID_PARSE_OBJECT("<<>>", PDF_OBJECT_TYPE_DICT);
    TEST_ASSERT_EQ((size_t)0, pdf_dict_entry_vec_len(object.data.dict.entries));

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_dict_unpadded) {
    SETUP_VALID_PARSE_OBJECT("<</A 1/B 2>>", PDF_OBJECT_TYPE_DICT);

    PdfDictEntry entry0;
    TEST_ASSERT(pdf_dict_entry_vec_get(object.data.dict.entries, 0, &entry0));
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_NAME, entry0.key->type);
    TEST_ASSERT_EQ("A", entry0.key->data.name);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_INTEGER, entry0.value->type);
    TEST_ASSERT_EQ((int32_t)1, entry0.value->data.integer);

    PdfDictEntry entry1;
    TEST_ASSERT(pdf_dict_entry_vec_get(object.data.dict.entries, 1, &entry1));
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_NAME, entry1.key->type);
    TEST_ASSERT_EQ("B", entry1.key->data.name);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_INTEGER, entry1.value->type);
    TEST_ASSERT_EQ((int32_t)2, entry1.value->data.integer);

    TEST_ASSERT_EQ((size_t)2, pdf_dict_entry_vec_len(object.data.dict.entries));
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_dict_offset) {
    SETUP_VALID_PARSE_OBJECT_WITH_OFFSET(
        "<< /X 10 >>    ",
        PDF_OBJECT_TYPE_DICT,
        11
    );
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_dict_unclosed) {
    SETUP_INVALID_PARSE_OBJECT("<< /Key (value)", PDF_ERR_CTX_EOF);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_dict_missing_value) {
    SETUP_INVALID_PARSE_OBJECT("<< /Key >>", PDF_ERR_INVALID_OBJECT);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_dict_non_name_key) {
    SETUP_INVALID_PARSE_OBJECT("<< 123 (value) >>", PDF_ERR_CTX_EXPECT);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_indirect) {
    SETUP_VALID_PARSE_OBJECT(
        "12 0 obj (Brillig) endobj",
        PDF_OBJECT_TYPE_INDIRECT_OBJECT
    );
    TEST_ASSERT_EQ((size_t)12, object.data.indirect_object.object_id);
    TEST_ASSERT_EQ((size_t)0, object.data.indirect_object.generation);
    TEST_ASSERT_EQ(
        (PdfObjectType)PDF_OBJECT_TYPE_STRING,
        object.data.indirect_object.object->type
    );
    TEST_ASSERT_EQ("Brillig", object.data.indirect_object.object->data.string);

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_unterminated) {
    SETUP_VALID_PARSE_OBJECT_WITH_OFFSET(
        "12 0 obj123endobj",
        PDF_OBJECT_TYPE_INTEGER,
        2
    );
    TEST_ASSERT_EQ((int32_t)12, object.data.integer);

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_indirect_nested) {
    SETUP_VALID_PARSE_OBJECT(
        "12 0 obj 54 3 obj /Name endobj endobj",
        PDF_OBJECT_TYPE_INDIRECT_OBJECT
    );
    TEST_ASSERT_EQ((size_t)12, object.data.indirect_object.object_id);
    TEST_ASSERT_EQ((size_t)0, object.data.indirect_object.generation);

    TEST_ASSERT_EQ(
        (PdfObjectType)PDF_OBJECT_TYPE_INDIRECT_OBJECT,
        object.data.indirect_object.object->type
    );
    TEST_ASSERT_EQ(
        (size_t)54,
        object.data.indirect_object.object->data.indirect_object.object_id
    );
    TEST_ASSERT_EQ(
        (size_t)3,
        object.data.indirect_object.object->data.indirect_object.generation
    );

    TEST_ASSERT_EQ(
        (PdfObjectType)PDF_OBJECT_TYPE_NAME,
        object.data.indirect_object.object->data.indirect_object.object->type
    );
    TEST_ASSERT_EQ(
        "Name",
        object.data.indirect_object.object->data.indirect_object.object->data
            .string
    );

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_indirect_ref) {
    SETUP_VALID_PARSE_OBJECT("12 0 R", PDF_OBJECT_TYPE_INDIRECT_REF);
    TEST_ASSERT_EQ((size_t)12, object.data.indirect_ref.object_id);
    TEST_ASSERT_EQ((size_t)0, object.data.indirect_ref.generation);

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_stream) {
    SETUP_VALID_PARSE_OBJECT(
        "0 0 obj << /Length 8 >> stream\n01234567\nendstream\n endobj",
        PDF_OBJECT_TYPE_INDIRECT_OBJECT
    );

    PdfObject* stream_object = object.data.indirect_object.object;
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_STREAM, stream_object->type);
    PdfStream stream = stream_object->data.stream;
    TEST_ASSERT(stream.stream_dict);
    TEST_ASSERT_EQ("01234567", stream.stream_bytes);

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_stream_crlf) {
    SETUP_VALID_PARSE_OBJECT(
        "0 0 obj << /Length 8 >> stream\r\n01234567\r\nendstream\r\n endobj",
        PDF_OBJECT_TYPE_INDIRECT_OBJECT
    );

    PdfObject* stream_object = object.data.indirect_object.object;
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_STREAM, stream_object->type);
    PdfStream stream = stream_object->data.stream;
    TEST_ASSERT(stream.stream_dict);
    TEST_ASSERT_EQ("01234567", stream.stream_bytes);

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_stream_cr) {
    SETUP_INVALID_PARSE_OBJECT(
        "0 0 obj << /Length 8 >> stream\r01234567\nendstream\n endobj",
        PDF_ERR_CTX_EXPECT
    );

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_stream_bad_length) {
    SETUP_INVALID_PARSE_OBJECT(
        "0 0 obj << /Length 7 >> stream\n01234567\nendstream\n endobj",
        PDF_ERR_CTX_EXPECT
    );

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_stream_bad_length2) {
    SETUP_INVALID_PARSE_OBJECT(
        "0 0 obj << /Length 9 >> stream\n01234567\nendstream\n endobj",
        PDF_ERR_CTX_EXPECT
    );

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_stream_no_line_end) {
    SETUP_INVALID_PARSE_OBJECT(
        "0 0 obj << /Length 9 >> stream\n01234567endstream\n endobj",
        PDF_ERR_CTX_EXPECT
    );

    return TEST_RESULT_PASS;
}

#endif // TEST
