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

    This plugin slices an image into multiple parts with customizable
   properties. It allows random division and shifting in random directions.

    Revision History

    Version     Change                                              Engineer
   Date
    =======     ======                                              ========
   ====== 1.0         Initial implementation yourname   04/28/2025

*/

#include "MultiSlicer.h"
#include <algorithm>
#include <math.h>
#include <stdlib.h>

// Clamp helper for color values
template <typename T> static inline T CLAMP(T value, T min, T max) {
  if (value < min)
    return min;
  if (value > max)
    return max;
  return value;
}

static inline float GetDownscaleFactor(const PF_RationalScale &scale) {
  if (scale.den == 0) {
    return 1.0f;
  }

  float value = static_cast<float>(scale.num) / static_cast<float>(scale.den);
  return (value > 0.0f) ? value : 1.0f;
}

static PF_Err About(PF_InData *in_data, PF_OutData *out_data,
                    PF_ParamDef *params[], PF_LayerDef *output) {
  AEGP_SuiteHandler suites(in_data->pica_basicP);

  suites.ANSICallbacksSuite1()->sprintf(out_data->return_msg, "%s v%d.%d\r%s",
                                        STR(StrID_Name), MAJOR_VERSION,
                                        MINOR_VERSION, STR(StrID_Description));
  return PF_Err_NONE;
}

static PF_Err GlobalSetup(PF_InData *in_data, PF_OutData *out_data,
                          PF_ParamDef *params[], PF_LayerDef *output) {
  out_data->my_version = PF_VERSION(MAJOR_VERSION, MINOR_VERSION, BUG_VERSION,
                                    STAGE_VERSION, BUILD_VERSION);

  // Support 16-bit, multiprocessing, and Multi-Frame Rendering
  out_data->out_flags = PF_OutFlag_DEEP_COLOR_AWARE;
  out_data->out_flags |= PF_OutFlag_PIX_INDEPENDENT;
  out_data->out_flags |= PF_OutFlag_SEND_UPDATE_PARAMS_UI;

  // Enable Multi-Frame Rendering support
  out_data->out_flags2 = PF_OutFlag2_SUPPORTS_THREADED_RENDERING;

  return PF_Err_NONE;
}
static PF_Err ParamsSetup(PF_InData *in_data, PF_OutData *out_data,
                          PF_ParamDef *params[], PF_LayerDef *output) {
  PF_Err err = PF_Err_NONE;
  PF_ParamDef def;

  AEFX_CLR_STRUCT(def);

  // Shift parameter - controls how much the slices move, in pixels
  PF_ADD_FLOAT_SLIDERX(STR(StrID_Shift_Param_Name), -10000, 10000, -500, 500, 0,
                       PF_Precision_INTEGER, 0, 0, SHIFT_DISK_ID);

  // Width parameter - controls the display width of split image from 0-100%
  AEFX_CLR_STRUCT(def);
  PF_ADD_FLOAT_SLIDERX(STR(StrID_Width_Param_Name), 0, 100, 0, 100, 100,
                       PF_Precision_TENTHS, 0, 0, WIDTH_DISK_ID);

  // Number of slices parameter
  AEFX_CLR_STRUCT(def);
  PF_ADD_SLIDER(STR(StrID_Slices_Param_Name), 1, 1000, 1, 50, 10,
                SLICES_DISK_ID);

  // Anchor Point - center point for rotation
  AEFX_CLR_STRUCT(def);
  PF_ADD_POINT("Anchor Point", MULTISLICER_ANCHOR_X_DFLT,
               MULTISLICER_ANCHOR_Y_DFLT, FALSE, ANCHOR_POINT_DISK_ID);

  // Angle parameter - determines the direction of slicing
  // NOTE: This angle is ABSOLUTE (not relative to Anchor Point)
  // - 0° = horizontal slices
  // - 90° = vertical slices
  // The slices rotate around the Anchor Point, but the slice direction is fixed
  // by this angle
  AEFX_CLR_STRUCT(def);
  PF_ADD_ANGLE(STR(StrID_Angle_Param_Name), MULTISLICER_ANGLE_DFLT,
               ANGLE_DISK_ID);

  // Seed for randomness
  AEFX_CLR_STRUCT(def);
  PF_ADD_SLIDER(STR(StrID_Seed_Param_Name), 0, 10000, 0, 500, 0, SEED_DISK_ID);

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

// Simple nearest neighbor sampling - no antialiasing
// Bilinear interpolation for 8-bit (Standard Premultiplied Alpha)
// Bilinear interpolation ensuring no black fringe (8-bit)
// Nearest Neighbor sampling to strictly preserve source colors (8-bit)
// No bilinear interpolation to avoid any color bleeding or dark halos.
static PF_Pixel SampleSourcePixel8(float srcX, float srcY,
                                   const SliceContext *ctx) {
  PF_Pixel result = {0, 0, 0, 0};

  // Round to nearest integer
  A_long x = static_cast<A_long>(srcX + 0.5f);
  A_long y = static_cast<A_long>(srcY + 0.5f);

  if (x < 0 || x >= ctx->width || y < 0 || y >= ctx->height) {
    return result;
  }

  PF_Pixel *p = reinterpret_cast<PF_Pixel *>(
      reinterpret_cast<char *>(ctx->srcData) + y * ctx->rowbytes +
      x * static_cast<A_long>(sizeof(PF_Pixel)));

  return *p;
}

// Bilinear interpolation for 16-bit
// Bilinear interpolation ensuring no black fringe (16-bit)
// Nearest Neighbor sampling to strictly preserve source colors (16-bit)
static PF_Pixel16 SampleSourcePixel16(float srcX, float srcY,
                                      const SliceContext *ctx) {
  PF_Pixel16 result = {0, 0, 0, 0};

  // Round to nearest integer
  A_long x = static_cast<A_long>(srcX + 0.5f);
  A_long y = static_cast<A_long>(srcY + 0.5f);

  if (x < 0 || x >= ctx->width || y < 0 || y >= ctx->height) {
    return result;
  }

  PF_Pixel16 *p = reinterpret_cast<PF_Pixel16 *>(
      reinterpret_cast<char *>(ctx->srcData) + y * ctx->rowbytes +
      x * static_cast<A_long>(sizeof(PF_Pixel16)));

  return *p;
}

static inline void ComputeShiftedSourceCoords(const SliceContext *ctx,
                                              const SliceSegment &segment,
                                              float worldX, float worldY,
                                              float &srcX, float &srcY) {
  float offsetPixels =
      ctx->shiftAmount * segment.shiftRandomFactor * segment.shiftDirection;
  srcX = worldX + ctx->shiftDirX * offsetPixels;
  srcY = worldY + ctx->shiftDirY * offsetPixels;
}

static void RotatePoint(float centerX, float centerY, float &x, float &y,
                        float angleCos, float angleSin) {
  float dx = x - centerX;
  float dy = y - centerY;

  // Rotate the point
  float newX = dx * angleCos - dy * angleSin + centerX;
  float newY = dx * angleSin + dy * angleCos + centerY;

  x = newX;
  y = newY;
}

// Binary search to find the slice whose [sliceStart, sliceEnd]
// range contains the given slice-space coordinate.
static inline A_long FindSliceIndex(const SliceContext *ctx, float sliceX) {
  if (!ctx || ctx->numSlices <= 0) {
    return -1;
  }

  A_long low = 0;
  A_long high = ctx->numSlices - 1;

  while (low <= high) {
    A_long mid = (low + high) >> 1;
    const SliceSegment &seg = ctx->segments[mid];

    if (sliceX < seg.sliceStart) {
      high = mid - 1;
    } else if (sliceX > seg.sliceEnd) {
      low = mid + 1;
    } else {
      return mid;
    }
  }

  return -1;
}

static PF_Err ProcessMultiSlice(void *refcon, A_long x, A_long y, PF_Pixel *in,
                                PF_Pixel *out) {
  PF_Err err = PF_Err_NONE;
  const SliceContext *ctx = reinterpret_cast<const SliceContext *>(refcon);
  if (!ctx || ctx->numSlices <= 0) {
    out->alpha = out->red = out->green = out->blue = 0;
    return err;
  }

  float worldX = static_cast<float>(x);
  float worldY = static_cast<float>(y);
  float sliceX = worldX;
  float sliceY = worldY;
  RotatePoint(ctx->centerX, ctx->centerY, sliceX, sliceY, ctx->angleCos,
              -ctx->angleSin);

  const A_long idx = FindSliceIndex(ctx, sliceX);
  if (idx < 0) {
    out->alpha = out->red = out->green = out->blue = 0;
    return err;
  }

  // Accumulate contributions:
  // For Alpha: Additive blending (sum of coverages).
  // For RGB: Select the color of the slice with the highest coverage.
  // CRITICAL FIX: Ignore transparent (alpha=0) pixels when selecting RGB,
  // to prevent picking "black" from outside the slice boundary.
  float accumA = 0.0f;
  PF_Pixel bestPixel = {0, 0, 0, 0};
  float maxCoverage = -1.0f;

  auto accumulateSlice = [&](A_long sliceIdx) {
    if (sliceIdx < 0 || sliceIdx >= ctx->numSlices)
      return;

    const SliceSegment &seg = ctx->segments[sliceIdx];

    // Soft interaction with edge
    float coverage = 1.0f;
    float feather = 0.5f;

    if (sliceX < seg.visibleStart - feather ||
        sliceX > seg.visibleEnd + feather) {
      return;
    } else if (sliceX < seg.visibleStart + feather) {
      coverage = (sliceX - (seg.visibleStart - feather)) / (2.0f * feather);
    } else if (sliceX > seg.visibleEnd - feather) {
      coverage = ((seg.visibleEnd + feather) - sliceX) / (2.0f * feather);
    }

    if (coverage <= 0.001f)
      return;

    float srcX = 0.0f, srcY = 0.0f;
    ComputeShiftedSourceCoords(ctx, seg, worldX, worldY, srcX, srcY);
    PF_Pixel p = SampleSourcePixel8(srcX, srcY, ctx);

    // Accumulate Alpha
    accumA += static_cast<float>(p.alpha) * coverage;

    // Logic to select best RGB:
    // Prioritize opaque pixels over transparent ones.
    // If both opaque (or both transparent), pick highest coverage.
    bool currentIsOpaque = (p.alpha > 0);
    bool bestIsOpaque = (bestPixel.alpha > 0);

    if (currentIsOpaque && !bestIsOpaque) {
      // Found an opaque pixel, take it immediately
      maxCoverage = coverage;
      bestPixel = p;
    } else if (currentIsOpaque == bestIsOpaque) {
      // Both opaque or both transparent -> use coverage
      if (coverage > maxCoverage) {
        maxCoverage = coverage;
        bestPixel = p;
      }
    }
  };

  accumulateSlice(idx);
  accumulateSlice(idx - 1);
  accumulateSlice(idx + 1);

  // Output: RGB from the best pixel (untouched), Alpha accumulated
  out->alpha = static_cast<A_u_char>(CLAMP(accumA + 0.5f, 0.0f, 255.0f));
  out->red = bestPixel.red;
  out->green = bestPixel.green;
  out->blue = bestPixel.blue;

  return err;
}

// Function to process a given (x,y) pixel for slice effects (16-bit)
static PF_Err ProcessMultiSlice16(void *refcon, A_long x, A_long y,
                                  PF_Pixel16 *in, PF_Pixel16 *out) {
  PF_Err err = PF_Err_NONE;
  const SliceContext *ctx = reinterpret_cast<const SliceContext *>(refcon);
  if (!ctx || ctx->numSlices <= 0) {
    out->alpha = out->red = out->green = out->blue = 0;
    return err;
  }

  float worldX = static_cast<float>(x);
  float worldY = static_cast<float>(y);
  float sliceX = worldX;
  float sliceY = worldY;
  RotatePoint(ctx->centerX, ctx->centerY, sliceX, sliceY, ctx->angleCos,
              -ctx->angleSin);

  const A_long idx = FindSliceIndex(ctx, sliceX);
  if (idx < 0) {
    out->alpha = out->red = out->green = out->blue = 0;
    return err;
  }

  float accumA = 0.0f;
  const float maxC = static_cast<float>(PF_MAX_CHAN16);
  PF_Pixel16 bestPixel = {0, 0, 0, 0};
  float maxCoverage = -1.0f;

  auto accumulateSlice = [&](A_long sliceIdx) {
    if (sliceIdx < 0 || sliceIdx >= ctx->numSlices)
      return;

    const SliceSegment &seg = ctx->segments[sliceIdx];

    // Soft interaction with edge
    float coverage = 1.0f;
    float feather = 0.5f;

    if (sliceX < seg.visibleStart - feather ||
        sliceX > seg.visibleEnd + feather) {
      return;
    } else if (sliceX < seg.visibleStart + feather) {
      coverage = (sliceX - (seg.visibleStart - feather)) / (2.0f * feather);
    } else if (sliceX > seg.visibleEnd - feather) {
      coverage = ((seg.visibleEnd + feather) - sliceX) / (2.0f * feather);
    }

    if (coverage <= 0.001f)
      return;

    float srcX = 0.0f, srcY = 0.0f;
    ComputeShiftedSourceCoords(ctx, seg, worldX, worldY, srcX, srcY);
    PF_Pixel16 p = SampleSourcePixel16(srcX, srcY, ctx);

    // Accumulate Alpha
    accumA += static_cast<float>(p.alpha) * coverage;

    // Logic to select best RGB:
    // Prioritize opaque pixels over transparent ones.
    bool currentIsOpaque = (p.alpha > 0);
    bool bestIsOpaque = (bestPixel.alpha > 0);

    if (currentIsOpaque && !bestIsOpaque) {
      maxCoverage = coverage;
      bestPixel = p;
    } else if (currentIsOpaque == bestIsOpaque) {
      if (coverage > maxCoverage) {
        maxCoverage = coverage;
        bestPixel = p;
      }
    }
  };

  accumulateSlice(idx);
  accumulateSlice(idx - 1);
  accumulateSlice(idx + 1);

  out->alpha = static_cast<A_u_short>(CLAMP(accumA + 0.5f, 0.0f, maxC));
  out->red = bestPixel.red;
  out->green = bestPixel.green;
  out->blue = bestPixel.blue;

  return err;
}

static PF_Err Render(PF_InData *in_data, PF_OutData *out_data,
                     PF_ParamDef *params[], PF_LayerDef *output) {
  PF_Err err = PF_Err_NONE;
  AEGP_SuiteHandler suites(in_data->pica_basicP);
  PF_EffectWorld *inputP = &params[MULTISLICER_INPUT]->u.ld;
  PF_EffectWorld *outputP = output;

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

  if ((shiftAmount < 0.001f && fabsf(width - 0.9999f) < 0.0001f) ||
      numSlices <= 1) {
    ERR(suites.WorldTransformSuite1()->copy_hq(in_data->effect_ref, inputP,
                                               output, NULL, NULL));
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
  float sliceLength =
      2.0f * sqrtf(static_cast<float>(imageWidth * imageWidth +
                                      imageHeight * imageHeight));

  PF_Handle segmentsHandle =
      suites.HandleSuite1()->host_new_handle(numSlices * sizeof(SliceSegment));
  if (!segmentsHandle) {
    return PF_Err_OUT_OF_MEMORY;
  }
  SliceSegment *segments = *((SliceSegment **)segmentsHandle);
  if (!segments) {
    suites.HandleSuite1()->host_dispose_handle(segmentsHandle);
    return PF_Err_OUT_OF_MEMORY;
  }

  PF_Handle divPointsHandle =
      suites.HandleSuite1()->host_new_handle((numSlices + 1) * sizeof(float));
  if (!divPointsHandle) {
    suites.HandleSuite1()->host_dispose_handle(segmentsHandle);
    return PF_Err_OUT_OF_MEMORY;
  }
  float *divPoints = *((float **)divPointsHandle);
  if (!divPoints) {
    suites.HandleSuite1()->host_dispose_handle(divPointsHandle);
    suites.HandleSuite1()->host_dispose_handle(segmentsHandle);
    return PF_Err_OUT_OF_MEMORY;
  }

  divPoints[0] = -sliceLength / 2.0f;
  divPoints[numSlices] = sliceLength / 2.0f;

  float baselineOffset =
      (GetRandomValue(seed, 12345) - 0.5f) * sliceLength * 0.1f;
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
      } else {
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
      } else {
        for (A_long i = 1; i < numSlices; i++) {
          divPoints[i] = divPoints[0] + (i * targetRange / numSlices);
        }
      }
    }
  }

  for (A_long i = 0; i < numSlices; i++) {
    SliceSegment &segment = segments[i];
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
  float axisSpan = fabsf(angleCos) + fabsf(angleSin);
  float pixelSpan = MAX(1e-3f, resolution_scale * axisSpan);
  context.pixelSpan = pixelSpan;

  if (PF_WORLD_IS_DEEP(inputP)) {
    ERR(suites.Iterate16Suite1()->iterate(in_data, 0, imageHeight, inputP, NULL,
                                          (void *)&context, ProcessMultiSlice16,
                                          outputP));
  } else {
    ERR(suites.Iterate8Suite1()->iterate(in_data, 0, imageHeight, inputP, NULL,
                                         (void *)&context, ProcessMultiSlice,
                                         outputP));
  }

  suites.HandleSuite1()->host_dispose_handle(segmentsHandle);

  return err;
}

extern "C" DllExport PF_Err PluginDataEntryFunction2(
    PF_PluginDataPtr inPtr, PF_PluginDataCB2 inPluginDataCallBackPtr,
    SPBasicSuite *inSPBasicSuitePtr, const char *inHostName,
    const char *inHostVersion) {
  PF_Err result = PF_Err_INVALID_CALLBACK;

  result = PF_REGISTER_EFFECT_EXT2(
      inPtr, inPluginDataCallBackPtr,
      "MultiSlicer",                                  // Name
      "361do MultiSlicer",                            // Match Name
      "361do_plugins",                                // Category
      AE_RESERVED_INFO,                               // Reserved Info
      "EffectMain",                                   // Entry point
      "https://github.com/rebuildup/Ae_MultiSlicer"); // support URL

  return result;
}

extern "C" DllExport PF_Err EffectMain(PF_Cmd cmd, PF_InData *in_data,
                                       PF_OutData *out_data,
                                       PF_ParamDef *params[],
                                       PF_LayerDef *output, void *extra) {
  PF_Err err = PF_Err_NONE;

  try {
    switch (cmd) {
    case PF_Cmd_ABOUT:
      err = About(in_data, out_data, params, output);
      break;

    case PF_Cmd_GLOBAL_SETUP:
      err = GlobalSetup(in_data, out_data, params, output);
      break;

    case PF_Cmd_PARAMS_SETUP:
      err = ParamsSetup(in_data, out_data, params, output);
      break;

    case PF_Cmd_RENDER:
      err = Render(in_data, out_data, params, output);
      break;
    }
  } catch (PF_Err &thrown_err) {
    err = thrown_err;
  }
  return err;
}
