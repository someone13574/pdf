#include "object.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include "ctx.h"
#include "log.h"
#include "pdf_object.h"
#include "pdf_result.h"
#include "vec.h"

PdfObject* pdf_parse_true(Arena* arena, PdfCtx* ctx, PdfResult* result);
PdfObject* pdf_parse_false(Arena* arena, PdfCtx* ctx, PdfResult* result);
PdfObject* pdf_parse_null(Arena* arena, PdfCtx* ctx, PdfResult* result);
PdfObject* pdf_parse_number(Arena* arena, PdfCtx* ctx, PdfResult* result);
PdfObject*
pdf_parse_string_literal(Arena* arena, PdfCtx* ctx, PdfResult* result);
PdfObject* pdf_parse_name(Arena* arena, PdfCtx* ctx, PdfResult* result);
PdfObject* pdf_parse_array(Arena* arena, PdfCtx* ctx, PdfResult* result);
PdfObject* pdf_parse_dict(
    Arena* arena,
    PdfCtx* ctx,
    PdfResult* result,
    bool in_direct_obj
);
PdfObject* pdf_parse_indirect(Arena* arena, PdfCtx* ctx, PdfResult* result);
char* pdf_parse_stream(
    Arena* arena,
    PdfCtx* ctx,
    PdfResult* result,
    Vec* stream_dict
);

PdfObject* pdf_parse_object(
    Arena* arena,
    PdfCtx* ctx,
    PdfResult* result,
    bool in_direct_obj
) {
    DBG_ASSERT(arena);
    DBG_ASSERT(ctx);
    DBG_ASSERT(result);

    LOG_DEBUG_G("object", "Parsing object at offset %zu", pdf_ctx_offset(ctx));

    *result = PDF_OK;

    char peeked, peeked_next;
    PDF_TRY_RET_NULL(pdf_ctx_peek(ctx, &peeked));

    PdfResult peeked_next_result = pdf_ctx_peek_next(ctx, &peeked_next);
    if (peeked == '<' && peeked_next_result != PDF_OK) {
        *result = peeked_next_result;
        return NULL;
    }

    if (peeked == 't') {
        return pdf_parse_true(arena, ctx, result);
    } else if (peeked == 'f') {
        return pdf_parse_false(arena, ctx, result);
    } else if (peeked == '.' || peeked == '+' || peeked == '-') {
        return pdf_parse_number(arena, ctx, result);
    } else if (isdigit((unsigned char)peeked)) {
        size_t restore_offset = pdf_ctx_offset(ctx);
        PdfObject* indirect = pdf_parse_indirect(arena, ctx, result);
        if (*result == PDF_OK) {
            return indirect;
        }

        LOG_DEBUG_G("object", "Failed to parse indirect object/reference");

        *result = PDF_OK;
        PDF_TRY_RET_NULL(pdf_ctx_seek(ctx, restore_offset));

        return pdf_parse_number(arena, ctx, result);
    } else if (peeked == '(') {
        return pdf_parse_string_literal(arena, ctx, result);
    } else if (peeked == '<' && peeked_next != '<') {
        LOG_TODO("Hexadecimal strings");
    } else if (peeked == '/') {
        return pdf_parse_name(arena, ctx, result);
    } else if (peeked == '[') {
        return pdf_parse_array(arena, ctx, result);
    } else if (peeked == '<' && peeked_next == '<') {
        return pdf_parse_dict(arena, ctx, result, in_direct_obj);
    } else if (peeked == 'n') {
        return pdf_parse_null(arena, ctx, result);
    }

    *result = PDF_ERR_INVALID_OBJECT;
    return NULL;
}

PdfObject* pdf_parse_true(Arena* arena, PdfCtx* ctx, PdfResult* result) {
    PDF_TRY_RET_NULL(pdf_ctx_expect(ctx, "true"));
    PDF_TRY_RET_NULL(pdf_ctx_require_char_type(ctx, true, &is_pdf_non_regular));

    PdfObject* object = arena_alloc(arena, sizeof(PdfObject));
    object->type = PDF_OBJECT_TYPE_BOOLEAN;
    object->data.boolean = true;

    return object;
}

PdfObject* pdf_parse_false(Arena* arena, PdfCtx* ctx, PdfResult* result) {
    PDF_TRY_RET_NULL(pdf_ctx_expect(ctx, "false"));
    PDF_TRY_RET_NULL(pdf_ctx_require_char_type(ctx, true, &is_pdf_non_regular));

    PdfObject* object = arena_alloc(arena, sizeof(PdfObject));
    object->type = PDF_OBJECT_TYPE_BOOLEAN;
    object->data.boolean = false;

    return object;
}

PdfObject* pdf_parse_null(Arena* arena, PdfCtx* ctx, PdfResult* result) {
    PDF_TRY_RET_NULL(pdf_ctx_expect(ctx, "null"));
    PDF_TRY_RET_NULL(pdf_ctx_require_char_type(ctx, true, &is_pdf_non_regular));

    PdfObject* object = arena_alloc(arena, sizeof(PdfObject));
    object->type = PDF_OBJECT_TYPE_NULL;

    return object;
}

PdfObject* pdf_parse_number(Arena* arena, PdfCtx* ctx, PdfResult* result) {
    LOG_TRACE_G("object", "Parsing number at offset %zu", pdf_ctx_offset(ctx));

    // Parse sign
    char sign_char;
    int32_t sign = 1;
    PDF_TRY_RET_NULL(pdf_ctx_peek(ctx, &sign_char));
    if (sign_char == '+' || sign_char == '-') {
        PDF_TRY_RET_NULL(pdf_ctx_peek_and_advance(ctx, NULL));

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

        PDF_TRY_RET_NULL(pdf_ctx_peek_and_advance(ctx, NULL));
    }

    // Parse decimal point
    char decimal_char;
    PdfResult decimal_peek_result = pdf_ctx_peek(ctx, &decimal_char);

    if ((decimal_peek_result == PDF_OK && decimal_char != '.')
        || decimal_peek_result == PDF_CTX_ERR_EOF) {
        LOG_TRACE_G("object", "Number is integer");
        PDF_TRY_RET_NULL(
            pdf_ctx_require_char_type(ctx, true, &is_pdf_non_regular)
        );

        if (!has_leading) {
            *result = PDF_ERR_INVALID_NUMBER;
            return NULL;
        }

        leading_acc *= sign;
        if (leading_acc < (int64_t)INT32_MIN
            || leading_acc > (int64_t)INT32_MAX) {
            *result = PDF_ERR_NUMBER_LIMIT;
            return NULL;
        }

        PdfObject* object = arena_alloc(arena, sizeof(PdfObject));
        object->type = PDF_OBJECT_TYPE_INTEGER;
        object->data.integer = (int32_t)leading_acc;
        return object;
    } else if (decimal_peek_result != PDF_OK) {
        *result = decimal_peek_result;
        return NULL;
    } else {
        PDF_TRY_RET_NULL(pdf_ctx_peek_and_advance(ctx, NULL));
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

        PDF_TRY_RET_NULL(pdf_ctx_peek_and_advance(ctx, NULL));
    }

    PDF_TRY_RET_NULL(pdf_ctx_require_char_type(ctx, true, &is_pdf_non_regular));

    double value = ((double)leading_acc + trailing_acc) * (double)sign;
    const double PDF_FLOAT_MAX = 3.403e38;

    if (value > PDF_FLOAT_MAX || value < -PDF_FLOAT_MAX) {
        *result = PDF_ERR_NUMBER_LIMIT;
        return NULL;
    }

    if (!has_leading && !has_trailing) {
        *result = PDF_ERR_INVALID_NUMBER;
        return NULL;
    }

    PdfObject* object = arena_alloc(arena, sizeof(PdfObject));
    object->type = PDF_OBJECT_TYPE_REAL;
    object->data.real = value;

    return object;
}

PdfObject*
pdf_parse_string_literal(Arena* arena, PdfCtx* ctx, PdfResult* result) {
    PDF_TRY_RET_NULL(pdf_ctx_expect(ctx, "("));

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
        *result = PDF_ERR_UNBALANCED_STR;
        return NULL;
    }

    PDF_TRY_RET_NULL(pdf_ctx_require_char_type(ctx, true, &is_pdf_non_regular));

    // Parse
    char* raw;
    PDF_TRY_RET_NULL(pdf_ctx_borrow_substr(
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

    PDF_TRY_RET_NULL(pdf_ctx_release_substr(ctx));
    parsed[write_offset] = '\0';

    PdfObject* object = arena_alloc(arena, sizeof(PdfObject));
    object->type = PDF_OBJECT_TYPE_STRING;
    object->data.string = parsed;

    return object;
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

PdfObject* pdf_parse_name(Arena* arena, PdfCtx* ctx, PdfResult* result) {
    PDF_TRY_RET_NULL(pdf_ctx_expect(ctx, "/"));

    // Find max length
    size_t start_offset = pdf_ctx_offset(ctx);
    size_t length = 0;
    char peeked;

    while (pdf_ctx_peek_and_advance(ctx, &peeked) == PDF_OK) {
        if (!is_pdf_regular(peeked)) {
            break;
        }

        if (peeked < '!' || peeked > '~') {
            *result = PDF_ERR_NAME_UNESCAPED_CHAR;
            return NULL;
        }

        length++;
    }

    LOG_DEBUG("Length %zu", length);
    PDF_TRY_RET_NULL(pdf_ctx_seek(ctx, start_offset + length));
    PDF_TRY_RET_NULL(pdf_ctx_require_char_type(ctx, true, &is_pdf_non_regular));

    // Parse name
    char* name = arena_alloc(arena, sizeof(char) * (length + 1));
    char* raw;
    PDF_TRY_RET_NULL(pdf_ctx_borrow_substr(ctx, start_offset, length, &raw));

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
                    *result = PDF_ERR_NAME_BAD_CHAR_CODE;
                    return NULL;
                }

                hex_code = value << 4;
                escape = 2;
                break;
            }
            case 2: {
                int value;
                if (!char_to_hex(c, &value)) {
                    *result = PDF_ERR_NAME_BAD_CHAR_CODE;
                    return NULL;
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
        *result = PDF_ERR_NAME_BAD_CHAR_CODE;
        return NULL;
    }

    PDF_TRY_RET_NULL(pdf_ctx_release_substr(ctx));
    name[write_offset] = '\0';

    PdfObject* object = arena_alloc(arena, sizeof(PdfObject));
    object->type = PDF_OBJECT_TYPE_NAME;
    object->data.name = name;

    return object;
}

PdfObject* pdf_parse_array(Arena* arena, PdfCtx* ctx, PdfResult* result) {
    PDF_TRY_RET_NULL(pdf_ctx_expect(ctx, "["));
    PDF_TRY_RET_NULL(pdf_ctx_consume_whitespace(ctx));

    Vec* elements = vec_new(arena);

    char peeked;
    while (pdf_ctx_peek(ctx, &peeked) == PDF_OK && peeked != ']') {
        PdfObject* element = pdf_parse_object(arena, ctx, result, false);
        if (*result != PDF_OK) {
            return NULL;
        }

        PDF_TRY_RET_NULL(
            pdf_ctx_require_char_type(ctx, false, is_pdf_non_regular)
        );
        PDF_TRY_RET_NULL(pdf_ctx_consume_whitespace(ctx));

        vec_push(elements, element);
    }

    PDF_TRY_RET_NULL(pdf_ctx_expect(ctx, "]"));
    PDF_TRY_RET_NULL(pdf_ctx_require_char_type(ctx, true, is_pdf_non_regular));

    PdfObject* object = arena_alloc(arena, sizeof(PdfObject));
    object->type = PDF_OBJECT_TYPE_ARRAY;
    object->data.array.elements = elements;

    return object;
}

PdfObject* pdf_parse_dict(
    Arena* arena,
    PdfCtx* ctx,
    PdfResult* result,
    bool in_direct_obj
) {
    PDF_TRY_RET_NULL(pdf_ctx_expect(ctx, "<<"));
    PDF_TRY_RET_NULL(pdf_ctx_consume_whitespace(ctx));

    Vec* entries = vec_new(arena);

    char peeked;
    while (pdf_ctx_peek(ctx, &peeked) == PDF_OK && peeked != '>') {
        PdfObject* key = pdf_parse_name(arena, ctx, result);
        if (*result != PDF_OK) {
            return NULL;
        }

        PDF_TRY_RET_NULL(
            pdf_ctx_require_char_type(ctx, false, is_pdf_non_regular)
        );
        PDF_TRY_RET_NULL(pdf_ctx_consume_whitespace(ctx));

        PdfObject* value = pdf_parse_object(arena, ctx, result, false);
        if (*result != PDF_OK) {
            return NULL;
        }

        PDF_TRY_RET_NULL(
            pdf_ctx_require_char_type(ctx, false, is_pdf_non_regular)
        );
        PDF_TRY_RET_NULL(pdf_ctx_consume_whitespace(ctx));

        PdfDictEntry* entry = arena_alloc(arena, sizeof(PdfDictEntry));
        entry->key = key;
        entry->value = value;

        vec_push(entries, entry);
    }

    PDF_TRY_RET_NULL(pdf_ctx_expect(ctx, ">>"));
    PDF_TRY_RET_NULL(pdf_ctx_require_char_type(ctx, true, is_pdf_non_regular));

    PdfObject* dict = arena_alloc(arena, sizeof(PdfObject));
    dict->type = PDF_OBJECT_TYPE_DICT;
    dict->data.dict.entries = entries;

    // Attempt to parse stream
    if (in_direct_obj) {
        size_t restore_offset = pdf_ctx_offset(ctx);

        PdfResult consume_result = pdf_ctx_consume_whitespace(ctx);
        if (consume_result != PDF_OK) {
            pdf_ctx_seek(ctx, restore_offset);
            goto not_stream;
        }

        PdfResult stream_result = PDF_OK;
        char* stream_bytes =
            pdf_parse_stream(arena, ctx, &stream_result, entries);
        if (stream_result != PDF_OK || !stream_bytes) {
            pdf_ctx_seek(ctx, restore_offset);
            goto not_stream;
        }

        PDF_TRY_RET_NULL(
            pdf_ctx_require_char_type(ctx, true, is_pdf_non_regular)
        );

        PdfObject* object = arena_alloc(arena, sizeof(PdfObject));
        object->type = PDF_OBJECT_TYPE_STREAM;
        object->data.stream.stream_dict = dict;
        object->data.stream.stream_bytes = stream_bytes;

        return object;
    }

    // Not a stream-object
not_stream: { return dict; }
}

char* pdf_parse_stream(
    Arena* arena,
    PdfCtx* ctx,
    PdfResult* result,
    Vec* stream_dict
) {
    // Parse start
    PDF_TRY_RET_NULL(pdf_ctx_expect(ctx, "stream"));

    char peeked_newline;
    PDF_TRY_RET_NULL(pdf_ctx_peek(ctx, &peeked_newline));
    if (peeked_newline == '\n') {
        PDF_TRY_RET_NULL(pdf_ctx_shift(ctx, 1));
    } else if (peeked_newline == '\r') {
        PDF_TRY_RET_NULL(pdf_ctx_expect(ctx, "\r\n"));
    } else {
        *result = PDF_CTX_ERR_EXPECT;
        return NULL;
    }

    // Get length
    int32_t length = -1;
    for (size_t entry_idx = 0; entry_idx < vec_len(stream_dict); entry_idx++) {
        PdfDictEntry* entry = vec_get(stream_dict, entry_idx);
        if (!entry || !entry->key || !entry->value
            || entry->value->type != PDF_OBJECT_TYPE_INTEGER
            || strcmp("Length", entry->key->data.name) != 0) {
            continue;
        }

        length = entry->value->data.integer;
    }

    if (length < 0) {
        *result = PDF_ERR_STREAM_INVALID_LENGTH;
        return NULL;
    }

    // Copy stream body
    char* borrowed;
    PDF_TRY_RET_NULL(pdf_ctx_borrow_substr(
        ctx,
        pdf_ctx_offset(ctx),
        (size_t)length,
        &borrowed
    ));

    char* body = arena_alloc(arena, sizeof(char) * (size_t)(length + 1));
    strncpy(body, borrowed, (size_t)length);
    body[length] = '\0';

    PDF_TRY_RET_NULL(pdf_ctx_release_substr(ctx));

    // Parse end
    PDF_TRY_RET_NULL(pdf_ctx_shift(ctx, length));
    if (pdf_ctx_expect(ctx, "\nendstream") != PDF_OK
        && pdf_ctx_expect(ctx, "\r\nendstream") != PDF_OK
        && pdf_ctx_expect(ctx, "\rendstream") != PDF_OK) {
        *result = PDF_CTX_ERR_EXPECT;
        return NULL;
    }

    return body;
}

PdfObject* pdf_parse_indirect(Arena* arena, PdfCtx* ctx, PdfResult* result) {
    LOG_DEBUG_G("object", "Parsing indirect object or reference");

    // Parse object id
    uint64_t object_id;
    uint32_t int_length;
    PDF_TRY_RET_NULL(pdf_ctx_parse_int(ctx, NULL, &object_id, &int_length));
    if (int_length == 0) {
        *result = PDF_CTX_ERR_EXPECT;
        return NULL;
    }

    PDF_TRY_RET_NULL(pdf_ctx_expect(ctx, " "));

    // Parse generation
    uint64_t generation;
    PDF_TRY_RET_NULL(pdf_ctx_parse_int(ctx, NULL, &generation, &int_length));
    if (int_length == 0) {
        *result = PDF_CTX_ERR_EXPECT;
        return NULL;
    }

    PDF_TRY_RET_NULL(pdf_ctx_expect(ctx, " "));

    // Determine if indirect object or reference
    char peeked;
    PDF_TRY_RET_NULL(pdf_ctx_peek(ctx, &peeked));

    if (peeked == 'R') {
        PDF_TRY_RET_NULL(pdf_ctx_peek_and_advance(ctx, NULL));

        LOG_DEBUG_G("object", "Parsed indirect reference");
        PdfObject* object = arena_alloc(arena, sizeof(PdfObject));
        object->type = PDF_OBJECT_TYPE_INDIRECT_REF;
        object->data.indirect_ref.object_id = object_id;
        object->data.indirect_ref.generation = generation;

        return object;
    } else {
        LOG_DEBUG_G("object", "Parsed indirect object");

        PDF_TRY_RET_NULL(pdf_ctx_expect(ctx, "obj"));
        PDF_TRY_RET_NULL(
            pdf_ctx_require_char_type(ctx, false, &is_pdf_non_regular)
        );
        PDF_TRY_RET_NULL(pdf_ctx_consume_whitespace(ctx));

        PdfObject* inner = pdf_parse_object(arena, ctx, result, true);
        if (*result != PDF_OK) {
            LOG_DEBUG_G(
                "object",
                "Failed to parsing inner of indirect object with result %d",
                *result
            );
            return NULL;
        }

        PDF_TRY_RET_NULL(
            pdf_ctx_require_char_type(ctx, false, &is_pdf_non_regular)
        );
        PDF_TRY_RET_NULL(pdf_ctx_consume_whitespace(ctx));
        PDF_TRY_RET_NULL(pdf_ctx_expect(ctx, "endobj"));
        PDF_TRY_RET_NULL(
            pdf_ctx_require_char_type(ctx, true, &is_pdf_non_regular)
        );

        PdfObject* object = arena_alloc(arena, sizeof(PdfObject));
        object->type = PDF_OBJECT_TYPE_INDIRECT_OBJECT;
        object->data.indirect_object.object = inner;
        object->data.indirect_object.object_id = object_id;
        object->data.indirect_object.generation = generation;

        return object;
    }

    return NULL;
}

char* format_alloc(Arena* arena, const char* fmt, ...)
    __attribute__((format(printf, 2, 3)));

char* format_alloc(Arena* arena, const char* fmt, ...) {
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

PdfObject*
pdf_object_dict_get(PdfObject* dict, const char* key, PdfResult* result) {
    if (!dict || !key || !result) {
        return NULL;
    }

    *result = PDF_OK;

    if (dict->type != PDF_OBJECT_TYPE_DICT) {
        *result = PDF_ERR_OBJECT_NOT_DICT;
        return NULL;
    }

    Vec* entries = dict->data.dict.entries;
    for (size_t idx = 0; idx < vec_len(entries); idx++) {
        PdfDictEntry* entry = vec_get(entries, idx);
        if (strcmp(entry->key->data.name, key) == 0) {
            return entry->value;
        }
    }

    *result = PDF_ERR_MISSING_DICT_KEY;
    return NULL;
}

char* pdf_fmt_object_indented(
    Arena* arena,
    PdfObject* object,
    int indent,
    bool* force_indent_parent
) {
    if (!object) {
        return NULL;
    }

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
            Vec* items = object->data.array.elements;
            size_t num_items = vec_len(items);
            if (num_items == 0) {
                return format_alloc(arena, "[]");
            }

            int length = indent;
            char** item_strs = arena_alloc(arena, sizeof(char*) * num_items);
            bool array_contains_indirect = false;
            for (size_t idx = 0; idx < num_items; idx++) {
                item_strs[idx] = pdf_fmt_object_indented(
                    arena,
                    vec_get(items, idx),
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
            Vec* entries = object->data.dict.entries;

            if (vec_len(entries) == 0) {
                return format_alloc(arena, "<< >>");
            } else {
                char* buffer = format_alloc(arena, "<<");
                for (size_t idx = 0; idx < vec_len(entries); idx++) {
                    PdfDictEntry* entry = vec_get(entries, idx);
                    char* key_text = pdf_fmt_object_indented(
                        arena,
                        entry->key,
                        indent + 2,
                        NULL
                    );
                    char* value_text = pdf_fmt_object_indented(
                        arena,
                        entry->value,
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
    if (!arena) {
        arena = arena_new(64);
        char* formatted = pdf_fmt_object_indented(arena, object, 0, NULL);
        char* copy = strdup(formatted);
        arena_free(arena);

        return copy;
    } else {
        return pdf_fmt_object_indented(arena, object, 0, NULL);
    }
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
    PdfResult result;                                                          \
    PdfObject* object = pdf_parse_object(arena, ctx, &result, false);          \
    TEST_ASSERT_EQ((PdfResult)PDF_OK, result, "Result was not ok");            \
    TEST_ASSERT(object, "PdfObject pointer is NULL");                          \
    TEST_ASSERT_EQ(                                                            \
        (PdfObjectType)(object_type),                                          \
        object->type,                                                          \
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
    PdfResult result;                                                          \
    PdfObject* object = pdf_parse_object(arena, ctx, &result, false);          \
    TEST_ASSERT_EQ((PdfResult)(err), result, "Incorrect error type");          \
    TEST_ASSERT(!object, "PdfObject pointer isn't NULL");

TEST_FUNC(test_object_bool_true) {
    SETUP_VALID_PARSE_OBJECT("true", PDF_OBJECT_TYPE_BOOLEAN);
    TEST_ASSERT(object->data.boolean);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_bool_false) {
    SETUP_VALID_PARSE_OBJECT("false", PDF_OBJECT_TYPE_BOOLEAN);
    TEST_ASSERT(!object->data.boolean);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_null) {
    SETUP_VALID_PARSE_OBJECT("null", PDF_OBJECT_TYPE_NULL);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_int_123) {
    SETUP_VALID_PARSE_OBJECT("123", PDF_OBJECT_TYPE_INTEGER);
    TEST_ASSERT_EQ((int32_t)123, object->data.integer);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_int_plus_17) {
    SETUP_VALID_PARSE_OBJECT("+17", PDF_OBJECT_TYPE_INTEGER);
    TEST_ASSERT_EQ((int32_t)17, object->data.integer);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_int_minus_98) {
    SETUP_VALID_PARSE_OBJECT("-98", PDF_OBJECT_TYPE_INTEGER);
    TEST_ASSERT_EQ((int32_t)-98, object->data.integer);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_int_0) {
    SETUP_VALID_PARSE_OBJECT("0", PDF_OBJECT_TYPE_INTEGER);
    TEST_ASSERT_EQ((int32_t)0, object->data.integer);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_int_with_whitespace) {
    SETUP_VALID_PARSE_OBJECT_WITH_OFFSET("-98 ", PDF_OBJECT_TYPE_INTEGER, 3);
    TEST_ASSERT_EQ((int32_t)-98, object->data.integer);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_int_large) {
    SETUP_VALID_PARSE_OBJECT("2147483647", PDF_OBJECT_TYPE_INTEGER);
    TEST_ASSERT_EQ((int32_t)2147483647, object->data.integer);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_int_neg_large) {
    SETUP_VALID_PARSE_OBJECT("-2147483648", PDF_OBJECT_TYPE_INTEGER);
    TEST_ASSERT_EQ((int32_t)-2147483648, object->data.integer);
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
    TEST_ASSERT_EQ_EPS((double)34.5, object->data.real, 1e-5L);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_real_minus_3_62) {
    SETUP_VALID_PARSE_OBJECT("-3.62", PDF_OBJECT_TYPE_REAL);
    TEST_ASSERT_EQ_EPS((double)-3.62, object->data.real, 1e-5L);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_real_plus_123_6) {
    SETUP_VALID_PARSE_OBJECT("123.6", PDF_OBJECT_TYPE_REAL);
    TEST_ASSERT_EQ_EPS((double)123.6, object->data.real, 1e-5L);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_real_4_) {
    SETUP_VALID_PARSE_OBJECT("4.", PDF_OBJECT_TYPE_REAL);
    TEST_ASSERT_EQ_EPS((double)4.0, object->data.real, 1e-5L);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_real_minus_dot_002) {
    SETUP_VALID_PARSE_OBJECT("-.002", PDF_OBJECT_TYPE_REAL);
    TEST_ASSERT_EQ_EPS((double)-0.002, object->data.real, 1e-5L);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_real_0_0) {
    SETUP_VALID_PARSE_OBJECT("0.0", PDF_OBJECT_TYPE_REAL);
    TEST_ASSERT_EQ_EPS((double)0.0, object->data.real, 1e-5L);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_real_no_digits) {
    SETUP_INVALID_PARSE_OBJECT(".", PDF_ERR_INVALID_NUMBER);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_real_trailing_whitespace) {
    SETUP_VALID_PARSE_OBJECT_WITH_OFFSET("123.6 ", PDF_OBJECT_TYPE_REAL, 5);
    TEST_ASSERT_EQ_EPS((double)123.6, object->data.real, 1e-5L);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_literal_str) {
    SETUP_VALID_PARSE_OBJECT("(This is a string)", PDF_OBJECT_TYPE_STRING);
    TEST_ASSERT_EQ("This is a string", object->data.string);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_literal_str_newline) {
    SETUP_VALID_PARSE_OBJECT(
        "(Strings may contain newlines\nand such.)",
        PDF_OBJECT_TYPE_STRING
    );
    TEST_ASSERT_EQ(
        "Strings may contain newlines\nand such.",
        object->data.string
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
        object->data.string
    );
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_literal_str_empty) {
    SETUP_VALID_PARSE_OBJECT("()", PDF_OBJECT_TYPE_STRING);
    TEST_ASSERT_EQ("", object->data.string);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_literal_str_escapes) {
    SETUP_VALID_PARSE_OBJECT(
        "(This is a line\\nThis is a newline\\r\\nThis is another line with tabs (\\t), backspaces(\\b), form feeds (\\f), unbalanced parenthesis \\(\\(\\(\\), and backslashes \\\\\\\\)",
        PDF_OBJECT_TYPE_STRING
    );
    TEST_ASSERT_EQ(
        "This is a line\nThis is a newline\r\nThis is another line with tabs (\t), backspaces(\b), form feeds (\f), unbalanced parenthesis (((), and backslashes \\\\",
        object->data.string
    );
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_literal_str_unbalanced) {
    SETUP_INVALID_PARSE_OBJECT("(", PDF_ERR_UNBALANCED_STR);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_name) {
    SETUP_VALID_PARSE_OBJECT("/Name1", PDF_OBJECT_TYPE_NAME);
    TEST_ASSERT_EQ("Name1", object->data.name);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_name_various_chars) {
    SETUP_VALID_PARSE_OBJECT(
        "/A;Name_With-Various***Characters?",
        PDF_OBJECT_TYPE_NAME
    );
    TEST_ASSERT_EQ("A;Name_With-Various***Characters?", object->data.name);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_name_dollars) {
    SETUP_VALID_PARSE_OBJECT("/$$", PDF_OBJECT_TYPE_NAME);
    TEST_ASSERT_EQ("$$", object->data.name);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_name_escape) {
    SETUP_VALID_PARSE_OBJECT(
        "/lime#20Green",
        PDF_OBJECT_TYPE_NAME
    ); // PDF 32000-1:2008 has a typo for this example
    TEST_ASSERT_EQ("lime Green", object->data.name);
    TEST_ASSERT_NE("Lime Green", object->data.name);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_name_terminated) {
    SETUP_VALID_PARSE_OBJECT_WITH_OFFSET(
        "/paired#28#29parentheses/",
        PDF_OBJECT_TYPE_NAME,
        24
    );
    TEST_ASSERT_EQ("paired()parentheses", object->data.name);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_name_whitespace_terminated) {
    SETUP_VALID_PARSE_OBJECT_WITH_OFFSET("/A#42 ", PDF_OBJECT_TYPE_NAME, 5);
    TEST_ASSERT_EQ("AB", object->data.name);
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

    PdfObject* element0 = vec_get(object->data.array.elements, 0);
    TEST_ASSERT(element0);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_INTEGER, element0->type);
    TEST_ASSERT_EQ((int32_t)549, element0->data.integer);

    PdfObject* element1 = vec_get(object->data.array.elements, 1);
    TEST_ASSERT(element1);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_REAL, element1->type);
    TEST_ASSERT_EQ(3.14, element1->data.real);

    PdfObject* element2 = vec_get(object->data.array.elements, 2);
    TEST_ASSERT(element2);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_BOOLEAN, element2->type);
    TEST_ASSERT_EQ(false, element2->data.boolean);

    PdfObject* element3 = vec_get(object->data.array.elements, 3);
    TEST_ASSERT(element3);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_STRING, element3->type);
    TEST_ASSERT_EQ("Ralph", element3->data.string);

    PdfObject* element4 = vec_get(object->data.array.elements, 4);
    TEST_ASSERT(element4);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_NAME, element4->type);
    TEST_ASSERT_EQ("SomeName", element4->data.name);

    TEST_ASSERT(!vec_get(object->data.array.elements, 5));

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_array_whitespace) {
    SETUP_VALID_PARSE_OBJECT(
        "[   549   3.14 false (Ralph)/SomeName  ]",
        PDF_OBJECT_TYPE_ARRAY
    );

    PdfObject* element0 = vec_get(object->data.array.elements, 0);
    TEST_ASSERT(element0);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_INTEGER, element0->type);
    TEST_ASSERT_EQ((int32_t)549, element0->data.integer);

    PdfObject* element1 = vec_get(object->data.array.elements, 1);
    TEST_ASSERT(element1);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_REAL, element1->type);
    TEST_ASSERT_EQ(3.14, element1->data.real);

    PdfObject* element2 = vec_get(object->data.array.elements, 2);
    TEST_ASSERT(element2);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_BOOLEAN, element2->type);
    TEST_ASSERT_EQ(false, element2->data.boolean);

    PdfObject* element3 = vec_get(object->data.array.elements, 3);
    TEST_ASSERT(element3);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_STRING, element3->type);
    TEST_ASSERT_EQ("Ralph", element3->data.string);

    PdfObject* element4 = vec_get(object->data.array.elements, 4);
    TEST_ASSERT(element4);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_NAME, element4->type);
    TEST_ASSERT_EQ("SomeName", element4->data.name);

    TEST_ASSERT(!vec_get(object->data.array.elements, 5));

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_array_nested) {
    SETUP_VALID_PARSE_OBJECT("[[1 2][3 4]]", PDF_OBJECT_TYPE_ARRAY);

    PdfObject* element0 = vec_get(object->data.array.elements, 0);
    TEST_ASSERT(element0);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_ARRAY, element0->type);

    PdfObject* element00 = vec_get(element0->data.array.elements, 0);
    TEST_ASSERT(element00);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_INTEGER, element00->type);
    TEST_ASSERT_EQ((int32_t)1, element00->data.integer);

    PdfObject* element01 = vec_get(element0->data.array.elements, 1);
    TEST_ASSERT(element01);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_INTEGER, element01->type);
    TEST_ASSERT_EQ((int32_t)2, element01->data.integer);

    PdfObject* element1 = vec_get(object->data.array.elements, 1);
    TEST_ASSERT(element1);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_ARRAY, element1->type);

    PdfObject* element10 = vec_get(element1->data.array.elements, 0);
    TEST_ASSERT(element10);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_INTEGER, element10->type);
    TEST_ASSERT_EQ((int32_t)3, element10->data.integer);

    PdfObject* element11 = vec_get(element1->data.array.elements, 1);
    TEST_ASSERT(element11);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_INTEGER, element11->type);
    TEST_ASSERT_EQ((int32_t)4, element11->data.integer);

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_array_empty) {
    SETUP_VALID_PARSE_OBJECT("[]", PDF_OBJECT_TYPE_ARRAY);
    TEST_ASSERT_EQ((size_t)0, vec_len(object->data.array.elements));

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_array_offset) {
    SETUP_VALID_PARSE_OBJECT_WITH_OFFSET("[]  ", PDF_OBJECT_TYPE_ARRAY, 2);
    TEST_ASSERT_EQ((size_t)0, vec_len(object->data.array.elements));

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_array_unterminated) {
    SETUP_INVALID_PARSE_OBJECT(
        "[549 3.14 false (Ralph)/SomeName",
        PDF_CTX_ERR_EOF
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

    PdfDictEntry* entry0 = vec_get(object->data.dict.entries, 0);
    TEST_ASSERT(entry0);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_NAME, entry0->key->type);
    TEST_ASSERT_EQ("Type", entry0->key->data.name);
    TEST_ASSERT(entry0->value);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_NAME, entry0->value->type);
    TEST_ASSERT_EQ("Example", entry0->value->data.name);

    PdfDictEntry* entry1 = vec_get(object->data.dict.entries, 1);
    TEST_ASSERT(entry1);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_NAME, entry1->key->type);
    TEST_ASSERT_EQ("Subtype", entry1->key->data.name);
    TEST_ASSERT(entry1->value);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_NAME, entry1->value->type);
    TEST_ASSERT_EQ("DictionaryExample", entry1->value->data.name);

    PdfDictEntry* entry2 = vec_get(object->data.dict.entries, 2);
    TEST_ASSERT(entry2);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_NAME, entry2->key->type);
    TEST_ASSERT_EQ("Version", entry2->key->data.name);
    TEST_ASSERT(entry2->value);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_REAL, entry2->value->type);
    TEST_ASSERT_EQ(0.01, entry2->value->data.real);

    PdfDictEntry* entry3 = vec_get(object->data.dict.entries, 3);
    TEST_ASSERT(entry3);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_NAME, entry3->key->type);
    TEST_ASSERT_EQ("IntegerItem", entry3->key->data.name);
    TEST_ASSERT(entry3->value);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_INTEGER, entry3->value->type);
    TEST_ASSERT_EQ((int32_t)12, entry3->value->data.integer);

    PdfDictEntry* entry4 = vec_get(object->data.dict.entries, 4);
    TEST_ASSERT(entry4);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_NAME, entry4->key->type);
    TEST_ASSERT_EQ("StringItem", entry4->key->data.name);
    TEST_ASSERT(entry4->value);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_STRING, entry4->value->type);
    TEST_ASSERT_EQ("a string", entry4->value->data.string);

    PdfDictEntry* entry5 = vec_get(object->data.dict.entries, 5);
    TEST_ASSERT(entry5);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_NAME, entry5->key->type);
    TEST_ASSERT_EQ("Subdictionary", entry5->key->data.name);
    TEST_ASSERT(entry5->value);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_DICT, entry5->value->type);

    Vec* subdict = entry5->value->data.dict.entries;

    PdfDictEntry* sub0 = vec_get(subdict, 0);
    TEST_ASSERT(sub0);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_NAME, sub0->key->type);
    TEST_ASSERT_EQ("Item1", sub0->key->data.name);
    TEST_ASSERT(sub0->value);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_REAL, sub0->value->type);
    TEST_ASSERT_EQ(0.4, sub0->value->data.real);

    PdfDictEntry* sub1 = vec_get(subdict, 1);
    TEST_ASSERT(sub1);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_NAME, sub1->key->type);
    TEST_ASSERT_EQ("Item2", sub1->key->data.name);
    TEST_ASSERT(sub1->value);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_BOOLEAN, sub1->value->type);
    TEST_ASSERT_EQ(true, sub1->value->data.boolean);

    PdfDictEntry* sub2 = vec_get(subdict, 2);
    TEST_ASSERT(sub2);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_NAME, sub2->key->type);
    TEST_ASSERT_EQ("LastItem", sub2->key->data.name);
    TEST_ASSERT(sub2->value);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_STRING, sub2->value->type);
    TEST_ASSERT_EQ("not!", sub2->value->data.string);

    PdfDictEntry* sub3 = vec_get(subdict, 3);
    TEST_ASSERT(sub3);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_NAME, sub3->key->type);
    TEST_ASSERT_EQ("VeryLastItem", sub3->key->data.name);
    TEST_ASSERT(sub3->value);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_STRING, sub3->value->type);
    TEST_ASSERT_EQ("OK", sub3->value->data.string);

    TEST_ASSERT_EQ((size_t)4, vec_len(subdict));
    TEST_ASSERT_EQ((size_t)6, vec_len(object->data.dict.entries));

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_dict_empty) {
    SETUP_VALID_PARSE_OBJECT("<<>>", PDF_OBJECT_TYPE_DICT);
    TEST_ASSERT_EQ((size_t)0, vec_len(object->data.dict.entries));

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_dict_unpadded) {
    SETUP_VALID_PARSE_OBJECT("<</A 1/B 2>>", PDF_OBJECT_TYPE_DICT);

    PdfDictEntry* entry0 = vec_get(object->data.dict.entries, 0);
    TEST_ASSERT(entry0);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_NAME, entry0->key->type);
    TEST_ASSERT_EQ("A", entry0->key->data.name);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_INTEGER, entry0->value->type);
    TEST_ASSERT_EQ((int32_t)1, entry0->value->data.integer);

    PdfDictEntry* entry1 = vec_get(object->data.dict.entries, 1);
    TEST_ASSERT(entry1);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_NAME, entry1->key->type);
    TEST_ASSERT_EQ("B", entry1->key->data.name);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_INTEGER, entry1->value->type);
    TEST_ASSERT_EQ((int32_t)2, entry1->value->data.integer);

    TEST_ASSERT_EQ((size_t)2, vec_len(object->data.dict.entries));
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
    SETUP_INVALID_PARSE_OBJECT("<< /Key (value)", PDF_CTX_ERR_EOF);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_dict_missing_value) {
    SETUP_INVALID_PARSE_OBJECT("<< /Key >>", PDF_ERR_INVALID_OBJECT);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_dict_non_name_key) {
    SETUP_INVALID_PARSE_OBJECT("<< 123 (value) >>", PDF_CTX_ERR_EXPECT);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_indirect) {
    SETUP_VALID_PARSE_OBJECT(
        "12 0 obj (Brillig) endobj",
        PDF_OBJECT_TYPE_INDIRECT_OBJECT
    );
    TEST_ASSERT_EQ((size_t)12, object->data.indirect_object.object_id);
    TEST_ASSERT_EQ((size_t)0, object->data.indirect_object.generation);
    TEST_ASSERT_EQ(
        (PdfObjectType)PDF_OBJECT_TYPE_STRING,
        object->data.indirect_object.object->type
    );
    TEST_ASSERT_EQ("Brillig", object->data.indirect_object.object->data.string);

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_unterminated) {
    SETUP_VALID_PARSE_OBJECT_WITH_OFFSET(
        "12 0 obj123endobj",
        PDF_OBJECT_TYPE_INTEGER,
        2
    );
    TEST_ASSERT_EQ((int32_t)12, object->data.integer);

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_indirect_nested) {
    SETUP_VALID_PARSE_OBJECT(
        "12 0 obj 54 3 obj /Name endobj endobj",
        PDF_OBJECT_TYPE_INDIRECT_OBJECT
    );
    TEST_ASSERT_EQ((size_t)12, object->data.indirect_object.object_id);
    TEST_ASSERT_EQ((size_t)0, object->data.indirect_object.generation);

    TEST_ASSERT_EQ(
        (PdfObjectType)PDF_OBJECT_TYPE_INDIRECT_OBJECT,
        object->data.indirect_object.object->type
    );
    TEST_ASSERT_EQ(
        (size_t)54,
        object->data.indirect_object.object->data.indirect_object.object_id
    );
    TEST_ASSERT_EQ(
        (size_t)3,
        object->data.indirect_object.object->data.indirect_object.generation
    );

    TEST_ASSERT_EQ(
        (PdfObjectType)PDF_OBJECT_TYPE_NAME,
        object->data.indirect_object.object->data.indirect_object.object->type
    );
    TEST_ASSERT_EQ(
        "Name",
        object->data.indirect_object.object->data.indirect_object.object->data
            .string
    );

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_indirect_ref) {
    SETUP_VALID_PARSE_OBJECT("12 0 R", PDF_OBJECT_TYPE_INDIRECT_REF);
    TEST_ASSERT_EQ((size_t)12, object->data.indirect_ref.object_id);
    TEST_ASSERT_EQ((size_t)0, object->data.indirect_ref.generation);

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_stream) {
    SETUP_VALID_PARSE_OBJECT(
        "0 0 obj << /Length 8 >> stream\n01234567\nendstream\n endobj",
        PDF_OBJECT_TYPE_INDIRECT_OBJECT
    );

    PdfObject* stream_object = object->data.indirect_object.object;
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

    PdfObject* stream_object = object->data.indirect_object.object;
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_STREAM, stream_object->type);
    PdfStream stream = stream_object->data.stream;
    TEST_ASSERT(stream.stream_dict);
    TEST_ASSERT_EQ("01234567", stream.stream_bytes);

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_stream_cr) {
    // Stream parsing fails and it falls back to the integer
    SETUP_VALID_PARSE_OBJECT_WITH_OFFSET(
        "0 0 obj << /Length 8 >> stream\r01234567\nendstream\n endobj",
        PDF_OBJECT_TYPE_INTEGER,
        1
    );

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_stream_bad_length) {
    // Stream parsing fails and it falls back to the integer
    SETUP_VALID_PARSE_OBJECT_WITH_OFFSET(
        "0 0 obj << /Length 7 >> stream\n01234567\nendstream\n endobj",
        PDF_OBJECT_TYPE_INTEGER,
        1
    );

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_stream_bad_length2) {
    // Stream parsing fails and it falls back to the integer
    SETUP_VALID_PARSE_OBJECT_WITH_OFFSET(
        "0 0 obj << /Length 9 >> stream\n01234567\nendstream\n endobj",
        PDF_OBJECT_TYPE_INTEGER,
        1
    );

    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_stream_no_line_end) {
    // Stream parsing fails and it falls back to the integer
    SETUP_VALID_PARSE_OBJECT_WITH_OFFSET(
        "0 0 obj << /Length 9 >> stream\n01234567endstream\n endobj",
        PDF_OBJECT_TYPE_INTEGER,
        1
    );

    return TEST_RESULT_PASS;
}

#endif // TEST
