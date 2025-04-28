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

typedef struct {
	A_u_long	index;
	A_char		str[256];
} TableString;

TableString		g_strs[StrID_NUMTYPES] = {
	StrID_NONE,						"",
	StrID_Name,						"MultiSlicer",
	StrID_Description,				"Slices objects into multiple pieces with animation effects.\rBased on Aviutl's MultiSlicer_P script.\rCopyright 2023 Adobe Inc.",
	StrID_Angle_Param_Name,			"Angle",
	StrID_Progress_Param_Name,		"Progress",
	StrID_Slices_Param_Name,		"Number of Slices",
	StrID_MinWidth_Param_Name,		"Min Slice Width",
	StrID_MaxWidth_Param_Name,		"Max Slice Width",
	StrID_Seed_Param_Name,			"Seed",
	StrID_Fade_Param_Name,			"Fade",
	StrID_FadeWidth_Param_Name,		"Fade Width",
	StrID_VirtualBuffer_Param_Name,	"Use Virtual Buffer",
};


char* GetStringPtr(int strNum)
{
	return g_strs[strNum].str;
}