#pragma once

#ifndef MULTISLICER_H
#define MULTISLICER_H

#define PF_DEEP_COLOR_AWARE 1

#include "AEConfig.h"

#ifdef AE_OS_WIN
typedef unsigned short PixelType;
#include <Windows.h>
#endif

#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "AE_EffectCBSuites.h"
#include "AE_GeneralPlug.h"
#include "AE_Macros.h"
#include "Param_Utils.h"
#include "String_Utils.h"
#include "entry.h"

/* Define PF_TABLE_BITS before including AEFX_ChannelDepthTpl.h */
#define PF_TABLE_BITS 12
#define PF_TABLE_SZ_16 4096

#include "AEFX_ChannelDepthTpl.h"
#include "AEGP_SuiteHandler.h"
#include <algorithm>

#include "MultiSlicer_Strings.h"

/* Versioning information */
#define MAJOR_VERSION 1
#define MINOR_VERSION 0
#define BUG_VERSION 0
#define STAGE_VERSION PF_Stage_DEVELOP
#define BUILD_VERSION 1

#define MULTISLICER_WIDTH_MIN 0
#define MULTISLICER_WIDTH_MAX 100
#define MULTISLICER_WIDTH_DFLT 100

#define MULTISLICER_SLICES_MIN 2
#define MULTISLICER_SLICES_MAX 100
#define MULTISLICER_SLICES_DFLT 10

#define MULTISLICER_SEED_MIN 0
#define MULTISLICER_SEED_MAX 10000
#define MULTISLICER_SEED_DFLT 1234

#define MULTISLICER_ANGLE_DFLT 0
#define MULTISLICER_ANCHOR_X_DFLT 50
#define MULTISLICER_ANCHOR_Y_DFLT 50

// Expansion calculation constants
#define EXPANSION_MULTIPLIER 2.5f
#define EXPANSION_MARGIN 5
#define MAX_EXPANSION 25000

// Feather and edge constants
#define DEFAULT_FEATHER 0.5f
#define FULL_WIDTH_THRESHOLD 1.0f
#define WIDTH_TOLERANCE 0.0001f
#define NO_EFFECT_THRESHOLD 0.001f

// Random seed multipliers
#define DIR_SEED_MULT 17
#define DIR_SEED_OFFSET 31
#define FACTOR_SEED_MULT 23
#define FACTOR_SEED_OFFSET 41
#define MAX_RANDOM_SHIFT_FACTOR 1.5f

// GetRandomValue algorithm constants (Tiny Mersenne Twister variant)
#define RANDOM_HASH_MULT1 1099087
#define RANDOM_HASH_MULT2 2654435761
#define RANDOM_HASH_MASK 0x7FFFFFFF
#define RANDOM_SINE_MULT 12.9898f
#define RANDOM_SINE_ADD 43758.5453f
#define RANDOM_ROUND_THRESHOLD 0.5f

// Division point calculation constants
#define DIV_BASE_RANDOM_INDEX1 3779
#define DIV_BASE_RANDOM_INDEX2 2971
#define DIV_RANDOM_THRESHOLD_1 0.7f
#define DIV_RANDOM_THRESHOLD_2 0.3f
#define DIV_RANDOM_FACTOR_LOW 0.2f
#define DIV_RANDOM_FACTOR_HIGH 1.0f
#define DIV_RANDOM_FACTOR_MAX 0.8f
#define DIV_MIN_SPACING_RATIO 0.05f
#define DIV_RANGE_CHECK_THRESHOLD 0.001f

// Sampling and coordinate constants
#define SAMPLE_ROUND_OFFSET 0.5f
#define BINARY_SEARCH_THRESHOLD 8
#define COVERAGE_THRESHOLD 0.001f
#define FEATHER_SOFT_EDGE (2.0f * DEFAULT_FEATHER)
#define FIXED_POINT_SCALE 65536.0f

// Search algorithm constants
#define SEARCH_HASH_BASE1 12345
#define SEARCH_LENGTH_MARGIN 0.1f
#define SEARCH_SLICE_VARIETY 2.0f

enum {
  MULTISLICER_INPUT = 0,
  MULTISLICER_SHIFT,
  MULTISLICER_WIDTH,
  MULTISLICER_SLICES,
  MULTISLICER_ANCHOR_POINT,
  MULTISLICER_ANGLE,
  MULTISLICER_SEED,
  MULTISLICER_NUM_PARAMS
};

enum {
  SHIFT_DISK_ID = 1,
  WIDTH_DISK_ID,
  SLICES_DISK_ID,
  ANCHOR_POINT_DISK_ID,
  ANGLE_DISK_ID,
  SEED_DISK_ID
};

// Slice metadata describing each horizontal band in slice space
typedef struct {
  float sliceStart;
  float sliceEnd;
  float visibleStart;
  float visibleEnd;
  float shiftDirection;
  float shiftRandomFactor;
} SliceSegment;

// Context shared across iterate callbacks
typedef struct {
  void *srcData;
  A_long rowbytes;
  A_long width;
  A_long height;
  float centerX;
  float centerY;
  float angleCos;
  float angleSin;
  float shiftDirX;
  float shiftDirY;
  float shiftAmount;
  A_long numSlices;
  const SliceSegment *segments;
  float pixelSpan;
  // Origin offset for coordinate transformation (buffer coords -> layer coords)
  float output_origin_x;
  float output_origin_y;
} SliceContext;

extern "C" {
DllExport PF_Err EffectMain(PF_Cmd cmd, PF_InData *in_data,
                            PF_OutData *out_data, PF_ParamDef *params[],
                            PF_LayerDef *output, void *extra);

DllExport PF_Err PluginDataEntryFunction2(
    PF_PluginDataPtr inPtr, PF_PluginDataCB2 inPluginDataCallBackPtr,
    SPBasicSuite *inSPBasicSuitePtr, const char *inHostName,
    const char *inHostVersion);
}

#endif // MULTISLICER_H
