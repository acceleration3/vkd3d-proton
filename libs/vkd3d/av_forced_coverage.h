#ifndef AV_FORCED_COVERAGE_H
#define AV_FORCED_COVERAGE_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum av_fc_status
{
    AV_FC_OK = 0,
    AV_FC_UNSUPPORTED,
    AV_FC_MALFORMED,
    AV_FC_NO_MEMORY,
};

enum
{
    AV_FC_OP_EXTENSION = 10,
    AV_FC_OP_ENTRY_POINT = 15,
    AV_FC_OP_CAPABILITY = 17,
    AV_FC_OP_TYPE_BOOL = 20,
    AV_FC_OP_TYPE_INT = 21,
    AV_FC_OP_TYPE_FLOAT = 22,
    AV_FC_OP_TYPE_VECTOR = 23,
    AV_FC_OP_TYPE_ARRAY = 28,
    AV_FC_OP_TYPE_POINTER = 32,
    AV_FC_OP_CONSTANT = 43,
    AV_FC_OP_CONSTANT_COMPOSITE = 44,
    AV_FC_OP_FUNCTION = 54,
    AV_FC_OP_FUNCTION_END = 56,
    AV_FC_OP_VARIABLE = 59,
    AV_FC_OP_LOAD = 61,
    AV_FC_OP_STORE = 62,
    AV_FC_OP_ACCESS_CHAIN = 65,
    AV_FC_OP_IN_BOUNDS_ACCESS_CHAIN = 66,
    AV_FC_OP_PTR_ACCESS_CHAIN = 67,
    AV_FC_OP_DECORATE = 71,
    AV_FC_OP_BITCAST = 124,
    AV_FC_OP_FADD = 129,
    AV_FC_OP_VECTOR_TIMES_SCALAR = 142,
    AV_FC_OP_ALL = 155,
    AV_FC_OP_SELECT = 169,
    AV_FC_OP_IEQUAL = 170,
    AV_FC_OP_FORD_GREATER_THAN_EQUAL = 190,
    AV_FC_OP_BITWISE_OR = 197,
    AV_FC_OP_DPDX = 207,
    AV_FC_OP_DPDY = 208,
    AV_FC_OP_PHI = 245,
    AV_FC_OP_SELECTION_MERGE = 247,
    AV_FC_OP_LABEL = 248,
    AV_FC_OP_BRANCH_CONDITIONAL = 250,
    AV_FC_OP_KILL = 252,
    AV_FC_OP_LINE = 8,
    AV_FC_OP_NO_LINE = 317,
    AV_FC_OP_TERMINATE_INVOCATION = 4416,
};

enum
{
    AV_FC_MODEL_FRAGMENT = 4,
    AV_FC_SC_INPUT = 1,
    AV_FC_SC_PRIVATE = 6,
    AV_FC_DEC_BUILTIN = 11,
    AV_FC_DEC_CENTROID = 16,
    AV_FC_DEC_SAMPLE = 17,
    AV_FC_BUILTIN_SAMPLE_MASK = 20,
    AV_FC_BUILTIN_BARY_COORD_NO_PERSP = 5287,
    AV_FC_CAPABILITY_FRAGMENT_BARYCENTRIC = 5284,
    AV_FC_MOST_SAMPLES = 16,
};

static const char av_fc_extension[] = "SPV_KHR_fragment_shader_barycentric";

static inline const int8_t *av_fc_pattern(uint32_t samples)
{
    static const int8_t two[] = {4, 4, -4, -4};
    static const int8_t four[] = {-2, -6, 6, -2, -6, 2, 2, 6};
    static const int8_t eight[] = {1, -3, -1, 3, 5, 1, -3, -5, -5, 5, -7, -1, 3, 7, 7, -7};
    static const int8_t sixteen[] = {1, 1, -1, -3, -3, 2, 4, -1, -5, -2, 2, 5, 5, 3, 3, -5,
                                     -2, 6, 0, -7, -4, -6, -6, 4, -8, 0, 7, -4, 6, 7, -7, -8};

    switch (samples)
    {
    case 2: return two;
    case 4: return four;
    case 8: return eight;
    case 16: return sixteen;
    default: return NULL;
    }
}

struct av_fc_words
{
    uint32_t *data;
    size_t count;
    size_t capacity;
    int failed;
};

static inline void av_fc_push(struct av_fc_words *words, uint32_t value)
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

static inline void av_fc_op(struct av_fc_words *words, uint32_t opcode, uint32_t length)
{
    av_fc_push(words, (length << 16) | opcode);
}

static inline void av_fc_op3(struct av_fc_words *words, uint32_t opcode, uint32_t a, uint32_t b, uint32_t c)
{
    av_fc_op(words, opcode, 4u);
    av_fc_push(words, a);
    av_fc_push(words, b);
    av_fc_push(words, c);
}

static inline void av_fc_op4(struct av_fc_words *words, uint32_t opcode, uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    av_fc_op(words, opcode, 5u);
    av_fc_push(words, a);
    av_fc_push(words, b);
    av_fc_push(words, c);
    av_fc_push(words, d);
}

struct av_fc_module
{
    const uint32_t *words;
    size_t count;
    uint32_t bound;
    uint32_t version;
    size_t first_extension;
    size_t decorations_at;
    size_t types_end;
    size_t entry_at;
    uint32_t entry;
    uint32_t bool_type, uint_type, float_type, vec3_type, bvec3_type;
    uint32_t mask_input, mask_array, mask_element;
    uint32_t bary_input;
    int has_capability, has_extension, bary_shifted;
};

static inline int av_fc_is_annotation(uint32_t opcode)
{
    return opcode == AV_FC_OP_DECORATE || opcode == 72u || opcode == 73u || opcode == 74u || opcode == 75u
            || opcode == 332u || opcode == 5632u || opcode == 5633u;
}

static inline int av_fc_is_preamble(uint32_t opcode)
{
    return opcode == 2u || opcode == 3u || opcode == 4u || opcode == 5u || opcode == 6u || opcode == 7u
            || opcode == AV_FC_OP_EXTENSION || opcode == 11u || opcode == 14u || opcode == AV_FC_OP_ENTRY_POINT
            || opcode == 16u || opcode == AV_FC_OP_CAPABILITY || opcode == 330u || opcode == 331u;
}

static inline int av_fc_names_extension(const uint32_t *words, size_t length)
{
    size_t bytes = (length - 1u) * sizeof(uint32_t);
    const char *text = (const char *)(words + 1);

    return bytes >= sizeof(av_fc_extension) && !memcmp(text, av_fc_extension, sizeof(av_fc_extension));
}

static inline uint32_t av_fc_find_type(const struct av_fc_module *module, uint32_t opcode, uint32_t a, uint32_t b,
        int operands)
{
    size_t at, length;

    for (at = 5u; at < module->types_end; at += length)
    {
        length = module->words[at] >> 16;
        if ((module->words[at] & 0xffffu) != opcode || length != (size_t)(2 + operands))
            continue;
        if (operands >= 1 && module->words[at + 2] != a)
            continue;
        if (operands >= 2 && module->words[at + 3] != b)
            continue;
        return module->words[at + 1];
    }
    return 0;
}

static inline uint32_t av_fc_pointee(const struct av_fc_module *module, uint32_t pointer, uint32_t *storage)
{
    size_t at, length;

    for (at = 5u; at < module->types_end; at += length)
    {
        length = module->words[at] >> 16;
        if ((module->words[at] & 0xffffu) == AV_FC_OP_TYPE_POINTER && length == 4u && module->words[at + 1] == pointer)
        {
            *storage = module->words[at + 2];
            return module->words[at + 3];
        }
    }
    return 0;
}

static inline int av_fc_module_open(struct av_fc_module *module, const uint32_t *words, size_t count)
{
    uint32_t opcode, mask_candidate = 0, bary_candidate = 0, pointer = 0, storage = 0, element = 0;
    size_t at, length;
    int entries = 0;

    memset(module, 0, sizeof(*module));
    if (!words || count < 5u || words[0] != 0x07230203u || !words[3] || words[3] > (1u << 22))
        return AV_FC_MALFORMED;
    module->words = words;
    module->count = count;
    module->bound = words[3];
    module->version = words[1];
    module->types_end = count;
    for (at = 5u; at < count; at += length)
    {
        length = words[at] >> 16;
        opcode = words[at] & 0xffffu;
        if (!length || at + length > count)
            return AV_FC_MALFORMED;
        if (opcode == AV_FC_OP_FUNCTION)
        {
            module->types_end = at;
            break;
        }
        if (opcode != AV_FC_OP_CAPABILITY && !module->first_extension)
            module->first_extension = at;
        if (av_fc_is_annotation(opcode))
            module->decorations_at = at + length;
        else if (!av_fc_is_preamble(opcode) && !module->decorations_at)
            module->decorations_at = at;
        if (opcode == AV_FC_OP_CAPABILITY && length == 2u && words[at + 1] == AV_FC_CAPABILITY_FRAGMENT_BARYCENTRIC)
            module->has_capability = 1;
        if (opcode == AV_FC_OP_EXTENSION && av_fc_names_extension(words + at, length))
            module->has_extension = 1;
        if (opcode == AV_FC_OP_ENTRY_POINT && length >= 4u)
        {
            entries++;
            if (words[at + 1] == AV_FC_MODEL_FRAGMENT)
            {
                module->entry_at = at;
                module->entry = words[at + 2];
            }
        }
        if (opcode == AV_FC_OP_DECORATE && length >= 4u && words[at + 2] == AV_FC_DEC_BUILTIN)
        {
            if (words[at + 3] == AV_FC_BUILTIN_SAMPLE_MASK)
                mask_candidate = words[at + 1];
            if (words[at + 3] == AV_FC_BUILTIN_BARY_COORD_NO_PERSP)
                bary_candidate = words[at + 1];
        }
    }
    if (entries != 1 || !module->entry || module->types_end == count || !module->decorations_at)
        return AV_FC_UNSUPPORTED;
    for (at = 5u; at < module->types_end; at += length)
    {
        length = words[at] >> 16;
        opcode = words[at] & 0xffffu;
        if (opcode == AV_FC_OP_DECORATE && length >= 3u && bary_candidate && words[at + 1] == bary_candidate
                && (words[at + 2] == AV_FC_DEC_CENTROID || words[at + 2] == AV_FC_DEC_SAMPLE))
            module->bary_shifted = 1;
        if (opcode == AV_FC_OP_VARIABLE && length >= 4u && words[at + 3] == AV_FC_SC_INPUT)
        {
            if (words[at + 2] == mask_candidate)
            {
                module->mask_input = mask_candidate;
                pointer = words[at + 1];
            }
            if (words[at + 2] == bary_candidate)
                module->bary_input = bary_candidate;
        }
    }
    if (module->bary_shifted)
        return AV_FC_UNSUPPORTED;
    module->bool_type = av_fc_find_type(module, AV_FC_OP_TYPE_BOOL, 0, 0, 0);
    module->uint_type = av_fc_find_type(module, AV_FC_OP_TYPE_INT, 32u, 0u, 2);
    module->float_type = av_fc_find_type(module, AV_FC_OP_TYPE_FLOAT, 32u, 0u, 1);
    if (module->float_type)
        module->vec3_type = av_fc_find_type(module, AV_FC_OP_TYPE_VECTOR, module->float_type, 3u, 2);
    if (module->bool_type)
        module->bvec3_type = av_fc_find_type(module, AV_FC_OP_TYPE_VECTOR, module->bool_type, 3u, 2);
    if (module->mask_input)
    {
        module->mask_array = av_fc_pointee(module, pointer, &storage);
        for (at = 5u; at < module->types_end && module->mask_array; at += length)
        {
            length = words[at] >> 16;
            if ((words[at] & 0xffffu) == AV_FC_OP_TYPE_ARRAY && length == 4u && words[at + 1] == module->mask_array)
                element = words[at + 2];
        }
        if (!element)
            return AV_FC_UNSUPPORTED;
        module->mask_element = element;
    }
    return AV_FC_OK;
}

static inline uint32_t av_fc_mask_signed(const struct av_fc_module *module)
{
    size_t at, length;

    for (at = 5u; at < module->types_end; at += length)
    {
        length = module->words[at] >> 16;
        if ((module->words[at] & 0xffffu) == AV_FC_OP_TYPE_INT && length == 4u
                && module->words[at + 1] == module->mask_element)
            return module->words[at + 3];
    }
    return 0;
}

static inline int av_fc_reads_mask_elsewhere(const struct av_fc_module *module)
{
    size_t at, length, i;
    uint32_t opcode;

    if (!module->mask_input)
        return 0;
    for (at = module->types_end; at < module->count; at += length)
    {
        length = module->words[at] >> 16;
        opcode = module->words[at] & 0xffffu;
        if (opcode == AV_FC_OP_LOAD || opcode == AV_FC_OP_ACCESS_CHAIN || opcode == AV_FC_OP_IN_BOUNDS_ACCESS_CHAIN
                || opcode == AV_FC_OP_PTR_ACCESS_CHAIN)
        {
            for (i = 4u; i < length; i++)
                if (module->words[at + i] == module->mask_input)
                    return 1;
            continue;
        }
        for (i = 1u; i < length; i++)
            if (module->words[at + i] == module->mask_input)
                return 1;
    }
    return 0;
}

static inline int av_fc_compute_coverage(const uint32_t *words, size_t count, uint32_t samples,
        uint32_t **out_words, size_t *out_count)
{
    struct av_fc_words out = {NULL, 0, 0, 0}, globals = {NULL, 0, 0, 0}, block = {NULL, 0, 0, 0};
    struct av_fc_module module;
    uint32_t bound, input_vec3_pointer, private_array_pointer = 0, private_element_pointer = 0, mask_private = 0;
    uint32_t zero_float, zero_vec3, zero_uint, bit, offset_x, offset_y, barycentric, dx, dy, scaled_x, scaled_y;
    uint32_t moved, at_sample, inside, all_inside, chosen, mask, next_mask, element, stored, is_zero;
    uint32_t kill_label = 0, continue_label = 0, entry_label = 0, opcode, signed_mask = 0, zero_index = 0;
    uint32_t bary, sample;
    const int8_t *pattern = av_fc_pattern(samples);
    size_t at, length, i;
    int status, in_entry = 0, entry_open = 0;
    union
    {
        float value;
        uint32_t bits;
    } number;

    *out_words = NULL;
    *out_count = 0;
    if (!pattern)
        return AV_FC_UNSUPPORTED;
    if ((status = av_fc_module_open(&module, words, count)))
        return status;
    if (av_fc_reads_mask_elsewhere(&module))
        return AV_FC_UNSUPPORTED;
    bound = module.bound;
    if (!module.bool_type)
    {
        module.bool_type = bound++;
        av_fc_op(&globals, AV_FC_OP_TYPE_BOOL, 2u);
        av_fc_push(&globals, module.bool_type);
    }
    if (!module.uint_type)
    {
        module.uint_type = bound++;
        av_fc_op3(&globals, AV_FC_OP_TYPE_INT, module.uint_type, 32u, 0u);
    }
    if (!module.float_type)
    {
        module.float_type = bound++;
        av_fc_op(&globals, AV_FC_OP_TYPE_FLOAT, 3u);
        av_fc_push(&globals, module.float_type);
        av_fc_push(&globals, 32u);
    }
    if (!module.vec3_type)
    {
        module.vec3_type = bound++;
        av_fc_op3(&globals, AV_FC_OP_TYPE_VECTOR, module.vec3_type, module.float_type, 3u);
    }
    if (!module.bvec3_type)
    {
        module.bvec3_type = bound++;
        av_fc_op3(&globals, AV_FC_OP_TYPE_VECTOR, module.bvec3_type, module.bool_type, 3u);
    }
    bary = module.bary_input;
    if (!bary)
    {
        input_vec3_pointer = bound++;
        av_fc_op3(&globals, AV_FC_OP_TYPE_POINTER, input_vec3_pointer, AV_FC_SC_INPUT, module.vec3_type);
        bary = bound++;
        av_fc_op3(&globals, AV_FC_OP_VARIABLE, input_vec3_pointer, bary, AV_FC_SC_INPUT);
    }
    if (module.mask_input)
    {
        signed_mask = av_fc_mask_signed(&module);
        private_array_pointer = bound++;
        av_fc_op3(&globals, AV_FC_OP_TYPE_POINTER, private_array_pointer, AV_FC_SC_PRIVATE, module.mask_array);
        private_element_pointer = bound++;
        av_fc_op3(&globals, AV_FC_OP_TYPE_POINTER, private_element_pointer, AV_FC_SC_PRIVATE, module.mask_element);
        mask_private = bound++;
        av_fc_op3(&globals, AV_FC_OP_VARIABLE, private_array_pointer, mask_private, AV_FC_SC_PRIVATE);
        zero_index = bound++;
        av_fc_op3(&globals, AV_FC_OP_CONSTANT, module.uint_type, zero_index, 0u);
    }
    zero_float = bound++;
    av_fc_op3(&globals, AV_FC_OP_CONSTANT, module.float_type, zero_float, 0u);
    zero_vec3 = bound++;
    av_fc_op(&globals, AV_FC_OP_CONSTANT_COMPOSITE, 6u);
    av_fc_push(&globals, module.vec3_type);
    av_fc_push(&globals, zero_vec3);
    av_fc_push(&globals, zero_float);
    av_fc_push(&globals, zero_float);
    av_fc_push(&globals, zero_float);
    zero_uint = bound++;
    av_fc_op3(&globals, AV_FC_OP_CONSTANT, module.uint_type, zero_uint, 0u);

    barycentric = bound++;
    av_fc_op3(&block, AV_FC_OP_LOAD, module.vec3_type, barycentric, bary);
    dx = bound++;
    av_fc_op3(&block, AV_FC_OP_DPDX, module.vec3_type, dx, barycentric);
    dy = bound++;
    av_fc_op3(&block, AV_FC_OP_DPDY, module.vec3_type, dy, barycentric);
    mask = zero_uint;
    for (sample = 0; sample < samples; sample++)
    {
        offset_x = bound++;
        number.value = (float)pattern[2u * sample] / 16.0f;
        av_fc_op3(&globals, AV_FC_OP_CONSTANT, module.float_type, offset_x, number.bits);
        offset_y = bound++;
        number.value = (float)pattern[2u * sample + 1u] / 16.0f;
        av_fc_op3(&globals, AV_FC_OP_CONSTANT, module.float_type, offset_y, number.bits);
        bit = bound++;
        av_fc_op3(&globals, AV_FC_OP_CONSTANT, module.uint_type, bit, 1u << sample);
        scaled_x = bound++;
        av_fc_op4(&block, AV_FC_OP_VECTOR_TIMES_SCALAR, module.vec3_type, scaled_x, dx, offset_x);
        scaled_y = bound++;
        av_fc_op4(&block, AV_FC_OP_VECTOR_TIMES_SCALAR, module.vec3_type, scaled_y, dy, offset_y);
        moved = bound++;
        av_fc_op4(&block, AV_FC_OP_FADD, module.vec3_type, moved, barycentric, scaled_x);
        at_sample = bound++;
        av_fc_op4(&block, AV_FC_OP_FADD, module.vec3_type, at_sample, moved, scaled_y);
        inside = bound++;
        av_fc_op4(&block, AV_FC_OP_FORD_GREATER_THAN_EQUAL, module.bvec3_type, inside, at_sample, zero_vec3);
        all_inside = bound++;
        av_fc_op3(&block, AV_FC_OP_ALL, module.bool_type, all_inside, inside);
        chosen = bound++;
        av_fc_op(&block, AV_FC_OP_SELECT, 6u);
        av_fc_push(&block, module.uint_type);
        av_fc_push(&block, chosen);
        av_fc_push(&block, all_inside);
        av_fc_push(&block, bit);
        av_fc_push(&block, zero_uint);
        next_mask = bound++;
        av_fc_op4(&block, AV_FC_OP_BITWISE_OR, module.uint_type, next_mask, mask, chosen);
        mask = next_mask;
    }
    if (mask_private)
    {
        element = bound++;
        av_fc_op4(&block, AV_FC_OP_ACCESS_CHAIN, private_element_pointer, element, mask_private, zero_index);
        stored = mask;
        if (signed_mask || module.mask_element != module.uint_type)
        {
            stored = bound++;
            av_fc_op3(&block, AV_FC_OP_BITCAST, module.mask_element, stored, mask);
        }
        av_fc_op(&block, AV_FC_OP_STORE, 3u);
        av_fc_push(&block, element);
        av_fc_push(&block, stored);
    }
    is_zero = bound++;
    av_fc_op4(&block, AV_FC_OP_IEQUAL, module.bool_type, is_zero, mask, zero_uint);
    kill_label = bound++;
    continue_label = bound++;
    av_fc_op(&block, AV_FC_OP_SELECTION_MERGE, 3u);
    av_fc_push(&block, continue_label);
    av_fc_push(&block, 0u);
    av_fc_op(&block, AV_FC_OP_BRANCH_CONDITIONAL, 4u);
    av_fc_push(&block, is_zero);
    av_fc_push(&block, kill_label);
    av_fc_push(&block, continue_label);
    av_fc_op(&block, AV_FC_OP_LABEL, 2u);
    av_fc_push(&block, kill_label);
    av_fc_op(&block, module.version >= 0x00010600u ? AV_FC_OP_TERMINATE_INVOCATION : AV_FC_OP_KILL, 1u);
    av_fc_op(&block, AV_FC_OP_LABEL, 2u);
    av_fc_push(&block, continue_label);

    for (i = 0; i < 5u; i++)
        av_fc_push(&out, words[i]);
    for (at = 5u; at < count; at += length)
    {
        length = words[at] >> 16;
        opcode = words[at] & 0xffffu;
        if (at == module.first_extension)
        {
            if (!module.has_capability)
            {
                av_fc_op(&out, AV_FC_OP_CAPABILITY, 2u);
                av_fc_push(&out, AV_FC_CAPABILITY_FRAGMENT_BARYCENTRIC);
            }
            if (!module.has_extension)
            {
                av_fc_op(&out, AV_FC_OP_EXTENSION, 1u + (uint32_t)((sizeof(av_fc_extension) + 3u) / 4u));
                for (i = 0; i < (sizeof(av_fc_extension) + 3u) / 4u; i++)
                {
                    uint32_t packed = 0;
                    size_t byte;

                    for (byte = 0; byte < 4u && 4u * i + byte < sizeof(av_fc_extension); byte++)
                        packed |= (uint32_t)(uint8_t)av_fc_extension[4u * i + byte] << (8u * byte);
                    av_fc_push(&out, packed);
                }
            }
        }
        if (at == module.decorations_at && !module.bary_input)
            av_fc_op3(&out, AV_FC_OP_DECORATE, bary, AV_FC_DEC_BUILTIN, AV_FC_BUILTIN_BARY_COORD_NO_PERSP);
        if (at == module.types_end)
            for (i = 0; i < globals.count; i++)
                av_fc_push(&out, globals.data[i]);
        if (at == module.entry_at)
        {
            uint32_t added = (bary != module.bary_input ? 1u : 0u)
                    + (mask_private && module.version >= 0x00010400u ? 1u : 0u);

            av_fc_push(&out, ((uint32_t)(length + added) << 16) | opcode);
            for (i = 1u; i < length; i++)
                av_fc_push(&out, words[at + i]);
            if (bary != module.bary_input)
                av_fc_push(&out, bary);
            if (mask_private && module.version >= 0x00010400u)
                av_fc_push(&out, mask_private);
            continue;
        }
        if (opcode == AV_FC_OP_FUNCTION && length == 5u)
            in_entry = words[at + 2] == module.entry;
        if (opcode == AV_FC_OP_FUNCTION_END)
            in_entry = 0;
        if (in_entry && opcode == AV_FC_OP_LABEL && !entry_label && length == 2u)
        {
            entry_label = words[at + 1];
            entry_open = 1;
            for (i = 0; i < length; i++)
                av_fc_push(&out, words[at + i]);
            continue;
        }
        if (entry_open && opcode != AV_FC_OP_VARIABLE && opcode != AV_FC_OP_LINE && opcode != AV_FC_OP_NO_LINE)
        {
            entry_open = 0;
            for (i = 0; i < block.count; i++)
                av_fc_push(&out, block.data[i]);
        }
        if (in_entry && opcode == AV_FC_OP_PHI)
        {
            for (i = 0; i < length; i++)
                av_fc_push(&out, i >= 4u && !(i & 1u) && words[at + i] == entry_label ? continue_label : words[at + i]);
            continue;
        }
        if (mask_private && at > module.types_end
                && (opcode == AV_FC_OP_ACCESS_CHAIN || opcode == AV_FC_OP_IN_BOUNDS_ACCESS_CHAIN
                    || opcode == AV_FC_OP_PTR_ACCESS_CHAIN)
                && length >= 4u && words[at + 3] == module.mask_input)
        {
            av_fc_push(&out, words[at]);
            av_fc_push(&out, length == 5u && opcode != AV_FC_OP_PTR_ACCESS_CHAIN ? private_element_pointer
                                                                                  : words[at + 1]);
            if (length != 5u || opcode == AV_FC_OP_PTR_ACCESS_CHAIN)
                status = AV_FC_UNSUPPORTED;
            av_fc_push(&out, words[at + 2]);
            av_fc_push(&out, mask_private);
            for (i = 4u; i < length; i++)
                av_fc_push(&out, words[at + i]);
            continue;
        }
        if (mask_private && at > module.types_end && opcode == AV_FC_OP_LOAD && length >= 4u
                && words[at + 3] == module.mask_input)
        {
            for (i = 0; i < length; i++)
                av_fc_push(&out, i == 3u ? mask_private : words[at + i]);
            continue;
        }
        for (i = 0; i < length; i++)
            av_fc_push(&out, words[at + i]);
    }
    if (!status && !entry_label)
        status = AV_FC_MALFORMED;
    if (!status && (out.failed || globals.failed || block.failed))
        status = AV_FC_NO_MEMORY;
    free(globals.data);
    free(block.data);
    if (status)
    {
        free(out.data);
        return status;
    }
    out.data[3] = bound;
    *out_words = out.data;
    *out_count = out.count;
    return AV_FC_OK;
}

#endif
