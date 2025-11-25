/*******************************************************************/
/*                                                                 */
/*                      ADOBE CONFIDENTIAL                         */
/*                   _ _ _ _ _ _ _ _ _ _ _ _ _                     */
/*                                                                 */
/* Copyright 2007-2023 Adobe Inc.                                  */
/* All Rights Reserved.                                            */
/*                                                                 */
/* NOTICE:  All information contained herein is, and remains the   */
/* property of Adobe Inc. and its suppliers, if                    */
/* any.  The intellectual and technical concepts contained         */
/* herein are proprietary to Adobe Inc. and its                    */
/* suppliers and may be covered by U.S. and Foreign Patents,       */
/* patents in process, and are protected by trade secret or        */
/* copyright law.  Dissemination of this information or            */
/* reproduction of this material is strictly forbidden unless      */
/* prior written permission is obtained from Adobe Inc.            */
/* Incorporated.                                                   */
/*                                                                 */
/*******************************************************************/

/*  MultiSlicer.cpp

    This plugin slices an image into multiple parts with customizable properties.
    It allows random division and shifting in random directions.

    Revision History

    Version     Change                                              Engineer   Date
    =======     ======                                              ========   ======
    1.0         Initial implementation                              yourname   04/28/2025

*/

#include "MultiSlicer.h"
#include <stdlib.h>
#include <math.h>
#include <algorithm>

static inline float GetDownscaleFactor(const PF_RationalScale& scale)
{
    if (scale.den == 0) {
        return 1.0f;
    }

    float value = static_cast<float>(scale.num) / static_cast<float>(scale.den);
    return (value > 0.0f) ? value : 1.0f;
}

static PF_Err
About(
    PF_InData* in_data,
    PF_OutData* out_data,
    PF_ParamDef* params[],
    PF_LayerDef* output)
{
    AEGP_SuiteHandler suites(in_data->pica_basicP);

    suites.ANSICallbacksSuite1()->sprintf(out_data->return_msg,
        "%s v%d.%d\r%s",
        STR(StrID_Name),
        MAJOR_VERSION,
        MINOR_VERSION,
        STR(StrID_Description));
    return PF_Err_NONE;
}

static PF_Err
GlobalSetup(
    PF_InData* in_data,
    PF_OutData* out_data,
    PF_ParamDef* params[],
    PF_LayerDef* output)
{
    out_data->my_version = PF_VERSION(MAJOR_VERSION,
        MINOR_VERSION,
        BUG_VERSION,
        STAGE_VERSION,
        BUILD_VERSION);

    // Support 16-bit, multiprocessing, and Multi-Frame Rendering
    out_data->out_flags = PF_OutFlag_DEEP_COLOR_AWARE;
    out_data->out_flags |= PF_OutFlag_PIX_INDEPENDENT;
    out_data->out_flags |= PF_OutFlag_SEND_UPDATE_PARAMS_UI;

    // Enable Multi-Frame Rendering support
    out_data->out_flags2 = PF_OutFlag2_SUPPORTS_THREADED_RENDERING;

    return PF_Err_NONE;
}
static PF_Err
ParamsSetup(
    PF_InData* in_data,
    PF_OutData* out_data,
    PF_ParamDef* params[],
    PF_LayerDef* output)
{
    PF_Err      err = PF_Err_NONE;
    PF_ParamDef def;

    AEFX_CLR_STRUCT(def);

    // Shift parameter - controls how much the slices move, in pixels
    PF_ADD_FLOAT_SLIDERX(STR(StrID_Shift_Param_Name),
        -10000,
        10000,
        -500,
        500,
        0,
        PF_Precision_INTEGER,
        0,
        0,
        SHIFT_DISK_ID);

    // Width parameter - controls the display width of split image from 0-100%
    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX(STR(StrID_Width_Param_Name),
        0,
        100,
        0,
        100,
        100,
        PF_Precision_TENTHS,
        0,
        0,
        WIDTH_DISK_ID);

    // Number of slices parameter
    AEFX_CLR_STRUCT(def);
    PF_ADD_SLIDER(STR(StrID_Slices_Param_Name),
        1,
        1000,
        1,
        50,
        10,
        SLICES_DISK_ID);

    // Anchor Point - center point for rotation
    AEFX_CLR_STRUCT(def);
    PF_ADD_POINT("Anchor Point",
        MULTISLICER_ANCHOR_X_DFLT,
        MULTISLICER_ANCHOR_Y_DFLT,
        FALSE,
        ANCHOR_POINT_DISK_ID);

    // Angle parameter - determines the direction of slicing
    // NOTE: This angle is ABSOLUTE (not relative to Anchor Point)
    // - 0° = horizontal slices
    // - 90° = vertical slices
    // The slices rotate around the Anchor Point, but the slice direction is fixed by this angle
    AEFX_CLR_STRUCT(def);
    PF_ADD_ANGLE(STR(StrID_Angle_Param_Name),
        MULTISLICER_ANGLE_DFLT,
        ANGLE_DISK_ID);

    // Seed for randomness
    AEFX_CLR_STRUCT(def);
    PF_ADD_SLIDER(STR(StrID_Seed_Param_Name),
        0,
        10000,
        0,
        500,
        0,
        SEED_DISK_ID);

    out_data->num_params = MULTISLICER_NUM_PARAMS;

    return err;
}

// Calculate deterministic random value for consistent slice patterns
static float GetRandomValue(A_long seed, A_long index) {
    // More complex hash function for better distribution
    A_long hash = ((seed * 1099087) + (index * 2654435761)) & 0x7FFFFFFF;
    float result = (float)hash / (float)0x7FFFFFFF;

    // Apply some additional transformation for more appealing randomness
    result = fabsf(sinf(result * 12.9898f) * 43758.5453f);
    result = result - floorf(result);

    return result;
}



// Sample pixel color from the source at given coordinates with bilinear interpolation
static PF_Pixel SampleSourcePixel8(
    float srcX,
    float srcY,
    const SliceContext* ctx)
{
    PF_Pixel result = { 0, 0, 0, 0 }; // Default to transparent black

    if (srcX < -0.5f || srcX >= ctx->width - 0.5f ||
        srcY < -0.5f || srcY >= ctx->height - 0.5f) {
        return result;
    }

    A_long x0 = static_cast<A_long>(floorf(srcX));
    A_long y0 = static_cast<A_long>(floorf(srcY));
    A_long x1 = x0 + 1;
    A_long y1 = y0 + 1;

    const float fx = srcX - x0;
    const float fy = srcY - y0;

    x0 = MAX(0L, MIN(x0, ctx->width - 1));
    x1 = MAX(0L, MIN(x1, ctx->width - 1));
    y0 = MAX(0L, MIN(y0, ctx->height - 1));
    y1 = MAX(0L, MIN(y1, ctx->height - 1));

    PF_Pixel* p00 = reinterpret_cast<PF_Pixel*>((reinterpret_cast<char*>(ctx->srcData)) + y0 * ctx->rowbytes + x0 * static_cast<A_long>(sizeof(PF_Pixel)));
    PF_Pixel* p10 = reinterpret_cast<PF_Pixel*>((reinterpret_cast<char*>(ctx->srcData)) + y0 * ctx->rowbytes + x1 * static_cast<A_long>(sizeof(PF_Pixel)));
    PF_Pixel* p01 = reinterpret_cast<PF_Pixel*>((reinterpret_cast<char*>(ctx->srcData)) + y1 * ctx->rowbytes + x0 * static_cast<A_long>(sizeof(PF_Pixel)));
    PF_Pixel* p11 = reinterpret_cast<PF_Pixel*>((reinterpret_cast<char*>(ctx->srcData)) + y1 * ctx->rowbytes + x1 * static_cast<A_long>(sizeof(PF_Pixel)));

    const float w00 = (1.0f - fx) * (1.0f - fy);
    const float w10 = fx * (1.0f - fy);
    const float w01 = (1.0f - fx) * fy;
    const float w11 = fx * fy;

    result.alpha = static_cast<A_u_char>(w00 * p00->alpha + w10 * p10->alpha + w01 * p01->alpha + w11 * p11->alpha + 0.5f);
    result.red = static_cast<A_u_char>(w00 * p00->red + w10 * p10->red + w01 * p01->red + w11 * p11->red + 0.5f);
    result.green = static_cast<A_u_char>(w00 * p00->green + w10 * p10->green + w01 * p01->green + w11 * p11->green + 0.5f);
    result.blue = static_cast<A_u_char>(w00 * p00->blue + w10 * p10->blue + w01 * p01->blue + w11 * p11->blue + 0.5f);

    return result;
}

// Sample 16-bit pixel color from the source with bilinear interpolation
static PF_Pixel16 SampleSourcePixel16(
    float srcX,
    float srcY,
    const SliceContext* ctx)
{
    PF_Pixel16 result = { 0, 0, 0, 0 };

    if (srcX < -0.5f || srcX >= ctx->width - 0.5f ||
        srcY < -0.5f || srcY >= ctx->height - 0.5f) {
        return result;
    }

    A_long x0 = static_cast<A_long>(floorf(srcX));
    A_long y0 = static_cast<A_long>(floorf(srcY));
    A_long x1 = x0 + 1;
    A_long y1 = y0 + 1;

    const float fx = srcX - x0;
    const float fy = srcY - y0;

    x0 = MAX(0L, MIN(x0, ctx->width - 1));
    x1 = MAX(0L, MIN(x1, ctx->width - 1));
    y0 = MAX(0L, MIN(y0, ctx->height - 1));
    y1 = MAX(0L, MIN(y1, ctx->height - 1));

    PF_Pixel16* p00 = reinterpret_cast<PF_Pixel16*>((reinterpret_cast<char*>(ctx->srcData)) + y0 * ctx->rowbytes + x0 * static_cast<A_long>(sizeof(PF_Pixel16)));
    PF_Pixel16* p10 = reinterpret_cast<PF_Pixel16*>((reinterpret_cast<char*>(ctx->srcData)) + y0 * ctx->rowbytes + x1 * static_cast<A_long>(sizeof(PF_Pixel16)));
    PF_Pixel16* p01 = reinterpret_cast<PF_Pixel16*>((reinterpret_cast<char*>(ctx->srcData)) + y1 * ctx->rowbytes + x0 * static_cast<A_long>(sizeof(PF_Pixel16)));
    PF_Pixel16* p11 = reinterpret_cast<PF_Pixel16*>((reinterpret_cast<char*>(ctx->srcData)) + y1 * ctx->rowbytes + x1 * static_cast<A_long>(sizeof(PF_Pixel16)));

    const float w00 = (1.0f - fx) * (1.0f - fy);
    const float w10 = fx * (1.0f - fy);
    const float w01 = (1.0f - fx) * fy;
    const float w11 = fx * fy;

    result.alpha = static_cast<A_u_short>(w00 * p00->alpha + w10 * p10->alpha + w01 * p01->alpha + w11 * p11->alpha + 0.5f);
    result.red = static_cast<A_u_short>(w00 * p00->red + w10 * p10->red + w01 * p01->red + w11 * p11->red + 0.5f);
    result.green = static_cast<A_u_short>(w00 * p00->green + w10 * p10->green + w01 * p01->green + w11 * p11->green + 0.5f);
    result.blue = static_cast<A_u_short>(w00 * p00->blue + w10 * p10->blue + w01 * p01->blue + w11 * p11->blue + 0.5f);

    return result;
}

static void RotatePoint(
    float centerX, float centerY,
    float& x, float& y,
    float angleCos, float angleSin)
{
    float dx = x - centerX;
    float dy = y - centerY;

    // Rotate the point
    float newX = dx * angleCos - dy * angleSin + centerX;
    float newY = dx * angleSin + dy * angleCos + centerY;

    x = newX;
    y = newY;
}

static inline float ComputeEdgeWeight(float dist, float feather)
{
    if (feather <= 0.0f) {
        return (dist >= 0.0f) ? 1.0f : 0.0f;
    }

    if (dist <= -feather) {
        return 0.0f;
    }
    if (dist >= feather) {
        return 1.0f;
    }

    return 0.5f + (dist / (2.0f * feather));
}

static inline float ComputeSliceCoverage(
    const SliceSegment& segment,
    float sliceX,
    float feather)
{
    if (segment.visibleEnd <= segment.visibleStart) {
        return 0.0f;
    }

    float startWeight = ComputeEdgeWeight(sliceX - segment.visibleStart, feather);
    float endWeight = ComputeEdgeWeight(segment.visibleEnd - sliceX, feather);
    float coverage = MIN(startWeight, endWeight);
    return MAX(0.0f, MIN(coverage, 1.0f));
}

static inline PF_Pixel SampleShiftedPixel8(
    const SliceContext* ctx,
    const SliceSegment& segment,
    float worldX,
    float worldY)
{
    float offsetPixels = ctx->shiftAmount * segment.shiftRandomFactor * segment.shiftDirection;
    float srcX = worldX + ctx->shiftDirX * offsetPixels;
    float srcY = worldY + ctx->shiftDirY * offsetPixels;
    return SampleSourcePixel8(srcX, srcY, ctx);
}

static inline PF_Pixel16 SampleShiftedPixel16(
    const SliceContext* ctx,
    const SliceSegment& segment,
    float worldX,
    float worldY)
{
    float offsetPixels = ctx->shiftAmount * segment.shiftRandomFactor * segment.shiftDirection;
    float srcX = worldX + ctx->shiftDirX * offsetPixels;
    float srcY = worldY + ctx->shiftDirY * offsetPixels;
    return SampleSourcePixel16(srcX, srcY, ctx);
}

static inline void AddWeightedPixel(
    const PF_Pixel& sample,
    float weight,
    float& accumA,
    float& accumR,
    float& accumG,
    float& accumB)
{
    accumA += weight * sample.alpha;
    accumR += weight * sample.red;
    accumG += weight * sample.green;
    accumB += weight * sample.blue;
}

static inline void AddWeightedPixel16(
    const PF_Pixel16& sample,
    float weight,
    float& accumA,
    float& accumR,
    float& accumG,
    float& accumB)
{
    accumA += weight * static_cast<float>(sample.alpha);
    accumR += weight * static_cast<float>(sample.red);
    accumG += weight * static_cast<float>(sample.green);
    accumB += weight * static_cast<float>(sample.blue);
}

// Binary search to find the slice whose [sliceStart, sliceEnd]
// range contains the given slice-space coordinate.
static inline A_long FindSliceIndex(const SliceContext* ctx, float sliceX)
{
    if (!ctx || ctx->numSlices <= 0) {
        return -1;
    }

    A_long low = 0;
    A_long high = ctx->numSlices - 1;

    while (low <= high) {
        A_long mid = (low + high) >> 1;
        const SliceSegment& seg = ctx->segments[mid];

        if (sliceX < seg.sliceStart) {
            high = mid - 1;
        }
        else if (sliceX > seg.sliceEnd) {
            low = mid + 1;
        }
        else {
            return mid;
        }
    }

    return -1;
}

static PF_Err
ProcessMultiSlice(
    void* refcon,
    A_long x,
    A_long y,
    PF_Pixel* in,
    PF_Pixel* out)
{
    PF_Err err = PF_Err_NONE;
    const SliceContext* ctx = reinterpret_cast<const SliceContext*>(refcon);
    if (!ctx || ctx->numSlices <= 0) {
        out->alpha = out->red = out->green = out->blue = 0;
        return err;
    }

    float worldX = static_cast<float>(x);
    float worldY = static_cast<float>(y);
    float sliceX = worldX;
    float sliceY = worldY;
    RotatePoint(ctx->centerX, ctx->centerY, sliceX, sliceY, ctx->angleCos, -ctx->angleSin);

    const A_long idx = FindSliceIndex(ctx, sliceX);
    if (idx < 0) {
        // No slice covers this coordinate: fully transparent.
        out->alpha = out->red = out->green = out->blue = 0;
        return err;
    }

    const SliceSegment& segment = ctx->segments[idx];
    if (ctx->fullWidth) {
        PF_Pixel samplePixel = SampleShiftedPixel8(ctx, segment, worldX, worldY);
        *out = samplePixel;
        return err;
    }
    float coverage = ComputeSliceCoverage(segment, sliceX, ctx->featherWidth);
    if (coverage <= 0.0001f) {
        // Outside visible band.
        out->alpha = out->red = out->green = out->blue = 0;
        return err;
    }

    PF_Pixel samplePixel = SampleShiftedPixel8(ctx, segment, worldX, worldY);
    float w = MAX(0.0f, MIN(coverage, 1.0f));

    out->alpha = static_cast<A_u_char>(MIN(255.0f, MAX(0.0f, w * samplePixel.alpha + 0.5f)));
    out->red   = static_cast<A_u_char>(MIN(255.0f, MAX(0.0f, w * samplePixel.red   + 0.5f)));
    out->green = static_cast<A_u_char>(MIN(255.0f, MAX(0.0f, w * samplePixel.green + 0.5f)));
    out->blue  = static_cast<A_u_char>(MIN(255.0f, MAX(0.0f, w * samplePixel.blue  + 0.5f)));

    return err;
}

// Function to process a given (x,y) pixel for slice effects (16-bit)
static PF_Err
ProcessMultiSlice16(
    void* refcon,
    A_long x,
    A_long y,
    PF_Pixel16* in,
    PF_Pixel16* out)
{
    PF_Err err = PF_Err_NONE;
    const SliceContext* ctx = reinterpret_cast<const SliceContext*>(refcon);
    if (!ctx || ctx->numSlices <= 0) {
        out->alpha = out->red = out->green = out->blue = 0;
        return err;
    }

    float worldX = static_cast<float>(x);
    float worldY = static_cast<float>(y);
    float sliceX = worldX;
    float sliceY = worldY;
    RotatePoint(ctx->centerX, ctx->centerY, sliceX, sliceY, ctx->angleCos, -ctx->angleSin);

    const A_long idx = FindSliceIndex(ctx, sliceX);
    if (idx < 0) {
        out->alpha = out->red = out->green = out->blue = 0;
        return err;
    }

    const SliceSegment& segment = ctx->segments[idx];
    if (ctx->fullWidth) {
        PF_Pixel16 samplePixel = SampleShiftedPixel16(ctx, segment, worldX, worldY);
        *out = samplePixel;
        return err;
    }
    float coverage = ComputeSliceCoverage(segment, sliceX, ctx->featherWidth);
    if (coverage <= 0.0001f) {
        out->alpha = out->red = out->green = out->blue = 0;
        return err;
    }

    PF_Pixel16 samplePixel = SampleShiftedPixel16(ctx, segment, worldX, worldY);
    float w = MAX(0.0f, MIN(coverage, 1.0f));

    out->alpha = static_cast<A_u_short>(MIN(PF_MAX_CHAN16, MAX(0.0f, w * samplePixel.alpha + 0.5f)));
    out->red   = static_cast<A_u_short>(MIN(PF_MAX_CHAN16, MAX(0.0f, w * samplePixel.red   + 0.5f)));
    out->green = static_cast<A_u_short>(MIN(PF_MAX_CHAN16, MAX(0.0f, w * samplePixel.green + 0.5f)));
    out->blue  = static_cast<A_u_short>(MIN(PF_MAX_CHAN16, MAX(0.0f, w * samplePixel.blue  + 0.5f)));

    return err;
}

static PF_Err
Render(
    PF_InData* in_data,
    PF_OutData* out_data,
    PF_ParamDef* params[],
    PF_LayerDef* output)
{
    PF_Err              err = PF_Err_NONE;
    AEGP_SuiteHandler   suites(in_data->pica_basicP);
    PF_EffectWorld* inputP = &params[MULTISLICER_INPUT]->u.ld;
    PF_EffectWorld* outputP = output;

    // Extract parameters
    float shiftRaw = params[MULTISLICER_SHIFT]->u.fs_d.value;
    float width = params[MULTISLICER_WIDTH]->u.fs_d.value / 100.0f;
    A_long numSlices = params[MULTISLICER_SLICES]->u.sd.value;
    PF_Fixed anchor_x = params[MULTISLICER_ANCHOR_POINT]->u.td.x_value;
    PF_Fixed anchor_y = params[MULTISLICER_ANCHOR_POINT]->u.td.y_value;
    A_long angle_long = params[MULTISLICER_ANGLE]->u.ad.value >> 16;
    A_long seed = params[MULTISLICER_SEED]->u.sd.value;

    numSlices = MAX(1, numSlices);

    float shiftDirection = (shiftRaw >= 0) ? 1.0f : -1.0f;

    float downscale_x = GetDownscaleFactor(in_data->downsample_x);
    float downscale_y = GetDownscaleFactor(in_data->downsample_y);
    float resolution_scale = MIN(downscale_x, downscale_y);
    float shiftAmount = fabsf(shiftRaw) * resolution_scale;
    float pixelSpan = MAX(0.001f, resolution_scale);
    float featherWidth = 0.70710678f * pixelSpan;

    if ((shiftAmount < 0.001f && fabsf(width - 0.9999f) < 0.0001f) || numSlices <= 1) {
        ERR(suites.WorldTransformSuite1()->copy_hq(
            in_data->effect_ref,
            inputP,
            output,
            NULL,
            NULL));
        return err;
    }

    A_long imageWidth = inputP->width;
    A_long imageHeight = inputP->height;

    float centerX = static_cast<float>(anchor_x) / 65536.0f;
    float centerY = static_cast<float>(anchor_y) / 65536.0f;
    centerX = MAX(0.0f, MIN(centerX, static_cast<float>(imageWidth - 1)));
    centerY = MAX(0.0f, MIN(centerY, static_cast<float>(imageHeight - 1)));

    float angleRad = (float)angle_long * PF_RAD_PER_DEGREE;
    float angleCos = cosf(angleRad);
    float angleSin = sinf(angleRad);
    float sliceLength = 2.0f * sqrtf(static_cast<float>(imageWidth * imageWidth + imageHeight * imageHeight));

    PF_Handle segmentsHandle = suites.HandleSuite1()->host_new_handle(numSlices * sizeof(SliceSegment));
    if (!segmentsHandle) {
        return PF_Err_OUT_OF_MEMORY;
    }
    SliceSegment* segments = *((SliceSegment**)segmentsHandle);
    if (!segments) {
        suites.HandleSuite1()->host_dispose_handle(segmentsHandle);
        return PF_Err_OUT_OF_MEMORY;
    }

    PF_Handle divPointsHandle = suites.HandleSuite1()->host_new_handle((numSlices + 1) * sizeof(float));
    if (!divPointsHandle) {
        suites.HandleSuite1()->host_dispose_handle(segmentsHandle);
        return PF_Err_OUT_OF_MEMORY;
    }
    float* divPoints = *((float**)divPointsHandle);
    if (!divPoints) {
        suites.HandleSuite1()->host_dispose_handle(divPointsHandle);
        suites.HandleSuite1()->host_dispose_handle(segmentsHandle);
        return PF_Err_OUT_OF_MEMORY;
    }

    divPoints[0] = -sliceLength / 2.0f;
    divPoints[numSlices] = sliceLength / 2.0f;

    float baselineOffset = (GetRandomValue(seed, 12345) - 0.5f) * sliceLength * 0.1f;
    float avgSpacing = sliceLength / numSlices;

    if (numSlices > 1) {
        for (A_long i = 1; i < numSlices; i++) {
            divPoints[i] = divPoints[0] + (i * avgSpacing);
        }

        for (A_long i = 1; i < numSlices; i++) {
            float baseRandom = GetRandomValue(seed, i * 3779 + 2971);
            float randomFactor;
            if (baseRandom < 0.7f) {
                randomFactor = 0.2f + (baseRandom / 0.7f) * 0.7f;
            }
            else {
                randomFactor = 1.0f + ((baseRandom - 0.7f) / 0.3f) * 0.8f;
            }

            float offset = (randomFactor - 1.0f) * avgSpacing;
            divPoints[i] += offset + baselineOffset;
        }

        for (A_long i = 1; i < numSlices; i++) {
            float key = divPoints[i];
            A_long j = i - 1;
            while (j >= 0 && divPoints[j] > key) {
                divPoints[j + 1] = divPoints[j];
                j--;
            }
            divPoints[j + 1] = key;
        }

        float minSpacing = avgSpacing * 0.05f;
        for (A_long i = 1; i < numSlices; i++) {
            if (divPoints[i] < divPoints[i - 1] + minSpacing) {
                divPoints[i] = divPoints[i - 1] + minSpacing;
            }
        }

        if (divPoints[numSlices - 1] < divPoints[numSlices] - minSpacing) {
            float actualRange = divPoints[numSlices - 1] - divPoints[0];
            float targetRange = divPoints[numSlices] - divPoints[0];

            if (actualRange > 0.001f) {
                for (A_long i = 1; i < numSlices; i++) {
                    float relativePos = (divPoints[i] - divPoints[0]) / actualRange;
                    divPoints[i] = divPoints[0] + relativePos * targetRange;
                }
            }
            else {
                for (A_long i = 1; i < numSlices; i++) {
                    divPoints[i] = divPoints[0] + (i * targetRange / numSlices);
                }
            }
        }
    }

    for (A_long i = 0; i < numSlices; i++) {
        SliceSegment& segment = segments[i];
        segment.sliceStart = divPoints[i];
        segment.sliceEnd = divPoints[i + 1];
        float sliceWidth = segment.sliceEnd - segment.sliceStart;
        float sliceCenter = segment.sliceStart + (sliceWidth * 0.5f);
        float halfVisible = MAX(0.0f, sliceWidth * width * 0.5f);
        segment.visibleStart = sliceCenter - halfVisible;
        segment.visibleEnd = sliceCenter + halfVisible;

        A_long dirSeed = (seed * 17 + i * 31) & 0x7FFF;
        A_long factorSeed = (seed * 23 + i * 41) & 0x7FFF;
        float randomDir = (GetRandomValue(dirSeed, 0) > 0.5f) ? 1.0f : -1.0f;
        float randomShiftFactor = 0.5f + GetRandomValue(factorSeed, 0) * 1.5f;

        segment.shiftDirection = shiftDirection * randomDir;
        segment.shiftRandomFactor = randomShiftFactor;
    }

    suites.HandleSuite1()->host_dispose_handle(divPointsHandle);

    SliceContext context = {};
    context.srcData = inputP->data;
    context.rowbytes = inputP->rowbytes;
    context.width = imageWidth;
    context.height = imageHeight;
    context.centerX = centerX;
    context.centerY = centerY;
    context.angleCos = angleCos;
    context.angleSin = angleSin;
    context.shiftDirX = -angleSin;
    context.shiftDirY = angleCos;
    context.shiftAmount = shiftAmount;
    context.numSlices = numSlices;
    context.segments = segments;
    bool fullWidth = (width >= 0.999f);
    context.featherWidth = fullWidth ? 0.0f : featherWidth;
    context.fullWidth = fullWidth;

    if (PF_WORLD_IS_DEEP(inputP)) {
        ERR(suites.Iterate16Suite1()->iterate(
            in_data,
            0,
            imageHeight,
            inputP,
            NULL,
            (void*)&context,
            ProcessMultiSlice16,
            outputP));
    }
    else {
        ERR(suites.Iterate8Suite1()->iterate(
            in_data,
            0,
            imageHeight,
            inputP,
            NULL,
            (void*)&context,
            ProcessMultiSlice,
            outputP));
    }

    suites.HandleSuite1()->host_dispose_handle(segmentsHandle);

    return err;
}

extern "C" DllExport
PF_Err PluginDataEntryFunction2(
    PF_PluginDataPtr inPtr,
    PF_PluginDataCB2 inPluginDataCallBackPtr,
    SPBasicSuite* inSPBasicSuitePtr,
    const char* inHostName,
    const char* inHostVersion)
{
    PF_Err result = PF_Err_INVALID_CALLBACK;

    result = PF_REGISTER_EFFECT_EXT2(
        inPtr,
        inPluginDataCallBackPtr,
        "MultiSlicer", // Name
        "361do MultiSlicer", // Match Name
        "361do_plugins", // Category
        AE_RESERVED_INFO, // Reserved Info
        "EffectMain", // Entry point
        "https://github.com/rebuildup/Ae_MultiSlicer"); // support URL

    return result;
}


extern "C" DllExport PF_Err
EffectMain(
    PF_Cmd          cmd,
    PF_InData* in_data,
    PF_OutData* out_data,
    PF_ParamDef* params[],
    PF_LayerDef* output,
    void* extra)
{
    PF_Err      err = PF_Err_NONE;

    try {
        switch (cmd) {
        case PF_Cmd_ABOUT:
            err = About(in_data,
                out_data,
                params,
                output);
            break;

        case PF_Cmd_GLOBAL_SETUP:
            err = GlobalSetup(in_data,
                out_data,
                params,
                output);
            break;

        case PF_Cmd_PARAMS_SETUP:
            err = ParamsSetup(in_data,
                out_data,
                params,
                output);
            break;

        case PF_Cmd_RENDER:
            err = Render(in_data,
                out_data,
                params,
                output);
            break;
        }
    }
    catch (PF_Err& thrown_err) {
        err = thrown_err;
    }
    return err;
}
