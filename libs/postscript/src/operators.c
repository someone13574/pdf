#include "operators.h"

#include <math.h>
#include <stdbool.h>

#include "arena/arena.h"
#include "err/error.h"
#include "logger/log.h"
#include "postscript/interpreter.h"
#include "postscript/object.h"
#include "postscript/resource.h"

static void push_operator(PSObjectList* dict, PSOperator operator, char* name) {
    RELEASE_ASSERT(dict);
    RELEASE_ASSERT(operator);
    RELEASE_ASSERT(name);

    LOG_DIAG(DEBUG, PS, "Adding systemdict operator `%s`", name);

    ps_object_list_push_back(
        dict,
        (PSObject) {.type = PS_OBJECT_NAME,
                    .data.name = name,
                    .access = PS_ACCESS_UNLIMITED,
                    .literal = true}
    );

    ps_object_list_push_back(
        dict,
        (PSObject) {.type = PS_OBJECT_OPERATOR,
                    .data.operator = operator,
                    .access = PS_ACCESS_EXECUTE_ONLY,
                    .literal = false}
    );
}

PSObject ps_systemdict_ops(Arena* arena) {
    RELEASE_ASSERT(arena);

    LOG_DIAG(INFO, PS, "Getting systemdict operators");

    PSObject dict = {
        .type = PS_OBJECT_DICT,
        .data.dict = ps_object_list_new(arena),
        .access = PS_ACCESS_READ_ONLY,
        .literal = true
    };

    push_operator(dict.data.dict, ps_builtin_op_pop, "pop");
    push_operator(dict.data.dict, ps_builtin_op_exch, "exch");
    push_operator(dict.data.dict, ps_builtin_op_dup, "dup");
    push_operator(dict.data.dict, ps_builtin_op_copy, "copy");
    push_operator(dict.data.dict, ps_builtin_op_index, "index");
    push_operator(dict.data.dict, ps_builtin_op_roll, "roll");

    push_operator(dict.data.dict, ps_builtin_op_add, "add");
    push_operator(dict.data.dict, ps_builtin_op_sub, "sub");
    push_operator(dict.data.dict, ps_builtin_op_mul, "mul");
    push_operator(dict.data.dict, ps_builtin_op_div, "div");
    push_operator(dict.data.dict, ps_builtin_op_idiv, "idiv");
    push_operator(dict.data.dict, ps_builtin_op_mod, "mod");
    push_operator(dict.data.dict, ps_builtin_op_neg, "neg");
    push_operator(dict.data.dict, ps_builtin_op_abs, "abs");
    push_operator(dict.data.dict, ps_builtin_op_ceiling, "ceiling");
    push_operator(dict.data.dict, ps_builtin_op_floor, "floor");
    push_operator(dict.data.dict, ps_builtin_op_round, "round");
    push_operator(dict.data.dict, ps_builtin_op_truncate, "truncate");
    push_operator(dict.data.dict, ps_builtin_op_sqrt, "sqrt");
    push_operator(dict.data.dict, ps_builtin_op_sin, "sin");
    push_operator(dict.data.dict, ps_builtin_op_cos, "cos");
    push_operator(dict.data.dict, ps_builtin_op_atan, "atan");
    push_operator(dict.data.dict, ps_builtin_op_exp, "exp");
    push_operator(dict.data.dict, ps_builtin_op_ln, "ln");
    push_operator(dict.data.dict, ps_builtin_op_log, "log");
    push_operator(dict.data.dict, ps_builtin_op_cvi, "cvi");
    push_operator(dict.data.dict, ps_builtin_op_cvr, "cvr");

    push_operator(dict.data.dict, ps_builtin_op_dict, "dict");
    push_operator(dict.data.dict, ps_builtin_op_def, "def");
    push_operator(dict.data.dict, ps_builtin_op_begin, "begin");
    push_operator(dict.data.dict, ps_builtin_op_end, "end");
    push_operator(dict.data.dict, ps_builtin_op_currentdict, "currentdict");
    push_operator(dict.data.dict, ps_builtin_op_findresource, "findresource");
    push_operator(
        dict.data.dict,
        ps_builtin_op_defineresource,
        "defineresource"
    );

    return dict;
}

Error* ps_builtin_op_pop(PSInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    PSObject popped;
    TRY(ps_interpreter_pop_operand(interpreter, &popped));

    return NULL;
}

Error* ps_builtin_op_exch(PSInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    PSObject a;
    TRY(ps_interpreter_pop_operand(interpreter, &a));
    PSObject b;
    TRY(ps_interpreter_pop_operand(interpreter, &b));

    ps_interpreter_operand_push(interpreter, a);
    ps_interpreter_operand_push(interpreter, b);

    return NULL;
}

Error* ps_builtin_op_dup(PSInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    PSObject object;
    TRY(ps_interpreter_pop_operand(interpreter, &object));

    ps_interpreter_operand_push(interpreter, object);
    ps_interpreter_operand_push(interpreter, object);

    return NULL;
}

Error* ps_builtin_op_copy(PSInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    PSObject count_object;
    TRY(ps_interpreter_pop_operand_typed(
        interpreter,
        PS_OBJECT_INTEGER,
        true,
        &count_object
    ));

    Arena* local_arena = arena_new(128);
    PSObjectList* list = ps_object_list_new(local_arena);

    for (int32_t idx = 0; idx < count_object.data.integer; idx++) {
        PSObject object;
        TRY(ps_interpreter_pop_operand(interpreter, &object));

        ps_object_list_push_back(list, object);
    }

    for (size_t pass = 0; pass < 2; pass++) {
        for (int32_t idx = 0; idx < count_object.data.integer; idx++) {
            PSObject object;
            RELEASE_ASSERT(ps_object_list_get(
                list,
                (size_t)(count_object.data.integer - idx - 1),
                &object
            ));

            ps_interpreter_operand_push(interpreter, object);
        }
    }

    arena_free(local_arena);
    return NULL;
}

Error* ps_builtin_op_index(PSInterpreter* interpreter) {
    PSObject idx_object;
    TRY(ps_interpreter_pop_operand_typed(
        interpreter,
        PS_OBJECT_INTEGER,
        true,
        &idx_object
    ));

    PSObjectList* stack = ps_interpreter_stack(interpreter);
    size_t len = ps_object_list_len(stack);
    if (idx_object.data.integer >= (int32_t)len
        || idx_object.data.integer < 0) {
        return ERROR(PS_ERR_OPERANDS_EMPTY);
    }

    PSObject object;
    if (!ps_object_list_get(
            stack,
            len - (size_t)idx_object.data.integer - 1,
            &object
        )) {
        return ERROR(PS_ERR_OPERANDS_EMPTY);
    }

    return NULL;
}

Error* ps_builtin_op_roll(PSInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    PSObject j_object;
    TRY(ps_interpreter_pop_operand_typed(
        interpreter,
        PS_OBJECT_INTEGER,
        true,
        &j_object
    ));

    PSObject n_object;
    TRY(ps_interpreter_pop_operand_typed(
        interpreter,
        PS_OBJECT_INTEGER,
        true,
        &n_object
    ));

    int32_t n = n_object.data.integer;
    int32_t j = j_object.data.integer;

    if (n < 0) {
        return ERROR(PS_ERR_OPERANDS_EMPTY);
    }
    if (n == 0) {
        return NULL;
    }

    Arena* local_arena = arena_new(128);
    PSObjectList* list = ps_object_list_new(local_arena);

    for (int32_t idx = 0; idx < n; idx++) {
        PSObject object;
        TRY(ps_interpreter_pop_operand(interpreter, &object));
        ps_object_list_push_back(list, object);
    }

    int32_t j_mod = j % n;
    if (j_mod < 0) {
        j_mod += n;
    }

    for (int32_t k = n - 1; k >= 0; k--) {
        PSObject object;
        bool success =
            ps_object_list_get(list, (size_t)((k + j_mod) % n), &object);
        RELEASE_ASSERT(success);
        ps_interpreter_operand_push(interpreter, object);
    }

    arena_free(local_arena);
    return NULL;
}

static Error* object_to_double(PSObject obj, double* out) {
    if (obj.type == PS_OBJECT_INTEGER) {
        *out = (double)obj.data.integer;
        return NULL;
    } else if (obj.type == PS_OBJECT_REAL) {
        *out = obj.data.real;
        return NULL;
    } else {
        return ERROR(
            PS_ERR_OPERAND_TYPE,
            "Postscript numbers must be an integer or real"
        );
    }
}

static inline void push_integer(PSInterpreter* interpreter, int32_t x) {
    ps_interpreter_operand_push(
        interpreter,
        (PSObject) {.type = PS_OBJECT_INTEGER,
                    .data.integer = x,
                    .literal = true,
                    .access = PS_ACCESS_UNLIMITED}
    );
}

static inline void push_real(PSInterpreter* interpreter, double x) {
    ps_interpreter_operand_push(
        interpreter,
        (PSObject) {.type = PS_OBJECT_REAL,
                    .data.real = x,
                    .literal = true,
                    .access = PS_ACCESS_UNLIMITED}
    );
}

static Error* binary_numeric_op(
    PSInterpreter* interpreter,
    int32_t (*integer_op)(int32_t, int32_t),
    double (*real_op)(double, double)
) {
    RELEASE_ASSERT(interpreter);
    RELEASE_ASSERT(real_op);

    PSObject x, y;
    TRY(ps_interpreter_pop_operand(interpreter, &y));
    TRY(ps_interpreter_pop_operand(interpreter, &x));

    if (integer_op && x.type == PS_OBJECT_INTEGER
        && y.type == PS_OBJECT_INTEGER) {
        push_integer(interpreter, integer_op(x.data.integer, y.data.integer));
        return NULL;
    }

    double x_real, y_real;
    TRY(object_to_double(x, &x_real));
    TRY(object_to_double(y, &y_real));

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

Error* ps_builtin_op_add(PSInterpreter* interpreter) {
    return binary_numeric_op(interpreter, int_add, real_add);
}

Error* ps_builtin_op_sub(PSInterpreter* interpreter) {
    return binary_numeric_op(interpreter, int_sub, real_sub);
}

Error* ps_builtin_op_mul(PSInterpreter* interpreter) {
    return binary_numeric_op(interpreter, int_mul, real_mul);
}

Error* ps_builtin_op_div(PSInterpreter* interpreter) {
    return binary_numeric_op(interpreter, NULL, real_div);
}

Error* ps_builtin_op_idiv(PSInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    PSObject x, y;
    TRY(ps_interpreter_pop_operand_typed(
        interpreter,
        PS_OBJECT_INTEGER,
        true,
        &y
    ));
    TRY(ps_interpreter_pop_operand_typed(
        interpreter,
        PS_OBJECT_INTEGER,
        true,
        &x
    ));

    push_integer(interpreter, x.data.integer / y.data.integer);
    return NULL;
}

Error* ps_builtin_op_mod(PSInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    PSObject x, y;
    TRY(ps_interpreter_pop_operand_typed(
        interpreter,
        PS_OBJECT_INTEGER,
        true,
        &y
    ));
    TRY(ps_interpreter_pop_operand_typed(
        interpreter,
        PS_OBJECT_INTEGER,
        true,
        &x
    ));

    push_integer(interpreter, x.data.integer % y.data.integer);
    return NULL;
}

static Error* unary_numeric_op(
    PSInterpreter* interpreter,
    int32_t (*integer_op)(int32_t),
    double (*real_op)(double)
) {
    RELEASE_ASSERT(interpreter);
    RELEASE_ASSERT(real_op);

    PSObject x;
    TRY(ps_interpreter_pop_operand(interpreter, &x));

    if (integer_op && x.type == PS_OBJECT_INTEGER) {
        push_integer(interpreter, integer_op(x.data.integer));
        return NULL;
    } else if (x.type == PS_OBJECT_INTEGER) {
        push_real(interpreter, real_op((double)x.data.integer));
        return NULL;
    } else if (x.type == PS_OBJECT_REAL) {
        push_real(interpreter, real_op(x.data.real));
        return NULL;
    }

    return ERROR(PS_ERR_OPERAND_TYPE);
}

static int32_t integer_neg(int32_t a) {
    return -a;
}
static double real_neg(double a) {
    return -a;
}

Error* ps_builtin_op_neg(PSInterpreter* interpreter) {
    return unary_numeric_op(interpreter, integer_neg, real_neg);
}

static int32_t integer_abs(int32_t a) {
    return a >= 0 ? a : -a;
}
static double real_abs(double a) {
    return a >= 0.0 ? a : -a;
}

Error* ps_builtin_op_abs(PSInterpreter* interpreter) {
    return unary_numeric_op(interpreter, integer_abs, real_abs);
}

static int32_t integer_noop(int32_t x) {
    return x;
}

Error* ps_builtin_op_ceiling(PSInterpreter* interpreter) {
    return unary_numeric_op(interpreter, integer_noop, ceil);
}

Error* ps_builtin_op_floor(PSInterpreter* interpreter) {
    return unary_numeric_op(interpreter, integer_noop, floor);
}

Error* ps_builtin_op_round(PSInterpreter* interpreter) {
    return unary_numeric_op(interpreter, integer_noop, round);
}

Error* ps_builtin_op_truncate(PSInterpreter* interpreter) {
    return unary_numeric_op(interpreter, integer_noop, trunc);
}

Error* ps_builtin_op_sqrt(PSInterpreter* interpreter) {
    return unary_numeric_op(interpreter, NULL, sqrt);
}

static double degrees_sin(double x) {
    return sin(x * M_PI / 180.0);
}

Error* ps_builtin_op_sin(PSInterpreter* interpreter) {
    return unary_numeric_op(interpreter, NULL, degrees_sin);
}

static double degrees_cos(double x) {
    return cos(x * M_PI / 180.0);
}

Error* ps_builtin_op_cos(PSInterpreter* interpreter) {
    return unary_numeric_op(interpreter, NULL, degrees_cos);
}

Error* ps_builtin_op_atan(PSInterpreter* interpreter) {
    return binary_numeric_op(interpreter, NULL, real_atan);
}

Error* ps_builtin_op_exp(PSInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    return binary_numeric_op(interpreter, NULL, real_pow);
}

Error* ps_builtin_op_ln(PSInterpreter* interpreter) {
    return unary_numeric_op(interpreter, NULL, log);
}

Error* ps_builtin_op_log(PSInterpreter* interpreter) {
    return unary_numeric_op(interpreter, NULL, log10);
}

Error* ps_builtin_op_cvi(PSInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    PSObject x;
    TRY(ps_interpreter_pop_operand(interpreter, &x));

    if (x.type == PS_OBJECT_INTEGER) {
        push_integer(interpreter, x.data.integer);
        return NULL;
    } else if (x.type == PS_OBJECT_REAL) {
        push_integer(interpreter, (int32_t)trunc(x.data.real));
        return NULL;
    } else {
        return ERROR(PS_ERR_OPERAND_TYPE);
    }
}

Error* ps_builtin_op_cvr(PSInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    PSObject x;
    TRY(ps_interpreter_pop_operand(interpreter, &x));

    if (x.type == PS_OBJECT_REAL) {
        push_real(interpreter, x.data.real);
        return NULL;
    } else if (x.type == PS_OBJECT_INTEGER) {
        push_real(interpreter, (double)x.data.integer);
        return NULL;
    } else {
        return ERROR(PS_ERR_OPERAND_TYPE);
    }
}

Error* ps_builtin_op_dict(PSInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    PSObject length_object;
    TRY(ps_interpreter_pop_operand_typed(
        interpreter,
        PS_OBJECT_INTEGER,
        true,
        &length_object
    ));

    ps_interpreter_operand_push(
        interpreter,
        (PSObject) {.type = PS_OBJECT_DICT,
                    .data.dict = ps_object_list_new(
                        ps_interpreter_get_arena(interpreter)
                    ),
                    .access = PS_ACCESS_UNLIMITED,
                    .literal = true}
    );

    return NULL;
}

Error* ps_builtin_op_def(PSInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    PSObject value;
    PSObject key;
    TRY(ps_interpreter_pop_operand(interpreter, &value));
    TRY(ps_interpreter_pop_operand(interpreter, &key));

    TRY(ps_interpreter_define(interpreter, key, value));

    return NULL;
}

Error* ps_builtin_op_begin(PSInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    PSObject dict_object;
    TRY(ps_interpreter_pop_operand_typed(
        interpreter,
        PS_OBJECT_DICT,
        true,
        &dict_object
    ));
    ps_interpreter_dict_push(interpreter, dict_object);

    return NULL;
}

Error* ps_builtin_op_end(PSInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    TRY(ps_interpreter_dict_pop(interpreter));

    return NULL;
}

Error* ps_builtin_op_currentdict(PSInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    PSObject current_dict;
    ps_interpreter_dict(interpreter, &current_dict);
    ps_interpreter_operand_push(interpreter, current_dict);

    return NULL;
}

Error* ps_builtin_op_defineresource(PSInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    PSObject category_name;
    PSObject instance;
    PSObject key_name;
    TRY(ps_interpreter_pop_operand_typed(
        interpreter,
        PS_OBJECT_NAME,
        true,
        &category_name
    ));
    TRY(ps_interpreter_pop_operand_typed(
        interpreter,
        PS_OBJECT_DICT,
        true,
        &instance
    ));
    TRY(ps_interpreter_pop_operand_typed(
        interpreter,
        PS_OBJECT_NAME,
        true,
        &key_name
    ));

    PSResourceCategory* category = ps_get_resource_category(
        ps_interpreter_get_resource_categories(interpreter),
        category_name.data.name
    );
    if (!category) {
        return ERROR(
            PS_ERR_UNKNOWN_RESOURCE,
            "Unknown resource category %s",
            category_name.data.name
        );
    }

    PSResource resource = ps_resource_new(key_name.data.name, instance);
    ps_resource_category_add_resource(category, resource);

    ps_interpreter_operand_push(interpreter, instance);

    return NULL;
}

Error* ps_builtin_op_findresource(PSInterpreter* interpreter) {
    RELEASE_ASSERT(interpreter);

    // Read operands
    PSObject category_name_object;
    TRY(ps_interpreter_pop_operand_typed(
        interpreter,
        PS_OBJECT_NAME,
        true,
        &category_name_object
    ));

    PSObject resource_name_object;
    TRY(ps_interpreter_pop_operand_typed(
        interpreter,
        PS_OBJECT_NAME,
        true,
        &resource_name_object
    ));

    // Lookup resource
    PSResourceCategory* category = ps_get_resource_category(
        ps_interpreter_get_resource_categories(interpreter),
        category_name_object.data.name
    );
    if (!category) {
        return ERROR(
            PS_ERR_UNKNOWN_RESOURCE,
            "Unknown resource category for resource %s/%s",
            category_name_object.data.name,
            resource_name_object.data.name
        );
    }

    PSResource* resource = ps_resource_category_get_resource(
        category,
        resource_name_object.data.name
    );
    if (!resource) {
        return ERROR(
            PS_ERR_UNKNOWN_RESOURCE,
            "Unknown resource %s in category %s",
            resource_name_object.data.name,
            category_name_object.data.name
        );
    }

    // Push resource to operand stack
    ps_interpreter_operand_push(interpreter, resource->object);

    return NULL;
}
