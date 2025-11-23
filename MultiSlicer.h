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

#define BUG_VERSION      0
#define STAGE_VERSION    PF_Stage_DEVELOP
#define BUILD_VERSION    1


/* Parameter defaults */

#define MULTISLICER_ANGLE_DFLT        0

#define MULTISLICER_SHIFT_MIN         -10000
#define MULTISLICER_SHIFT_MAX         10000
#define MULTISLICER_SHIFT_DFLT        0

#define MULTISLICER_WIDTH_MIN         0
#define MULTISLICER_WIDTH_MAX         100
#define MULTISLICER_WIDTH_DFLT        100

#define MULTISLICER_SLICES_MIN        2
#define MULTISLICER_SLICES_MAX        100
#define MULTISLICER_SLICES_DFLT       10

#define MULTISLICER_SEED_MIN          0
#define MULTISLICER_SEED_MAX          10000
#define MULTISLICER_SEED_DFLT         1234

// Macro for math operations
#define CLAMP(x, min, max) ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define ABS(x) ((x) < 0 ? -(x) : (x))

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
}

#endif // MultiSlicer_H
