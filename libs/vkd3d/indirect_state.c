#define VKD3D_DBG_CHANNEL VKD3D_DBG_CHANNEL_API

#include "vkd3d_private.h"
#include "av_line_coverage.h"

enum
{
    SPV_OP_EXTENSION = 10,
    SPV_OP_LINE = 8,
    SPV_OP_NO_LINE = 317,
    SPV_OP_IN_BOUNDS_ACCESS_CHAIN = 66,
    SPV_OP_PTR_ACCESS_CHAIN = 67,
    SPV_OP_BITCAST = 124,
    SPV_OP_NAME = 5,
    SPV_SC_PRIVATE = 6,
    SPV_SC_PHYSICAL_STORAGE_BUFFER = 5349,
    SPV_CAP_PHYSICAL_STORAGE_BUFFER_ADDRESSES = 5347,
    SPV_ADDRESSING_PHYSICAL_STORAGE_BUFFER_64 = 5348,
    SPV_MEMORY_ACCESS_ALIGNED = 2,
};

struct push_block_rewrite
{
    const uint32_t *words;
    size_t count;
    uint32_t bound;
    uint32_t *twin;
    uint8_t *redirected;
    uint32_t pc_var;
    uint32_t pc_struct;
    uint32_t uint_type;
    uint32_t uvec2_type;
    uint32_t entry_function;
    bool has_capability;
    bool has_extension;
};

static bool push_block_find_globals(struct push_block_rewrite *rw)
{
    const uint32_t *words = rw->words;
    size_t at, length;
    uint32_t opcode;

    for (at = 5; at < rw->count; at += length)
    {
        opcode = words[at] & 0xffffu;
        length = words[at] >> 16;
        if (!length || at + length > rw->count)
            return false;

        if (opcode == AV_LC_OP_CAPABILITY && length == 2 && words[at + 1] == SPV_CAP_PHYSICAL_STORAGE_BUFFER_ADDRESSES)
            rw->has_capability = true;
        else if (opcode == SPV_OP_EXTENSION && length == 9 &&
                !memcmp(&words[at + 1], "SPV_KHR_physical_storage_buffer", 32))
            rw->has_extension = true;
        else if (opcode == AV_LC_OP_ENTRY_POINT && length >= 3 && !rw->entry_function)
            rw->entry_function = words[at + 2];
        else if (opcode == AV_LC_OP_TYPE_INT && length == 4 && words[at + 2] == 32 && words[at + 3] == 0)
            rw->uint_type = words[at + 1];
        else if (opcode == AV_LC_OP_TYPE_POINTER && length == 4 && words[at + 2] == AV_LC_SC_PUSH_CONSTANT &&
                words[at + 1] < rw->bound)
            rw->twin[words[at + 1]] = UINT32_MAX;
        else if (opcode == AV_LC_OP_VARIABLE && length >= 4 && words[at + 3] == AV_LC_SC_PUSH_CONSTANT)
        {
            if (rw->pc_var)
                return false;
            rw->pc_var = words[at + 2];
            rw->pc_struct = words[at + 1];
        }
    }

    if (rw->uint_type)
    {
        for (at = 5; at < rw->count; at += length)
        {
            length = words[at] >> 16;
            if ((words[at] & 0xffffu) == AV_LC_OP_TYPE_VECTOR && length == 4 &&
                    words[at + 2] == rw->uint_type && words[at + 3] == 2)
                rw->uvec2_type = words[at + 1];
        }
    }

    for (at = 5; at < rw->count; at += length)
    {
        length = words[at] >> 16;
        if ((words[at] & 0xffffu) == AV_LC_OP_TYPE_POINTER && length == 4 && words[at + 1] == rw->pc_struct)
        {
            rw->pc_struct = words[at + 3];
            return true;
        }
    }
    return false;
}

static bool push_block_mentions(const struct push_block_rewrite *rw, const uint32_t *ins, size_t length)
{
    size_t i;

    for (i = 1; i < length; i++)
    {
        if (ins[i] == rw->pc_var || (ins[i] < rw->bound && rw->redirected[ins[i]]))
            return true;
    }
    return false;
}

int vkd3d_rewrite_push_block_to_address(const uint32_t *words, size_t count, uint32_t **out_words, size_t *out_count)
{
    uint32_t c_zero, block_type, block_pointer, block_var, uvec2_pointer, address_pointer, private_pointer, address_var;
    struct av_lc_words out = {NULL, 0, 0, 0}, types = {NULL, 0, 0, 0};
    uint32_t bound, opcode, base, loaded, id, i;
    struct push_block_rewrite rw;
    bool caps_done = false, annotations_done = false, globals_done = false;
    bool in_entry = false, pending_init = false;
    size_t at, length, j;
    int status = AV_LC_OK;

    memset(&rw, 0, sizeof(rw));
    if (count < 5 || words[0] != 0x07230203u || !words[3] || words[3] > (1u << 22))
        return AV_LC_MALFORMED;
    rw.words = words;
    rw.count = count;
    rw.bound = words[3];
    rw.twin = vkd3d_calloc(rw.bound, sizeof(*rw.twin));
    rw.redirected = vkd3d_calloc(rw.bound, 1);
    if (!rw.twin || !rw.redirected)
    {
        vkd3d_free(rw.twin);
        vkd3d_free(rw.redirected);
        return AV_LC_NO_MEMORY;
    }

    if (!push_block_find_globals(&rw))
    {
        status = rw.pc_var ? AV_LC_MALFORMED : AV_LC_UNCHANGED;
        goto done;
    }

    bound = rw.bound;
    if (!rw.uint_type)
    {
        rw.uint_type = bound++;
        av_lc_emit(&types, AV_LC_OP_TYPE_INT, 3, rw.uint_type, 32u, 0u);
    }
    if (!rw.uvec2_type)
    {
        rw.uvec2_type = bound++;
        av_lc_emit(&types, AV_LC_OP_TYPE_VECTOR, 3, rw.uvec2_type, rw.uint_type, 2u);
    }
    c_zero = bound++;
    av_lc_emit(&types, AV_LC_OP_CONSTANT, 3, rw.uint_type, c_zero, 0u);
    block_type = bound++;
    av_lc_emit(&types, AV_LC_OP_TYPE_STRUCT, 2, block_type, rw.uvec2_type);
    block_pointer = bound++;
    av_lc_emit(&types, AV_LC_OP_TYPE_POINTER, 3, block_pointer, AV_LC_SC_PUSH_CONSTANT, block_type);
    block_var = bound++;
    av_lc_emit(&types, AV_LC_OP_VARIABLE, 3, block_pointer, block_var, AV_LC_SC_PUSH_CONSTANT);
    uvec2_pointer = bound++;
    av_lc_emit(&types, AV_LC_OP_TYPE_POINTER, 3, uvec2_pointer, AV_LC_SC_PUSH_CONSTANT, rw.uvec2_type);
    address_pointer = bound++;
    av_lc_emit(&types, AV_LC_OP_TYPE_POINTER, 3, address_pointer, SPV_SC_PHYSICAL_STORAGE_BUFFER, rw.pc_struct);
    private_pointer = bound++;
    av_lc_emit(&types, AV_LC_OP_TYPE_POINTER, 3, private_pointer, SPV_SC_PRIVATE, address_pointer);
    address_var = bound++;
    av_lc_emit(&types, AV_LC_OP_VARIABLE, 3, private_pointer, address_var, SPV_SC_PRIVATE);

    for (at = 5; at < count; at += length)
    {
        length = words[at] >> 16;
        if ((words[at] & 0xffffu) == AV_LC_OP_TYPE_POINTER && length == 4 && words[at + 2] == AV_LC_SC_PUSH_CONSTANT)
        {
            id = words[at + 1];
            rw.twin[id] = bound++;
            av_lc_emit(&types, AV_LC_OP_TYPE_POINTER, 3, rw.twin[id], SPV_SC_PHYSICAL_STORAGE_BUFFER, words[at + 3]);
        }
    }

    av_lc_words_append(&out, words, 5);
    for (at = 5; at < count && status == AV_LC_OK; at += length)
    {
        opcode = words[at] & 0xffffu;
        length = words[at] >> 16;

        if (!caps_done && opcode != AV_LC_OP_CAPABILITY)
        {
            caps_done = true;
            if (!rw.has_capability)
                av_lc_emit(&out, AV_LC_OP_CAPABILITY, 1, SPV_CAP_PHYSICAL_STORAGE_BUFFER_ADDRESSES);
            if (!rw.has_extension && words[1] < 0x00010500u)
            {
                av_lc_words_push(&out, (9u << 16) | SPV_OP_EXTENSION);
                av_lc_emit_string(&out, "SPV_KHR_physical_storage_buffer");
            }
        }

        if (!annotations_done && !av_lc_is_preamble(opcode))
        {
            annotations_done = true;
            av_lc_emit(&out, AV_LC_OP_DECORATE, 2, block_type, AV_LC_DEC_BLOCK);
            av_lc_emit(&out, AV_LC_OP_MEMBER_DECORATE, 4, block_type, 0u, AV_LC_DEC_OFFSET, 0u);
        }

        if (!globals_done && opcode == AV_LC_OP_FUNCTION)
        {
            globals_done = true;
            av_lc_words_append(&out, types.data, types.count);
        }

        if (pending_init && opcode != AV_LC_OP_VARIABLE && opcode != SPV_OP_LINE && opcode != SPV_OP_NO_LINE)
        {
            uint32_t chain = bound++, value = bound++, pointer = bound++;

            pending_init = false;
            av_lc_emit(&out, AV_LC_OP_ACCESS_CHAIN, 4, uvec2_pointer, chain, block_var, c_zero);
            av_lc_emit(&out, AV_LC_OP_LOAD, 3, rw.uvec2_type, value, chain);
            av_lc_emit(&out, SPV_OP_BITCAST, 3, address_pointer, pointer, value);
            av_lc_emit(&out, AV_LC_OP_STORE, 2, address_var, pointer);
        }

        switch (opcode)
        {
            case AV_LC_OP_MEMORY_MODEL:
                if (words[at + 1] != 0 && words[at + 1] != SPV_ADDRESSING_PHYSICAL_STORAGE_BUFFER_64)
                    status = AV_LC_REFUSED_TYPE;
                av_lc_emit(&out, opcode, 2, SPV_ADDRESSING_PHYSICAL_STORAGE_BUFFER_64, words[at + 2]);
                continue;

            case AV_LC_OP_ENTRY_POINT:
                if (words[1] < 0x00010400u || words[at + 2] != rw.entry_function)
                    break;
                av_lc_words_push(&out, ((uint32_t)(length + 1) << 16) | opcode);
                for (j = 1; j < length; j++)
                    av_lc_words_push(&out, words[at + j] == rw.pc_var ? block_var : words[at + j]);
                av_lc_words_push(&out, address_var);
                continue;

            case SPV_OP_NAME:
            case AV_LC_OP_DECORATE:
                if (length >= 2 && words[at + 1] == rw.pc_var)
                    continue;
                break;

            case AV_LC_OP_VARIABLE:
                if (length >= 3 && words[at + 2] == rw.pc_var)
                    continue;
                break;

            case AV_LC_OP_FUNCTION:
                in_entry = length >= 3 && words[at + 2] == rw.entry_function;
                break;

            case AV_LC_OP_LABEL:
                if (in_entry)
                {
                    in_entry = false;
                    av_lc_words_append(&out, &words[at], length);
                    pending_init = true;
                    continue;
                }
                break;

            case AV_LC_OP_ACCESS_CHAIN:
            case SPV_OP_IN_BOUNDS_ACCESS_CHAIN:
                base = words[at + 3];
                if (base != rw.pc_var && !(base < rw.bound && rw.redirected[base]))
                    break;
                if (words[at + 1] >= rw.bound || !rw.twin[words[at + 1]] || rw.twin[words[at + 1]] == UINT32_MAX)
                {
                    status = AV_LC_REFUSED_TYPE;
                    continue;
                }
                if (base == rw.pc_var)
                {
                    base = bound++;
                    av_lc_emit(&out, AV_LC_OP_LOAD, 3, address_pointer, base, address_var);
                }
                if (words[at + 2] < rw.bound)
                    rw.redirected[words[at + 2]] = 1;
                av_lc_words_push(&out, words[at]);
                av_lc_words_push(&out, rw.twin[words[at + 1]]);
                av_lc_words_push(&out, words[at + 2]);
                av_lc_words_push(&out, base);
                av_lc_words_append(&out, &words[at + 4], length - 4);
                continue;

            case AV_LC_OP_LOAD:
                if (length < 4 || (words[at + 3] != rw.pc_var && !(words[at + 3] < rw.bound && rw.redirected[words[at + 3]])))
                    break;
                if (length != 4)
                {
                    status = AV_LC_REFUSED_TYPE;
                    continue;
                }
                loaded = words[at + 3];
                if (loaded == rw.pc_var)
                {
                    loaded = bound++;
                    av_lc_emit(&out, AV_LC_OP_LOAD, 3, address_pointer, loaded, address_var);
                }
                av_lc_emit(&out, AV_LC_OP_LOAD, 5, words[at + 1], words[at + 2], loaded,
                        SPV_MEMORY_ACCESS_ALIGNED, 4u);
                continue;

            default:
                if (globals_done && push_block_mentions(&rw, &words[at], length))
                    status = AV_LC_REFUSED_TYPE;
                break;
        }

        for (i = 0; i < length; i++)
            av_lc_words_push(&out, words[at + i]);
    }

    if (status == AV_LC_OK && (out.failed || types.failed || !globals_done))
        status = out.failed || types.failed ? AV_LC_NO_MEMORY : AV_LC_MALFORMED;

    if (status == AV_LC_OK)
    {
        out.data[3] = bound;
        *out_words = out.data;
        *out_count = out.count;
        out.data = NULL;
    }

done:
    free(out.data);
    free(types.data);
    vkd3d_free(rw.twin);
    vkd3d_free(rw.redirected);
    return status;
}

enum
{
    SPV_OP_CONVERT_S_TO_F = 111,
    SPV_OP_CONVERT_U_TO_F = 112,
    SPV_OP_IADD = 128,
    SPV_OP_ISUB = 130,
    SPV_OP_IMUL = 132,
    SPV_OP_UDIV = 134,
    SPV_OP_IEQUAL = 170,
    SPV_OP_ULESS_THAN = 176,
    SPV_OP_ULESS_THAN_EQUAL = 178,
    SPV_OP_SHIFT_RIGHT_LOGICAL = 194,
    SPV_OP_SHIFT_RIGHT_ARITHMETIC = 195,
    SPV_OP_SHIFT_LEFT_LOGICAL = 196,
    SPV_OP_BITWISE_OR = 197,
    SPV_OP_BITWISE_AND = 199,
    SPV_BUILTIN_VERTEX_INDEX = 42,
    SPV_BUILTIN_INSTANCE_INDEX = 43,
    SPV_BUILTIN_BASE_VERTEX = 4424,
    SPV_GLSL_FMAX = 40,
    SPV_GLSL_UNPACK_HALF_2X16 = 62,
};

enum vertex_pull_kind
{
    PULL_UNORM,
    PULL_SNORM,
    PULL_UINT,
    PULL_SINT,
    PULL_FLOAT,
    PULL_HALF,
};

struct vertex_pull_format
{
    VkFormat format;
    uint8_t components;
    uint8_t bits[4];
    uint8_t kind;
    bool bgra;
};

static const struct vertex_pull_format vertex_pull_formats[] =
{
    {VK_FORMAT_R8_UNORM, 1, {8}, PULL_UNORM},
    {VK_FORMAT_R8_SNORM, 1, {8}, PULL_SNORM},
    {VK_FORMAT_R8_UINT, 1, {8}, PULL_UINT},
    {VK_FORMAT_R8_SINT, 1, {8}, PULL_SINT},
    {VK_FORMAT_R8G8_UNORM, 2, {8, 8}, PULL_UNORM},
    {VK_FORMAT_R8G8_SNORM, 2, {8, 8}, PULL_SNORM},
    {VK_FORMAT_R8G8_UINT, 2, {8, 8}, PULL_UINT},
    {VK_FORMAT_R8G8_SINT, 2, {8, 8}, PULL_SINT},
    {VK_FORMAT_R8G8B8A8_UNORM, 4, {8, 8, 8, 8}, PULL_UNORM},
    {VK_FORMAT_R8G8B8A8_SNORM, 4, {8, 8, 8, 8}, PULL_SNORM},
    {VK_FORMAT_R8G8B8A8_UINT, 4, {8, 8, 8, 8}, PULL_UINT},
    {VK_FORMAT_R8G8B8A8_SINT, 4, {8, 8, 8, 8}, PULL_SINT},
    {VK_FORMAT_B8G8R8A8_UNORM, 4, {8, 8, 8, 8}, PULL_UNORM, true},
    {VK_FORMAT_A2B10G10R10_UNORM_PACK32, 4, {10, 10, 10, 2}, PULL_UNORM},
    {VK_FORMAT_A2B10G10R10_UINT_PACK32, 4, {10, 10, 10, 2}, PULL_UINT},
    {VK_FORMAT_R16_UNORM, 1, {16}, PULL_UNORM},
    {VK_FORMAT_R16_SNORM, 1, {16}, PULL_SNORM},
    {VK_FORMAT_R16_UINT, 1, {16}, PULL_UINT},
    {VK_FORMAT_R16_SINT, 1, {16}, PULL_SINT},
    {VK_FORMAT_R16_SFLOAT, 1, {16}, PULL_HALF},
    {VK_FORMAT_R16G16_UNORM, 2, {16, 16}, PULL_UNORM},
    {VK_FORMAT_R16G16_SNORM, 2, {16, 16}, PULL_SNORM},
    {VK_FORMAT_R16G16_UINT, 2, {16, 16}, PULL_UINT},
    {VK_FORMAT_R16G16_SINT, 2, {16, 16}, PULL_SINT},
    {VK_FORMAT_R16G16_SFLOAT, 2, {16, 16}, PULL_HALF},
    {VK_FORMAT_R16G16B16A16_UNORM, 4, {16, 16, 16, 16}, PULL_UNORM},
    {VK_FORMAT_R16G16B16A16_SNORM, 4, {16, 16, 16, 16}, PULL_SNORM},
    {VK_FORMAT_R16G16B16A16_UINT, 4, {16, 16, 16, 16}, PULL_UINT},
    {VK_FORMAT_R16G16B16A16_SINT, 4, {16, 16, 16, 16}, PULL_SINT},
    {VK_FORMAT_R16G16B16A16_SFLOAT, 4, {16, 16, 16, 16}, PULL_HALF},
    {VK_FORMAT_R32_UINT, 1, {32}, PULL_UINT},
    {VK_FORMAT_R32_SINT, 1, {32}, PULL_SINT},
    {VK_FORMAT_R32_SFLOAT, 1, {32}, PULL_FLOAT},
    {VK_FORMAT_R32G32_UINT, 2, {32, 32}, PULL_UINT},
    {VK_FORMAT_R32G32_SINT, 2, {32, 32}, PULL_SINT},
    {VK_FORMAT_R32G32_SFLOAT, 2, {32, 32}, PULL_FLOAT},
    {VK_FORMAT_R32G32B32_UINT, 3, {32, 32, 32}, PULL_UINT},
    {VK_FORMAT_R32G32B32_SINT, 3, {32, 32, 32}, PULL_SINT},
    {VK_FORMAT_R32G32B32_SFLOAT, 3, {32, 32, 32}, PULL_FLOAT},
    {VK_FORMAT_R32G32B32A32_UINT, 4, {32, 32, 32, 32}, PULL_UINT},
    {VK_FORMAT_R32G32B32A32_SINT, 4, {32, 32, 32, 32}, PULL_SINT},
    {VK_FORMAT_R32G32B32A32_SFLOAT, 4, {32, 32, 32, 32}, PULL_FLOAT},
};

static const struct vertex_pull_format *vertex_pull_format_find(VkFormat format)
{
    unsigned int i;

    for (i = 0; i < ARRAY_SIZE(vertex_pull_formats); i++)
        if (vertex_pull_formats[i].format == format)
            return &vertex_pull_formats[i];
    return NULL;
}

bool vkd3d_vertex_pull_supports_format(VkFormat format)
{
    return !!vertex_pull_format_find(format);
}

struct pull_key
{
    uint32_t opcode, a, b, id;
};

struct pull
{
    const uint32_t *words;
    size_t count;
    uint32_t module_bound;
    uint32_t bound;
    struct av_lc_words types;
    struct av_lc_words decorations;
    struct av_lc_words init;
    struct pull_key keys[192];
    uint32_t key_count;
    uint32_t glsl;
    bool failed;
};

static uint32_t pull_id(struct pull *p)
{
    return p->bound++;
}

static uint32_t pull_type(struct pull *p, uint32_t opcode, uint32_t count, uint32_t a, uint32_t b)
{
    struct pull_key *key;
    uint32_t i;

    for (i = 0; i < p->key_count; i++)
    {
        key = &p->keys[i];
        if (key->opcode == opcode && key->a == a && key->b == b)
            return key->id;
    }
    if (p->key_count == ARRAY_SIZE(p->keys))
    {
        p->failed = true;
        return 0;
    }
    key = &p->keys[p->key_count++];
    key->opcode = opcode;
    key->a = a;
    key->b = b;
    key->id = pull_id(p);
    if (opcode == AV_LC_OP_CONSTANT)
        av_lc_emit(&p->types, opcode, 3, a, key->id, b);
    else if (count == 0)
        av_lc_emit(&p->types, opcode, 1, key->id);
    else if (count == 1)
        av_lc_emit(&p->types, opcode, 2, key->id, a);
    else
        av_lc_emit(&p->types, opcode, 3, key->id, a, b);
    return key->id;
}

static void pull_know(struct pull *p, uint32_t opcode, uint32_t a, uint32_t b, uint32_t id)
{
    struct pull_key *key;

    if (p->key_count == ARRAY_SIZE(p->keys))
        return;
    key = &p->keys[p->key_count++];
    key->opcode = opcode;
    key->a = a;
    key->b = b;
    key->id = id;
}

static uint32_t pull_uint(struct pull *p) { return pull_type(p, AV_LC_OP_TYPE_INT, 2, 32, 0); }
static uint32_t pull_sint(struct pull *p) { return pull_type(p, AV_LC_OP_TYPE_INT, 2, 32, 1); }
static uint32_t pull_float(struct pull *p) { return pull_type(p, AV_LC_OP_TYPE_FLOAT, 1, 32, 0); }
static uint32_t pull_bool(struct pull *p) { return pull_type(p, AV_LC_OP_TYPE_BOOL, 0, 0, 0); }
static uint32_t pull_vector(struct pull *p, uint32_t scalar, uint32_t n) { return pull_type(p, AV_LC_OP_TYPE_VECTOR, 2, scalar, n); }
static uint32_t pull_u32(struct pull *p, uint32_t v) { return pull_type(p, AV_LC_OP_CONSTANT, 2, pull_uint(p), v); }
static uint32_t pull_f32(struct pull *p, float v) { return pull_type(p, AV_LC_OP_CONSTANT, 2, pull_float(p), av_lc_float_bits(v)); }
static uint32_t pull_s32(struct pull *p, int32_t v) { return pull_type(p, AV_LC_OP_CONSTANT, 2, pull_sint(p), (uint32_t)v); }

static uint32_t pull_op(struct pull *p, uint32_t opcode, uint32_t type, uint32_t n, uint32_t x, uint32_t y, uint32_t z)
{
    uint32_t id = pull_id(p);

    if (n == 1)
        av_lc_emit(&p->init, opcode, 3, type, id, x);
    else if (n == 2)
        av_lc_emit(&p->init, opcode, 4, type, id, x, y);
    else
        av_lc_emit(&p->init, opcode, 5, type, id, x, y, z);
    return id;
}

static uint32_t pull_bin(struct pull *p, uint32_t opcode, uint32_t type, uint32_t x, uint32_t y)
{
    return pull_op(p, opcode, type, 2, x, y, 0);
}

static uint32_t pull_select(struct pull *p, uint32_t type, uint32_t condition, uint32_t x, uint32_t y)
{
    return pull_op(p, AV_LC_OP_SELECT, type, 3, condition, x, y);
}

static uint32_t pull_address_add(struct pull *p, uint32_t address, uint32_t offset)
{
    uint32_t u = pull_uint(p), v2 = pull_vector(p, u, 2);
    uint32_t lo = pull_bin(p, AV_LC_OP_COMPOSITE_EXTRACT, u, address, 0);
    uint32_t hi = pull_bin(p, AV_LC_OP_COMPOSITE_EXTRACT, u, address, 1);
    uint32_t sum = pull_bin(p, SPV_OP_IADD, u, lo, offset);
    uint32_t carry = pull_select(p, u, pull_bin(p, SPV_OP_ULESS_THAN, pull_bool(p), sum, lo), pull_u32(p, 1), pull_u32(p, 0));

    return pull_bin(p, AV_LC_OP_COMPOSITE_CONSTRUCT, v2, sum, pull_bin(p, SPV_OP_IADD, u, hi, carry));
}

static uint32_t pull_load_word(struct pull *p, uint32_t address)
{
    uint32_t u = pull_uint(p);
    uint32_t pointer_type = pull_type(p, AV_LC_OP_TYPE_POINTER, 2, SPV_SC_PHYSICAL_STORAGE_BUFFER, u);
    uint32_t pointer = pull_op(p, SPV_OP_BITCAST, pointer_type, 1, address, 0, 0);
    uint32_t id = pull_id(p);

    av_lc_emit(&p->init, AV_LC_OP_LOAD, 5, u, id, pointer, SPV_MEMORY_ACCESS_ALIGNED, 4u);
    return id;
}

static uint32_t pull_guarded_word(struct pull *p, uint32_t address, uint32_t offset, uint32_t limit)
{
    uint32_t u = pull_uint(p), label_read = pull_id(p), label_merge = pull_id(p), label_entry = pull_id(p);
    uint32_t inside = pull_bin(p, SPV_OP_ULESS_THAN, pull_bool(p), offset, limit), word, result;

    av_lc_emit(&p->init, AV_LC_OP_BRANCH, 1, label_entry);
    av_lc_emit(&p->init, AV_LC_OP_LABEL, 1, label_entry);
    av_lc_emit(&p->init, AV_LC_OP_SELECTION_MERGE, 2, label_merge, 0u);
    av_lc_emit(&p->init, AV_LC_OP_BRANCH_CONDITIONAL, 3, inside, label_read, label_merge);
    av_lc_emit(&p->init, AV_LC_OP_LABEL, 1, label_read);
    word = pull_load_word(p, pull_address_add(p, address, offset));
    av_lc_emit(&p->init, AV_LC_OP_BRANCH, 1, label_merge);
    av_lc_emit(&p->init, AV_LC_OP_LABEL, 1, label_merge);
    result = pull_id(p);
    av_lc_emit(&p->init, 245, 6, u, result, word, label_read, pull_u32(p, 0), label_entry);
    return result;
}

struct pull_block
{
    uint32_t address;
};

static uint32_t pull_block_word(struct pull *p, const struct pull_block *block, uint32_t byte_offset)
{
    return pull_load_word(p, pull_address_add(p, block->address, pull_u32(p, byte_offset)));
}

static uint32_t pull_attribute(struct pull *p, const struct pull_block *block,
        const struct vkd3d_vertex_pull_layout *layout, const struct vkd3d_vertex_pull_attribute *attribute,
        uint32_t vertex, uint32_t instance, uint32_t first_instance, uint32_t *scalar_kind)
{
    const struct vertex_pull_format *format = vertex_pull_format_find(attribute->format);
    uint32_t u = pull_uint(p), b = pull_bool(p), s = pull_sint(p), f = pull_float(p);
    uint32_t entry = layout->vertex_table_offset + 16 * attribute->slot;
    uint32_t va, size, stride, element, offset, bytes = 0, end, inside, aligned, shift, words[6], dwords[5];
    uint32_t start, i, components[4], value, bit, scalar, result, word_count;

    for (i = 0; i < format->components; i++)
        bytes += format->bits[i];
    bytes /= 8;

    va = pull_bin(p, AV_LC_OP_COMPOSITE_CONSTRUCT, pull_vector(p, u, 2),
            pull_block_word(p, block, entry), pull_block_word(p, block, entry + 4));
    size = pull_block_word(p, block, entry + 8);
    stride = pull_block_word(p, block, entry + 12);

    if (attribute->per_instance)
    {
        element = attribute->divisor ? pull_bin(p, SPV_OP_IADD, u, first_instance,
                pull_bin(p, SPV_OP_UDIV, u, pull_bin(p, SPV_OP_ISUB, u, instance, first_instance),
                pull_u32(p, attribute->divisor))) : first_instance;
    }
    else
        element = vertex;

    offset = pull_bin(p, SPV_OP_IADD, u, pull_bin(p, SPV_OP_IMUL, u, element, stride), pull_u32(p, attribute->offset));
    end = pull_bin(p, SPV_OP_IADD, u, offset, pull_u32(p, bytes));
    inside = pull_bin(p, SPV_OP_ULESS_THAN_EQUAL, b, end, size);
    aligned = pull_bin(p, SPV_OP_BITWISE_AND, u, offset, pull_u32(p, ~3u));
    shift = pull_bin(p, SPV_OP_SHIFT_LEFT_LOGICAL, u, pull_bin(p, SPV_OP_BITWISE_AND, u, offset, pull_u32(p, 3)),
            pull_u32(p, 3));
    size = pull_select(p, u, inside, size, pull_u32(p, 0));

    word_count = (bytes + 3) / 4;
    for (i = 0; i <= word_count; i++)
        words[i] = pull_guarded_word(p, va, pull_bin(p, SPV_OP_IADD, u, aligned, pull_u32(p, 4 * i)), size);
    for (i = 0; i < word_count; i++)
    {
        uint32_t low = pull_bin(p, SPV_OP_SHIFT_RIGHT_LOGICAL, u, words[i], shift);
        uint32_t high = pull_bin(p, SPV_OP_SHIFT_LEFT_LOGICAL, u, words[i + 1],
                pull_bin(p, SPV_OP_ISUB, u, pull_u32(p, 32), shift));
        dwords[i] = pull_select(p, u, pull_bin(p, SPV_OP_IEQUAL, b, shift, pull_u32(p, 0)), words[i],
                pull_bin(p, SPV_OP_BITWISE_OR, u, low, high));
    }

    scalar = format->kind == PULL_UINT ? u : format->kind == PULL_SINT ? s : f;
    *scalar_kind = format->kind == PULL_UINT ? AV_LC_UINT : format->kind == PULL_SINT ? AV_LC_SINT : AV_LC_FLOAT;

    for (i = 0, start = 0; i < 4; i++)
    {
        if (i >= format->components)
        {
            if (scalar == f)
                components[i] = pull_f32(p, i == 3 ? 1.0f : 0.0f);
            else
                components[i] = scalar == u ? pull_u32(p, i == 3 ? 1 : 0) : pull_s32(p, i == 3 ? 1 : 0);
            continue;
        }
        bit = start % 32;
        value = dwords[start / 32];
        if (bit)
            value = pull_bin(p, SPV_OP_SHIFT_RIGHT_LOGICAL, u, value, pull_u32(p, bit));
        if (format->bits[i] < 32)
            value = pull_bin(p, SPV_OP_BITWISE_AND, u, value, pull_u32(p, (1u << format->bits[i]) - 1));
        switch (format->kind)
        {
            case PULL_UNORM:
                value = pull_bin(p, AV_LC_OP_FMUL, f, pull_op(p, SPV_OP_CONVERT_U_TO_F, f, 1, value, 0, 0),
                        pull_f32(p, 1.0f / (float)((1u << format->bits[i]) - 1)));
                break;
            case PULL_SNORM:
            case PULL_SINT:
                if (format->bits[i] < 32)
                    value = pull_bin(p, SPV_OP_SHIFT_RIGHT_ARITHMETIC, s,
                            pull_op(p, SPV_OP_BITCAST, s, 1, pull_bin(p, SPV_OP_SHIFT_LEFT_LOGICAL, u, value,
                            pull_u32(p, 32 - format->bits[i])), 0, 0), pull_u32(p, 32 - format->bits[i]));
                else
                    value = pull_op(p, SPV_OP_BITCAST, s, 1, value, 0, 0);
                if (format->kind == PULL_SNORM)
                {
                    uint32_t operands[5];

                    value = pull_bin(p, AV_LC_OP_FMUL, f, pull_op(p, SPV_OP_CONVERT_S_TO_F, f, 1, value, 0, 0),
                            pull_f32(p, 1.0f / (float)((1u << (format->bits[i] - 1)) - 1)));
                    operands[0] = f;
                    operands[1] = pull_id(p);
                    operands[2] = p->glsl;
                    operands[3] = SPV_GLSL_FMAX;
                    operands[4] = value;
                    av_lc_emit(&p->init, AV_LC_OP_EXT_INST, 6, operands[0], operands[1], operands[2], operands[3],
                            operands[4], pull_f32(p, -1.0f));
                    value = operands[1];
                }
                break;
            case PULL_FLOAT:
                value = pull_op(p, SPV_OP_BITCAST, f, 1, value, 0, 0);
                break;
            case PULL_HALF:
                result = pull_id(p);
                av_lc_emit(&p->init, AV_LC_OP_EXT_INST, 5, pull_vector(p, f, 2), result, p->glsl,
                        SPV_GLSL_UNPACK_HALF_2X16, value);
                value = pull_bin(p, AV_LC_OP_COMPOSITE_EXTRACT, f, result, 0);
                break;
            default:
                break;
        }
        components[i] = value;
        start += format->bits[i];
    }

    if (format->bgra)
    {
        value = components[0];
        components[0] = components[2];
        components[2] = value;
    }

    result = pull_id(p);
    av_lc_emit(&p->init, AV_LC_OP_COMPOSITE_CONSTRUCT, 6, pull_vector(p, scalar, 4), result,
            components[0], components[1], components[2], components[3]);
    return result;
}

struct pull_input
{
    uint32_t var;
    uint32_t pointee;
    uint32_t replacement;
    uint32_t location;
    uint32_t component;
    uint32_t builtin;
    bool is_builtin;
};

static const struct vkd3d_vertex_pull_attribute *pull_find_attribute(const struct vkd3d_vertex_pull_layout *layout,
        uint32_t location)
{
    uint32_t i;

    for (i = 0; i < layout->attribute_count; i++)
        if (layout->attributes[i].location == location)
            return &layout->attributes[i];
    return NULL;
}

static uint32_t pull_builtin_input(struct pull *p, uint32_t builtin, const struct pull_input *inputs,
        uint32_t input_count, uint32_t *listed, uint32_t *listed_count)
{
    uint32_t s = pull_sint(p), pointer, var, i;

    for (i = 0; i < input_count; i++)
    {
        if (inputs[i].is_builtin && inputs[i].builtin == builtin && !inputs[i].replacement)
            return pull_op(p, SPV_OP_BITCAST, pull_uint(p), 1,
                    pull_op(p, AV_LC_OP_LOAD, inputs[i].pointee, 1, inputs[i].var, 0, 0), 0, 0);
    }

    pointer = pull_type(p, AV_LC_OP_TYPE_POINTER, 2, AV_LC_SC_INPUT, s);
    var = pull_id(p);
    av_lc_emit(&p->types, AV_LC_OP_VARIABLE, 3, pointer, var, AV_LC_SC_INPUT);
    av_lc_emit(&p->decorations, AV_LC_OP_DECORATE, 3, var, AV_LC_DEC_BUILTIN, builtin);
    listed[(*listed_count)++] = var;
    return pull_op(p, SPV_OP_BITCAST, pull_uint(p), 1, pull_op(p, AV_LC_OP_LOAD, s, 1, var, 0, 0), 0, 0);
}

int vkd3d_rewrite_vertex_pull(const uint32_t *words, size_t count, const struct vkd3d_vertex_pull_layout *layout,
        uint32_t **out_words, size_t *out_count)
{
    uint32_t interface_extra[48], interface_extra_count = 0, *twin = NULL;
    struct pull_input inputs[40];
    uint32_t input_count = 0, i, j, id, opcode, entry_function = 0;
    uint32_t pc_var = 0, pc_block = 0, vertex, instance, first_instance, base_vertex = 0, u, b;
    struct av_lc_words out = {NULL, 0, 0, 0};
    bool caps_done = false, annotations_done = false, globals_done = false, in_entry = false;
    bool has_capability = false, has_extension = false, pending_init = false;
    struct pull_block block;
    uint8_t *replaced = NULL;
    size_t at, length, k;
    struct pull *p;
    int status = AV_LC_OK;

    if (count < 5 || words[0] != 0x07230203u || !words[3] || words[3] > (1u << 22))
        return AV_LC_MALFORMED;
    if (!(p = vkd3d_calloc(1, sizeof(*p))))
        return AV_LC_NO_MEMORY;
    p->words = words;
    p->count = count;
    p->module_bound = words[3];
    p->bound = words[3];
    twin = vkd3d_calloc(p->module_bound, sizeof(*twin));
    replaced = vkd3d_calloc(p->module_bound, 1);
    if (!twin || !replaced)
    {
        status = AV_LC_NO_MEMORY;
        goto done;
    }

    for (at = 5; at < count; at += length)
    {
        opcode = words[at] & 0xffffu;
        length = words[at] >> 16;
        if (!length || at + length > count)
        {
            status = AV_LC_MALFORMED;
            goto done;
        }
        switch (opcode)
        {
            case AV_LC_OP_CAPABILITY:
                if (length == 2 && words[at + 1] == SPV_CAP_PHYSICAL_STORAGE_BUFFER_ADDRESSES)
                    has_capability = true;
                break;
            case SPV_OP_EXTENSION:
                if (length == 9 && !memcmp(&words[at + 1], "SPV_KHR_physical_storage_buffer", 32))
                    has_extension = true;
                break;
            case AV_LC_OP_EXT_INST_IMPORT:
                if (length == 6 && !memcmp(&words[at + 2], "GLSL.std.450", 13))
                    p->glsl = words[at + 1];
                break;
            case AV_LC_OP_ENTRY_POINT:
                if (!entry_function && length >= 3)
                    entry_function = words[at + 2];
                break;
            case AV_LC_OP_TYPE_BOOL:
                pull_know(p, opcode, 0, 0, words[at + 1]);
                break;
            case AV_LC_OP_TYPE_INT:
                if (length == 4)
                    pull_know(p, opcode, words[at + 2], words[at + 3], words[at + 1]);
                break;
            case AV_LC_OP_TYPE_FLOAT:
                if (length == 3)
                    pull_know(p, opcode, words[at + 2], 0, words[at + 1]);
                break;
            case AV_LC_OP_TYPE_VECTOR:
            case AV_LC_OP_TYPE_POINTER:
                if (length == 4)
                    pull_know(p, opcode, words[at + 2], words[at + 3], words[at + 1]);
                break;
            case AV_LC_OP_VARIABLE:
                if (length >= 4 && words[at + 3] == AV_LC_SC_PUSH_CONSTANT)
                {
                    pc_var = words[at + 2];
                    pc_block = words[at + 1];
                }
                break;
            default:
                break;
        }
    }

    {
        struct av_lc_module module;

        if ((status = av_lc_module_open(&module, words, count)))
            goto done;
        for (at = module.globals; at < module.functions; at += length)
        {
            length = words[at] >> 16;
            if ((words[at] & 0xffffu) != AV_LC_OP_VARIABLE || length < 4 || words[at + 3] != AV_LC_SC_INPUT)
                continue;
            id = words[at + 2];
            if (input_count == ARRAY_SIZE(inputs))
            {
                status = AV_LC_TOO_MANY;
                break;
            }
            inputs[input_count].var = id;
            inputs[input_count].pointee = av_lc_module_def(&module, words[at + 1]) ?
                    av_lc_module_def(&module, words[at + 1])[3] : 0;
            inputs[input_count].is_builtin = !!(module.flags[id] & AV_LC_HAS_BUILTIN);
            inputs[input_count].builtin = module.builtin[id];
            inputs[input_count].location = module.location[id];
            inputs[input_count].component = module.component[id];
            inputs[input_count].replacement = 0;
            if (inputs[input_count].is_builtin ? (layout->pull_indices &&
                    (module.builtin[id] == SPV_BUILTIN_VERTEX_INDEX || module.builtin[id] == SPV_BUILTIN_BASE_VERTEX))
                    : !!(module.flags[id] & AV_LC_HAS_LOCATION))
            {
                inputs[input_count].replacement = UINT32_MAX;
                replaced[id] = 1;
            }
            input_count++;
        }
        av_lc_module_free(&module);
        if (status)
            goto done;
    }

    if (!p->glsl)
        p->glsl = pull_id(p);

    u = pull_uint(p);
    b = pull_bool(p);

    for (at = 5; at < count; at += length)
    {
        length = words[at] >> 16;
        if ((words[at] & 0xffffu) == AV_LC_OP_TYPE_POINTER && length == 4 && words[at + 2] == AV_LC_SC_INPUT)
            twin[words[at + 1]] = pull_type(p, AV_LC_OP_TYPE_POINTER, 2, SPV_SC_PRIVATE, words[at + 3]);
    }

    for (i = 0; i < input_count; i++)
    {
        if (!inputs[i].replacement)
            continue;
        inputs[i].replacement = pull_id(p);
        av_lc_emit(&p->types, AV_LC_OP_VARIABLE, 3, pull_type(p, AV_LC_OP_TYPE_POINTER, 2, SPV_SC_PRIVATE,
                inputs[i].pointee), inputs[i].replacement, SPV_SC_PRIVATE);
        if (words[1] >= 0x00010400u)
            interface_extra[interface_extra_count++] = inputs[i].replacement;
    }

    if (!pc_var)
    {
        pc_block = pull_id(p);
        av_lc_emit(&p->types, AV_LC_OP_TYPE_STRUCT, 2, pc_block, pull_vector(p, u, 2));
        av_lc_emit(&p->decorations, AV_LC_OP_DECORATE, 2, pc_block, AV_LC_DEC_BLOCK);
        av_lc_emit(&p->decorations, AV_LC_OP_MEMBER_DECORATE, 4, pc_block, 0u, AV_LC_DEC_OFFSET, 0u);
        pc_var = pull_id(p);
        pc_block = pull_type(p, AV_LC_OP_TYPE_POINTER, 2, AV_LC_SC_PUSH_CONSTANT, pc_block);
        av_lc_emit(&p->types, AV_LC_OP_VARIABLE, 3, pc_block, pc_var, AV_LC_SC_PUSH_CONSTANT);
        if (words[1] >= 0x00010400u)
            interface_extra[interface_extra_count++] = pc_var;
    }

    block.address = pull_op(p, AV_LC_OP_LOAD, pull_vector(p, u, 2), 1,
            pull_bin(p, AV_LC_OP_ACCESS_CHAIN, pull_type(p, AV_LC_OP_TYPE_POINTER, 2, AV_LC_SC_PUSH_CONSTANT,
            pull_vector(p, u, 2)), pc_var, pull_u32(p, 0)), 0, 0);

    first_instance = pull_block_word(p, &block, layout->draw_offset + 8);
    instance = pull_builtin_input(p, SPV_BUILTIN_INSTANCE_INDEX, inputs, input_count, interface_extra, &interface_extra_count);
    vertex = pull_builtin_input(p, SPV_BUILTIN_VERTEX_INDEX, inputs, input_count, interface_extra, &interface_extra_count);

    if (layout->pull_indices)
    {
        uint32_t ib_va, ib_size, ib_bytes, position, index_offset, word, shift, index;

        ib_va = pull_bin(p, AV_LC_OP_COMPOSITE_CONSTRUCT, pull_vector(p, u, 2),
                pull_block_word(p, &block, layout->index_offset), pull_block_word(p, &block, layout->index_offset + 4));
        ib_size = pull_block_word(p, &block, layout->index_offset + 8);
        ib_bytes = pull_block_word(p, &block, layout->index_offset + 12);
        position = pull_bin(p, SPV_OP_IADD, u, pull_block_word(p, &block, layout->draw_offset), vertex);
        index_offset = pull_bin(p, SPV_OP_IMUL, u, position, ib_bytes);
        ib_size = pull_select(p, u, pull_bin(p, SPV_OP_ULESS_THAN_EQUAL, b,
                pull_bin(p, SPV_OP_IADD, u, index_offset, ib_bytes), ib_size), ib_size, pull_u32(p, 0));
        word = pull_guarded_word(p, ib_va, pull_bin(p, SPV_OP_BITWISE_AND, u, index_offset, pull_u32(p, ~3u)), ib_size);
        shift = pull_bin(p, SPV_OP_SHIFT_LEFT_LOGICAL, u, pull_bin(p, SPV_OP_BITWISE_AND, u, index_offset, pull_u32(p, 2)),
                pull_u32(p, 3));
        index = pull_select(p, u, pull_bin(p, SPV_OP_IEQUAL, b, ib_bytes, pull_u32(p, 2)),
                pull_bin(p, SPV_OP_BITWISE_AND, u, pull_bin(p, SPV_OP_SHIFT_RIGHT_LOGICAL, u, word, shift),
                pull_u32(p, 0xffff)), word);
        base_vertex = pull_block_word(p, &block, layout->draw_offset + 4);
        vertex = pull_bin(p, SPV_OP_IADD, u, index, base_vertex);
    }

    for (i = 0; i < input_count && status == AV_LC_OK; i++)
    {
        const struct vkd3d_vertex_pull_attribute *attribute;
        struct av_lc_varying varying;
        struct av_lc_module module;
        uint32_t value, kind, wanted, type, scalar_type, extracted[4];

        if (!inputs[i].replacement)
            continue;
        if (inputs[i].is_builtin)
        {
            value = inputs[i].builtin == SPV_BUILTIN_VERTEX_INDEX ? vertex : base_vertex;
            av_lc_emit(&p->init, AV_LC_OP_STORE, 2, inputs[i].replacement,
                    pull_op(p, SPV_OP_BITCAST, pull_sint(p), 1, value, 0, 0));
            continue;
        }

        if ((status = av_lc_module_open(&module, words, count)))
            break;
        memset(&varying, 0, sizeof(varying));
        status = av_lc_describe_type(&module, inputs[i].pointee, &varying);
        av_lc_module_free(&module);
        if (status || varying.elements)
        {
            status = AV_LC_REFUSED_TYPE;
            break;
        }

        wanted = varying.scalar == AV_LC_FLOAT ? pull_float(p) : varying.scalar == AV_LC_SINT ? pull_sint(p) : u;
        if (!(attribute = pull_find_attribute(layout, inputs[i].location)))
        {
            for (j = 0; j < varying.columns; j++)
                extracted[j] = varying.scalar == AV_LC_FLOAT ? pull_f32(p, 0.0f) : varying.scalar == AV_LC_SINT ?
                        pull_s32(p, 0) : pull_u32(p, 0);
        }
        else
        {
            value = pull_attribute(p, &block, layout, attribute, vertex, instance, first_instance, &kind);
            scalar_type = kind == AV_LC_FLOAT ? pull_float(p) : kind == AV_LC_SINT ? pull_sint(p) : u;
            for (j = 0; j < varying.columns; j++)
            {
                extracted[j] = pull_bin(p, AV_LC_OP_COMPOSITE_EXTRACT, scalar_type, value, inputs[i].component + j);
                if (scalar_type != wanted)
                    extracted[j] = pull_op(p, SPV_OP_BITCAST, wanted, 1, extracted[j], 0, 0);
            }
        }

        type = inputs[i].pointee;
        if (varying.columns == 1)
            value = extracted[0];
        else
        {
            value = pull_id(p);
            av_lc_words_push(&p->init, ((3u + varying.columns) << 16) | AV_LC_OP_COMPOSITE_CONSTRUCT);
            av_lc_words_push(&p->init, type);
            av_lc_words_push(&p->init, value);
            av_lc_words_append(&p->init, extracted, varying.columns);
        }
        av_lc_emit(&p->init, AV_LC_OP_STORE, 2, inputs[i].replacement, value);
    }

    if (status || p->failed)
    {
        if (!status)
            status = AV_LC_TOO_MANY;
        goto done;
    }

    av_lc_words_append(&out, words, 5);
    for (at = 5; at < count && status == AV_LC_OK; at += length)
    {
        opcode = words[at] & 0xffffu;
        length = words[at] >> 16;

        if (!caps_done && opcode != AV_LC_OP_CAPABILITY)
        {
            caps_done = true;
            if (!has_capability)
                av_lc_emit(&out, AV_LC_OP_CAPABILITY, 1, SPV_CAP_PHYSICAL_STORAGE_BUFFER_ADDRESSES);
            if (!has_extension && words[1] < 0x00010500u)
            {
                av_lc_words_push(&out, (9u << 16) | SPV_OP_EXTENSION);
                av_lc_emit_string(&out, "SPV_KHR_physical_storage_buffer");
            }
        }
        if (opcode == AV_LC_OP_MEMORY_MODEL && p->glsl >= p->module_bound)
        {
            av_lc_words_push(&out, (6u << 16) | AV_LC_OP_EXT_INST_IMPORT);
            av_lc_words_push(&out, p->glsl);
            av_lc_emit_string(&out, "GLSL.std.450");
        }
        if (!annotations_done && !av_lc_is_preamble(opcode))
        {
            annotations_done = true;
            av_lc_words_append(&out, p->decorations.data, p->decorations.count);
        }
        if (!globals_done && opcode == AV_LC_OP_FUNCTION)
        {
            globals_done = true;
            av_lc_words_append(&out, p->types.data, p->types.count);
        }
        if (pending_init && opcode != AV_LC_OP_VARIABLE && opcode != SPV_OP_LINE && opcode != SPV_OP_NO_LINE)
        {
            pending_init = false;
            av_lc_words_append(&out, p->init.data, p->init.count);
        }

        switch (opcode)
        {
            case AV_LC_OP_MEMORY_MODEL:
                if (words[at + 1] != 0 && words[at + 1] != SPV_ADDRESSING_PHYSICAL_STORAGE_BUFFER_64)
                    status = AV_LC_REFUSED_TYPE;
                av_lc_emit(&out, opcode, 2, SPV_ADDRESSING_PHYSICAL_STORAGE_BUFFER_64, words[at + 2]);
                continue;

            case AV_LC_OP_ENTRY_POINT:
                if (words[at + 2] != entry_function)
                    break;
            {
                size_t interface_start = 3 + av_lc_string_words(&words[at + 3], (uint32_t)length - 3), kept = 0;

                for (k = interface_start; k < length; k++)
                    if (!(words[at + k] < p->module_bound && replaced[words[at + k]]))
                        kept++;
                av_lc_words_push(&out, ((uint32_t)(interface_start + kept + interface_extra_count) << 16) | opcode);
                for (k = 1; k < length; k++)
                    if (k < interface_start || !(words[at + k] < p->module_bound && replaced[words[at + k]]))
                        av_lc_words_push(&out, words[at + k]);
                av_lc_words_append(&out, interface_extra, interface_extra_count);
                continue;
            }

            case SPV_OP_NAME:
            case AV_LC_OP_DECORATE:
                if (length >= 2 && words[at + 1] < p->module_bound && replaced[words[at + 1]])
                    continue;
                break;

            case AV_LC_OP_VARIABLE:
                if (length >= 3 && words[at + 2] < p->module_bound && replaced[words[at + 2]])
                    continue;
                break;

            case AV_LC_OP_FUNCTION:
                in_entry = length >= 3 && words[at + 2] == entry_function;
                break;

            case AV_LC_OP_LABEL:
                if (in_entry)
                {
                    in_entry = false;
                    av_lc_words_append(&out, &words[at], length);
                    pending_init = true;
                    continue;
                }
                break;

            case AV_LC_OP_ACCESS_CHAIN:
            case SPV_OP_IN_BOUNDS_ACCESS_CHAIN:
                if (words[at + 3] >= p->module_bound || !replaced[words[at + 3]])
                    break;
                if (!twin[words[at + 1]])
                {
                    status = AV_LC_REFUSED_TYPE;
                    continue;
                }
                for (i = 0; i < input_count; i++)
                    if (inputs[i].var == words[at + 3])
                        break;
                replaced[words[at + 2]] = 1;
                av_lc_words_push(&out, words[at]);
                av_lc_words_push(&out, twin[words[at + 1]]);
                av_lc_words_push(&out, words[at + 2]);
                av_lc_words_push(&out, i < input_count ? inputs[i].replacement : words[at + 3]);
                av_lc_words_append(&out, &words[at + 4], length - 4);
                continue;

            case AV_LC_OP_LOAD:
                if (length < 4 || words[at + 3] >= p->module_bound || !replaced[words[at + 3]])
                    break;
                for (i = 0; i < input_count; i++)
                    if (inputs[i].var == words[at + 3])
                        break;
                av_lc_words_push(&out, words[at]);
                av_lc_words_push(&out, words[at + 1]);
                av_lc_words_push(&out, words[at + 2]);
                av_lc_words_push(&out, i < input_count ? inputs[i].replacement : words[at + 3]);
                av_lc_words_append(&out, &words[at + 4], length - 4);
                continue;

            default:
                if (globals_done)
                {
                    for (k = 1; k < length; k++)
                        if (words[at + k] < p->module_bound && replaced[words[at + k]])
                            status = AV_LC_REFUSED_TYPE;
                }
                break;
        }

        av_lc_words_append(&out, &words[at], length);
    }

    if (status == AV_LC_OK && (out.failed || p->types.failed || p->init.failed || p->decorations.failed))
        status = AV_LC_NO_MEMORY;
    if (status == AV_LC_OK)
    {
        out.data[3] = p->bound;
        *out_words = out.data;
        *out_count = out.count;
        out.data = NULL;
    }

done:
    free(out.data);
    free(p->types.data);
    free(p->decorations.data);
    free(p->init.data);
    vkd3d_free(p);
    vkd3d_free(twin);
    vkd3d_free(replaced);
    return status;
}
