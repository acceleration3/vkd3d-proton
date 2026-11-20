#ifndef AV_LINE_COVERAGE_H
#define AV_LINE_COVERAGE_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define AV_LC_HALF_EXTENT 1.2f
#define AV_LC_CLIP_W 1.0e-5f

enum av_lc_status
{
    AV_LC_OK = 0,
    AV_LC_UNCHANGED,
    AV_LC_MALFORMED,
    AV_LC_NO_MEMORY,
    AV_LC_NO_POSITION,
    AV_LC_REFUSED_BUILTIN,
    AV_LC_REFUSED_TYPE,
    AV_LC_REFUSED_BLOCK,
    AV_LC_REFUSED_STREAM_OUTPUT,
    AV_LC_TOO_MANY,
};

enum av_lc_kind
{
    AV_LC_KIND_LOCATION = 0,
    AV_LC_KIND_POSITION,
    AV_LC_KIND_CLIP,
    AV_LC_KIND_CULL,
};

enum av_lc_scalar
{
    AV_LC_FLOAT = 0,
    AV_LC_SINT,
    AV_LC_UINT,
};

struct av_lc_varying
{
    uint32_t kind;
    uint32_t location;
    uint32_t component;
    uint32_t scalar;
    uint32_t columns;
    uint32_t elements;
};

enum
{
    AV_LC_OP_EXT_INST_IMPORT = 11,
    AV_LC_OP_EXT_INST = 12,
    AV_LC_OP_MEMORY_MODEL = 14,
    AV_LC_OP_ENTRY_POINT = 15,
    AV_LC_OP_EXECUTION_MODE = 16,
    AV_LC_OP_CAPABILITY = 17,
    AV_LC_OP_TYPE_VOID = 19,
    AV_LC_OP_TYPE_BOOL = 20,
    AV_LC_OP_TYPE_INT = 21,
    AV_LC_OP_TYPE_FLOAT = 22,
    AV_LC_OP_TYPE_VECTOR = 23,
    AV_LC_OP_TYPE_ARRAY = 28,
    AV_LC_OP_TYPE_STRUCT = 30,
    AV_LC_OP_TYPE_POINTER = 32,
    AV_LC_OP_TYPE_FUNCTION = 33,
    AV_LC_OP_CONSTANT = 43,
    AV_LC_OP_FUNCTION = 54,
    AV_LC_OP_FUNCTION_END = 56,
    AV_LC_OP_VARIABLE = 59,
    AV_LC_OP_LOAD = 61,
    AV_LC_OP_STORE = 62,
    AV_LC_OP_ACCESS_CHAIN = 65,
    AV_LC_OP_DECORATE = 71,
    AV_LC_OP_MEMBER_DECORATE = 72,
    AV_LC_OP_COMPOSITE_CONSTRUCT = 80,
    AV_LC_OP_COMPOSITE_EXTRACT = 81,
    AV_LC_OP_FNEGATE = 127,
    AV_LC_OP_FADD = 129,
    AV_LC_OP_FSUB = 131,
    AV_LC_OP_FMUL = 133,
    AV_LC_OP_FDIV = 136,
    AV_LC_OP_VECTOR_TIMES_SCALAR = 142,
    AV_LC_OP_LOGICAL_AND = 167,
    AV_LC_OP_LOGICAL_NOT = 168,
    AV_LC_OP_SELECT = 169,
    AV_LC_OP_FORD_LESS_THAN = 184,
    AV_LC_OP_FORD_GREATER_THAN = 186,
    AV_LC_OP_EMIT_VERTEX = 218,
    AV_LC_OP_END_PRIMITIVE = 219,
    AV_LC_OP_SELECTION_MERGE = 247,
    AV_LC_OP_LABEL = 248,
    AV_LC_OP_BRANCH = 249,
    AV_LC_OP_BRANCH_CONDITIONAL = 250,
    AV_LC_OP_RETURN = 253,
};

enum
{
    AV_LC_SC_INPUT = 1,
    AV_LC_SC_OUTPUT = 3,
    AV_LC_SC_PUSH_CONSTANT = 9,
};

enum
{
    AV_LC_DEC_BLOCK = 2,
    AV_LC_DEC_BUILTIN = 11,
    AV_LC_DEC_NO_PERSPECTIVE = 13,
    AV_LC_DEC_LOCATION = 30,
    AV_LC_DEC_COMPONENT = 31,
    AV_LC_DEC_INDEX = 32,
    AV_LC_DEC_OFFSET = 35,
    AV_LC_DEC_XFB_BUFFER = 36,
};

enum
{
    AV_LC_BUILTIN_POSITION = 0,
    AV_LC_BUILTIN_POINT_SIZE = 1,
    AV_LC_BUILTIN_CLIP_DISTANCE = 3,
    AV_LC_BUILTIN_CULL_DISTANCE = 4,
    AV_LC_BUILTIN_PRIMITIVE_ID = 7,
};

enum
{
    AV_LC_GLSL_FABS = 4,
    AV_LC_GLSL_FCLAMP = 43,
    AV_LC_GLSL_LENGTH = 66,
};

struct av_lc_words
{
    uint32_t *data;
    size_t count;
    size_t capacity;
    int failed;
};

static inline void av_lc_words_push(struct av_lc_words *words, uint32_t value)
{
    uint32_t *grown;
    size_t capacity;

    if (words->failed)
        return;
    if (words->count == words->capacity)
    {
        capacity = words->capacity ? 2u * words->capacity : 256u;
        grown = (uint32_t *)realloc(words->data, capacity * sizeof(*grown));
        if (!grown)
        {
            words->failed = 1;
            return;
        }
        words->data = grown;
        words->capacity = capacity;
    }
    words->data[words->count++] = value;
}

static inline void av_lc_words_append(struct av_lc_words *words, const uint32_t *values, size_t count)
{
    size_t i;

    for (i = 0; i < count; i++)
        av_lc_words_push(words, values[i]);
}

static inline void av_lc_emit(struct av_lc_words *words, uint32_t opcode, uint32_t operand_count, ...)
{
    va_list operands;
    uint32_t i;

    av_lc_words_push(words, ((operand_count + 1u) << 16) | opcode);
    va_start(operands, operand_count);
    for (i = 0; i < operand_count; i++)
        av_lc_words_push(words, (uint32_t)va_arg(operands, unsigned int));
    va_end(operands);
}

static inline void av_lc_emit_list(struct av_lc_words *words, uint32_t opcode,
        const uint32_t *operands, uint32_t operand_count)
{
    av_lc_words_push(words, ((operand_count + 1u) << 16) | opcode);
    av_lc_words_append(words, operands, operand_count);
}

static inline void av_lc_emit_string(struct av_lc_words *words, const char *text)
{
    size_t length = strlen(text) + 1u;
    size_t i;
    uint32_t word;

    for (i = 0; i < length; i += 4u)
    {
        word = 0;
        memcpy(&word, text + i, length - i < 4u ? length - i : 4u);
        av_lc_words_push(words, word);
    }
}

static inline uint32_t av_lc_float_bits(float value)
{
    uint32_t bits;

    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

struct av_lc_type_key
{
    uint32_t opcode;
    uint32_t a, b, c;
    uint32_t id;
};

struct av_lc_builder
{
    struct av_lc_words globals;
    struct av_lc_words code;
    struct av_lc_type_key keys[256];
    uint32_t key_count;
    uint32_t bound;
    int failed;
};

static inline uint32_t av_lc_id(struct av_lc_builder *builder)
{
    return builder->bound++;
}

static inline uint32_t av_lc_keyed(struct av_lc_builder *builder, uint32_t opcode, uint32_t count,
        uint32_t a, uint32_t b, uint32_t c)
{
    struct av_lc_type_key *key;
    uint32_t i, id;

    for (i = 0; i < builder->key_count; i++)
    {
        key = &builder->keys[i];
        if (key->opcode == opcode && key->a == a && key->b == b && key->c == c)
            return key->id;
    }
    if (builder->key_count == sizeof(builder->keys) / sizeof(builder->keys[0]))
    {
        builder->failed = 1;
        return 0;
    }
    id = av_lc_id(builder);
    key = &builder->keys[builder->key_count++];
    key->opcode = opcode;
    key->a = a;
    key->b = b;
    key->c = c;
    key->id = id;
    if (opcode == AV_LC_OP_CONSTANT)
        av_lc_emit(&builder->globals, opcode, 3, a, id, b);
    else if (count == 0)
        av_lc_emit(&builder->globals, opcode, 1, id);
    else if (count == 1)
        av_lc_emit(&builder->globals, opcode, 2, id, a);
    else if (count == 2)
        av_lc_emit(&builder->globals, opcode, 3, id, a, b);
    else
        av_lc_emit(&builder->globals, opcode, 4, id, a, b, c);
    return id;
}

static inline uint32_t av_lc_type_void(struct av_lc_builder *b) { return av_lc_keyed(b, AV_LC_OP_TYPE_VOID, 0, 0, 0, 0); }
static inline uint32_t av_lc_type_bool(struct av_lc_builder *b) { return av_lc_keyed(b, AV_LC_OP_TYPE_BOOL, 0, 0, 0, 0); }
static inline uint32_t av_lc_type_float(struct av_lc_builder *b) { return av_lc_keyed(b, AV_LC_OP_TYPE_FLOAT, 1, 32, 0, 0); }
static inline uint32_t av_lc_type_uint(struct av_lc_builder *b) { return av_lc_keyed(b, AV_LC_OP_TYPE_INT, 2, 32, 0, 0); }

static inline uint32_t av_lc_type_scalar(struct av_lc_builder *b, uint32_t scalar)
{
    if (scalar == AV_LC_FLOAT)
        return av_lc_type_float(b);
    return av_lc_keyed(b, AV_LC_OP_TYPE_INT, 2, 32, scalar == AV_LC_SINT ? 1u : 0u, 0);
}

static inline uint32_t av_lc_type_vector(struct av_lc_builder *b, uint32_t component, uint32_t count)
{
    if (count <= 1u)
        return component;
    return av_lc_keyed(b, AV_LC_OP_TYPE_VECTOR, 2, component, count, 0);
}

static inline uint32_t av_lc_const_uint(struct av_lc_builder *b, uint32_t value)
{
    return av_lc_keyed(b, AV_LC_OP_CONSTANT, 2, av_lc_type_uint(b), value, 0);
}

static inline uint32_t av_lc_const_float(struct av_lc_builder *b, float value)
{
    return av_lc_keyed(b, AV_LC_OP_CONSTANT, 2, av_lc_type_float(b), av_lc_float_bits(value), 0);
}

static inline uint32_t av_lc_type_array(struct av_lc_builder *b, uint32_t element, uint32_t length)
{
    return av_lc_keyed(b, AV_LC_OP_TYPE_ARRAY, 2, element, av_lc_const_uint(b, length), 0);
}

static inline uint32_t av_lc_type_pointer(struct av_lc_builder *b, uint32_t storage, uint32_t pointee)
{
    return av_lc_keyed(b, AV_LC_OP_TYPE_POINTER, 2, storage, pointee, 0);
}

static inline uint32_t av_lc_value(struct av_lc_builder *b, uint32_t opcode, uint32_t type,
        uint32_t operand_count, const uint32_t *operands)
{
    uint32_t id = av_lc_id(b);
    uint32_t i;

    av_lc_words_push(&b->code, ((operand_count + 3u) << 16) | opcode);
    av_lc_words_push(&b->code, type);
    av_lc_words_push(&b->code, id);
    for (i = 0; i < operand_count; i++)
        av_lc_words_push(&b->code, operands[i]);
    return id;
}

static inline uint32_t av_lc_binary(struct av_lc_builder *b, uint32_t opcode, uint32_t type, uint32_t x, uint32_t y)
{
    uint32_t operands[2];

    operands[0] = x;
    operands[1] = y;
    return av_lc_value(b, opcode, type, 2, operands);
}

static inline uint32_t av_lc_extract(struct av_lc_builder *b, uint32_t type, uint32_t composite, uint32_t index)
{
    return av_lc_binary(b, AV_LC_OP_COMPOSITE_EXTRACT, type, composite, index);
}

static inline uint32_t av_lc_load(struct av_lc_builder *b, uint32_t type, uint32_t pointer)
{
    return av_lc_value(b, AV_LC_OP_LOAD, type, 1, &pointer);
}

static inline uint32_t av_lc_chain(struct av_lc_builder *b, uint32_t pointer_type, uint32_t base,
        uint32_t index_count, const uint32_t *indices)
{
    uint32_t operands[3];
    uint32_t i;

    operands[0] = base;
    for (i = 0; i < index_count; i++)
        operands[1u + i] = indices[i];
    return av_lc_value(b, AV_LC_OP_ACCESS_CHAIN, pointer_type, 1u + index_count, operands);
}

static inline uint32_t av_lc_varying_type(struct av_lc_builder *b, const struct av_lc_varying *varying)
{
    uint32_t columns = varying->kind == AV_LC_KIND_POSITION ? 4u : varying->columns;
    uint32_t scalar = varying->kind == AV_LC_KIND_LOCATION ? varying->scalar : (uint32_t)AV_LC_FLOAT;

    return av_lc_type_vector(b, av_lc_type_scalar(b, scalar), columns);
}

static inline uint32_t av_lc_output_components(const struct av_lc_varying *varyings, uint32_t count)
{
    uint32_t total = 1u, i, elements;

    for (i = 0; i < count; i++)
    {
        elements = varyings[i].elements ? varyings[i].elements : 1u;
        if (varyings[i].kind == AV_LC_KIND_POSITION)
            total += 4u;
        else if (varyings[i].kind == AV_LC_KIND_LOCATION)
            total += 4u * elements;
        else
            total += elements;
    }
    return total;
}

static inline uint32_t av_lc_next_location(const struct av_lc_varying *varyings, uint32_t count)
{
    uint32_t next = 0, i, end;

    for (i = 0; i < count; i++)
    {
        if (varyings[i].kind != AV_LC_KIND_LOCATION)
            continue;
        end = varyings[i].location + (varyings[i].elements ? varyings[i].elements : 1u);
        if (end > next)
            next = end;
    }
    return next;
}

static inline uint32_t av_lc_lerp(struct av_lc_builder *b, uint32_t type, uint32_t columns,
        uint32_t x, uint32_t y, uint32_t t, uint32_t one_minus_t)
{
    uint32_t f = av_lc_type_float(b);
    uint32_t opcode = columns > 1u ? AV_LC_OP_VECTOR_TIMES_SCALAR : AV_LC_OP_FMUL;
    uint32_t xs = av_lc_binary(b, opcode, columns > 1u ? type : f, x, one_minus_t);
    uint32_t ys = av_lc_binary(b, opcode, columns > 1u ? type : f, y, t);

    return av_lc_binary(b, AV_LC_OP_FADD, type, xs, ys);
}

static inline void av_lc_assemble(struct av_lc_words *out, uint32_t bound, const struct av_lc_words *sections,
        uint32_t section_count)
{
    uint32_t i;

    av_lc_words_push(out, 0x07230203u);
    av_lc_words_push(out, 0x00010000u);
    av_lc_words_push(out, 0u);
    av_lc_words_push(out, bound);
    av_lc_words_push(out, 0u);
    for (i = 0; i < section_count; i++)
    {
        if (sections[i].failed)
            out->failed = 1;
        av_lc_words_append(out, sections[i].data, sections[i].count);
    }
}

static inline int av_lc_build_geometry_shader(const struct av_lc_varying *varyings, uint32_t count,
        uint32_t adjacency, uint32_t coverage_location, uint32_t push_offset,
        uint32_t **out_words, size_t *out_count)
{
    struct av_lc_words head = {NULL, 0, 0, 0}, decorations = {NULL, 0, 0, 0}, result = {NULL, 0, 0, 0};
    struct av_lc_words sections[4];
    struct av_lc_builder *b;
    uint32_t interface_ids[72];
    uint32_t inputs[32], outputs[32];
    uint32_t ends[32][2][16];
    uint32_t i, e, k, end, interface_count = 0, position = UINT32_MAX;
    uint32_t vertex_count = adjacency ? 4u : 2u;
    uint32_t first = adjacency ? 1u : 0u;
    uint32_t f, v2, v4, boolean, main_id, glsl, push_type, push_var, coverage_var, c_first, c_second;
    uint32_t p[2], w[2], behind[2], dw, tc, t[2], omt[2], q[2], half, ndc, s[2], d, len, n, offset;
    uint32_t both, draw, body_label, merge_label, indices[2], pointer, element_type, type;
    uint32_t has_clip = 0, has_cull = 0;
    uint32_t ox, oy, side_x, side_y, corner, operands[4], qw, qx, qy, primitive_in, primitive_out, primitive;
    float side;
    int status = AV_LC_OK;

    if (count > 32u)
        return AV_LC_TOO_MANY;
    for (i = 0; i < count; i++)
    {
        if (varyings[i].kind == AV_LC_KIND_POSITION)
            position = i;
        if ((varyings[i].elements ? varyings[i].elements : 1u) > 16u)
            return AV_LC_TOO_MANY;
    }
    if (position == UINT32_MAX)
        return AV_LC_NO_POSITION;

    b = (struct av_lc_builder *)calloc(1, sizeof(*b));
    if (!b)
        return AV_LC_NO_MEMORY;
    b->bound = 1;

    main_id = av_lc_id(b);
    glsl = av_lc_id(b);
    f = av_lc_type_float(b);
    v2 = av_lc_type_vector(b, f, 2);
    v4 = av_lc_type_vector(b, f, 4);
    boolean = av_lc_type_bool(b);

    for (i = 0; i < count; i++)
    {
        type = av_lc_varying_type(b, &varyings[i]);
        if (varyings[i].elements)
            type = av_lc_type_array(b, type, varyings[i].elements);
        inputs[i] = av_lc_id(b);
        outputs[i] = av_lc_id(b);
        av_lc_emit(&b->globals, AV_LC_OP_VARIABLE, 3,
                av_lc_type_pointer(b, AV_LC_SC_INPUT, av_lc_type_array(b, type, vertex_count)), inputs[i], AV_LC_SC_INPUT);
        av_lc_emit(&b->globals, AV_LC_OP_VARIABLE, 3, av_lc_type_pointer(b, AV_LC_SC_OUTPUT, type), outputs[i], AV_LC_SC_OUTPUT);
        interface_ids[interface_count++] = inputs[i];
        interface_ids[interface_count++] = outputs[i];
        for (k = 0; k < 2; k++)
        {
            uint32_t target = k ? outputs[i] : inputs[i];

            if (varyings[i].kind == AV_LC_KIND_LOCATION)
            {
                av_lc_emit(&decorations, AV_LC_OP_DECORATE, 3, target, AV_LC_DEC_LOCATION, varyings[i].location);
                if (varyings[i].component)
                    av_lc_emit(&decorations, AV_LC_OP_DECORATE, 3, target, AV_LC_DEC_COMPONENT, varyings[i].component);
            }
            else
            {
                av_lc_emit(&decorations, AV_LC_OP_DECORATE, 3, target, AV_LC_DEC_BUILTIN,
                        varyings[i].kind == AV_LC_KIND_POSITION ? AV_LC_BUILTIN_POSITION
                        : varyings[i].kind == AV_LC_KIND_CLIP ? AV_LC_BUILTIN_CLIP_DISTANCE : AV_LC_BUILTIN_CULL_DISTANCE);
            }
        }
        if (varyings[i].kind == AV_LC_KIND_CLIP)
            has_clip = 1;
        if (varyings[i].kind == AV_LC_KIND_CULL)
            has_cull = 1;
    }

    {
        uint32_t sint = av_lc_type_scalar(b, AV_LC_SINT);

        primitive_in = av_lc_id(b);
        primitive_out = av_lc_id(b);
        av_lc_emit(&b->globals, AV_LC_OP_VARIABLE, 3, av_lc_type_pointer(b, AV_LC_SC_INPUT, sint), primitive_in, AV_LC_SC_INPUT);
        av_lc_emit(&b->globals, AV_LC_OP_VARIABLE, 3, av_lc_type_pointer(b, AV_LC_SC_OUTPUT, sint), primitive_out, AV_LC_SC_OUTPUT);
        av_lc_emit(&decorations, AV_LC_OP_DECORATE, 3, primitive_in, AV_LC_DEC_BUILTIN, AV_LC_BUILTIN_PRIMITIVE_ID);
        av_lc_emit(&decorations, AV_LC_OP_DECORATE, 3, primitive_out, AV_LC_DEC_BUILTIN, AV_LC_BUILTIN_PRIMITIVE_ID);
        interface_ids[interface_count++] = primitive_in;
        interface_ids[interface_count++] = primitive_out;
    }

    coverage_var = av_lc_id(b);
    av_lc_emit(&b->globals, AV_LC_OP_VARIABLE, 3, av_lc_type_pointer(b, AV_LC_SC_OUTPUT, f), coverage_var, AV_LC_SC_OUTPUT);
    av_lc_emit(&decorations, AV_LC_OP_DECORATE, 3, coverage_var, AV_LC_DEC_LOCATION, coverage_location);
    av_lc_emit(&decorations, AV_LC_OP_DECORATE, 2, coverage_var, AV_LC_DEC_NO_PERSPECTIVE);
    interface_ids[interface_count++] = coverage_var;

    push_type = av_lc_id(b);
    av_lc_emit(&b->globals, AV_LC_OP_TYPE_STRUCT, 2, push_type, v2);
    push_var = av_lc_id(b);
    av_lc_emit(&b->globals, AV_LC_OP_VARIABLE, 3, av_lc_type_pointer(b, AV_LC_SC_PUSH_CONSTANT, push_type),
            push_var, AV_LC_SC_PUSH_CONSTANT);
    av_lc_emit(&decorations, AV_LC_OP_DECORATE, 2, push_type, AV_LC_DEC_BLOCK);
    av_lc_emit(&decorations, AV_LC_OP_MEMBER_DECORATE, 4, push_type, 0, AV_LC_DEC_OFFSET, push_offset);

    c_first = av_lc_const_uint(b, first);
    c_second = av_lc_const_uint(b, first + 1u);

    {
        uint32_t void_type = av_lc_type_void(b);
        uint32_t function_type = av_lc_keyed(b, AV_LC_OP_TYPE_FUNCTION, 1, void_type, 0, 0);

        av_lc_emit(&b->code, AV_LC_OP_FUNCTION, 4, void_type, main_id, 0, function_type);
        av_lc_emit(&b->code, AV_LC_OP_LABEL, 1, av_lc_id(b));
    }

    for (k = 0; k < 2; k++)
    {
        indices[0] = k ? c_second : c_first;
        pointer = av_lc_chain(b, av_lc_type_pointer(b, AV_LC_SC_INPUT, v4), inputs[position], 1, indices);
        p[k] = av_lc_load(b, v4, pointer);
        w[k] = av_lc_extract(b, f, p[k], 3);
        behind[k] = av_lc_binary(b, AV_LC_OP_FORD_LESS_THAN, boolean, w[k], av_lc_const_float(b, AV_LC_CLIP_W));
    }
    dw = av_lc_binary(b, AV_LC_OP_FSUB, f, w[1], w[0]);
    tc = av_lc_binary(b, AV_LC_OP_FDIV, f, av_lc_binary(b, AV_LC_OP_FSUB, f, av_lc_const_float(b, AV_LC_CLIP_W), w[0]), dw);
    operands[0] = behind[0];
    operands[1] = tc;
    operands[2] = av_lc_const_float(b, 0.0f);
    t[0] = av_lc_value(b, AV_LC_OP_SELECT, f, 3, operands);
    operands[0] = behind[1];
    operands[2] = av_lc_const_float(b, 1.0f);
    t[1] = av_lc_value(b, AV_LC_OP_SELECT, f, 3, operands);
    for (k = 0; k < 2; k++)
    {
        omt[k] = av_lc_binary(b, AV_LC_OP_FSUB, f, av_lc_const_float(b, 1.0f), t[k]);
        q[k] = av_lc_lerp(b, v4, 4, p[0], p[1], t[k], omt[k]);
    }

    indices[0] = av_lc_const_uint(b, 0);
    pointer = av_lc_chain(b, av_lc_type_pointer(b, AV_LC_SC_PUSH_CONSTANT, v2), push_var, 1, indices);
    half = av_lc_binary(b, AV_LC_OP_VECTOR_TIMES_SCALAR, v2, av_lc_load(b, v2, pointer), av_lc_const_float(b, 0.5f));
    for (k = 0; k < 2; k++)
    {
        operands[0] = av_lc_extract(b, f, q[k], 0);
        operands[1] = av_lc_extract(b, f, q[k], 1);
        ndc = av_lc_binary(b, AV_LC_OP_VECTOR_TIMES_SCALAR, v2, av_lc_value(b, AV_LC_OP_COMPOSITE_CONSTRUCT, v2, 2, operands),
                av_lc_binary(b, AV_LC_OP_FDIV, f, av_lc_const_float(b, 1.0f), av_lc_extract(b, f, q[k], 3)));
        s[k] = av_lc_binary(b, AV_LC_OP_FMUL, v2, ndc, half);
    }
    d = av_lc_binary(b, AV_LC_OP_FSUB, v2, s[1], s[0]);
    operands[0] = glsl;
    operands[1] = AV_LC_GLSL_LENGTH;
    operands[2] = d;
    len = av_lc_value(b, AV_LC_OP_EXT_INST, f, 3, operands);
    {
        uint32_t dy = av_lc_extract(b, f, d, 1);
        uint32_t dx = av_lc_extract(b, f, d, 0);

        operands[0] = av_lc_value(b, AV_LC_OP_FNEGATE, f, 1, &dy);
        operands[1] = dx;
    }
    n = av_lc_binary(b, AV_LC_OP_VECTOR_TIMES_SCALAR, v2, av_lc_value(b, AV_LC_OP_COMPOSITE_CONSTRUCT, v2, 2, operands),
            av_lc_binary(b, AV_LC_OP_FDIV, f, av_lc_const_float(b, AV_LC_HALF_EXTENT), len));
    offset = av_lc_binary(b, AV_LC_OP_FDIV, v2, n, half);

    both = av_lc_binary(b, AV_LC_OP_LOGICAL_AND, boolean, behind[0], behind[1]);
    draw = av_lc_binary(b, AV_LC_OP_LOGICAL_AND, boolean, av_lc_value(b, AV_LC_OP_LOGICAL_NOT, boolean, 1, &both),
            av_lc_binary(b, AV_LC_OP_FORD_GREATER_THAN, boolean, len, av_lc_const_float(b, 0.0f)));

    body_label = av_lc_id(b);
    merge_label = av_lc_id(b);
    av_lc_emit(&b->code, AV_LC_OP_SELECTION_MERGE, 2, merge_label, 0);
    av_lc_emit(&b->code, AV_LC_OP_BRANCH_CONDITIONAL, 3, draw, body_label, merge_label);
    av_lc_emit(&b->code, AV_LC_OP_LABEL, 1, body_label);

    for (i = 0; i < count; i++)
    {
        uint32_t elements = varyings[i].elements ? varyings[i].elements : 1u;
        uint32_t columns = varyings[i].kind == AV_LC_KIND_LOCATION ? varyings[i].columns : 1u;
        int lerped = varyings[i].kind != AV_LC_KIND_LOCATION || varyings[i].scalar == AV_LC_FLOAT;

        if (i == position)
            continue;
        element_type = av_lc_varying_type(b, &varyings[i]);
        for (e = 0; e < elements; e++)
        {
            uint32_t values[2];

            for (k = 0; k < 2; k++)
            {
                uint32_t chain[2];

                chain[0] = k ? c_second : c_first;
                chain[1] = av_lc_const_uint(b, e);
                pointer = av_lc_chain(b, av_lc_type_pointer(b, AV_LC_SC_INPUT, element_type), inputs[i],
                        varyings[i].elements ? 2u : 1u, chain);
                values[k] = av_lc_load(b, element_type, pointer);
            }
            for (end = 0; end < 2; end++)
                ends[i][end][e] = lerped ? av_lc_lerp(b, element_type, columns, values[0], values[1], t[end], omt[end])
                        : values[0];
        }
    }

    primitive = av_lc_load(b, av_lc_type_scalar(b, AV_LC_SINT), primitive_in);
    for (corner = 0; corner < 4; corner++)
    {
        end = corner >> 1;
        side = (corner & 1u) ? 1.0f : -1.0f;
        qw = av_lc_extract(b, f, q[end], 3);
        qx = av_lc_extract(b, f, q[end], 0);
        qy = av_lc_extract(b, f, q[end], 1);
        side_x = av_lc_binary(b, AV_LC_OP_FMUL, f, av_lc_extract(b, f, offset, 0), av_lc_const_float(b, side));
        side_y = av_lc_binary(b, AV_LC_OP_FMUL, f, av_lc_extract(b, f, offset, 1), av_lc_const_float(b, side));
        ox = av_lc_binary(b, AV_LC_OP_FMUL, f, side_x, qw);
        oy = av_lc_binary(b, AV_LC_OP_FMUL, f, side_y, qw);
        operands[0] = av_lc_binary(b, AV_LC_OP_FADD, f, qx, ox);
        operands[1] = av_lc_binary(b, AV_LC_OP_FADD, f, qy, oy);
        operands[2] = av_lc_extract(b, f, q[end], 2);
        operands[3] = qw;
        av_lc_emit(&b->code, AV_LC_OP_STORE, 2, outputs[position], av_lc_value(b, AV_LC_OP_COMPOSITE_CONSTRUCT, v4, 4, operands));

        for (i = 0; i < count; i++)
        {
            uint32_t elements = varyings[i].elements ? varyings[i].elements : 1u;

            if (i == position)
                continue;
            element_type = av_lc_varying_type(b, &varyings[i]);
            for (e = 0; e < elements; e++)
            {
                if (varyings[i].elements)
                {
                    indices[0] = av_lc_const_uint(b, e);
                    pointer = av_lc_chain(b, av_lc_type_pointer(b, AV_LC_SC_OUTPUT, element_type), outputs[i], 1, indices);
                }
                else
                {
                    pointer = outputs[i];
                }
                av_lc_emit(&b->code, AV_LC_OP_STORE, 2, pointer, ends[i][end][e]);
            }
        }
        av_lc_emit(&b->code, AV_LC_OP_STORE, 2, primitive_out, primitive);
        av_lc_emit(&b->code, AV_LC_OP_STORE, 2, coverage_var, av_lc_const_float(b, side * AV_LC_HALF_EXTENT));
        av_lc_emit(&b->code, AV_LC_OP_EMIT_VERTEX, 0);
    }
    av_lc_emit(&b->code, AV_LC_OP_END_PRIMITIVE, 0);
    av_lc_emit(&b->code, AV_LC_OP_BRANCH, 1, merge_label);
    av_lc_emit(&b->code, AV_LC_OP_LABEL, 1, merge_label);
    av_lc_emit(&b->code, AV_LC_OP_RETURN, 0);
    av_lc_emit(&b->code, AV_LC_OP_FUNCTION_END, 0);

    av_lc_emit(&head, AV_LC_OP_CAPABILITY, 1, 1u);
    av_lc_emit(&head, AV_LC_OP_CAPABILITY, 1, 2u);
    if (has_clip)
        av_lc_emit(&head, AV_LC_OP_CAPABILITY, 1, 32u);
    if (has_cull)
        av_lc_emit(&head, AV_LC_OP_CAPABILITY, 1, 33u);
    av_lc_words_push(&head, (6u << 16) | AV_LC_OP_EXT_INST_IMPORT);
    av_lc_words_push(&head, glsl);
    av_lc_emit_string(&head, "GLSL.std.450");
    av_lc_emit(&head, AV_LC_OP_MEMORY_MODEL, 2, 0u, 1u);
    av_lc_words_push(&head, ((uint32_t)(5u + interface_count) << 16) | AV_LC_OP_ENTRY_POINT);
    av_lc_words_push(&head, 3u);
    av_lc_words_push(&head, main_id);
    av_lc_emit_string(&head, "main");
    av_lc_words_append(&head, interface_ids, interface_count);
    av_lc_emit(&head, AV_LC_OP_EXECUTION_MODE, 3, main_id, 0u, 1u);
    av_lc_emit(&head, AV_LC_OP_EXECUTION_MODE, 2, main_id, adjacency ? 21u : 20u);
    av_lc_emit(&head, AV_LC_OP_EXECUTION_MODE, 2, main_id, 29u);
    av_lc_emit(&head, AV_LC_OP_EXECUTION_MODE, 3, main_id, 26u, 4u);

    sections[0] = head;
    sections[1] = decorations;
    sections[2] = b->globals;
    sections[3] = b->code;
    av_lc_assemble(&result, b->bound, sections, 4);
    if (result.failed || b->failed || b->globals.failed || b->code.failed || head.failed || decorations.failed)
        status = AV_LC_NO_MEMORY;

    free(head.data);
    free(decorations.data);
    free(b->globals.data);
    free(b->code.data);
    free(b);

    if (status != AV_LC_OK)
    {
        free(result.data);
        return status;
    }
    *out_words = result.data;
    *out_count = result.count;
    return AV_LC_OK;
}

struct av_lc_module
{
    const uint32_t *words;
    size_t count;
    uint32_t bound;
    uint32_t *offsets;
    uint32_t *location;
    uint32_t *component;
    uint32_t *builtin;
    uint32_t *index;
    uint8_t *flags;
    uint8_t *member_builtin;
    size_t entry_point;
    uint32_t entry_function;
    uint32_t execution_model;
    uint32_t glsl;
    size_t memory_model;
    size_t globals;
    size_t functions;
};

enum
{
    AV_LC_HAS_LOCATION = 1,
    AV_LC_HAS_COMPONENT = 2,
    AV_LC_HAS_BUILTIN = 4,
    AV_LC_HAS_INDEX = 8,
    AV_LC_HAS_XFB = 16,
};

static inline void av_lc_module_free(struct av_lc_module *module)
{
    free(module->offsets);
    free(module->location);
    free(module->component);
    free(module->builtin);
    free(module->index);
    free(module->flags);
    free(module->member_builtin);
}

static inline int av_lc_is_preamble(uint32_t opcode)
{
    switch (opcode)
    {
        case 2: case 3: case 4: case 5: case 6: case 7: case 10: case 11: case 14: case 15: case 16: case 17:
        case 71: case 72: case 73: case 74: case 75: case 330: case 331: case 332: case 5632: case 5633:
            return 1;
        default:
            return 0;
    }
}

static inline int av_lc_module_open(struct av_lc_module *module, const uint32_t *words, size_t count)
{
    size_t at, length;
    uint32_t opcode, id;

    memset(module, 0, sizeof(*module));
    if (count < 5u || words[0] != 0x07230203u || !words[3] || words[3] > (1u << 22))
        return AV_LC_MALFORMED;
    module->words = words;
    module->count = count;
    module->bound = words[3];
    module->offsets = (uint32_t *)calloc(module->bound, sizeof(uint32_t));
    module->location = (uint32_t *)calloc(module->bound, sizeof(uint32_t));
    module->component = (uint32_t *)calloc(module->bound, sizeof(uint32_t));
    module->builtin = (uint32_t *)calloc(module->bound, sizeof(uint32_t));
    module->index = (uint32_t *)calloc(module->bound, sizeof(uint32_t));
    module->flags = (uint8_t *)calloc(module->bound, 1);
    module->member_builtin = (uint8_t *)calloc(module->bound, 1);
    if (!module->offsets || !module->location || !module->component || !module->builtin || !module->index
            || !module->flags || !module->member_builtin)
    {
        av_lc_module_free(module);
        return AV_LC_NO_MEMORY;
    }
    module->entry_point = SIZE_MAX;
    module->memory_model = SIZE_MAX;
    module->globals = SIZE_MAX;
    module->functions = SIZE_MAX;

    for (at = 5u; at < count; at += length)
    {
        opcode = words[at] & 0xffffu;
        length = words[at] >> 16;
        if (!length || at + length > count)
        {
            av_lc_module_free(module);
            return AV_LC_MALFORMED;
        }
        if (module->globals == SIZE_MAX && !av_lc_is_preamble(opcode))
            module->globals = at;
        if (opcode == AV_LC_OP_FUNCTION && module->functions == SIZE_MAX)
            module->functions = at;
        if (opcode == AV_LC_OP_MEMORY_MODEL)
            module->memory_model = at;
        if (opcode == AV_LC_OP_ENTRY_POINT && module->entry_point == SIZE_MAX && length >= 4u)
        {
            module->entry_point = at;
            module->execution_model = words[at + 1];
            module->entry_function = words[at + 2];
        }
        if (opcode == AV_LC_OP_EXT_INST_IMPORT && length == 6u && !memcmp(&words[at + 2], "GLSL.std.450", 13u))
            module->glsl = words[at + 1];
        if (opcode == AV_LC_OP_DECORATE && length >= 3u && words[at + 1] < module->bound)
        {
            id = words[at + 1];
            switch (words[at + 2])
            {
                case AV_LC_DEC_LOCATION:
                    if (length >= 4u) { module->location[id] = words[at + 3]; module->flags[id] |= AV_LC_HAS_LOCATION; }
                    break;
                case AV_LC_DEC_COMPONENT:
                    if (length >= 4u) { module->component[id] = words[at + 3]; module->flags[id] |= AV_LC_HAS_COMPONENT; }
                    break;
                case AV_LC_DEC_BUILTIN:
                    if (length >= 4u) { module->builtin[id] = words[at + 3]; module->flags[id] |= AV_LC_HAS_BUILTIN; }
                    break;
                case AV_LC_DEC_INDEX:
                    if (length >= 4u) { module->index[id] = words[at + 3]; module->flags[id] |= AV_LC_HAS_INDEX; }
                    break;
                case AV_LC_DEC_XFB_BUFFER:
                    module->flags[id] |= AV_LC_HAS_XFB;
                    break;
                default:
                    break;
            }
        }
        if (opcode == AV_LC_OP_MEMBER_DECORATE && length >= 4u && words[at + 1] < module->bound
                && words[at + 3] == AV_LC_DEC_BUILTIN)
            module->member_builtin[words[at + 1]] = 1;
        if (opcode == AV_LC_OP_EXECUTION_MODE && length >= 3u && words[at + 2] == 11u)
            module->flags[0] |= AV_LC_HAS_XFB;
        if (opcode != AV_LC_OP_FUNCTION && (opcode == AV_LC_OP_VARIABLE || opcode == AV_LC_OP_CONSTANT
                || (opcode >= AV_LC_OP_TYPE_VOID && opcode <= AV_LC_OP_TYPE_FUNCTION)))
        {
            id = opcode == AV_LC_OP_VARIABLE || opcode == AV_LC_OP_CONSTANT ? (length >= 3u ? words[at + 2] : 0u)
                    : (length >= 2u ? words[at + 1] : 0u);
            if (id && id < module->bound)
                module->offsets[id] = (uint32_t)at;
        }
    }
    if (module->entry_point == SIZE_MAX || module->memory_model == SIZE_MAX || module->functions == SIZE_MAX
            || module->globals == SIZE_MAX)
    {
        av_lc_module_free(module);
        return AV_LC_MALFORMED;
    }
    return AV_LC_OK;
}

static inline const uint32_t *av_lc_module_def(const struct av_lc_module *module, uint32_t id)
{
    if (!id || id >= module->bound || !module->offsets[id])
        return NULL;
    return &module->words[module->offsets[id]];
}

static inline uint32_t av_lc_string_words(const uint32_t *words, uint32_t available)
{
    uint32_t i;

    for (i = 0; i < available; i++)
        if (!(words[i] & 0xff000000u))
            return i + 1u;
    return available;
}

static inline int av_lc_module_in_interface(const struct av_lc_module *module, uint32_t id)
{
    const uint32_t *ins = &module->words[module->entry_point];
    uint32_t length = ins[0] >> 16, i;

    for (i = 3u + av_lc_string_words(&ins[3], length - 3u); i < length; i++)
        if (ins[i] == id)
            return 1;
    return 0;
}

static inline int av_lc_describe_scalar(const struct av_lc_module *module, uint32_t type, uint32_t *scalar)
{
    const uint32_t *def = av_lc_module_def(module, type);

    if (!def)
        return 0;
    if ((def[0] & 0xffffu) == AV_LC_OP_TYPE_FLOAT && def[2] == 32u)
    {
        *scalar = AV_LC_FLOAT;
        return 1;
    }
    if ((def[0] & 0xffffu) == AV_LC_OP_TYPE_INT && def[2] == 32u)
    {
        *scalar = def[3] ? AV_LC_SINT : AV_LC_UINT;
        return 1;
    }
    return 0;
}

static inline int av_lc_describe_type(const struct av_lc_module *module, uint32_t type, struct av_lc_varying *varying)
{
    const uint32_t *def = av_lc_module_def(module, type);
    const uint32_t *length;

    varying->elements = 0;
    varying->columns = 1;
    if (!def)
        return AV_LC_MALFORMED;
    if ((def[0] & 0xffffu) == AV_LC_OP_TYPE_STRUCT)
        return AV_LC_REFUSED_BLOCK;
    if ((def[0] & 0xffffu) == AV_LC_OP_TYPE_ARRAY)
    {
        length = av_lc_module_def(module, def[3]);
        if (!length || (length[0] & 0xffffu) != AV_LC_OP_CONSTANT || !length[3] || length[3] > 32u)
            return AV_LC_REFUSED_TYPE;
        varying->elements = length[3];
        def = av_lc_module_def(module, def[2]);
        if (!def)
            return AV_LC_MALFORMED;
    }
    if ((def[0] & 0xffffu) == AV_LC_OP_TYPE_VECTOR)
    {
        varying->columns = def[3];
        if (!av_lc_describe_scalar(module, def[2], &varying->scalar) || def[3] > 4u)
            return AV_LC_REFUSED_TYPE;
        return AV_LC_OK;
    }
    if (!av_lc_describe_scalar(module, def[1], &varying->scalar))
        return AV_LC_REFUSED_TYPE;
    return AV_LC_OK;
}

static inline int av_lc_reflect(const uint32_t *words, size_t count, uint32_t storage_class,
        struct av_lc_varying *varyings, uint32_t capacity, uint32_t *found)
{
    struct av_lc_module module;
    struct av_lc_varying varying;
    const uint32_t *pointer;
    size_t at, length;
    uint32_t id, builtin;
    int status;

    *found = 0;
    if ((status = av_lc_module_open(&module, words, count)))
        return status;
    if (storage_class == AV_LC_SC_OUTPUT && (module.flags[0] & AV_LC_HAS_XFB))
    {
        av_lc_module_free(&module);
        return AV_LC_REFUSED_STREAM_OUTPUT;
    }
    for (at = module.globals; at < module.functions && status == AV_LC_OK; at += length)
    {
        length = words[at] >> 16;
        if ((words[at] & 0xffffu) != AV_LC_OP_VARIABLE || length < 4u || words[at + 3] != storage_class)
            continue;
        id = words[at + 2];
        if (!av_lc_module_in_interface(&module, id))
            continue;
        pointer = av_lc_module_def(&module, words[at + 1]);
        if (!pointer || (pointer[0] & 0xffffu) != AV_LC_OP_TYPE_POINTER)
        {
            status = AV_LC_MALFORMED;
            break;
        }
        if (module.member_builtin[pointer[3]])
        {
            status = storage_class == AV_LC_SC_OUTPUT ? AV_LC_REFUSED_BLOCK : AV_LC_OK;
            continue;
        }
        if (module.flags[id] & AV_LC_HAS_XFB)
        {
            status = AV_LC_REFUSED_STREAM_OUTPUT;
            break;
        }
        memset(&varying, 0, sizeof(varying));
        if (module.flags[id] & AV_LC_HAS_BUILTIN)
        {
            if (storage_class != AV_LC_SC_OUTPUT)
                continue;
            builtin = module.builtin[id];
            if (builtin == AV_LC_BUILTIN_POINT_SIZE)
                continue;
            if (builtin == AV_LC_BUILTIN_POSITION)
                varying.kind = AV_LC_KIND_POSITION;
            else if (builtin == AV_LC_BUILTIN_CLIP_DISTANCE)
                varying.kind = AV_LC_KIND_CLIP;
            else if (builtin == AV_LC_BUILTIN_CULL_DISTANCE)
                varying.kind = AV_LC_KIND_CULL;
            else
            {
                status = AV_LC_REFUSED_BUILTIN;
                break;
            }
        }
        else if (module.flags[id] & AV_LC_HAS_LOCATION)
        {
            varying.kind = AV_LC_KIND_LOCATION;
            varying.location = module.location[id];
            varying.component = module.component[id];
        }
        else
        {
            status = AV_LC_MALFORMED;
            break;
        }
        status = av_lc_describe_type(&module, pointer[3], &varying);
        if (status == AV_LC_REFUSED_TYPE && storage_class == AV_LC_SC_INPUT)
            status = AV_LC_OK;
        if (status)
            break;
        if (varying.kind != AV_LC_KIND_LOCATION && varying.scalar != AV_LC_FLOAT)
        {
            status = AV_LC_REFUSED_TYPE;
            break;
        }
        if (*found == capacity)
        {
            status = AV_LC_TOO_MANY;
            break;
        }
        varyings[(*found)++] = varying;
    }
    av_lc_module_free(&module);
    return status;
}

static inline uint32_t av_lc_find_type(const struct av_lc_module *module, uint32_t opcode, uint32_t a, uint32_t b,
        uint32_t operand_count)
{
    size_t at, length;
    const uint32_t *words = module->words;

    for (at = module->globals; at < module->functions; at += length)
    {
        length = words[at] >> 16;
        if ((words[at] & 0xffffu) != opcode || length != operand_count + 2u)
            continue;
        if (operand_count >= 1u && words[at + 2] != a)
            continue;
        if (operand_count >= 2u && words[at + 3] != b)
            continue;
        return words[at + 1];
    }
    return 0;
}

static inline int av_lc_patch_fragment_shader(const uint32_t *words, size_t count, uint32_t coverage_location,
        uint32_t **out_words, size_t *out_count)
{
    struct av_lc_module module;
    struct av_lc_words out = {NULL, 0, 0, 0}, globals = {NULL, 0, 0, 0};
    const uint32_t *pointer, *vector;
    uint32_t alpha_target = 0, float_type = 0, uint_type, ptr_output, ptr_input, coverage, glsl;
    uint32_t c_three, c_extent, c_zero, c_one, bound, id, opcode;
    size_t at, length, i;
    int in_entry = 0, status;

    if ((status = av_lc_module_open(&module, words, count)))
        return status;
    if (module.execution_model != 4u)
    {
        av_lc_module_free(&module);
        return AV_LC_MALFORMED;
    }
    for (at = module.globals; at < module.functions; at += length)
    {
        length = words[at] >> 16;
        if ((words[at] & 0xffffu) != AV_LC_OP_VARIABLE || length < 4u || words[at + 3] != AV_LC_SC_OUTPUT)
            continue;
        id = words[at + 2];
        if (!(module.flags[id] & AV_LC_HAS_LOCATION) || module.location[id] != 0u
                || ((module.flags[id] & AV_LC_HAS_INDEX) && module.index[id] != 0u)
                || ((module.flags[id] & AV_LC_HAS_COMPONENT) && module.component[id] != 0u)
                || !av_lc_module_in_interface(&module, id))
            continue;
        pointer = av_lc_module_def(&module, words[at + 1]);
        vector = pointer ? av_lc_module_def(&module, pointer[3]) : NULL;
        if (!vector || (vector[0] & 0xffffu) != AV_LC_OP_TYPE_VECTOR || vector[3] != 4u)
            continue;
        {
            const uint32_t *component = av_lc_module_def(&module, vector[2]);

            if (!component || (component[0] & 0xffffu) != AV_LC_OP_TYPE_FLOAT || component[2] != 32u)
                continue;
        }
        alpha_target = id;
        float_type = vector[2];
        break;
    }
    if (!alpha_target)
    {
        av_lc_module_free(&module);
        return AV_LC_UNCHANGED;
    }

    bound = module.bound;
    uint_type = av_lc_find_type(&module, AV_LC_OP_TYPE_INT, 32u, 0u, 2u);
    if (!uint_type)
    {
        uint_type = bound++;
        av_lc_emit(&globals, AV_LC_OP_TYPE_INT, 3, uint_type, 32u, 0u);
    }
    ptr_output = av_lc_find_type(&module, AV_LC_OP_TYPE_POINTER, AV_LC_SC_OUTPUT, float_type, 2u);
    if (!ptr_output)
    {
        ptr_output = bound++;
        av_lc_emit(&globals, AV_LC_OP_TYPE_POINTER, 3, ptr_output, AV_LC_SC_OUTPUT, float_type);
    }
    ptr_input = av_lc_find_type(&module, AV_LC_OP_TYPE_POINTER, AV_LC_SC_INPUT, float_type, 2u);
    if (!ptr_input)
    {
        ptr_input = bound++;
        av_lc_emit(&globals, AV_LC_OP_TYPE_POINTER, 3, ptr_input, AV_LC_SC_INPUT, float_type);
    }
    c_three = bound++;
    av_lc_emit(&globals, AV_LC_OP_CONSTANT, 3, uint_type, c_three, 3u);
    c_extent = bound++;
    av_lc_emit(&globals, AV_LC_OP_CONSTANT, 3, float_type, c_extent, av_lc_float_bits(AV_LC_HALF_EXTENT));
    c_zero = bound++;
    av_lc_emit(&globals, AV_LC_OP_CONSTANT, 3, float_type, c_zero, av_lc_float_bits(0.0f));
    c_one = bound++;
    av_lc_emit(&globals, AV_LC_OP_CONSTANT, 3, float_type, c_one, av_lc_float_bits(1.0f));
    coverage = bound++;
    av_lc_emit(&globals, AV_LC_OP_VARIABLE, 3, ptr_input, coverage, AV_LC_SC_INPUT);
    glsl = module.glsl ? module.glsl : bound++;

    av_lc_words_append(&out, words, 5);
    for (at = 5u; at < count; at += length)
    {
        opcode = words[at] & 0xffffu;
        length = words[at] >> 16;
        if (at == module.memory_model && !module.glsl)
        {
            av_lc_words_push(&out, (6u << 16) | AV_LC_OP_EXT_INST_IMPORT);
            av_lc_words_push(&out, glsl);
            av_lc_emit_string(&out, "GLSL.std.450");
        }
        if (at == module.globals)
        {
            av_lc_emit(&out, AV_LC_OP_DECORATE, 3, coverage, AV_LC_DEC_LOCATION, coverage_location);
            av_lc_emit(&out, AV_LC_OP_DECORATE, 2, coverage, AV_LC_DEC_NO_PERSPECTIVE);
        }
        if (at == module.functions)
            av_lc_words_append(&out, globals.data, globals.count);
        if (at == module.entry_point)
        {
            av_lc_words_push(&out, ((uint32_t)(length + 1u) << 16) | opcode);
            av_lc_words_append(&out, &words[at + 1], length - 1u);
            av_lc_words_push(&out, coverage);
            continue;
        }
        if (opcode == AV_LC_OP_FUNCTION)
            in_entry = length >= 3u && words[at + 2] == module.entry_function;
        if (opcode == AV_LC_OP_RETURN && in_entry)
        {
            uint32_t distance = bound++, magnitude = bound++, remaining = bound++, factor = bound++;
            uint32_t component = bound++, alpha = bound++, scaled = bound++;

            av_lc_emit(&out, AV_LC_OP_LOAD, 3, float_type, distance, coverage);
            av_lc_emit(&out, AV_LC_OP_EXT_INST, 5, float_type, magnitude, glsl, AV_LC_GLSL_FABS, distance);
            av_lc_emit(&out, AV_LC_OP_FSUB, 4, float_type, remaining, c_extent, magnitude);
            av_lc_emit(&out, AV_LC_OP_EXT_INST, 7, float_type, factor, glsl, AV_LC_GLSL_FCLAMP, remaining, c_zero, c_one);
            av_lc_emit(&out, AV_LC_OP_ACCESS_CHAIN, 4, ptr_output, component, alpha_target, c_three);
            av_lc_emit(&out, AV_LC_OP_LOAD, 3, float_type, alpha, component);
            av_lc_emit(&out, AV_LC_OP_FMUL, 4, float_type, scaled, alpha, factor);
            av_lc_emit(&out, AV_LC_OP_STORE, 2, component, scaled);
        }
        if (opcode == AV_LC_OP_FUNCTION_END)
            in_entry = 0;
        for (i = 0; i < length; i++)
            av_lc_words_push(&out, words[at + i]);
    }
    out.data[3] = bound;
    status = out.failed || globals.failed ? AV_LC_NO_MEMORY : AV_LC_OK;
    free(globals.data);
    av_lc_module_free(&module);
    if (status != AV_LC_OK)
    {
        free(out.data);
        return status;
    }
    *out_words = out.data;
    *out_count = out.count;
    return AV_LC_OK;
}

#endif
