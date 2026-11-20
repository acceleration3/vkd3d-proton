#ifndef AV_STREAM_SWAP_H
#define AV_STREAM_SWAP_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum av_ss_status
{
    AV_SS_OK = 0,
    AV_SS_UNCHANGED,
    AV_SS_MALFORMED,
    AV_SS_NO_MEMORY,
};

enum
{
    AV_SS_OP_CAPABILITY = 17,
    AV_SS_OP_TYPE_INT = 21,
    AV_SS_OP_TYPE_STRUCT = 30,
    AV_SS_OP_TYPE_POINTER = 32,
    AV_SS_OP_CONSTANT = 43,
    AV_SS_OP_FUNCTION = 54,
    AV_SS_OP_VARIABLE = 59,
    AV_SS_OP_DECORATE = 71,
    AV_SS_OP_MEMBER_DECORATE = 72,
    AV_SS_OP_DECORATION_GROUP = 73,
    AV_SS_OP_GROUP_DECORATE = 74,
    AV_SS_OP_GROUP_MEMBER_DECORATE = 75,
    AV_SS_OP_EMIT_VERTEX = 218,
    AV_SS_OP_END_PRIMITIVE = 219,
    AV_SS_OP_EMIT_STREAM_VERTEX = 220,
    AV_SS_OP_END_STREAM_PRIMITIVE = 221,
    AV_SS_OP_DECORATE_ID = 332,
    AV_SS_OP_DECORATE_STRING = 5632,
    AV_SS_OP_MEMBER_DECORATE_STRING = 5633,
};

enum
{
    AV_SS_CAPABILITY_GEOMETRY_STREAMS = 54,
    AV_SS_DEC_STREAM = 29,
    AV_SS_SC_OUTPUT = 3,
    AV_SS_MAX_MEMBERS = 64,
};

enum
{
    AV_SS_IS_CONSTANT = 1u << 0,
    AV_SS_IS_UINT32 = 1u << 1,
    AV_SS_IS_OUTPUT = 1u << 2,
    AV_SS_HAS_STREAM = 1u << 3,
    AV_SS_IS_STRUCT = 1u << 4,
    AV_SS_IS_POINTER = 1u << 5,
    AV_SS_IS_INT32 = 1u << 6,
};

struct av_ss_module
{
    const uint32_t *words;
    size_t count;
    uint32_t bound;
    uint8_t *flags;
    uint32_t *value;
    uint32_t *pointee;
    uint64_t *member_streams;
    size_t annotations_end;
    size_t types_begin;
    size_t functions;
    int has_streams_capability;
};

static inline uint32_t av_ss_swapped(uint32_t value, uint32_t stream)
{
    if (value == 0u)
        return stream;
    return value == stream ? 0u : value;
}

static inline int av_ss_is_annotation(uint32_t opcode)
{
    return opcode == AV_SS_OP_DECORATE || opcode == AV_SS_OP_MEMBER_DECORATE || opcode == AV_SS_OP_DECORATION_GROUP
            || opcode == AV_SS_OP_GROUP_DECORATE || opcode == AV_SS_OP_GROUP_MEMBER_DECORATE
            || opcode == AV_SS_OP_DECORATE_ID || opcode == AV_SS_OP_DECORATE_STRING
            || opcode == AV_SS_OP_MEMBER_DECORATE_STRING;
}

static inline int av_ss_is_preamble(uint32_t opcode)
{
    return opcode == 2u || opcode == 3u || opcode == 4u || opcode == 5u || opcode == 6u || opcode == 7u
            || opcode == 10u || opcode == 11u || opcode == 14u || opcode == 15u || opcode == 16u
            || opcode == AV_SS_OP_CAPABILITY || opcode == 330u || opcode == 331u;
}

static inline void av_ss_module_free(struct av_ss_module *module)
{
    free(module->flags);
    free(module->value);
    free(module->pointee);
    free(module->member_streams);
}

static inline int av_ss_module_open(struct av_ss_module *module, const uint32_t *words, size_t count)
{
    uint32_t opcode, id;
    size_t at, length;

    memset(module, 0, sizeof(*module));
    if (!words || count < 5u || words[0] != 0x07230203u || !words[3] || words[3] > (1u << 22))
        return AV_SS_MALFORMED;
    module->words = words;
    module->count = count;
    module->bound = words[3];
    module->flags = (uint8_t *)calloc(module->bound, sizeof(*module->flags));
    module->value = (uint32_t *)calloc(module->bound, sizeof(*module->value));
    module->pointee = (uint32_t *)calloc(module->bound, sizeof(*module->pointee));
    module->member_streams = (uint64_t *)calloc(module->bound, sizeof(*module->member_streams));
    if (!module->flags || !module->value || !module->pointee || !module->member_streams)
    {
        av_ss_module_free(module);
        return AV_SS_NO_MEMORY;
    }
    module->functions = count;
    for (at = 5u; at < count; at += length)
    {
        length = words[at] >> 16;
        opcode = words[at] & 0xffffu;
        if (!length || at + length > count)
        {
            av_ss_module_free(module);
            return AV_SS_MALFORMED;
        }
        if (opcode == AV_SS_OP_FUNCTION && module->functions == count)
            module->functions = at;
        if (module->functions != count)
            continue;
        if (av_ss_is_annotation(opcode))
            module->annotations_end = at + length;
        if (!module->types_begin && !av_ss_is_preamble(opcode) && !av_ss_is_annotation(opcode))
            module->types_begin = at;
        if (opcode == AV_SS_OP_CAPABILITY && length >= 2u && words[at + 1] == AV_SS_CAPABILITY_GEOMETRY_STREAMS)
            module->has_streams_capability = 1;
        if (opcode == AV_SS_OP_TYPE_INT && length >= 4u && words[at + 1] < module->bound && words[at + 2] == 32u)
            module->flags[words[at + 1]] |= words[at + 3] ? AV_SS_IS_INT32 : AV_SS_IS_UINT32;
        if (opcode == AV_SS_OP_TYPE_STRUCT && length >= 2u && words[at + 1] < module->bound)
        {
            module->flags[words[at + 1]] |= AV_SS_IS_STRUCT;
            module->value[words[at + 1]] = (uint32_t)(length - 2u);
        }
        if (opcode == AV_SS_OP_TYPE_POINTER && length >= 4u && words[at + 1] < module->bound)
        {
            module->flags[words[at + 1]] |= AV_SS_IS_POINTER;
            module->pointee[words[at + 1]] = words[at + 3];
        }
        if (opcode == AV_SS_OP_CONSTANT && length >= 4u && words[at + 1] < module->bound && words[at + 2] < module->bound
                && (module->flags[words[at + 1]] & (AV_SS_IS_UINT32 | AV_SS_IS_INT32)))
        {
            module->flags[words[at + 2]] |= AV_SS_IS_CONSTANT;
            module->value[words[at + 2]] = words[at + 3];
            module->pointee[words[at + 2]] = words[at + 1];
        }
        if (opcode == AV_SS_OP_VARIABLE && length >= 4u && words[at + 3] == AV_SS_SC_OUTPUT
                && words[at + 2] < module->bound && words[at + 1] < module->bound)
        {
            module->flags[words[at + 2]] |= AV_SS_IS_OUTPUT;
            module->pointee[words[at + 2]] = module->pointee[words[at + 1]];
        }
        if (opcode == AV_SS_OP_DECORATE && length >= 4u && words[at + 2] == AV_SS_DEC_STREAM && words[at + 1] < module->bound)
            module->flags[words[at + 1]] |= AV_SS_HAS_STREAM;
        if (opcode == AV_SS_OP_MEMBER_DECORATE && length >= 5u && words[at + 3] == AV_SS_DEC_STREAM
                && words[at + 1] < module->bound && words[at + 2] < AV_SS_MAX_MEMBERS)
            module->member_streams[words[at + 1]] |= 1ull << words[at + 2];
    }
    for (id = 0; id < module->bound; id++)
        if ((module->flags[id] & AV_SS_IS_STRUCT) && module->value[id] > AV_SS_MAX_MEMBERS && module->member_streams[id])
        {
            av_ss_module_free(module);
            return AV_SS_MALFORMED;
        }
    if (!module->types_begin)
        module->types_begin = module->functions;
    if (!module->annotations_end)
        module->annotations_end = module->types_begin;
    return AV_SS_OK;
}

struct av_ss_words
{
    uint32_t *data;
    size_t count;
    size_t capacity;
    int failed;
};

static inline void av_ss_push(struct av_ss_words *words, uint32_t value)
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

static inline uint32_t av_ss_constant(const struct av_ss_module *module, uint32_t type, uint32_t value,
        struct av_ss_words *globals, uint32_t *bound)
{
    size_t at;
    uint32_t id;

    for (at = 5u; at < module->functions; at += module->words[at] >> 16)
        if ((module->words[at] & 0xffffu) == AV_SS_OP_CONSTANT && (module->words[at] >> 16) == 4u
                && module->words[at + 1] == type && module->words[at + 3] == value)
            return module->words[at + 2];
    for (at = 0; at + 3u < globals->count; at += globals->data[at] >> 16)
        if ((globals->data[at] & 0xffffu) == AV_SS_OP_CONSTANT && globals->data[at + 1] == type
                && globals->data[at + 3] == value)
            return globals->data[at + 2];
    id = (*bound)++;
    av_ss_push(globals, (4u << 16) | AV_SS_OP_CONSTANT);
    av_ss_push(globals, type);
    av_ss_push(globals, id);
    av_ss_push(globals, value);
    return id;
}

static inline int av_ss_rasterize_stream_as_zero(const uint32_t *words, size_t count, uint32_t stream,
        uint32_t **out_words, size_t *out_count)
{
    struct av_ss_module module;
    struct av_ss_words out = {NULL, 0, 0, 0}, globals = {NULL, 0, 0, 0}, decorations = {NULL, 0, 0, 0};
    uint32_t bound, uint_type = 0, id, opcode, member, value, type;
    uint64_t added_members;
    size_t at, length, i;
    int status, uses_streams = 0;

    *out_words = NULL;
    *out_count = 0;
    if (stream == 0u)
        return AV_SS_UNCHANGED;
    if ((status = av_ss_module_open(&module, words, count)))
        return status;
    bound = module.bound;
    for (id = 0; id < module.bound && !uint_type; id++)
        if (module.flags[id] & AV_SS_IS_UINT32)
            uint_type = id;
    if (!uint_type)
    {
        uint_type = bound++;
        av_ss_push(&globals, (4u << 16) | AV_SS_OP_TYPE_INT);
        av_ss_push(&globals, uint_type);
        av_ss_push(&globals, 32u);
        av_ss_push(&globals, 0u);
    }
    for (at = module.functions; at < count; at += length)
    {
        length = words[at] >> 16;
        opcode = words[at] & 0xffffu;
        if (opcode == AV_SS_OP_EMIT_VERTEX || opcode == AV_SS_OP_END_PRIMITIVE)
            av_ss_constant(&module, uint_type, stream, &globals, &bound);
        if ((opcode == AV_SS_OP_EMIT_STREAM_VERTEX || opcode == AV_SS_OP_END_STREAM_PRIMITIVE) && length == 2u
                && words[at + 1] < module.bound && (module.flags[words[at + 1]] & AV_SS_IS_CONSTANT))
            av_ss_constant(&module, module.pointee[words[at + 1]],
                    av_ss_swapped(module.value[words[at + 1]], stream), &globals, &bound);
    }
    for (id = 0; id < module.bound; id++)
    {
        if (!(module.flags[id] & AV_SS_IS_OUTPUT) || (module.flags[id] & AV_SS_HAS_STREAM))
            continue;
        type = module.pointee[id];
        if (type < module.bound && (module.flags[type] & AV_SS_IS_STRUCT) && module.member_streams[type])
        {
            added_members = 0;
            for (member = 0; member < module.value[type]; member++)
                if (!(module.member_streams[type] & (1ull << member)) && !(added_members & (1ull << member)))
                {
                    av_ss_push(&decorations, (5u << 16) | AV_SS_OP_MEMBER_DECORATE);
                    av_ss_push(&decorations, type);
                    av_ss_push(&decorations, member);
                    av_ss_push(&decorations, AV_SS_DEC_STREAM);
                    av_ss_push(&decorations, stream);
                    added_members |= 1ull << member;
                }
            module.member_streams[type] = ~0ull;
            continue;
        }
        av_ss_push(&decorations, (4u << 16) | AV_SS_OP_DECORATE);
        av_ss_push(&decorations, id);
        av_ss_push(&decorations, AV_SS_DEC_STREAM);
        av_ss_push(&decorations, stream);
    }
    for (i = 0; i < 5u; i++)
        av_ss_push(&out, words[i]);
    if (!module.has_streams_capability)
    {
        av_ss_push(&out, (2u << 16) | AV_SS_OP_CAPABILITY);
        av_ss_push(&out, AV_SS_CAPABILITY_GEOMETRY_STREAMS);
    }
    for (at = 5u; at < count; at += length)
    {
        length = words[at] >> 16;
        opcode = words[at] & 0xffffu;
        if (at == module.annotations_end)
            for (i = 0; i < decorations.count; i++)
                av_ss_push(&out, decorations.data[i]);
        if (at == module.functions)
            for (i = 0; i < globals.count; i++)
                av_ss_push(&out, globals.data[i]);
        if (opcode == AV_SS_OP_DECORATE && length >= 4u && words[at + 2] == AV_SS_DEC_STREAM)
        {
            av_ss_push(&out, words[at]);
            av_ss_push(&out, words[at + 1]);
            av_ss_push(&out, words[at + 2]);
            av_ss_push(&out, av_ss_swapped(words[at + 3], stream));
            for (i = 4u; i < length; i++)
                av_ss_push(&out, words[at + i]);
            continue;
        }
        if (opcode == AV_SS_OP_MEMBER_DECORATE && length >= 5u && words[at + 3] == AV_SS_DEC_STREAM)
        {
            for (i = 0; i < 4u; i++)
                av_ss_push(&out, words[at + i]);
            av_ss_push(&out, av_ss_swapped(words[at + 4], stream));
            for (i = 5u; i < length; i++)
                av_ss_push(&out, words[at + i]);
            continue;
        }
        if (opcode == AV_SS_OP_EMIT_VERTEX || opcode == AV_SS_OP_END_PRIMITIVE)
        {
            uses_streams = 1;
            av_ss_push(&out, (2u << 16) | (opcode == AV_SS_OP_EMIT_VERTEX ? AV_SS_OP_EMIT_STREAM_VERTEX
                                                                            : AV_SS_OP_END_STREAM_PRIMITIVE));
            av_ss_push(&out, av_ss_constant(&module, uint_type, stream, &globals, &bound));
            continue;
        }
        if ((opcode == AV_SS_OP_EMIT_STREAM_VERTEX || opcode == AV_SS_OP_END_STREAM_PRIMITIVE) && length == 2u)
        {
            uses_streams = 1;
            id = words[at + 1];
            if (id >= module.bound || !(module.flags[id] & AV_SS_IS_CONSTANT))
            {
                status = AV_SS_MALFORMED;
                break;
            }
            value = av_ss_swapped(module.value[id], stream);
            av_ss_push(&out, words[at]);
            av_ss_push(&out, value == module.value[id] ? id
                                                         : av_ss_constant(&module, module.pointee[id], value, &globals, &bound));
            continue;
        }
        for (i = 0; i < length; i++)
            av_ss_push(&out, words[at + i]);
    }
    if (!status && module.functions == count)
        status = AV_SS_MALFORMED;
    if (!status && !uses_streams)
        status = AV_SS_UNCHANGED;
    if (!status && (out.failed || globals.failed || decorations.failed))
        status = AV_SS_NO_MEMORY;
    av_ss_module_free(&module);
    free(globals.data);
    free(decorations.data);
    if (status)
    {
        free(out.data);
        return status;
    }
    out.data[3] = bound;
    *out_words = out.data;
    *out_count = out.count;
    return AV_SS_OK;
}

#endif
