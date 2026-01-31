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
   ====== 1.0         Initial implementation                                  rebuildup   02/01/2026

*/

#include "MultiSlicer.h"
#include <math.h>
#include <stdlib.h>
#include <limits.h>

// Clamp helper for color values
template <typename T> static inline T CLAMP(T value, T min, T max) {
  if (value < min)
    return min;
  if (value > max)
    return max;
  return value;
}

// CRITICAL FIX: Add check for scale.num == 0 to prevent division issues
static inline float GetDownscaleFactor(const PF_RationalScale &scale) {
  if (scale.den == 0 || scale.num == 0) {
    return 1.0f;
  }

  float value = static_cast<float>(scale.num) / static_cast<float>(scale.den);
  return (value > 0.0f) ? value : 1.0f;
}

static PF_Err About(PF_InData *in_data, PF_OutData *out_data,
                    PF_ParamDef *params[], PF_LayerDef *output) {
  AEGP_SuiteHandler suites(in_data->pica_basicP);

  // CRITICAL FIX: Use snprintf instead of sprintf to prevent buffer overflow
  suites.ANSICallbacksSuite1()->snprintf(out_data->return_msg,
                                          sizeof(out_data->return_msg),
                                          "%s v%d.%d\r%s",
                                          STR(StrID_Name), MAJOR_VERSION,
                                          MINOR_VERSION, STR(StrID_Description));
  return PF_Err_NONE;
}

static PF_Err GlobalSetup(PF_InData *in_data, PF_OutData *out_data,
                          PF_ParamDef *params[], PF_LayerDef *output) {
  out_data->my_version = PF_VERSION(MAJOR_VERSION, MINOR_VERSION, BUG_VERSION,
                                    STAGE_VERSION, BUILD_VERSION);

  // Support 16-bit, pixel-independent, and buffer expansion for out-of-bounds rendering
  // CRITICAL FIX: Match PiPL flags (0x06000602)
  out_data->out_flags = PF_OutFlag_DEEP_COLOR_AWARE |
                        PF_OutFlag_PIX_INDEPENDENT |
                        PF_OutFlag_I_EXPAND_BUFFER |
                        PF_OutFlag_SEND_UPDATE_PARAMS_UI |
                        PF_OutFlag_WIDE_TIME_INPUT;

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

// FrameSetup: expand output buffer based on shift amount
static PF_Err FrameSetup(PF_InData *in_data, PF_OutData *out_data,
                         PF_ParamDef *params[], PF_LayerDef *output) {
  PF_Err err = PF_Err_NONE;

  // Get input dimensions
  PF_LayerDef *input = &params[MULTISLICER_INPUT]->u.ld;
  const int input_width = input->width;
  const int input_height = input->height;

  if (input_width <= 0 || input_height <= 0) {
    return PF_Err_NONE;
  }

  // Get shift parameter
  float shiftRaw = params[MULTISLICER_SHIFT]->u.fs_d.value;

  // Downsample adjustment
  float downscale_x = GetDownscaleFactor(in_data->downsample_x);
  float downscale_y = GetDownscaleFactor(in_data->downsample_y);
  float resolution_scale = MIN(downscale_x, downscale_y);
  float shiftAmount = fabsf(shiftRaw) * resolution_scale;

  // If no shift, no expansion needed
  if (shiftAmount < NO_EFFECT_THRESHOLD) {
    return PF_Err_NONE;
  }

  // Calculate expansion needed (shift can occur in any direction)
  // Use maximum possible shift with some margin
  // CRITICAL FIX #5: Check for integer overflow before setting dimensions
  int expansion = static_cast<int>(ceilf(shiftAmount * EXPANSION_MULTIPLIER)) + EXPANSION_MARGIN;
  expansion = MIN(expansion, MAX_EXPANSION);

  // Check for integer overflow before setting dimensions
  if (input_width > INT_MAX - expansion * 2 || input_height > INT_MAX - expansion * 2) {
    // Skip expansion on overflow to prevent undefined behavior
    return PF_Err_NONE;
  }

  // Set output dimensions and origin
  out_data->width = input_width + expansion * 2;
  out_data->height = input_height + expansion * 2;
  // CRITICAL FIX: Clamp expansion to SHRT_MAX to prevent short overflow
  out_data->origin.h = static_cast<short>(MIN(expansion, SHRT_MAX));
  out_data->origin.v = static_cast<short>(MIN(expansion, SHRT_MAX));

  return err;
}

/**
 * Calculate deterministic random value for consistent slice patterns.
 *
 * Uses a multiplicative hash combined with a sine-based transformation
 * to generate pseudo-random values in [0, 1) range. The hash algorithm:
 * 1. Combines seed and index using large prime multipliers (1099087, 2654435761)
 * 2. Masks to 31 bits (0x7FFFFFFF) to ensure positive values
 * 3. Applies sine transformation with constants (12.9898, 43758.5453)
 * 4. Uses fractional part for final [0, 1) range
 *
 * This is a variant of the Tiny Mersenne Twister algorithm adapted for
 * real-time graphics rendering where deterministic output is required.
 *
 * @param seed Base seed value for randomness
 * @param index Index offset for variation
 * @return Random value in range [0.0, 1.0)
 */
static float GetRandomValue(A_long seed, A_long index) {
  // Multiplicative hash using prime numbers for good distribution
  A_long hash = ((seed * RANDOM_HASH_MULT1) + (index * RANDOM_HASH_MULT2)) & RANDOM_HASH_MASK;
  float result = (float)hash / (float)RANDOM_HASH_MASK;

  // Apply sine-based transformation for more appealing randomness
  // Constants chosen to avoid periodic patterns
  result = fabsf(sinf(result * RANDOM_SINE_MULT) * RANDOM_SINE_ADD);
  result = result - floorf(result);

  return result;
}

// Nearest neighbor sampling to preserve source colors without interpolation
static PF_Pixel SampleSourcePixel8(float srcX, float srcY,
                                   const SliceContext *ctx) {
  PF_Pixel result = {0, 0, 0, 0};

  // Round to nearest integer
  A_long x = static_cast<A_long>(srcX + SAMPLE_ROUND_OFFSET);
  A_long y = static_cast<A_long>(srcY + SAMPLE_ROUND_OFFSET);

  if (x < 0 || x >= ctx->width || y < 0 || y >= ctx->height) {
    return result;
  }

  PF_Pixel *p = reinterpret_cast<PF_Pixel *>(
      reinterpret_cast<char *>(ctx->srcData) + y * ctx->rowbytes +
      x * static_cast<A_long>(sizeof(PF_Pixel)));

  return *p;
}

// Nearest neighbor sampling to preserve source colors without interpolation (16-bit)
static PF_Pixel16 SampleSourcePixel16(float srcX, float srcY,
                                      const SliceContext *ctx) {
  PF_Pixel16 result = {0, 0, 0, 0};

  // Round to nearest integer
  A_long x = static_cast<A_long>(srcX + SAMPLE_ROUND_OFFSET);
  A_long y = static_cast<A_long>(srcY + SAMPLE_ROUND_OFFSET);

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

/**
 * Rotate a 2D point around a center point.
 *
 * Performs counter-clockwise rotation using pre-computed cosine/sine values
 * for efficiency. The rotation formula:
 *   newX = centerX + (x - centerX) * cos(angle) - (y - centerY) * sin(angle)
 *   newY = centerY + (x - centerX) * sin(angle) + (y - centerY) * cos(angle)
 *
 * @param centerX X coordinate of rotation center point
 * @param centerY Y coordinate of rotation center point
 * @param x In/out parameter: X coordinate to rotate (modified in place)
 * @param y In/out parameter: Y coordinate to rotate (modified in place)
 * @param angleCos Pre-computed cosine of the rotation angle
 * @param angleSin Pre-computed sine of the rotation angle
 */
static void RotatePoint(float centerX, float centerY, float &x, float &y,
                        float angleCos, float angleSin) {
  float dx = x - centerX;
  float dy = y - centerY;

  // Rotate the point using standard 2D rotation matrix
  float newX = dx * angleCos - dy * angleSin + centerX;
  float newY = dx * angleSin + dy * angleCos + centerY;

  x = newX;
  y = newY;
}

/**
 * Binary search to find the slice containing a given slice-space coordinate.
 *
 * Searches for the slice whose [sliceStart, sliceEnd] range contains the
 * given coordinate. Uses linear search for small slice counts (<=8) for
 * better cache locality, and binary search for larger counts.
 *
 * Return value behavior:
 * - Returns first slice (index 0) if coordinate is before the first slice
 * - Returns last slice (numSlices-1) if coordinate is after the last slice
 * - Returns -1 only if numSlices <= 0 (invalid/empty state)
 *
 * @param ctx Slice context containing segment array and slice count
 * @param sliceX Coordinate in slice space to find containing slice for
 * @return Index of the containing slice, -1 if numSlices <= 0
 */
static inline A_long FindSliceIndex(const SliceContext *ctx, float sliceX) {
  const A_long numSlices = ctx->numSlices;

  if (numSlices <= 0) {
    return -1;
  }

  // Fast path: single slice
  if (numSlices == 1) {
    return 0;
  }

  const SliceSegment *segments = ctx->segments;

  // Check if before first slice - return first slice
  if (sliceX < segments[0].sliceStart) {
    return 0;
  }

  // Check if after last slice - return last slice
  if (sliceX > segments[numSlices - 1].sliceEnd) {
    return numSlices - 1;
  }

  // For small slice counts, linear search is faster due to cache locality
  if (numSlices <= BINARY_SEARCH_THRESHOLD) {
    for (A_long i = 0; i < numSlices; ++i) {
      if (sliceX >= segments[i].sliceStart && sliceX <= segments[i].sliceEnd) {
        return i;
      }
    }
    // Fallback to nearest
    return 0;
  }

  // Binary search for larger slice counts
  A_long low = 0;
  A_long high = numSlices - 1;

  while (low <= high) {
    A_long mid = (low + high) >> 1;
    const SliceSegment &seg = segments[mid];

    if (sliceX < seg.sliceStart) {
      high = mid - 1;
    } else if (sliceX > seg.sliceEnd) {
      low = mid + 1;
    } else {
      return mid;
    }
  }

  // Should not reach here, but return nearest slice
  return (low < numSlices) ? low : numSlices - 1;
}

// =============================================================================
// Template-based pixel processing for both 8-bit and 16-bit color depths
// =============================================================================

/**
 * Template function to process a single pixel for slice effects.
 *
 * This function implements the core slice rendering logic:
 * 1. Converts buffer coordinates to layer coordinates
 * 2. Rotates the point by the slice angle around the anchor point
 * 3. Finds the appropriate slice using optimized binary/linear search
 * 4. Accumulates pixel data from the slice and its neighbors for soft edge blending
 * 5. Outputs RGB from the slice with highest coverage, accumulated alpha
 *
 * Template parameters:
 * - PixelType: PF_Pixel for 8-bit, PF_Pixel16 for 16-bit
 * - ChannelType: A_u_char for 8-bit, A_u_short for 16-bit
 * - MaxChannel: 255 for 8-bit, PF_MAX_CHAN16 for 16-bit
 * - SampleFunc: SampleSourcePixel8 or SampleSourcePixel16
 *
 * @param refcon Pointer to SliceContext containing rendering parameters
 * @param x X coordinate in buffer space
 * @param y Y coordinate in buffer space
 * @param in Input pixel (unused, kept for iterate signature compatibility)
 * @param out Output pixel to write result
 * @return PF_Err error code (always PF_Err_NONE)
 */
template <typename PixelType, typename ChannelType, ChannelType MaxChannel,
          PixelType (*SampleFunc)(float, float, const SliceContext *)>
static PF_Err ProcessMultiSliceT(void *refcon, A_long x, A_long y,
                                  PixelType *in, PixelType *out) {
  (void)in; // Unused - kept for iterate callback signature
  PF_Err err = PF_Err_NONE;
  const SliceContext *ctx = reinterpret_cast<const SliceContext *>(refcon);

  if (!ctx || ctx->numSlices <= 0) {
    out->alpha = out->red = out->green = out->blue = 0;
    return err;
  }

  // Convert buffer coordinates to layer coordinates
  // Buffer coord (x,y) -> Layer coord (x - origin_x, y - origin_y)
  float worldX = static_cast<float>(x) - ctx->output_origin_x;
  float worldY = static_cast<float>(y) - ctx->output_origin_y;
  float sliceX = worldX;
  float sliceY = worldY;
  RotatePoint(ctx->centerX, ctx->centerY, sliceX, sliceY, ctx->angleCos,
              -ctx->angleSin);

  const A_long idx = FindSliceIndex(ctx, sliceX);
  if (idx < 0) {
    out->alpha = out->red = out->green = out->blue = 0;
    return err;
  }

  // Accumulate contributions from adjacent slices for soft edge blending
  // Alpha: Additive blending (sum of coverages)
  // RGB: Select color from slice with highest coverage
  // CRITICAL FIX: Ignore transparent (alpha=0) pixels when selecting RGB,
  // to prevent picking "black" from outside the slice boundary
  float accumA = 0.0f;
  PixelType bestPixel = {0, 0, 0, 0};
  float maxCoverage = -1.0f;

  // Lambda to accumulate contribution from a single slice
  auto accumulateSlice = [&](A_long sliceIdx) {
    if (sliceIdx < 0 || sliceIdx >= ctx->numSlices)
      return;

    const SliceSegment &seg = ctx->segments[sliceIdx];

    // Calculate soft coverage based on distance from visible slice edges
    float coverage = 1.0f;
    constexpr float feather = DEFAULT_FEATHER;

    if (sliceX < seg.visibleStart - feather ||
        sliceX > seg.visibleEnd + feather) {
      return; // Outside feathered region
    } else if (sliceX < seg.visibleStart + feather) {
      // Fade in at leading edge
      coverage = (sliceX - (seg.visibleStart - feather)) / FEATHER_SOFT_EDGE;
    } else if (sliceX > seg.visibleEnd - feather) {
      // Fade out at trailing edge
      coverage = ((seg.visibleEnd + feather) - sliceX) / FEATHER_SOFT_EDGE;
    }

    if (coverage <= COVERAGE_THRESHOLD)
      return;

    // Compute source coordinates with shift applied
    float srcX = 0.0f, srcY = 0.0f;
    ComputeShiftedSourceCoords(ctx, seg, worldX, worldY, srcX, srcY);
    PixelType p = SampleFunc(srcX, srcY, ctx);

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

  // Accumulate from primary slice and adjacent slices for edge blending
  accumulateSlice(idx);
  accumulateSlice(idx - 1);
  accumulateSlice(idx + 1);

  // Output: RGB from the best pixel (untouched), Alpha accumulated
  const float maxC = static_cast<float>(MaxChannel);
  out->alpha = static_cast<ChannelType>(CLAMP(accumA + 0.5f, 0.0f, maxC));
  out->red = bestPixel.red;
  out->green = bestPixel.green;
  out->blue = bestPixel.blue;

  return err;
}

// =============================================================================
// Iterate callback wrappers for template functions
// CRITICAL FIX #1: Replace std::thread with SDK Iterate Pattern
// =============================================================================

static PF_Err Iterate8Callback(void *refcon, A_long x, A_long y, PF_Pixel *in, PF_Pixel *out) {
  return ProcessMultiSliceT<PF_Pixel, A_u_char, 255, SampleSourcePixel8>(refcon, x, y, in, out);
}

static PF_Err Iterate16Callback(void *refcon, A_long x, A_long y, PF_Pixel16 *in, PF_Pixel16 *out) {
  return ProcessMultiSliceT<PF_Pixel16, A_u_short, PF_MAX_CHAN16, SampleSourcePixel16>(refcon, x, y, in, out);
}

// =============================================================================
// Division points calculation - extracted from Render for modularity
// =============================================================================

/**
 * Calculate division points that define slice boundaries.
 *
 * Creates randomized but ordered division points across the slice length:
 * 1. Sets first and last points at slice boundaries
 * 2. Distributes remaining points with average spacing
 * 3. Applies random offsets for visual variety (70% get 20-90% spacing, 30% get 100-180%)
 * 4. Sorts using insertion sort (optimal for small arrays)
 * 5. Enforces minimum spacing (5% of average) to prevent overlap
 * 6. Scales to fit full range if needed (prevents compression at edges)
 *
 * @param seed Random seed for consistent patterns
 * @param numSlices Number of slices to create
 * @param sliceLength Total length of slice space
 * @param divPoints Output array of size (numSlices + 1) for division points
 */
static void CalculateDivisionPoints(A_long seed, A_long numSlices, float sliceLength, float *divPoints) {
  // Set first and last division points at slice boundaries
  divPoints[0] = -sliceLength / 2.0f;
  divPoints[numSlices] = sliceLength / 2.0f;

  // Calculate baseline random offset for organic feel
  float baselineOffset = (GetRandomValue(seed, SEARCH_HASH_BASE1) - RANDOM_ROUND_THRESHOLD) * sliceLength * SEARCH_LENGTH_MARGIN;
  float avgSpacing = sliceLength / numSlices;

  if (numSlices > 1) {
    // Initial even distribution
    for (A_long i = 1; i < numSlices; i++) {
      divPoints[i] = divPoints[0] + (i * avgSpacing);
    }

    // Apply random offsets for visual variety
    // Distribution: 70% get 20-90% spacing, 30% get 100-180% spacing
    for (A_long i = 1; i < numSlices; i++) {
      float baseRandom = GetRandomValue(seed, i * DIV_BASE_RANDOM_INDEX1 + DIV_BASE_RANDOM_INDEX2);
      float randomFactor;
      if (baseRandom < DIV_RANDOM_THRESHOLD_1) {
        randomFactor = DIV_RANDOM_FACTOR_LOW + (baseRandom / DIV_RANDOM_THRESHOLD_1) * DIV_RANDOM_THRESHOLD_1;
      } else {
        randomFactor = DIV_RANDOM_FACTOR_HIGH + ((baseRandom - DIV_RANDOM_THRESHOLD_1) / DIV_RANDOM_THRESHOLD_2) * DIV_RANDOM_FACTOR_MAX;
      }

      float offset = (randomFactor - DIV_RANDOM_FACTOR_HIGH) * avgSpacing;
      divPoints[i] += offset + baselineOffset;
    }

    // Sort division points (insertion sort - small array, good cache locality)
    for (A_long i = 1; i < numSlices; i++) {
      float key = divPoints[i];
      A_long j = i - 1;
      while (j >= 0 && divPoints[j] > key) {
        divPoints[j + 1] = divPoints[j];
        j--;
      }
      divPoints[j + 1] = key;
    }

    // Enforce minimum spacing to prevent overlapping slices
    float minSpacing = avgSpacing * DIV_MIN_SPACING_RATIO;
    for (A_long i = 1; i < numSlices; i++) {
      if (divPoints[i] < divPoints[i - 1] + minSpacing) {
        divPoints[i] = divPoints[i - 1] + minSpacing;
      }
    }

    // Scale to fit full range if needed (prevents compression at edges)
    if (divPoints[numSlices - 1] < divPoints[numSlices] - minSpacing) {
      float actualRange = divPoints[numSlices - 1] - divPoints[0];
      float targetRange = divPoints[numSlices] - divPoints[0];

      if (actualRange > DIV_RANGE_CHECK_THRESHOLD) {
        // Proportional scaling
        for (A_long i = 1; i < numSlices; i++) {
          float relativePos = (divPoints[i] - divPoints[0]) / actualRange;
          divPoints[i] = divPoints[0] + relativePos * targetRange;
        }
      } else {
        // Fallback to even distribution
        for (A_long i = 1; i < numSlices; i++) {
          divPoints[i] = divPoints[0] + (i * targetRange / numSlices);
        }
      }
    }
  }
}

/**
 * Initialize slice segment metadata from division points.
 *
 * For each slice, calculates:
 * - Boundaries (sliceStart, sliceEnd) from division points
 * - Visible region (visibleStart, visibleEnd) based on width parameter
 *   (controls how much of each slice is displayed)
 * - Random shift direction (perpendicular to slice direction)
 * - Random shift magnitude (affects how far slices move)
 *
 * @param seed Random seed for consistent patterns
 * @param numSlices Number of slices to initialize
 * @param width Width parameter (0.0-1.0) for visible portion
 * @param shiftDirection Global shift direction (1.0 or -1.0)
 * @param divPoints Array of division points (size numSlices + 1)
 * @param segments Output array of SliceSegment structures (size numSlices)
 */
static void InitializeSliceSegments(A_long seed, A_long numSlices, float width,
                                     float shiftDirection, const float *divPoints,
                                     SliceSegment *segments) {
  for (A_long i = 0; i < numSlices; i++) {
    SliceSegment &segment = segments[i];

    // Set slice boundaries from division points
    segment.sliceStart = divPoints[i];
    segment.sliceEnd = divPoints[i + 1];
    float sliceWidth = segment.sliceEnd - segment.sliceStart;
    float sliceCenter = segment.sliceStart + (sliceWidth * 0.5f);

    // Calculate visible region based on width parameter
    float halfVisible = MAX(0.0f, sliceWidth * width * 0.5f);
    segment.visibleStart = sliceCenter - halfVisible;
    segment.visibleEnd = sliceCenter + halfVisible;

    // Generate random shift properties
    A_long dirSeed = (seed * DIR_SEED_MULT + i * DIR_SEED_OFFSET) & 0x7FFF;
    A_long factorSeed = (seed * FACTOR_SEED_MULT + i * FACTOR_SEED_OFFSET) & 0x7FFF;
    float randomDir = (GetRandomValue(dirSeed, 0) > 0.5f) ? 1.0f : -1.0f;
    float randomShiftFactor = DEFAULT_FEATHER + GetRandomValue(factorSeed, 0) * MAX_RANDOM_SHIFT_FACTOR;

    segment.shiftDirection = shiftDirection * randomDir;
    segment.shiftRandomFactor = randomShiftFactor;
  }
}

// =============================================================================
// Main render function - orchestrates slice calculation and pixel processing
// =============================================================================

static PF_Err Render(PF_InData *in_data, PF_OutData *out_data,
                     PF_ParamDef *params[], PF_LayerDef *output) {
  PF_Err err = PF_Err_NONE;
  AEGP_SuiteHandler suites(in_data->pica_basicP);

  // CRITICAL FIX: Validate input parameters to prevent NULL pointer dereference
  if (!params || !params[MULTISLICER_INPUT]) {
    return PF_Err_BAD_CALLBACK_PARAM;
  }
  PF_EffectWorld *inputP = &params[MULTISLICER_INPUT]->u.ld;
  if (!inputP || !inputP->data || inputP->width <= 0 || inputP->height <= 0) {
    return PF_Err_BAD_CALLBACK_PARAM;
  }
  PF_EffectWorld *outputP = output;
  if (!outputP || !outputP->data) {
    return PF_Err_BAD_CALLBACK_PARAM;
  }

  // CRITICAL FIX: Add FPU Context Set/Restore for synthetic render with NULL check
#if PF_WANTED_SYNTHETIC_RENDER
  PF_FPUContextData fpuContext;
  if (suites.FPUSuite1()) {
    suites.FPUSuite1()->FPU_CONTEXT_SET(&fpuContext);
  }
#endif

  // Extract parameters
  float shiftRaw = params[MULTISLICER_SHIFT]->u.fs_d.value;
  float width = params[MULTISLICER_WIDTH]->u.fs_d.value / 100.0f;
  A_long numSlices = params[MULTISLICER_SLICES]->u.sd.value;
  PF_Fixed anchor_x = params[MULTISLICER_ANCHOR_POINT]->u.td.x_value;
  PF_Fixed anchor_y = params[MULTISLICER_ANCHOR_POINT]->u.td.y_value;
  A_long angle_long = params[MULTISLICER_ANGLE]->u.ad.value >> 16;
  A_long seed = params[MULTISLICER_SEED]->u.sd.value;

  // CRITICAL FIX: Validate numSlices to prevent integer overflow
  if (numSlices > 1000 || numSlices < 1) {
    return PF_Err_BAD_PARAM;
  }
  numSlices = MAX(1, numSlices);

  float shiftDirection = (shiftRaw >= 0) ? 1.0f : -1.0f;

  float downscale_x = GetDownscaleFactor(in_data->downsample_x);
  float downscale_y = GetDownscaleFactor(in_data->downsample_y);
  float resolution_scale = MIN(downscale_x, downscale_y);
  float shiftAmount = fabsf(shiftRaw) * resolution_scale;

  // Early exit conditions for no-op cases
  // CRITICAL FIX: Use constants from header instead of duplicate definitions
  bool isNoShiftEffect = (shiftAmount < NO_EFFECT_THRESHOLD);
  bool isFullWidth = (fabsf(width - FULL_WIDTH_THRESHOLD) < WIDTH_TOLERANCE);
  bool isSingleSlice = (numSlices <= 1);

  if ((isNoShiftEffect && isFullWidth) || isSingleSlice) {
    // CRITICAL FIX #4: Add ERR() macro to copy_hq call
    err = suites.WorldTransformSuite1()->copy_hq(in_data->effect_ref, inputP,
                                                  output, NULL, NULL);
    ERR(err);
    goto render_cleanup;
  }

  A_long imageWidth = inputP->width;
  A_long imageHeight = inputP->height;

  // Calculate anchor point in pixel coordinates
  float centerX = static_cast<float>(anchor_x) / FIXED_POINT_SCALE;
  float centerY = static_cast<float>(anchor_y) / FIXED_POINT_SCALE;
  centerX = MAX(0.0f, MIN(centerX, static_cast<float>(imageWidth - 1)));
  centerY = MAX(0.0f, MIN(centerY, static_cast<float>(imageHeight - 1)));

  // Calculate rotation parameters
  float angleRad = (float)angle_long * PF_RAD_PER_DEGREE;
  float angleCos = cosf(angleRad);
  float angleSin = sinf(angleRad);

  // Use LAYER size for sliceLength (controls slice spacing/appearance)
  // Not expanded buffer size - that would stretch the slices
  float sliceLength =
      2.0f * sqrtf(static_cast<float>(imageWidth * imageWidth +
                                      imageHeight * imageHeight));

  // Allocate memory for slice segments and division points
  PF_Handle segmentsHandle = nullptr;
  PF_Handle divPointsHandle = nullptr;

  segmentsHandle = suites.HandleSuite1()->host_new_handle(numSlices * sizeof(SliceSegment));
  if (!segmentsHandle) {
    err = PF_Err_OUT_OF_MEMORY;
    goto render_cleanup;
  }
  SliceSegment *segments = *((SliceSegment **)segmentsHandle);
  if (!segments) {
    suites.HandleSuite1()->host_dispose_handle(segmentsHandle);
    segmentsHandle = nullptr;
    err = PF_Err_OUT_OF_MEMORY;
    goto render_cleanup;
  }

  divPointsHandle = suites.HandleSuite1()->host_new_handle((numSlices + 1) * sizeof(float));
  if (!divPointsHandle) {
    err = PF_Err_OUT_OF_MEMORY;
    goto render_cleanup;
  }
  float *divPoints = *((float **)divPointsHandle);
  if (!divPoints) {
    suites.HandleSuite1()->host_dispose_handle(divPointsHandle);
    divPointsHandle = nullptr;
    suites.HandleSuite1()->host_dispose_handle(segmentsHandle);
    segmentsHandle = nullptr;
    err = PF_Err_OUT_OF_MEMORY;
    goto render_cleanup;
  }

  // Calculate division points using extracted function
  CalculateDivisionPoints(seed, numSlices, sliceLength, divPoints);

  // Initialize slice segments using extracted function
  InitializeSliceSegments(seed, numSlices, width, shiftDirection, divPoints, segments);

  // Division points no longer needed after segment initialization
  suites.HandleSuite1()->host_dispose_handle(divPointsHandle);
  divPointsHandle = nullptr;

  // Build render context for iterate callbacks
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
  // pixelSpan reserved for future use in advanced interpolation
  context.pixelSpan = MAX(1e-3f, resolution_scale * (fabsf(angleCos) + fabsf(angleSin)));
  // Set origin for coordinate transformation (from FrameSetup expansion)
  context.output_origin_x = static_cast<float>(in_data->output_origin_x);
  context.output_origin_y = static_cast<float>(in_data->output_origin_y);

  // CRITICAL FIX #1: Replace std::thread with SDK Iterate Pattern
  // Use AE SDK's iterate suite for proper MFR (Multi-Frame Rendering) support
  if (PF_WORLD_IS_DEEP(inputP)) {
    // 16-bit rendering path
    PF_RenderPixelFilterDef filter_def = {
        nullptr,                // input_world
        outputP,                // output_world
        nullptr,                // world_extent
        &context,               // refcon
        Iterate16Callback       // callback
    };
    err = suites.Iterate16Suite1()->iterate(in_data, 0, outputP->width,
                                             0, outputP->height,
                                             &filter_def, inputP, outputP);
    ERR(err);
  } else {
    // 8-bit rendering path
    PF_RenderPixelFilterDef filter_def = {
        nullptr,                // input_world
        outputP,                // output_world
        nullptr,                // world_extent
        &context,               // refcon
        Iterate8Callback        // callback
    };
    err = suites.Iterate8Suite1()->iterate(in_data, 0, outputP->width,
                                            0, outputP->height,
                                            &filter_def, inputP, outputP);
    ERR(err);
  }

render_cleanup:
  if (segmentsHandle) {
    suites.HandleSuite1()->host_dispose_handle(segmentsHandle);
  }
  if (divPointsHandle) {
    suites.HandleSuite1()->host_dispose_handle(divPointsHandle);
  }

  // CRITICAL FIX: Add FPU Context Restore before return with NULL check
#if PF_WANTED_SYNTHETIC_RENDER
  if (suites.FPUSuite1()) {
    suites.FPUSuite1()->FPU_CONTEXT_RESTORE(&fpuContext);
  }
#endif

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

  case PF_Cmd_FRAME_SETUP:
    err = FrameSetup(in_data, out_data, params, output);
    break;

  case PF_Cmd_FRAME_SETDOWN:
    // CRITICAL FIX #3: Add missing PF_Cmd_FRAME_SETDOWN handler
    // Paired with FRAME_SETUP, called after rendering completes
    err = PF_Err_NONE;
    break;

  case PF_Cmd_RENDER:
    err = Render(in_data, out_data, params, output);
    break;

  // CRITICAL FIX #3: Add missing PF_Cmd handlers
  case PF_Cmd_COMPLETE_GENERAL:
    // Called when After Effects needs to complete any pending operations
    err = PF_Err_NONE;
    break;

  case PF_Cmd_RESET:
    // Called when the effect needs to reset its state
    err = PF_Err_NONE;
    break;

  case PF_Cmd_EVENT:
    // Called for UI events (e.g., parameter changes)
    err = PF_Err_NONE;
    break;

  case PF_Cmd_ARGB_GATE_HORIZON_MASK8:
    // Called for 8-bit ARGB gating
    err = PF_Err_NONE;
    break;

  case PF_Cmd_ARGB_GATE_HORIZON_MASK16:
    // Called for 16-bit ARGB gating
    err = PF_Err_NONE;
    break;

  case PF_Cmd_GET_FLATTENED_SEQUENCE_DATA:
    // Called to get flattened sequence data for project saving
    err = PF_Err_NONE;
    break;

  case PF_Cmd_SEQUENCE_SETDOWN:
    // Called when sequence data is being torn down
    err = PF_Err_NONE;
    break;

  default:
    err = PF_Err_UNRECOGNIZED_PARAM;
    break;
  }

  return err;
}
