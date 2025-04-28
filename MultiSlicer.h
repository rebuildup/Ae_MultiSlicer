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

typedef unsigned char		u_char;
typedef unsigned short		u_short;
typedef unsigned short		u_int16;
typedef unsigned long		u_long;
typedef short int			int16;
#define PF_TABLE_BITS	12
#define PF_TABLE_SZ_16	4096

#define PF_DEEP_COLOR_AWARE 1	// make sure we get 16bpc pixels

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

#define	MAJOR_VERSION	1
#define	MINOR_VERSION	0
#define	BUG_VERSION		0
#define	STAGE_VERSION	PF_Stage_DEVELOP
#define	BUILD_VERSION	1


/* Parameter defaults */

#define	MULTISLICER_PROGRESS_MIN		0
#define	MULTISLICER_PROGRESS_MAX		100
#define	MULTISLICER_PROGRESS_DFLT		0

#define	MULTISLICER_SLICES_MIN			2
#define	MULTISLICER_SLICES_MAX			100
#define	MULTISLICER_SLICES_DFLT			10

#define	MULTISLICER_WIDTH_MIN			0.1
#define	MULTISLICER_WIDTH_MAX			5.0
#define	MULTISLICER_WIDTH_DFLT			1.0

#define	MULTISLICER_SEED_MIN			0
#define	MULTISLICER_SEED_MAX			10000
#define	MULTISLICER_SEED_DFLT			1234

#define	MULTISLICER_FADE_MIN			0
#define	MULTISLICER_FADE_MAX			100
#define	MULTISLICER_FADE_DFLT			0

#define	MULTISLICER_FADE_WIDTH_MIN		0
#define	MULTISLICER_FADE_WIDTH_MAX		1.0
#define	MULTISLICER_FADE_WIDTH_DFLT		0.1

// Macro for math operations
#define CLAMP(x, min, max) ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define ABS(x) ((x) < 0 ? -(x) : (x))

enum {
	MULTISLICER_INPUT = 0,
	MULTISLICER_ANGLE,
	MULTISLICER_PROGRESS,
	MULTISLICER_SLICES,
	MULTISLICER_MIN_WIDTH,
	MULTISLICER_MAX_WIDTH,
	MULTISLICER_SEED,
	MULTISLICER_FADE,
	MULTISLICER_FADE_WIDTH,
	MULTISLICER_VIRTUAL_BUFFER,
	MULTISLICER_NUM_PARAMS
};

enum {
	ANGLE_DISK_ID = 1,
	PROGRESS_DISK_ID,
	SLICES_DISK_ID,
	MIN_WIDTH_DISK_ID,
	MAX_WIDTH_DISK_ID,
	SEED_DISK_ID,
	FADE_DISK_ID,
	FADE_WIDTH_DISK_ID,
	VIRTUAL_BUFFER_DISK_ID
};

// Structure to hold slice information for processing
typedef struct SliceInfo {
	void* srcData;         // Source image data
	A_long    rowbytes;         // Row bytes of source
	A_long    width;            // Width of source
	A_long    height;           // Height of source
	float     centerX;          // Center X coordinate
	float     centerY;          // Center Y coordinate
	float     angleCos;         // Cosine of angle
	float     angleSin;         // Sine of angle
	float     sliceStart;       // Starting position of slice
	float     sliceWidth;       // Width of this slice
	float     sliceOffset;      // Offset of this slice
	float     progress;         // Animation progress
	float     fade;             // Fade amount
	float     fadeWidth;        // Fade width at edges
} SliceInfo, * SliceInfoP, ** SliceInfoH;


extern "C" {

	DllExport
		PF_Err
		EffectMain(
			PF_Cmd			cmd,
			PF_InData* in_data,
			PF_OutData* out_data,
			PF_ParamDef* params[],
			PF_LayerDef* output,
			void* extra);

}

#endif // MultiSlicer_H