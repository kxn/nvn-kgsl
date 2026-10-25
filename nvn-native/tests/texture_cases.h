/* SPDX-License-Identifier: MIT */
#ifndef NN_TEXTURE_CASES_H
#define NN_TEXTURE_CASES_H
#include <math.h>
#include <stdint.h>
#include <string.h>

/* Closed-form test reference derived manually from the selected original
 * Maxwell shaders, independent of the target IR3 compiler/machine code.
 * VS control 0x1db3884 (96757bb36a024b91), FS 0x1db4418 (a7672f1d3bfeb7d0).
 * Offsets below are relative to their original selected code containers.
 * These are declared test inputs, not captured original game frame state. */
enum { NN_TEXTURE_CASE_COUNT = 4, NN_TEXTURE_WIDTH = 64, NN_TEXTURE_HEIGHT = 64 };
enum { NN_TEXTURE_OUTSIDE, NN_TEXTURE_DISCARD, NN_TEXTURE_SHADED, NN_TEXTURE_INVALID };
struct nn_texture_case_inputs {
    const char *name;
    /* location0.xy position, location1.xy UV, location2.rgba color; stride32. */
    float vertices[3][8];
    float vs_ubo[16]; /* CB3, four row-major vec4 matrix rows, bytes0..63. */
    float fs_ubo[16]; /* CB3, targetRGBA/mix/gamma/contrast; 64-byte push range. */
    uint8_t texels[16]; /* 2x2 RGBA8_UNORM, row-major; upload using FDL pitch. */
    uint8_t clear[4];
};
struct nn_texture_expected { uint8_t rgba[4]; uint8_t reason, tolerance; };

static int nn_texture_case_prepare(unsigned index, struct nn_texture_case_inputs *out)
{
    static const float positions[3][2] = {{-1,-1},{3,-1},{-1,3}};
    static const float uvs[3][2] = {{0,0},{2,0},{0,2}};
    static const uint8_t texels[2][16] = {
        {64,128,192,255, 192,64,128,128, 128,192,64,64, 96,160,224,0},
        {192,128,64,255, 128,192,64,192, 64,96,160,128, 224,160,96,0}
    };
    static const char *names[NN_TEXTURE_CASE_COUNT] = {
        "texture-base", "texture-update-same-ib", "constants-color-uv-update-same-ib",
        "matrix-gamma-contrast-update-same-ib"
    };
    if (!out || index >= NN_TEXTURE_CASE_COUNT) return -1;
    memset(out, 0, sizeof(*out));
    out->name = names[index];
    for (unsigned i = 0; i < 4; i++) out->vs_ubo[i * 4 + i] = 1.0f;
    const float target[4] = {.25f,.5f,.75f,.875f};
    memcpy(out->fs_ubo, target, sizeof(target));
    out->fs_ubo[5] = out->fs_ubo[6] = 1.0f;
    const uint8_t clear[4] = {17,29,43,59};
    memcpy(out->clear, clear, sizeof(clear));
    memcpy(out->texels, texels[index == 1 || index == 2], sizeof(out->texels));
    float color[4] = {1,1,1,1};
    if (index == 2) {
        color[0] = .5f; color[1] = .75f; color[3] = .5f;
        out->fs_ubo[4] = .5f; out->fs_ubo[6] = .5f;
    } else if (index == 3) {
        color[0] = .75f; color[1] = .5f; color[3] = .75f;
        out->vs_ubo[0] = .5f; out->vs_ubo[3] = -.5f;
        out->fs_ubo[4] = .25f; out->fs_ubo[5] = 2.0f; out->fs_ubo[6] = .5f;
    }
    for (unsigned i = 0; i < 3; i++) {
        memcpy(out->vertices[i], positions[i], sizeof(positions[i]));
        out->vertices[i][2] = index == 2 ? 1.0f - uvs[i][0] : uvs[i][0];
        out->vertices[i][3] = uvs[i][1];
        memcpy(out->vertices[i] + 4, color, sizeof(color));
    }
    return 0;
}

static uint8_t nn_texture_quantize(float value)
{
    double scaled = floor((double)value * 255.0 + .5);
    return (uint8_t)(scaled < 0 ? 0 : scaled > 255 ? 255 : scaled);
}

static struct nn_texture_expected nn_texture_case_pixel(const struct nn_texture_case_inputs *in,
                                                        unsigned x, unsigned y)
{
    struct nn_texture_expected out = {{0,0,0,0}, NN_TEXTURE_INVALID, 0};
    if (!in || x >= NN_TEXTURE_WIDTH || y >= NN_TEXTURE_HEIGHT) return out;
    memcpy(out.rgba, in->clear, sizeof(out.rgba));
    /* Original VS ALD0x090..0x0f0 loads position.xy/UV.xy/color.rgba.
     * FMUL/FFMA0x0e8..0x1b8 and FADD0x1c8..0x1f8 form
     * clip[row] = ((x*M[row][0] + y*M[row][1]) + .5*M[row][2]) + M[row][3].
     * AST0x108..0x170 passes color and UV without modification.
     * Each explicit float assignment models an FP32 rounding boundary; fmaf
     * preserves the original fused operations. All test operands are normal. */
    float clip[3][4];
    double ndc[3][2];
    for (unsigned i = 0; i < 3; i++) {
        for (unsigned row = 0; row < 4; row++) {
            const float *m = in->vs_ubo + row * 4;
            float v = in->vertices[i][0] * m[0];
            v = fmaf(in->vertices[i][1], m[1], v);
            v = fmaf(m[2], .5f, v);
            clip[i][row] = v + m[3];
        }
        if (!(clip[i][3] > 0)) return out;
        ndc[i][0] = (double)clip[i][0] / clip[i][3];
        ndc[i][1] = (double)clip[i][1] / clip[i][3];
    }
    /* Explicit native viewport scale32/offset32 on both axes. Positive clip
     * Y increases framebuffer row; no permissive orientation fallback. */
    double px = ((double)x + .5 - 32.0) / 32.0;
    double py = ((double)y + .5 - 32.0) / 32.0;
    double den = (ndc[1][1] - ndc[2][1]) * (ndc[0][0] - ndc[2][0]) +
                 (ndc[2][0] - ndc[1][0]) * (ndc[0][1] - ndc[2][1]);
    if (den == 0) return out;
    double bary[3];
    bary[0] = ((ndc[1][1] - ndc[2][1]) * (px - ndc[2][0]) +
               (ndc[2][0] - ndc[1][0]) * (py - ndc[2][1])) / den;
    bary[1] = ((ndc[2][1] - ndc[0][1]) * (px - ndc[2][0]) +
               (ndc[0][0] - ndc[2][0]) * (py - ndc[2][1])) / den;
    bary[2] = 1.0 - bary[0] - bary[1];
    out.reason = NN_TEXTURE_OUTSIDE;
    if (bary[0] < 0 || bary[1] < 0 || bary[2] < 0) return out;
    double invw_sum = 0;
    for (unsigned i = 0; i < 3; i++) { bary[i] /= clip[i][3]; invw_sum += bary[i]; }
    float varying[6];
    for (unsigned k = 0; k < 6; k++) {
        double v = 0;
        for (unsigned i = 0; i < 3; i++) v += bary[i] * in->vertices[i][2 + k] / invw_sum;
        varying[k] = (float)v;
    }
    /* Normalized nearest/clamp2x2, one mip, RGBA identity swizzle, no sRGB.
     * Test UVs/pixel centers never land on an interior texel boundary. */
    int tx = (int)floor((double)varying[0] * 2), ty = (int)floor((double)varying[1] * 2);
    tx = tx < 0 ? 0 : tx > 1 ? 1 : tx; ty = ty < 0 ? 0 : ty > 1 ? 1 : ty;
    const uint8_t *texel = in->texels + (ty * 2 + tx) * 4;
    float sample[4];
    for (unsigned k = 0; k < 4; k++) sample[k] = (float)((double)texel[k] / 255.0);
    float alpha = sample[3] * varying[5];
    /* FS0x0f8/0x110/0x118: FSETP.EQ(alpha, RZ), then predicated KIL.
     * The comparison precedes the final mix; target alpha cannot revive it. */
    out.reason = NN_TEXTURE_DISCARD;
    if (alpha == 0) return out;
    float mix = in->fs_ubo[4], gamma = in->fs_ubo[5], contrast = in->fs_ubo[6];
    if (!(gamma > 0)) { out.reason = NN_TEXTURE_INVALID; return out; }
    float inverse_gamma = 1.0f / gamma;
    for (unsigned k = 0; k < 3; k++) {
        float product = sample[k] * varying[2 + k];
        /* Original FS MUFU.LG2.Abs/EX2 chain0x138..0x1d8, then
         * FFMA.SAT0x208/0x210/0x238, LG2/EX2 gamma0x218..0x2a8.
         * pow is the manually derived mathematical function, not an opcode
         * interpreter or an attempt to duplicate target transcendental units. */
        float encoded = (float)pow(fabs((double)product), inverse_gamma);
        float centered = encoded - .5f;
        float corrected = fmaf(centered, contrast, .5f);
        corrected = corrected < 0 ? 0 : corrected > 1 ? 1 : corrected;
        float linear = (float)pow(corrected, gamma);
        float difference = in->fs_ubo[k] - linear;
        out.rgba[k] = nn_texture_quantize(fmaf(difference, mix, linear));
    }
    /* FS0x1d0/0x1f8: alpha + mix*(targetAlpha-alpha), no saturation. */
    float alpha_difference = in->fs_ubo[3] - alpha;
    out.rgba[3] = nn_texture_quantize(fmaf(alpha_difference, mix, alpha));
    out.reason = NN_TEXTURE_SHADED;
    /* One UNORM8 code allows nearest-quantization tie direction and small
     * FP32/transcendental rounding. No tolerance for wrong coverage/discard.
     * A failure beyond one code requires investigation, not wider tolerance. */
    out.tolerance = 1;
    return out;
}

/* Mode qualification changes only declared input constants and raster state.
 * The original source VS/FS and the four baseline case semantics stay intact. */
enum { NN_TEXTURE_MODE_CASE_COUNT = 12, NN_TEXTURE_DEPTH_CLIPPED = 4 };
struct nn_texture_mode_case { unsigned window_origin, depth_mode; float clip_z; };

static inline int nn_texture_mode_case_prepare(unsigned index, struct nn_texture_case_inputs *out,
                                               struct nn_texture_mode_case *mode)
{
    static const char *names[NN_TEXTURE_MODE_CASE_COUNT] = {
        "mode-o0-d0-zneg", "mode-o0-d0-zpos", "mode-o0-d0-zfar",
        "mode-o0-d1-zneg", "mode-o0-d1-zpos", "mode-o0-d1-zfar",
        "mode-o1-d0-zneg", "mode-o1-d0-zpos", "mode-o1-d0-zfar",
        "mode-o1-d1-zneg", "mode-o1-d1-zpos", "mode-o1-d1-zfar"
    };
    static const float z[3] = {-.5f,.5f,1.5f};
    if (!mode || index >= NN_TEXTURE_MODE_CASE_COUNT || nn_texture_case_prepare(0,out)) return -1;
    mode->window_origin = index / 6; mode->depth_mode = (index / 3) % 2; mode->clip_z = z[index % 3];
    out->name = names[index];
    /* Source VS forms clip.z = x*M20+y*M21+.5*M22+M23. Set a
     * constant non-boundary clip Z, keep clip W=1 and original full triangle. */
    memset(out->vs_ubo + 8,0,4*sizeof(float)); out->vs_ubo[11] = mode->clip_z;
    return 0;
}

static inline struct nn_texture_expected nn_texture_mode_case_pixel(const struct nn_texture_case_inputs *in,
        const struct nn_texture_mode_case *mode, unsigned x, unsigned y)
{
    struct nn_texture_expected out = {{0,0,0,0},NN_TEXTURE_INVALID,0};
    if (!in || !mode || mode->window_origin > 1 || mode->depth_mode > 1 ||
        x >= NN_TEXTURE_WIDTH || y >= NN_TEXTURE_HEIGHT ||
        in->vs_ubo[8] != 0 || in->vs_ubo[9] != 0 ||
        in->vs_ubo[12] != 0 || in->vs_ubo[13] != 0) return out;
    /* Same source-VS closed form, independent of target compiler/machine code.
     * Only constant-Z/W triangles are admitted by this bounded mode oracle. */
    float z = fmaf(in->vs_ubo[10],.5f,in->vs_ubo[11]);
    float w = fmaf(in->vs_ubo[14],.5f,in->vs_ubo[15]);
    if (!(w > 0) || !isfinite(z) || !isfinite(w)) return out;
    if (z < (mode->depth_mode ? 0.f : -w) || z > w) {
        memcpy(out.rgba,in->clear,sizeof(out.rgba)); out.reason = NN_TEXTURE_DEPTH_CLIPPED; return out;
    }
    /* A negative viewport Y scale samples the original closed form at row63-y.
     * This transforms the oracle coordinate, never CPU-copies GPU output. */
    return nn_texture_case_pixel(in,x,mode->window_origin ? NN_TEXTURE_HEIGHT-1-y : y);
}
#endif
