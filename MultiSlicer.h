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

/*
    MultiSlicer.h
*/

#pragma once

#ifndef MultiSlicer_H
#define MultiSlicer_H

typedef unsigned char        u_char;
typedef unsigned short       u_short;
typedef unsigned short       u_int16;
typedef unsigned long        u_long;
typedef short int            int16;
#define PF_TABLE_BITS    12
#define PF_TABLE_SZ_16   4096

#define PF_DEEP_COLOR_AWARE 1   // make sure we get 16bpc pixels

#include "AEConfig.h"

#ifdef AE_OS_WIN
typedef unsigned short PixelType;
#include <Windows.h>
#endif

#include "entry.h"
#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "AE_Macros.h"
#include "Param_Utils.h"
#include "AE_EffectCBSuites.h"
#include "String_Utils.h"
#include "AE_GeneralPlug.h"
#include "AEFX_ChannelDepthTpl.h"
#include "AEGP_SuiteHandler.h"

#include "MultiSlicer_Strings.h"

// Macros for After Effects SDK functions
#define PF_NEW_WORLD(EFFECT_REF, WIDTH, HEIGHT, FLAGS, WORLD_PTR) \
    (*(in_data->utils->new_world))((EFFECT_REF), (WIDTH), (HEIGHT), (FLAGS), (WORLD_PTR))

#define PF_DISPOSE_WORLD(EFFECT_REF, WORLD_PTR) \
    (*(in_data->utils->dispose_world))((EFFECT_REF), (WORLD_PTR))

/* Versioning information */

#define MAJOR_VERSION    1
#define MINOR_VERSION    0
#define BUG_VERSION      1  // Incremented for bug fixes
#define STAGE_VERSION    PF_Stage_DEVELOP
#define BUILD_VERSION    2  // Incremented for this robust version

/* Parameter defaults */

#define MULTISLICER_ANGLE_DFLT        0

// Extend range but keep default at 0
#define MULTISLICER_SHIFT_MIN         -1000  // Reduced max range for better stability
#define MULTISLICER_SHIFT_MAX         1000   // Reduced max range for better stability
#define MULTISLICER_SHIFT_DFLT        0

// Allow decimal precision for width
#define MULTISLICER_WIDTH_MIN         0
#define MULTISLICER_WIDTH_MAX         100
#define MULTISLICER_WIDTH_DFLT        100

// More reasonable number of slices range
#define MULTISLICER_SLICES_MIN        1      // Changed from 2 to allow single slice
#define MULTISLICER_SLICES_MAX        50     // Reduced max for better performance
#define MULTISLICER_SLICES_DFLT       10

// Better seed range
#define MULTISLICER_SEED_MIN          0
#define MULTISLICER_SEED_MAX          10000
#define MULTISLICER_SEED_DFLT         1234

// Maximum image dimension allowed (safety check)
#define MULTISLICER_MAX_DIMENSION     16000

// Macro for math operations
#define CLAMP(x, min, max) ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define ABS(x) ((x) < 0 ? -(x) : (x))

// FIXED: Parameter enumeration now matches what's actually set up
enum {
    MULTISLICER_INPUT = 0,
    MULTISLICER_ANGLE,
    MULTISLICER_SHIFT,
    MULTISLICER_WIDTH,
    MULTISLICER_SLICES,
    MULTISLICER_SEED,
    MULTISLICER_NUM_PARAMS
};

enum {
    ANGLE_DISK_ID = 1,
    SHIFT_DISK_ID,
    WIDTH_DISK_ID,
    SLICES_DISK_ID,
    SEED_DISK_ID
};

// Structure to hold slice information for processing
typedef struct SliceInfo {
    void* srcData;           // Source image data
    A_long    rowbytes;          // Row bytes of source
    A_long    width;             // Width of source
    A_long    height;            // Height of source
    A_long    numSlices;         // Total number of slices
    float     centerX;           // Center X coordinate
    float     centerY;           // Center Y coordinate
    float     angleCos;          // Cosine of angle
    float     angleSin;          // Sine of angle
    float     sliceStart;        // Starting position of slice
    float     sliceWidth;        // Width of this slice
    float     shiftAmount;       // Raw shift amount in pixels
    float     shiftDirection;    // Direction of shift (-1 or 1)
    float     shiftRandomFactor; // Random multiplier for shift amount
    float     widthScale;        // Width scale factor (0-1)
} SliceInfo, * SliceInfoP, ** SliceInfoH;

// Error codes specific to MultiSlicer
enum {
    MULTISLICER_ERR_NONE = 0,
    MULTISLICER_ERR_OUT_OF_MEMORY,
    MULTISLICER_ERR_IMAGE_TOO_LARGE,
    MULTISLICER_ERR_INVALID_PARAM,
    MULTISLICER_ERR_BUFFER_OVERFLOW
};

// Function prototypes
extern "C" {
    DllExport
        PF_Err
        EffectMain(
            PF_Cmd         cmd,
            PF_InData* in_data,
            PF_OutData* out_data,
            PF_ParamDef* params[],
            PF_LayerDef* output,
            void* extra);
}

#endif // MultiSlicer_H