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

#include "MultiSlicer.h"

// Define safe string buffer size constant
#define STRING_BUFFER_SIZE 256

typedef struct {
    A_u_long    index;
    A_char      str[STRING_BUFFER_SIZE];
} TableString;

// Forward declaration of g_strs for GetStringPtr
extern TableString g_strs[StrID_NUMTYPES];

// GetStringPtr must be exported as C for compatibility with String_Utils.h macro
extern "C" A_char* GetStringPtr(int strNum)
{
    if (strNum < 0 || strNum >= StrID_NUMTYPES) {
        return g_strs[StrID_NONE].str;
    }
    return g_strs[strNum].str;
}

// Definition of g_strs (must come after GetStringPtr that references it)
TableString     g_strs[StrID_NUMTYPES] = {
    StrID_NONE,                         "",
    StrID_Name,                         "MultiSlicer",
    StrID_Description,                  "Slices objects into multiple pieces with randomized shifting effects.\rCopyright 2025 Adobe Inc.",
    StrID_Angle_Param_Name,             "Angle",
    StrID_Shift_Param_Name,             "Shift",
    StrID_Width_Param_Name,             "Width",
    StrID_Slices_Param_Name,            "Number of Slices",
    StrID_Seed_Param_Name,              "Seed",
};


// Safely get stdlib string pointer with bounds checking
char* GetStdStringPtr(int strNum)
{
    if (strNum < 0 || strNum >= StrID_NUMTYPES) {
        return g_strs[StrID_NONE].str;
    }
    return g_strs[strNum].str;
}

// Get string length safely (for external use)
A_long GetStringSafeLength(int strNum)
{
    if (strNum < 0 || strNum >= StrID_NUMTYPES) {
        return 0;
    }
    // Use safe string length check
    const A_char* str = g_strs[strNum].str;
    A_long len = 0;
    while (len < STRING_BUFFER_SIZE && str[len] != '\0') {
        len++;
    }
    return len;
}