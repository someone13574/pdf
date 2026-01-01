#include "object.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena/arena.h"
#include "arena/string.h"
#include "ctx.h"
#include "deser.h"
#include "err/error.h"
#include "logger/log.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "resolver.h"
#include "stream/filters.h"

#define DVEC_NAME PdfObjectVec
#define DVEC_LOWERCASE_NAME pdf_object_vec
#define DVEC_TYPE PdfObject*
#include "arena/dvec_impl.h"

#define DVEC_NAME PdfDictEntryVec
#define DVEC_LOWERCASE_NAME pdf_dict_entry_vec
#define DVEC_TYPE PdfDictEntry
#include "arena/dvec_impl.h"

#define DVEC_NAME PdfBooleanVec
#define DVEC_LOWERCASE_NAME pdf_boolean_vec
#define DVEC_TYPE PdfBoolean
#include "arena/dvec_impl.h"

DESER_IMPL_OPTIONAL(PdfBooleanVecOptional, pdf_boolean_vec_op_init)

PdfObject* pdf_dict_get(const PdfDict* dict, PdfName key) {
    RELEASE_ASSERT(dict);
    RELEASE_ASSERT(key);

    for (size_t idx = 0; idx < pdf_dict_entry_vec_len(dict->entries); idx++) {
        PdfDictEntry entry;
        RELEASE_ASSERT(pdf_dict_entry_vec_get(dict->entries, idx, &entry));

        if (entry.key->type != PDF_OBJECT_TYPE_NAME) {
            continue;
        }

        if (strcmp(key, entry.key->data.name) == 0) {
            return entry.value;
        }
    }

    return NULL;
}

Error* pdf_parse_true(PdfCtx* ctx, PdfObject* object);
Error* pdf_parse_false(PdfCtx* ctx, PdfObject* object);
Error* pdf_parse_null(PdfCtx* ctx, PdfObject* object);
Error* pdf_parse_number(PdfCtx* ctx, PdfObject* object);
Error* pdf_parse_string_literal(Arena* arena, PdfCtx* ctx, PdfObject* object);
Error* pdf_parse_hex_string(Arena* arena, PdfCtx* ctx, PdfObject* object);
Error* pdf_parse_name(Arena* arena, PdfCtx* ctx, PdfObject* object);
Error* pdf_parse_array(
    Arena* arena,
    PdfCtx* ctx,
    PdfResolver* resolver,
    PdfObject* object
);
Error* pdf_parse_dict(
    Arena* arena,
    PdfCtx* ctx,
    PdfResolver* resolver,
    PdfObject* object,
    bool in_indirect_obj
);
Error* pdf_parse_indirect(
    PdfResolver* resolver,
    PdfObject* object,
    bool* number_fallback
);
Error* pdf_parse_stream(
    Arena* arena,
    PdfCtx* ctx,
    PdfResolver* resolver,
    uint8_t** stream_bytes,
    size_t* decoded_len,
    PdfObject* stream_dict,
    PdfStreamDict* stream_dict_out
);

Error* pdf_parse_object(
    PdfResolver* resolver,
    PdfObject* object,
    bool in_indirect_obj
) {
    RELEASE_ASSERT(resolver);
    RELEASE_ASSERT(object);

    PdfCtx* ctx = pdf_resolver_ctx(resolver);
    Arena* arena = pdf_resolver_arena(resolver);

    LOG_DIAG(INFO, OBJECT, "Parsing object at offset %zu", pdf_ctx_offset(ctx));

    uint8_t peeked, peeked_next;
    TRY(pdf_ctx_peek(ctx, &peeked));

    Error* peeked_next_error = pdf_ctx_peek_next(ctx, &peeked_next);
    if (peeked == '<' && peeked_next_error) {
        return peeked_next_error;
    } else if (peeked_next_error) {
        error_free(peeked_next_error);
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
        Error* error = pdf_parse_indirect(resolver, object, &number_fallback);

        if (!error || !number_fallback) {
            return error;
        }

        if (error) {
            error_free(error);
        }

        LOG_DIAG(
            DEBUG,
            OBJECT,
            "Failed to parse indirect object/reference. Attempting to parse number."
        );
        TRY(pdf_ctx_seek(ctx, restore_offset));

        return pdf_parse_number(ctx, object);
    } else if (peeked == '(') {
        return pdf_parse_string_literal(arena, ctx, object);
    } else if (peeked == '<' && peeked_next != '<') {
        return pdf_parse_hex_string(arena, ctx, object);
    } else if (peeked == '/') {
        return pdf_parse_name(arena, ctx, object);
    } else if (peeked == '[') {
        return pdf_parse_array(arena, ctx, resolver, object);
    } else if (peeked == '<' && peeked_next == '<') {
        return pdf_parse_dict(arena, ctx, resolver, object, in_indirect_obj);
    } else if (peeked == 'n') {
        return pdf_parse_null(ctx, object);
    }

    return ERROR(
        PDF_ERR_INVALID_OBJECT,
        "There wasn't a valid signaler for the object at offset %zu",
        pdf_ctx_offset(ctx)
    );
}

Error* pdf_parse_operand_object(Arena* arena, PdfCtx* ctx, PdfObject* object) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(object);

    LOG_DIAG(
        DEBUG,
        OBJECT,
        "Parsing operand object at offset %zu in stream ctx",
        pdf_ctx_offset(ctx)
    );

    uint8_t peeked, peeked_next;
    TRY(pdf_ctx_peek(ctx, &peeked));

    Error* peeked_next_error = pdf_ctx_peek_next(ctx, &peeked_next);
    if (peeked == '<' && peeked_next_error) {
        return peeked_next_error;
    }

    if (peeked_next_error) {
        error_free(peeked_next_error);
    }

    if (peeked == 't') {
        return pdf_parse_true(ctx, object);
    } else if (peeked == 'f') {
        return pdf_parse_false(ctx, object);
    } else if (peeked == '.' || peeked == '+' || peeked == '-'
               || isdigit((unsigned char)peeked)) {
        return pdf_parse_number(ctx, object);
    } else if (peeked == '(') {
        return pdf_parse_string_literal(arena, ctx, object);
    } else if (peeked == '<' && peeked_next != '<') {
        return pdf_parse_hex_string(arena, ctx, object);
    } else if (peeked == '/') {
        return pdf_parse_name(arena, ctx, object);
    } else if (peeked == '[') {
        return pdf_parse_array(arena, ctx, NULL, object);
    } else if (peeked == '<' && peeked_next == '<') {
        return pdf_parse_dict(arena, ctx, NULL, object, false);
    } else if (peeked == 'n') {
        return pdf_parse_null(ctx, object);
    }

    return ERROR(
        PDF_ERR_INVALID_OBJECT,
        "There wasn't a valid signaler for the object. peeked=`%c`",
        peeked
    );
}

Error* pdf_parse_true(PdfCtx* ctx, PdfObject* object) {
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(object);

    TRY(pdf_ctx_expect(ctx, "true"));
    TRY(pdf_ctx_require_byte_type(ctx, true, &is_pdf_non_regular));

    object->type = PDF_OBJECT_TYPE_BOOLEAN;
    object->data.boolean = true;

    return NULL;
}

Error* pdf_parse_false(PdfCtx* ctx, PdfObject* object) {
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(object);

    TRY(pdf_ctx_expect(ctx, "false"));
    TRY(pdf_ctx_require_byte_type(ctx, true, &is_pdf_non_regular));

    object->type = PDF_OBJECT_TYPE_BOOLEAN;
    object->data.boolean = false;

    return NULL;
}

Error* pdf_parse_null(PdfCtx* ctx, PdfObject* object) {
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(object);

    TRY(pdf_ctx_expect(ctx, "null"));
    TRY(pdf_ctx_require_byte_type(ctx, true, &is_pdf_non_regular));

    object->type = PDF_OBJECT_TYPE_NULL;

    return NULL;
}

Error* pdf_parse_number(PdfCtx* ctx, PdfObject* object) {
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(object);

    LOG_DIAG(
        TRACE,
        OBJECT,
        "Parsing number at offset %zu",
        pdf_ctx_offset(ctx)
    );

    // Parse sign
    uint8_t sign_byte;
    int32_t sign = 1;
    TRY(pdf_ctx_peek(ctx, &sign_byte));
    if (sign_byte == '+' || sign_byte == '-') {
        TRY(pdf_ctx_peek_and_advance(ctx, NULL));

        if (sign_byte == '-') {
            sign = -1;
        }
    }

    // Parse leading digits
    uint8_t digit_byte;
    int64_t leading_acc = 0;
    bool has_leading = false;

    while (error_free_is_ok(pdf_ctx_peek(ctx, &digit_byte))
           && isdigit(digit_byte)) {
        has_leading = true;
        int64_t digit = digit_byte - '0';

        if (leading_acc <= (INT64_MAX - digit) / 10) {
            leading_acc *= 10;
            leading_acc += digit;
        } else {
            leading_acc = INT64_MAX;
        }

        TRY(pdf_ctx_peek_and_advance(ctx, NULL));
    }

    // Parse decimal point
    uint8_t decimal_byte;
    Error* decimal_peek_error = pdf_ctx_peek(ctx, &decimal_byte);

    if ((!decimal_peek_error && decimal_byte != '.')
        || (decimal_peek_error
            && error_code(decimal_peek_error) == PDF_ERR_CTX_EOF)) {
        if (decimal_peek_error) {
            error_free(decimal_peek_error);
        }

        LOG_DIAG(TRACE, OBJECT, "Number is integer");
        TRY(pdf_ctx_require_byte_type(ctx, true, &is_pdf_non_regular));

        if (!has_leading) {
            return ERROR(PDF_ERR_INVALID_NUMBER);
        }

        leading_acc *= sign;
        if (leading_acc < (int64_t)INT32_MIN
            || leading_acc > (int64_t)INT32_MAX) {
            return ERROR(
                PDF_ERR_NUMBER_LIMIT,
                "The magnitude of the parsed integer was too large"
            );
        }

        object->type = PDF_OBJECT_TYPE_INTEGER;
        object->data.integer = (int32_t)leading_acc;

        return NULL;
    } else if (decimal_peek_error) {
        return decimal_peek_error;
    } else {
        TRY(pdf_ctx_peek_and_advance(ctx, NULL));
    }

    LOG_DIAG(TRACE, OBJECT, "Number is real");

    // Parse trailing digits
    double trailing_acc = 0.0;
    double trailing_weight = 0.1;
    bool has_trailing = false;

    while (error_free_is_ok(pdf_ctx_peek(ctx, &digit_byte))
           && isdigit(digit_byte)) {
        has_trailing = true;

        trailing_acc += (double)(digit_byte - '0') * trailing_weight;
        trailing_weight *= 0.1;

        TRY(pdf_ctx_peek_and_advance(ctx, NULL));
    }

    TRY(pdf_ctx_require_byte_type(ctx, true, &is_pdf_non_regular));

    double value = ((double)leading_acc + trailing_acc) * (double)sign;
    const double PDF_REAL_MAX = 3.403e38;

    if (value > PDF_REAL_MAX || value < -PDF_REAL_MAX) {
        return ERROR(
            PDF_ERR_NUMBER_LIMIT,
            "The magnitude of the parsed real was too large"
        );
    }

    if (!has_leading && !has_trailing) {
        return ERROR(
            PDF_ERR_INVALID_NUMBER,
            "A real value shall contain one or more decimal digits"
        );
    }

    object->type = PDF_OBJECT_TYPE_REAL;
    object->data.real = value;

    return NULL;
}

Error* pdf_parse_string_literal(Arena* arena, PdfCtx* ctx, PdfObject* object) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(object);

    TRY(pdf_ctx_expect(ctx, "("));

    // Find length upper bound
    uint8_t peeked;
    int escape = 0;
    int open_parenthesis = 1;
    unsigned long length = 0;
    size_t start_offset = pdf_ctx_offset(ctx);

    while (open_parenthesis > 0
           && error_free_is_ok(pdf_ctx_peek_and_advance(ctx, &peeked))) {
        length++;

        if (peeked == '(' && escape == 0) {
            open_parenthesis++;
        } else if (peeked == ')' && escape == 0) {
            open_parenthesis--;
        }

        if (peeked == '\\' && escape == 0) {
            escape = 1;
        } else {
            escape = 0;
        }
    }

    length--;

    if (open_parenthesis != 0) {
        return ERROR(
            PDF_ERR_UNBALANCED_STR,
            "String literal parenthesis was not closed."
        );
    }

    // Parse
    const uint8_t* raw = pdf_ctx_get_raw(ctx) + start_offset;
    uint8_t* parsed = arena_alloc(arena, sizeof(uint8_t) * length);

    escape = 0;
    size_t write_offset = 0;

    for (size_t read_offset = 0; read_offset < length; read_offset++) {
        uint8_t read_byte = raw[read_offset];

        switch (escape) {
            case 0: {
                if (read_byte == '\\') {
                    escape = 1;
                    break;
                }

                parsed[write_offset++] = read_byte;
                break;
            }
            case 1: {
                switch (read_byte) {
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

    object->type = PDF_OBJECT_TYPE_STRING;
    object->data.string.data = parsed;
    object->data.string.len = write_offset;

    return NULL;
}

static bool char_to_hex(uint8_t c, int* out) {
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

Error* pdf_parse_hex_string(Arena* arena, PdfCtx* ctx, PdfObject* object) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(object);

    TRY(pdf_ctx_expect(ctx, "<"));

    bool upper = true;
    size_t decoded_len = 0;

    size_t allocated_len = 64;
    uint8_t* decoded = malloc(sizeof(uint8_t) * allocated_len);

    while (true) {
        uint8_t peeked;
        Error* next_err = pdf_ctx_peek_and_advance(ctx, &peeked);
        if (next_err) {
            free(decoded);
            TRY(next_err);
        }

        if (is_pdf_whitespace(peeked)) {
            continue;
        }

        if (peeked == '>') {
            if (!upper) {
                decoded_len += 1;
            }

            break;
        }

        int hex;
        if (!char_to_hex(peeked, &hex)) {
            free(decoded);
            return ERROR(
                PDF_ERR_ASCII_HEX_INVALID,
                "Invalid character in hex string"
            );
        }

        if (upper) {
            if (decoded_len == allocated_len) {
                allocated_len <<= 2;
                uint8_t* realloced_decoded =
                    realloc(decoded, sizeof(uint8_t) * allocated_len);
                RELEASE_ASSERT(realloced_decoded);
                decoded = realloced_decoded;
            }

            decoded[decoded_len] = (uint8_t)(hex << 4);
            upper = false;
        } else {
            decoded[decoded_len] |= (uint8_t)hex;
            decoded_len += 1;
            upper = true;
        }
    }

    object->type = PDF_OBJECT_TYPE_STRING;
    object->data.string.data = arena_alloc(arena, decoded_len);
    memcpy(object->data.string.data, decoded, sizeof(uint8_t) * decoded_len);
    object->data.string.len = decoded_len;

    free(decoded);

    return NULL;
}

Error* pdf_parse_name(Arena* arena, PdfCtx* ctx, PdfObject* object) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(object);

    TRY(pdf_ctx_expect(ctx, "/"));

    // Find max length
    size_t start_offset = pdf_ctx_offset(ctx);
    size_t length = 0;
    uint8_t peeked;

    while (error_free_is_ok(pdf_ctx_peek_and_advance(ctx, &peeked))) {
        if (!is_pdf_regular(peeked)) {
            break;
        }

        if (peeked < '!' || peeked > '~') {
            return ERROR(
                PDF_ERR_NAME_UNESCAPED_CHAR,
                "Regular characters that are outside the range EXCLAMATION MARK(21h) (!) to TILDE (7Eh) (~) should be written using the hexadecimal notation."
            );
        }

        length++;
    }

    TRY(pdf_ctx_seek(ctx, start_offset + length));
    TRY(pdf_ctx_require_byte_type(ctx, true, &is_pdf_non_regular));

    // Parse name
    char* name = arena_alloc(arena, sizeof(char) * (length + 1));
    const uint8_t* raw = pdf_ctx_get_raw(ctx) + start_offset;

    size_t write_offset = 0;
    int escape = 0;
    int hex_code = 0;

    for (size_t read_offset = 0; read_offset < length; read_offset++) {
        uint8_t byte = raw[read_offset];

        switch (escape) {
            case 0: {
                if (byte == '#') {
                    escape = 1;
                    continue;
                }

                name[write_offset++] = (char)byte;
                break;
            }
            case 1: {
                int value;
                if (!char_to_hex(byte, &value)) {
                    return ERROR(
                        PDF_ERR_NAME_BAD_CHAR_CODE,
                        "Expected a hex value following a number sign in a name object"
                    );
                }

                hex_code = value << 4;
                escape = 2;
                break;
            }
            case 2: {
                int value;
                if (!char_to_hex(byte, &value)) {
                    return ERROR(
                        PDF_ERR_NAME_BAD_CHAR_CODE,
                        "Expected two hex values following a number sign in a name object"
                    );
                }

                hex_code |= (uint8_t)value;
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
        return ERROR(
            PDF_ERR_NAME_BAD_CHAR_CODE,
            "Non-regular character in name escape sequence"
        );
    }

    name[write_offset] = '\0';

    object->type = PDF_OBJECT_TYPE_NAME;
    object->data.name = name;

    return NULL;
}

Error* pdf_parse_array(
    Arena* arena,
    PdfCtx* ctx,
    PdfResolver* resolver,
    PdfObject* object
) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(object);

    TRY(pdf_ctx_expect(ctx, "["));
    TRY(pdf_ctx_consume_whitespace(ctx));

    PdfObjectVec* elements = pdf_object_vec_new(arena);

    uint8_t peeked;
    while (error_free_is_ok(pdf_ctx_peek(ctx, &peeked)) && peeked != ']') {
        PdfObject element;
        if (resolver) {
            TRY(pdf_parse_object(resolver, &element, false));
        } else {
            TRY(pdf_parse_operand_object(arena, ctx, &element));
        }
        TRY(pdf_ctx_consume_whitespace(ctx));

        PdfObject* allocated = arena_alloc(arena, sizeof(PdfObject));
        *allocated = element;
        pdf_object_vec_push(elements, allocated);
    }

    TRY(pdf_ctx_expect(ctx, "]"));

    object->type = PDF_OBJECT_TYPE_ARRAY;
    object->data.array.elements = elements;

    return NULL;
}

Error* pdf_parse_dict(
    Arena* arena,
    PdfCtx* ctx,
    PdfResolver* resolver,
    PdfObject* object,
    bool in_indirect_obj
) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(object);

    TRY(pdf_ctx_expect(ctx, "<<"));
    TRY(pdf_ctx_consume_whitespace(ctx));

    PdfDictEntryVec* entries = pdf_dict_entry_vec_new(arena);

    uint8_t peeked;
    while (error_free_is_ok(pdf_ctx_peek(ctx, &peeked)) && peeked != '>') {
        PdfObject* key = arena_alloc(arena, sizeof(PdfObject));
        TRY(pdf_parse_name(arena, ctx, key));
        TRY(pdf_ctx_consume_whitespace(ctx));

        PdfObject* value = arena_alloc(arena, sizeof(PdfObject));
        if (resolver) {
            TRY(pdf_parse_object(resolver, value, false));
        } else {
            TRY(pdf_parse_operand_object(arena, ctx, value));
        }
        TRY(pdf_ctx_consume_whitespace(ctx));

        pdf_dict_entry_vec_push(
            entries,
            (PdfDictEntry) {.key = key, .value = value}
        );
    }

    TRY(pdf_ctx_expect(ctx, ">>"));

    object->type = PDF_OBJECT_TYPE_DICT;
    object->data.dict.entries = entries;

    // Attempt to parse stream
    if (in_indirect_obj) {
        size_t restore_offset = pdf_ctx_offset(ctx);

        Error* consume_error = pdf_ctx_consume_whitespace(ctx);
        if (consume_error) {
            // Not a stream
            error_free_is_ok(pdf_ctx_seek(ctx, restore_offset));
            return NULL;
        }

        uint8_t* stream_bytes = NULL;
        size_t decoded_len;
        PdfStreamDict stream_dict;
        if (!error_free_is_ok(pdf_parse_stream(
                arena,
                ctx,
                resolver,
                &stream_bytes,
                &decoded_len,
                object,
                &stream_dict
            ))
            || !stream_bytes) {
            // Not a stream
            error_free_is_ok(pdf_ctx_seek(ctx, restore_offset));
            return NULL;
        }

        // Type is a stream
        PdfStreamDict* stream_dict_ptr =
            arena_alloc(arena, sizeof(PdfStreamDict));
        *stream_dict_ptr = stream_dict;

        object->type = PDF_OBJECT_TYPE_STREAM;
        object->data.stream.stream_dict = stream_dict_ptr;
        object->data.stream.stream_bytes = stream_bytes;
        object->data.stream.decoded_stream_len = decoded_len;

        return NULL;
    }

    return NULL;
}

Error* pdf_parse_stream(
    Arena* arena,
    PdfCtx* ctx,
    PdfResolver* resolver,
    uint8_t** stream_body,
    size_t* decoded_len,
    PdfObject* stream_dict_obj,
    PdfStreamDict* stream_dict_out
) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(ctx);
    RELEASE_ASSERT(resolver);
    RELEASE_ASSERT(stream_body);
    RELEASE_ASSERT(decoded_len);
    RELEASE_ASSERT(stream_dict_obj);
    RELEASE_ASSERT(stream_dict_out);

    // Parse start
    TRY(pdf_ctx_expect(ctx, "stream"));

    uint8_t peeked_newline;
    TRY(pdf_ctx_peek(ctx, &peeked_newline));
    if (peeked_newline == '\n') {
        TRY(pdf_ctx_shift(ctx, 1));
    } else if (peeked_newline == '\r') {
        TRY(pdf_ctx_expect(ctx, "\r\n"));
    } else {
        return ERROR(
            PDF_ERR_CTX_EXPECT,
            "There must be a newline following the `stream` keyword"
        );
    }

    // TODO: Do not allow fallback to single number parsing past this point.
    // Doing so only masks the problem, and can leave the ctx in a weird state
    // if an error occurs during stream decoding (due to a borrowed substr)

    // Deserialize stream dict
    size_t resume_offset = pdf_ctx_offset(
        ctx
    ); // If there is an indirect length, we don't want the offset to move
    PdfStreamDict stream_dict;
    REQUIRE(pdf_deser_stream_dict(stream_dict_obj, &stream_dict, resolver));
    pdf_ctx_seek(ctx, resume_offset);

    if (stream_dict.length < 0) {
        return ERROR(PDF_ERR_STREAM_INVALID_LENGTH);
    }

    // Decode stream body
    REQUIRE(pdf_decode_filtered_stream(
        arena,
        pdf_ctx_get_raw(ctx) + pdf_ctx_offset(ctx),
        (size_t)stream_dict.length,
        stream_dict.filter,
        stream_body,
        decoded_len
    ));

    // Parse end
    TRY(pdf_ctx_shift(ctx, stream_dict.length));
    if (!error_free_is_ok(pdf_ctx_expect(ctx, "\nendstream"))
        && !error_free_is_ok(pdf_ctx_expect(ctx, "\r\nendstream"))
        && !error_free_is_ok(pdf_ctx_expect(ctx, "\rendstream"))
        && !error_free_is_ok(pdf_ctx_expect(ctx, "endstream"))) {
        return ERROR(
            PDF_ERR_CTX_EXPECT,
            "Missing newline and `endstream` keyword at expected location"
        );
    }

    *stream_dict_out = stream_dict;

    return NULL;
}

Error* pdf_parse_indirect(
    PdfResolver* resolver,
    PdfObject* object,
    bool* number_fallback
) {
    RELEASE_ASSERT(resolver);
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(number_fallback);

    PdfCtx* ctx = pdf_resolver_ctx(resolver);

    LOG_DIAG(DEBUG, OBJECT, "Parsing indirect object or reference");

    // Parse object id
    uint64_t object_id;
    uint32_t int_length;
    TRY(pdf_ctx_parse_int(ctx, NULL, &object_id, &int_length));
    if (int_length == 0) {
        return ERROR(
            PDF_ERR_CTX_EXPECT,
            "Expected an integer for the object_id of indirect reference/object"
        );
    }

    TRY(pdf_ctx_expect(ctx, " "));

    // Parse generation
    uint64_t generation;
    TRY(pdf_ctx_parse_int(ctx, NULL, &generation, &int_length));
    if (int_length == 0) {
        return ERROR(
            PDF_ERR_CTX_EXPECT,
            "Expected an integer for the generation of indirect reference/object"
        );
    }

    TRY(pdf_ctx_expect(ctx, " "));

    // Determine if indirect object or reference
    uint8_t peeked;
    TRY(pdf_ctx_peek(ctx, &peeked));

    if (peeked == 'R') {
        TRY(pdf_ctx_peek_and_advance(ctx, NULL));

        LOG_DIAG(DEBUG, OBJECT, "Parsed indirect reference");

        object->type = PDF_OBJECT_TYPE_INDIRECT_REF;
        object->data.indirect_ref.object_id = object_id;
        object->data.indirect_ref.generation = generation;

        return NULL;
    } else {
        LOG_DIAG(DEBUG, OBJECT, "Parsed indirect object");

        TRY(pdf_ctx_expect(ctx, "obj"));
        TRY(pdf_ctx_require_byte_type(ctx, false, &is_pdf_non_regular));
        TRY(pdf_ctx_consume_whitespace(ctx));
        *number_fallback = false;

        PdfObject* inner =
            arena_alloc(pdf_resolver_arena(resolver), sizeof(PdfObject));
        TRY(pdf_parse_object(resolver, inner, true));

        TRY(pdf_ctx_consume_whitespace(ctx));
        TRY(pdf_ctx_expect(ctx, "endobj"));
        TRY(pdf_ctx_require_byte_type(ctx, true, &is_pdf_non_regular));

        object->type = PDF_OBJECT_TYPE_INDIRECT_OBJECT;
        object->data.indirect_object.object = inner;
        object->data.indirect_object.object_id = object_id;
        object->data.indirect_object.generation = generation;

        return NULL;
    }
}

ArenaString* pdf_fmt_object_indented(
    Arena* arena,
    const PdfObject* object,
    int indent,
    bool* force_indent_parent
) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(object);

    switch (object->type) {
        case PDF_OBJECT_TYPE_BOOLEAN: {
            if (object->data.boolean) {
                return arena_string_new_fmt(arena, "true");
            } else {
                return arena_string_new_fmt(arena, "false");
            }
        }
        case PDF_OBJECT_TYPE_INTEGER: {
            return arena_string_new_fmt(arena, "%d", object->data.integer);
        }
        case PDF_OBJECT_TYPE_REAL: {
            return arena_string_new_fmt(arena, "%g", object->data.real);
        }
        case PDF_OBJECT_TYPE_STRING: {
            return arena_string_new_fmt(
                arena,
                "(%s)",
                pdf_string_as_cstr(object->data.string, arena)
            );
        }
        case PDF_OBJECT_TYPE_NAME: {
            return arena_string_new_fmt(arena, "/%s", object->data.name);
        }
        case PDF_OBJECT_TYPE_ARRAY: {
            PdfObjectVec* items = object->data.array.elements;
            size_t num_items = pdf_object_vec_len(items);
            if (num_items == 0) {
                return arena_string_new_fmt(arena, "[]");
            }

            int length = indent;
            ArenaString** item_strs = (ArenaString**)
                arena_alloc(arena, sizeof(ArenaString*) * num_items);
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
                length += (int)arena_string_len(item_strs[idx]);
            }

            if (length > 80 || array_contains_indirect) {
                ArenaString* buffer = arena_string_new_fmt(arena, "[");
                for (size_t idx = 0; idx < num_items; idx++) {
                    ArenaString* new_buffer = arena_string_new_fmt(
                        arena,
                        "%s\n  %*s%s",
                        arena_string_buffer(buffer),
                        indent,
                        "",
                        arena_string_buffer(item_strs[idx])
                    );
                    if (new_buffer) {
                        buffer = new_buffer;
                    } else {
                        LOG_ERROR(OBJECT, "Object formatting failed");
                        return NULL;
                    }
                }

                return arena_string_new_fmt(
                    arena,
                    "%s\n%*s]",
                    arena_string_buffer(buffer),
                    indent,
                    ""
                );
            } else {
                ArenaString* buffer = arena_string_new_fmt(arena, "[");
                for (size_t idx = 0; idx < num_items; idx++) {
                    buffer = arena_string_new_fmt(
                        arena,
                        "%s %s",
                        arena_string_buffer(buffer),
                        arena_string_buffer(item_strs[idx])
                    );
                }

                return arena_string_new_fmt(
                    arena,
                    "%s ]",
                    arena_string_buffer(buffer)
                );
            }
        }
        case PDF_OBJECT_TYPE_DICT: {
            PdfDictEntryVec* entries = object->data.dict.entries;

            if (pdf_dict_entry_vec_len(entries) == 0) {
                return arena_string_new_fmt(arena, "<< >>");
            } else {
                ArenaString* buffer = arena_string_new_fmt(arena, "<<");
                for (size_t idx = 0; idx < pdf_dict_entry_vec_len(entries);
                     idx++) {
                    PdfDictEntry entry;
                    RELEASE_ASSERT(
                        pdf_dict_entry_vec_get(entries, idx, &entry)
                    );
                    ArenaString* key_text = pdf_fmt_object_indented(
                        arena,
                        entry.key,
                        indent + 2,
                        NULL
                    );
                    ArenaString* value_text = pdf_fmt_object_indented(
                        arena,
                        entry.value,
                        indent + (int)arena_string_len(key_text) + 3,
                        NULL
                    );

                    buffer = arena_string_new_fmt(
                        arena,
                        "%s\n  %*s%s %s",
                        arena_string_buffer(buffer),
                        indent,
                        "",
                        arena_string_buffer(key_text),
                        arena_string_buffer(value_text)
                    );
                }

                return arena_string_new_fmt(
                    arena,
                    "%s\n%*s>>",
                    arena_string_buffer(buffer),
                    indent,
                    ""
                );
            }
        }
        case PDF_OBJECT_TYPE_STREAM: {
            return arena_string_new_fmt(
                arena,
                "%s\n%*sstream\n%*s...\n%*sendstream",
                arena_string_buffer(pdf_fmt_object_indented(
                    arena,
                    object->data.stream.stream_dict->raw_dict,
                    indent,
                    NULL
                )),
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

            return arena_string_new_fmt(
                arena,
                "%zu %zu obj\n%*s%s\n%*sendobj",
                object->data.indirect_object.object_id,
                object->data.indirect_object.generation,
                indent + 2,
                "",
                arena_string_buffer(pdf_fmt_object_indented(
                    arena,
                    object->data.indirect_object.object,
                    indent + 2,
                    NULL
                )),
                indent,
                ""
            );
        }
        case PDF_OBJECT_TYPE_INDIRECT_REF: {
            if (force_indent_parent) {
                *force_indent_parent = true;
            }
            return arena_string_new_fmt(
                arena,
                "%zu %zu R",
                object->data.indirect_ref.object_id,
                object->data.indirect_ref.generation
            );
        }
        case PDF_OBJECT_TYPE_NULL: {
            return arena_string_new_fmt(arena, "null");
        }
    }

    return arena_string_new_fmt(arena, "unreachable");
}

char* pdf_fmt_object(Arena* arena, const PdfObject* object) {
    RELEASE_ASSERT(arena);
    RELEASE_ASSERT(object);

    Arena* temp_arena = arena_new(512);
    ArenaString* formatted =
        pdf_fmt_object_indented(temp_arena, object, 0, NULL);

    size_t len = arena_string_len(formatted) + 1;
    char* allocated = arena_alloc(arena, sizeof(char) * len);
    strncpy(allocated, arena_string_buffer(formatted), len);
    arena_free(temp_arena);

    return allocated;
}

char* pdf_string_as_cstr(PdfString pdf_string, Arena* arena) {
    RELEASE_ASSERT(arena);

    char* cstr = arena_alloc(arena, sizeof(char) * (pdf_string.len + 1));
    memcpy(cstr, pdf_string.data, pdf_string.len);
    cstr[pdf_string.len] = '\0';

    return cstr;
}

#ifdef TEST
#include <string.h>

#include "test/test.h"
#include "test_helpers.h"

#define SETUP_VALID_PARSE_OBJECT_WITH_OFFSET(                                  \
    buf,                                                                       \
    object_type,                                                               \
    expected_offset                                                            \
)                                                                              \
    Arena* arena = arena_new(128);                                             \
    uint8_t buffer[] = buf;                                                    \
    PdfCtx* ctx =                                                              \
        pdf_ctx_new(arena, buffer, sizeof(buffer) / sizeof(uint8_t) - 1);      \
    PdfResolver* resolver = pdf_fake_resolver_new(arena, ctx);                 \
    PdfObject object;                                                          \
    TEST_REQUIRE(pdf_parse_object(resolver, &object, false));                  \
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
    uint8_t buffer[] = buf;                                                    \
    PdfCtx* ctx =                                                              \
        pdf_ctx_new(arena, buffer, sizeof(buffer) / sizeof(uint8_t) - 1);      \
    PdfResolver* resolver = pdf_fake_resolver_new(arena, ctx);                 \
                                                                               \
    PdfObject object;                                                          \
    TEST_REQUIRE_ERR(pdf_parse_object(resolver, &object, false), (err));

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
    TEST_ASSERT_EQ(
        "This is a string",
        pdf_string_as_cstr(object.data.string, arena)
    );
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_literal_str_newline) {
    SETUP_VALID_PARSE_OBJECT(
        "(Strings may contain newlines\nand such.)",
        PDF_OBJECT_TYPE_STRING
    );
    TEST_ASSERT_EQ(
        "Strings may contain newlines\nand such.",
        pdf_string_as_cstr(object.data.string, arena)
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
        pdf_string_as_cstr(object.data.string, arena)
    );
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_literal_str_empty) {
    SETUP_VALID_PARSE_OBJECT("()", PDF_OBJECT_TYPE_STRING);
    TEST_ASSERT_EQ("", pdf_string_as_cstr(object.data.string, arena));
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_literal_str_escapes) {
    SETUP_VALID_PARSE_OBJECT(
        "(This is a line\\nThis is a newline\\r\\nThis is another line with tabs (\\t), backspaces(\\b), form feeds (\\f), unbalanced parenthesis \\(\\(\\(\\), and backslashes \\\\\\\\)",
        PDF_OBJECT_TYPE_STRING
    );
    TEST_ASSERT_EQ(
        "This is a line\nThis is a newline\r\nThis is another line with tabs (\t), backspaces(\b), form feeds (\f), unbalanced parenthesis (((), and backslashes \\\\",
        pdf_string_as_cstr(object.data.string, arena)
    );
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_literal_str_unbalanced) {
    SETUP_INVALID_PARSE_OBJECT("(", PDF_ERR_UNBALANCED_STR);
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_hex_string) {
    SETUP_VALID_PARSE_OBJECT(
        "<68656C6C6F20776F726C64>",
        PDF_OBJECT_TYPE_STRING
    );
    TEST_ASSERT_EQ(
        "hello world",
        pdf_string_as_cstr(object.data.string, arena)
    );
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_hex_string_spaces) {
    SETUP_VALID_PARSE_OBJECT(
        "< 686  56  \r\fC6C6F\n207\t76F 726C6 4>",
        PDF_OBJECT_TYPE_STRING
    );
    TEST_ASSERT_EQ(
        "hello world",
        pdf_string_as_cstr(object.data.string, arena)
    );
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_hex_string_even_len) {
    SETUP_VALID_PARSE_OBJECT("<901fa3>", PDF_OBJECT_TYPE_STRING);
    TEST_ASSERT_EQ(
        "\x90\x1f\xa3",
        pdf_string_as_cstr(object.data.string, arena)
    );
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_hex_string_odd_len) {
    SETUP_VALID_PARSE_OBJECT("<901fa>", PDF_OBJECT_TYPE_STRING);
    TEST_ASSERT_EQ(
        "\x90\x1f\xa0",
        pdf_string_as_cstr(object.data.string, arena)
    );
    return TEST_RESULT_PASS;
}

TEST_FUNC(test_object_hex_string_unterminated) {
    SETUP_INVALID_PARSE_OBJECT("<68656C6C6F20776F726C64", PDF_ERR_CTX_EOF);
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
    TEST_ASSERT(!element2->data.boolean);

    PdfObject* element3;
    TEST_ASSERT(pdf_object_vec_get(object.data.array.elements, 3, &element3));
    TEST_ASSERT(element3);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_STRING, element3->type);
    TEST_ASSERT_EQ("Ralph", pdf_string_as_cstr(element3->data.string, arena));

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
    TEST_ASSERT(!element2->data.boolean);

    PdfObject* element3;
    TEST_ASSERT(pdf_object_vec_get(object.data.array.elements, 3, &element3));
    TEST_ASSERT(element3);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_STRING, element3->type);
    TEST_ASSERT_EQ("Ralph", pdf_string_as_cstr(element3->data.string, arena));

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
    TEST_ASSERT(
        pdf_object_vec_get(element0->data.array.elements, 0, &element00)
    );
    TEST_ASSERT(element00);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_INTEGER, element00->type);
    TEST_ASSERT_EQ((int32_t)1, element00->data.integer);

    PdfObject* element01;
    TEST_ASSERT(
        pdf_object_vec_get(element0->data.array.elements, 1, &element01)
    );
    TEST_ASSERT(element01);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_INTEGER, element01->type);
    TEST_ASSERT_EQ((int32_t)2, element01->data.integer);

    PdfObject* element1;
    TEST_ASSERT(pdf_object_vec_get(object.data.array.elements, 1, &element1));
    TEST_ASSERT(element1);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_ARRAY, element1->type);

    PdfObject* element10;
    TEST_ASSERT(
        pdf_object_vec_get(element1->data.array.elements, 0, &element10)
    );
    TEST_ASSERT(element10);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_INTEGER, element10->type);
    TEST_ASSERT_EQ((int32_t)3, element10->data.integer);

    PdfObject* element11;
    TEST_ASSERT(
        pdf_object_vec_get(element1->data.array.elements, 1, &element11)
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
    TEST_ASSERT_EQ(
        "a string",
        pdf_string_as_cstr(entry4.value->data.string, arena)
    );

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
    TEST_ASSERT(sub1.value->data.boolean);

    PdfDictEntry sub2;
    TEST_ASSERT(pdf_dict_entry_vec_get(subdict, 2, &sub2));
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_NAME, sub2.key->type);
    TEST_ASSERT_EQ("LastItem", sub2.key->data.name);
    TEST_ASSERT(sub2.value);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_STRING, sub2.value->type);
    TEST_ASSERT_EQ("not!", pdf_string_as_cstr(sub2.value->data.string, arena));

    PdfDictEntry sub3;
    TEST_ASSERT(pdf_dict_entry_vec_get(subdict, 3, &sub3));
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_NAME, sub3.key->type);
    TEST_ASSERT_EQ("VeryLastItem", sub3.key->data.name);
    TEST_ASSERT(sub3.value);
    TEST_ASSERT_EQ((PdfObjectType)PDF_OBJECT_TYPE_STRING, sub3.value->type);
    TEST_ASSERT_EQ("OK", pdf_string_as_cstr(sub3.value->data.string, arena));

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
    TEST_ASSERT_EQ(
        "Brillig",
        pdf_string_as_cstr(
            object.data.indirect_object.object->data.string,
            arena
        )
    );

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
            .name
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
    TEST_ASSERT_EQ((size_t)8, stream.decoded_stream_len);
    TEST_ASSERT(
        memcmp("01234567", stream.stream_bytes, stream.decoded_stream_len) == 0
    );

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
    TEST_ASSERT_EQ((size_t)8, stream.decoded_stream_len);
    TEST_ASSERT(
        memcmp("01234567", stream.stream_bytes, stream.decoded_stream_len) == 0
    );

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

TEST_FUNC(test_object_stream_no_line_end) {
    SETUP_INVALID_PARSE_OBJECT(
        "0 0 obj << /Length 9 >> stream\n01234567endstream\n endobj",
        PDF_ERR_CTX_EXPECT
    );

    return TEST_RESULT_PASS;
}

#endif // TEST
