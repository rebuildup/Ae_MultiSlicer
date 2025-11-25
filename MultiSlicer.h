#pragma once

#ifndef MULTISLICER_H
#define MULTISLICER_H

#define PF_DEEP_COLOR_AWARE 1

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

/* Define PF_TABLE_BITS before including AEFX_ChannelDepthTpl.h */
#define PF_TABLE_BITS	12
#define PF_TABLE_SZ_16	4096

#include "AEFX_ChannelDepthTpl.h"
#include "AEGP_SuiteHandler.h"

#include "MultiSlicer_Strings.h"

/* Versioning information */
#define	MAJOR_VERSION	1
#define	MINOR_VERSION	0
#define	BUG_VERSION		0
#define	STAGE_VERSION	PF_Stage_DEVELOP
#define	BUILD_VERSION	1

#define MULTISLICER_WIDTH_MIN         0
#define MULTISLICER_WIDTH_MAX         100
#define MULTISLICER_WIDTH_DFLT        100

#define MULTISLICER_SLICES_MIN        2
#define MULTISLICER_SLICES_MAX        100
#define MULTISLICER_SLICES_DFLT       10

#define MULTISLICER_SEED_MIN          0
#define MULTISLICER_SEED_MAX          10000
#define MULTISLICER_SEED_DFLT         1234

#define MULTISLICER_ANGLE_DFLT        0
#define MULTISLICER_ANCHOR_X_DFLT     50
#define MULTISLICER_ANCHOR_Y_DFLT     50

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
    void* srcData;
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
    const SliceSegment* segments;
    float featherWidth;
} SliceContext;

extern "C" {
    DllExport
    PF_Err
    EffectMain(
        PF_Cmd          cmd,
        PF_InData       *in_data,
        PF_OutData      *out_data,
        PF_ParamDef     *params[],
        PF_LayerDef     *output,
        void            *extra);
}

#endif // MULTISLICER_H


