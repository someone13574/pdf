#include "operators.h"

#include <math.h>
#include <stdbool.h>

#include "arena/arena.h"
#include "logger/log.h"
#include "pdf_error/error.h"
#include "postscript/interpreter.h"
#include "postscript/object.h"
#include "postscript/resource.h"

static void push_operator(
    PostscriptObjectList* dict,
    PostscriptOperator operator,
    char* name
) {
    RELEASE_ASSERT(dict);
    RELEASE_ASSERT(operator);
    RELEASE_ASSERT(name);

    LOG_DIAG(DEBUG, PS, "Adding systemdict operator `%s`", name);

    postscript_object_list_push_back(
        dict,
        (PostscriptObject) {.type = POSTSCRIPT_OBJECT_NAME,
                            .data.name = name,
                            .access = POSTSCRIPT_ACCESS_UNLIMITED,
                            .literal = true}
    );

    postscript_object_list_push_back(
        dict,
        (PostscriptObject) {.type = POSTSCRIPT_OBJECT_OPERATOR,
                            .data.operator = operator,
                            .access = POSTSCRIPT_ACCESS_EXECUTE_ONLY,
                            .literal = false}
    );
}

PostscriptObject postscript_systemdict_ops(Arena* arena) {
    RELEASE_ASSERT(arena);

    LOG_DIAG(INFO, PS, "Getting systemdict operators");

    PostscriptObject dict = {
        .type = POSTSCRIPT_OBJECT_DICT,
        .data.dict = postscript_object_list_new(arena),
        .access = POSTSCRIPT_ACCESS_READ_ONLY,
        .literal = true
    };

    push_operator(dict.data.dict, postscript_op_pop, "pop");
    push_operator(dict.data.dict, postscript_op_exch, "exch");
    push_operator(dict.data.dict, postscript_op_dup, "dup");
    push_operator(dict.data.dict, postscript_op_copy, "copy");
    push_operator(dict.data.dict, postscript_op_index, "index");
    push_operator(dict.data.dict, postscript_op_roll, "roll");

    push_operator(dict.data.dict, postscript_op_add, "add");
    push_operator(dict.data.dict, postscript_op_sub, "sub");
    push_operator(dict.data.dict, postscript_op_mul, "mul");
    push_operator(dict.data.dict, postscript_op_div, "div");
    push_operator(dict.data.dict, postscript_op_idiv, "idiv");
    push_operator(dict.data.dict, postscript_op_mod, "mod");
    push_operator(dict.data.dict, postscript_op_neg, "neg");
    push_operator(dict.data.dict, postscript_op_abs, "abs");
    push_operator(dict.data.dict, postscript_op_ceiling, "ceiling");
    push_operator(dict.data.dict, postscript_op_floor, "floor");
    push_operator(dict.data.dict, postscript_op_round, "round");
    push_operator(dict.data.dict, postscript_op_truncate, "truncate");
    push_operator(dict.data.dict, postscript_op_sqrt, "sqrt");
    push_operator(dict.data.dict, postscript_op_sin, "sin");
    push_operator(dict.data.dict, postscript_op_cos, "cos");
    push_operator(dict.data.dict, postscript_op_atan, "atan");
    push_operator(dict.data.dict, postscript_op_exp, "exp");
    push_operator(dict.data.dict, postscript_op_ln, "ln");
    push_operator(dict.data.dict, postscript_op_log, "log");
    push_operator(dict.data.dict, postscript_op_cvi, "cvi");
    push_operator(dict.data.dict, postscript_op_cvr, "cvr");

    push_operator(dict.data.dict, postscript_op_dict, "dict");
    push_operator(dict.data.dict, postscript_op_def, "def");
    push_operator(dict.data.dict, postscript_op_begin, "begin");
    push_operator(dict.data.dict, postscript_op_end, "end");
    push_operator(dict.data.dict, postscript_op_currentdict, "currentdict");
    push_operator(dict.data.dict, postscript_op_findresource, "findresource");
    push_operator(
        dict.data.dict,
        postscript_op_defineresource,
        "defineresource"
    );

    return dict;
}

PdfError* postscript_op_pop(PostscriptInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    PostscriptObject popped;
    PDF_PROPAGATE(postscript_interpreter_pop_operand(interpreter, &popped));

    return NULL;
}

PdfError* postscript_op_exch(PostscriptInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    PostscriptObject a;
    PDF_PROPAGATE(postscript_interpreter_pop_operand(interpreter, &a));
    PostscriptObject b;
    PDF_PROPAGATE(postscript_interpreter_pop_operand(interpreter, &b));

    postscript_interpreter_operand_push(interpreter, a);
    postscript_interpreter_operand_push(interpreter, b);

    return NULL;
}

PdfError* postscript_op_dup(PostscriptInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    PostscriptObject object;
    PDF_PROPAGATE(postscript_interpreter_pop_operand(interpreter, &object));

    postscript_interpreter_operand_push(interpreter, object);
    postscript_interpreter_operand_push(interpreter, object);

    return NULL;
}

PdfError* postscript_op_copy(PostscriptInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    PostscriptObject count_object;
    PDF_PROPAGATE(postscript_interpreter_pop_operand_typed(
        interpreter,
        POSTSCRIPT_OBJECT_INTEGER,
        true,
        &count_object
    ));

    Arena* local_arena = arena_new(128);
    PostscriptObjectList* list = postscript_object_list_new(local_arena);

    for (int32_t idx = 0; idx < count_object.data.integer; idx++) {
        PostscriptObject object;
        PDF_PROPAGATE(postscript_interpreter_pop_operand(interpreter, &object));

        postscript_object_list_push_back(list, object);
    }

    for (size_t pass = 0; pass < 2; pass++) {
        for (int32_t idx = 0; idx < count_object.data.integer; idx++) {
            PostscriptObject object;
            RELEASE_ASSERT(postscript_object_list_get(
                list,
                (size_t)(count_object.data.integer - idx - 1),
                &object
            ));

            postscript_interpreter_operand_push(interpreter, object);
        }
    }

    arena_free(local_arena);
    return NULL;
}

PdfError* postscript_op_index(PostscriptInterpreter* interpreter) {
    PostscriptObject idx_object;
    PDF_PROPAGATE(postscript_interpreter_pop_operand_typed(
        interpreter,
        POSTSCRIPT_OBJECT_INTEGER,
        true,
        &idx_object
    ));

    PostscriptObjectList* stack = postscript_interpreter_stack(interpreter);
    size_t len = postscript_object_list_len(stack);
    if (idx_object.data.integer >= (int32_t)len
        || idx_object.data.integer < 0) {
        return PDF_ERROR(PDF_ERR_POSTSCRIPT_OPERANDS_EMPTY);
    }

    PostscriptObject object;
    if (!postscript_object_list_get(
            stack,
            len - (size_t)idx_object.data.integer - 1,
            &object
        )) {
        return PDF_ERROR(PDF_ERR_POSTSCRIPT_OPERANDS_EMPTY);
    }

    return NULL;
}

PdfError* postscript_op_roll(PostscriptInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    PostscriptObject j_object;
    PDF_PROPAGATE(postscript_interpreter_pop_operand_typed(
        interpreter,
        POSTSCRIPT_OBJECT_INTEGER,
        true,
        &j_object
    ));

    PostscriptObject n_object;
    PDF_PROPAGATE(postscript_interpreter_pop_operand_typed(
        interpreter,
        POSTSCRIPT_OBJECT_INTEGER,
        true,
        &n_object
    ));

    int32_t n = n_object.data.integer;
    int32_t j = j_object.data.integer;

    if (n < 0) {
        return PDF_ERROR(PDF_ERR_POSTSCRIPT_OPERANDS_EMPTY);
    }
    if (n == 0) {
        return NULL;
    }

    Arena* local_arena = arena_new(128);
    PostscriptObjectList* list = postscript_object_list_new(local_arena);

    for (int32_t idx = 0; idx < n; idx++) {
        PostscriptObject object;
        PDF_PROPAGATE(postscript_interpreter_pop_operand(interpreter, &object));
        postscript_object_list_push_back(list, object);
    }

    int32_t j_mod = j % n;
    if (j_mod < 0) {
        j_mod += n;
    }

    for (int32_t k = n - 1; k >= 0; k--) {
        PostscriptObject object;
        bool success = postscript_object_list_get(
            list,
            (size_t)((k + j_mod) % n),
            &object
        );
        RELEASE_ASSERT(success);
        postscript_interpreter_operand_push(interpreter, object);
    }

    arena_free(local_arena);
    return NULL;
}

static PdfError* object_to_double(PostscriptObject obj, double* out) {
    if (obj.type == POSTSCRIPT_OBJECT_INTEGER) {
        *out = (double)obj.data.integer;
        return NULL;
    } else if (obj.type == POSTSCRIPT_OBJECT_REAL) {
        *out = obj.data.real;
        return NULL;
    } else {
        return PDF_ERROR(
            PDF_ERR_POSTSCRIPT_OPERAND_TYPE,
            "Postscript numbers must be an integer or real"
        );
    }
}

static inline void push_integer(PostscriptInterpreter* interpreter, int32_t x) {
    postscript_interpreter_operand_push(
        interpreter,
        (PostscriptObject) {.type = POSTSCRIPT_OBJECT_INTEGER,
                            .data.integer = x,
                            .literal = true,
                            .access = POSTSCRIPT_ACCESS_UNLIMITED}
    );
}

static inline void push_real(PostscriptInterpreter* interpreter, double x) {
    postscript_interpreter_operand_push(
        interpreter,
        (PostscriptObject) {.type = POSTSCRIPT_OBJECT_REAL,
                            .data.real = x,
                            .literal = true,
                            .access = POSTSCRIPT_ACCESS_UNLIMITED}
    );
}

static PdfError* binary_numeric_op(
    PostscriptInterpreter* interpreter,
    int32_t (*integer_op)(int32_t, int32_t),
    double (*real_op)(double, double)
) {
    RELEASE_ASSERT(interpreter);
    RELEASE_ASSERT(real_op);

    PostscriptObject x, y;
    PDF_PROPAGATE(postscript_interpreter_pop_operand(interpreter, &y));
    PDF_PROPAGATE(postscript_interpreter_pop_operand(interpreter, &x));

    if (integer_op && x.type == POSTSCRIPT_OBJECT_INTEGER
        && y.type == POSTSCRIPT_OBJECT_INTEGER) {
        push_integer(interpreter, integer_op(x.data.integer, y.data.integer));
        return NULL;
    }

    double x_real, y_real;
    PDF_PROPAGATE(object_to_double(x, &x_real));
    PDF_PROPAGATE(object_to_double(y, &y_real));

    push_real(interpreter, real_op(x_real, y_real));
    return NULL;
}

static int32_t int_add(int32_t a, int32_t b) {
    return a + b;
}
static int32_t int_sub(int32_t a, int32_t b) {
    return a - b;
}
static int32_t int_mul(int32_t a, int32_t b) {
    return a * b;
}

static double real_add(double a, double b) {
    return a + b;
}
static double real_sub(double a, double b) {
    return a - b;
}
static double real_mul(double a, double b) {
    return a * b;
}
static double real_div(double a, double b) {
    return a / b;
}
static double real_pow(double a, double b) {
    return pow(a, b);
}
static double real_atan(double x, double y) {
    return atan(y / x) * 180.0 / M_PI;
}

PdfError* postscript_op_add(PostscriptInterpreter* interpreter) {
    return binary_numeric_op(interpreter, int_add, real_add);
}

PdfError* postscript_op_sub(PostscriptInterpreter* interpreter) {
    return binary_numeric_op(interpreter, int_sub, real_sub);
}

PdfError* postscript_op_mul(PostscriptInterpreter* interpreter) {
    return binary_numeric_op(interpreter, int_mul, real_mul);
}

PdfError* postscript_op_div(PostscriptInterpreter* interpreter) {
    return binary_numeric_op(interpreter, NULL, real_div);
}

PdfError* postscript_op_idiv(PostscriptInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    PostscriptObject x, y;
    PDF_PROPAGATE(postscript_interpreter_pop_operand_typed(
        interpreter,
        POSTSCRIPT_OBJECT_INTEGER,
        true,
        &y
    ));
    PDF_PROPAGATE(postscript_interpreter_pop_operand_typed(
        interpreter,
        POSTSCRIPT_OBJECT_INTEGER,
        true,
        &x
    ));

    push_integer(interpreter, x.data.integer / y.data.integer);
    return NULL;
}

PdfError* postscript_op_mod(PostscriptInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    PostscriptObject x, y;
    PDF_PROPAGATE(postscript_interpreter_pop_operand_typed(
        interpreter,
        POSTSCRIPT_OBJECT_INTEGER,
        true,
        &y
    ));
    PDF_PROPAGATE(postscript_interpreter_pop_operand_typed(
        interpreter,
        POSTSCRIPT_OBJECT_INTEGER,
        true,
        &x
    ));

    push_integer(interpreter, x.data.integer % y.data.integer);
    return NULL;
}

static PdfError* unary_numeric_op(
    PostscriptInterpreter* interpreter,
    int32_t (*integer_op)(int32_t),
    double (*real_op)(double)
) {
    RELEASE_ASSERT(interpreter);
    RELEASE_ASSERT(real_op);

    PostscriptObject x;
    PDF_PROPAGATE(postscript_interpreter_pop_operand(interpreter, &x));

    if (integer_op && x.type == POSTSCRIPT_OBJECT_INTEGER) {
        push_integer(interpreter, integer_op(x.data.integer));
        return NULL;
    } else if (x.type == POSTSCRIPT_OBJECT_INTEGER) {
        push_real(interpreter, real_op((double)x.data.integer));
        return NULL;
    } else if (x.type == POSTSCRIPT_OBJECT_REAL) {
        push_real(interpreter, real_op(x.data.real));
        return NULL;
    }

    return PDF_ERROR(PDF_ERR_POSTSCRIPT_OPERAND_TYPE);
}

static int32_t integer_neg(int32_t a) {
    return -a;
}
static double real_neg(double a) {
    return -a;
}

PdfError* postscript_op_neg(PostscriptInterpreter* interpreter) {
    return unary_numeric_op(interpreter, integer_neg, real_neg);
}

static int32_t integer_abs(int32_t a) {
    return a >= 0 ? a : -a;
}
static double real_abs(double a) {
    return a >= 0.0 ? a : -a;
}

PdfError* postscript_op_abs(PostscriptInterpreter* interpreter) {
    return unary_numeric_op(interpreter, integer_abs, real_abs);
}

static int32_t integer_noop(int32_t x) {
    return x;
}

PdfError* postscript_op_ceiling(PostscriptInterpreter* interpreter) {
    return unary_numeric_op(interpreter, integer_noop, ceil);
}

PdfError* postscript_op_floor(PostscriptInterpreter* interpreter) {
    return unary_numeric_op(interpreter, integer_noop, floor);
}

PdfError* postscript_op_round(PostscriptInterpreter* interpreter) {
    return unary_numeric_op(interpreter, integer_noop, round);
}

PdfError* postscript_op_truncate(PostscriptInterpreter* interpreter) {
    return unary_numeric_op(interpreter, integer_noop, trunc);
}

PdfError* postscript_op_sqrt(PostscriptInterpreter* interpreter) {
    return unary_numeric_op(interpreter, NULL, sqrt);
}

static double degrees_sin(double x) {
    return sin(x * M_PI / 180.0);
}

PdfError* postscript_op_sin(PostscriptInterpreter* interpreter) {
    return unary_numeric_op(interpreter, NULL, degrees_sin);
}

static double degrees_cos(double x) {
    return cos(x * M_PI / 180.0);
}

PdfError* postscript_op_cos(PostscriptInterpreter* interpreter) {
    return unary_numeric_op(interpreter, NULL, degrees_cos);
}

PdfError* postscript_op_atan(PostscriptInterpreter* interpreter) {
    return binary_numeric_op(interpreter, NULL, real_atan);
}

PdfError* postscript_op_exp(PostscriptInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    return binary_numeric_op(interpreter, NULL, real_pow);
}

PdfError* postscript_op_ln(PostscriptInterpreter* interpreter) {
    return unary_numeric_op(interpreter, NULL, log);
}

PdfError* postscript_op_log(PostscriptInterpreter* interpreter) {
    return unary_numeric_op(interpreter, NULL, log10);
}

PdfError* postscript_op_cvi(PostscriptInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    PostscriptObject x;
    PDF_PROPAGATE(postscript_interpreter_pop_operand(interpreter, &x));

    if (x.type == POSTSCRIPT_OBJECT_INTEGER) {
        push_integer(interpreter, x.data.integer);
        return NULL;
    } else if (x.type == POSTSCRIPT_OBJECT_REAL) {
        push_integer(interpreter, (int32_t)trunc(x.data.real));
        return NULL;
    } else {
        return PDF_ERROR(PDF_ERR_POSTSCRIPT_OPERAND_TYPE);
    }
}

PdfError* postscript_op_cvr(PostscriptInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    PostscriptObject x;
    PDF_PROPAGATE(postscript_interpreter_pop_operand(interpreter, &x));

    if (x.type == POSTSCRIPT_OBJECT_REAL) {
        push_real(interpreter, x.data.real);
        return NULL;
    } else if (x.type == POSTSCRIPT_OBJECT_INTEGER) {
        push_real(interpreter, (double)x.data.integer);
        return NULL;
    } else {
        return PDF_ERROR(PDF_ERR_POSTSCRIPT_OPERAND_TYPE);
    }
}

PdfError* postscript_op_dict(PostscriptInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    PostscriptObject length_object;
    PDF_PROPAGATE(postscript_interpreter_pop_operand_typed(
        interpreter,
        POSTSCRIPT_OBJECT_INTEGER,
        true,
        &length_object
    ));

    postscript_interpreter_operand_push(
        interpreter,
        (PostscriptObject) {.type = POSTSCRIPT_OBJECT_DICT,
                            .data.dict = postscript_object_list_new(
                                postscript_interpreter_get_arena(interpreter)
                            ),
                            .access = POSTSCRIPT_ACCESS_UNLIMITED,
                            .literal = true}
    );

    return NULL;
}

PdfError* postscript_op_def(PostscriptInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    PostscriptObject value;
    PostscriptObject key;
    PDF_PROPAGATE(postscript_interpreter_pop_operand(interpreter, &value));
    PDF_PROPAGATE(postscript_interpreter_pop_operand(interpreter, &key));

    PDF_PROPAGATE(postscript_interpreter_define(interpreter, key, value));

    return NULL;
}

PdfError* postscript_op_begin(PostscriptInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    PostscriptObject dict_object;
    PDF_PROPAGATE(postscript_interpreter_pop_operand_typed(
        interpreter,
        POSTSCRIPT_OBJECT_DICT,
        true,
        &dict_object
    ));
    postscript_interpreter_dict_push(interpreter, dict_object);

    return NULL;
}

PdfError* postscript_op_end(PostscriptInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    PDF_PROPAGATE(postscript_interpreter_dict_pop(interpreter));

    return NULL;
}

PdfError* postscript_op_currentdict(PostscriptInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    PostscriptObject current_dict;
    postscript_interpreter_dict(interpreter, &current_dict);
    postscript_interpreter_operand_push(interpreter, current_dict);

    return NULL;
}

PdfError* postscript_op_defineresource(PostscriptInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    PostscriptObject category_name;
    PostscriptObject instance;
    PostscriptObject key_name;
    PDF_PROPAGATE(postscript_interpreter_pop_operand_typed(
        interpreter,
        POSTSCRIPT_OBJECT_NAME,
        true,
        &category_name
    ));
    PDF_PROPAGATE(postscript_interpreter_pop_operand_typed(
        interpreter,
        POSTSCRIPT_OBJECT_DICT,
        true,
        &instance
    ));
    PDF_PROPAGATE(postscript_interpreter_pop_operand_typed(
        interpreter,
        POSTSCRIPT_OBJECT_NAME,
        true,
        &key_name
    ));

    PostscriptResourceCategory* category = postscript_get_resource_category(
        postscript_interpreter_get_resource_categories(interpreter),
        category_name.data.name
    );
    if (!category) {
        return PDF_ERROR(
            PDF_ERR_POSTSCRIPT_UNKNOWN_RESOURCE,
            "Unknown resource category %s",
            category_name.data.name
        );
    }

    PostscriptResource resource =
        postscript_resource_new(key_name.data.name, instance);
    postscript_resource_category_add_resource(category, resource);

    postscript_interpreter_operand_push(interpreter, instance);

    return NULL;
}

PdfError* postscript_op_findresource(PostscriptInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    // Read operands
    PostscriptObject category_name_object;
    PDF_PROPAGATE(postscript_interpreter_pop_operand_typed(
        interpreter,
        POSTSCRIPT_OBJECT_NAME,
        true,
        &category_name_object
    ));

    PostscriptObject resource_name_object;
    PDF_PROPAGATE(postscript_interpreter_pop_operand_typed(
        interpreter,
        POSTSCRIPT_OBJECT_NAME,
        true,
        &resource_name_object
    ));

    // Lookup resource
    PostscriptResourceCategory* category = postscript_get_resource_category(
        postscript_interpreter_get_resource_categories(interpreter),
        category_name_object.data.name
    );
    if (!category) {
        return PDF_ERROR(
            PDF_ERR_POSTSCRIPT_UNKNOWN_RESOURCE,
            "Unknown resource category for resource %s/%s",
            category_name_object.data.name,
            resource_name_object.data.name
        );
    }

    PostscriptResource* resource = postscript_resource_category_get_resource(
        category,
        resource_name_object.data.name
    );
    if (!resource) {
        return PDF_ERROR(
            PDF_ERR_POSTSCRIPT_UNKNOWN_RESOURCE,
            "Unknown resource %s in category %s",
            resource_name_object.data.name,
            category_name_object.data.name
        );
    }

    // Push resource to operand stack
    postscript_interpreter_operand_push(interpreter, resource->object);

    return NULL;
}
