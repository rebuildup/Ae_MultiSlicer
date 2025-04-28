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

/*	MultiSlicer.cpp

	This plugin implements functionality similar to Aviutl's MultiSlicer_P.
	It slices an image into multiple parts with customizable properties.

	Revision History

	Version		Change													Engineer	Date
	=======		======													========	======
	1.0			Initial implementation									yourname	04/28/2025

*/

#include "MultiSlicer.h"

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

	out_data->out_flags = PF_OutFlag_DEEP_COLOR_AWARE;	// Support 16bpc
	out_data->out_flags |= PF_OutFlag_PIX_INDEPENDENT;   // Support multiprocessing
	out_data->out_flags |= PF_OutFlag_SEND_UPDATE_PARAMS_UI;

	return PF_Err_NONE;
}

static PF_Err
ParamsSetup(
	PF_InData* in_data,
	PF_OutData* out_data,
	PF_ParamDef* params[],
	PF_LayerDef* output)
{
	PF_Err		err = PF_Err_NONE;
	PF_ParamDef	def;

	AEFX_CLR_STRUCT(def);

	// Angle parameter - determines the direction of slicing
	PF_ADD_ANGLE(STR(StrID_Angle_Param_Name),
		0,
		ANGLE_DISK_ID);

	// Progress parameter - controls animation progress
	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX(STR(StrID_Progress_Param_Name),
		MULTISLICER_PROGRESS_MIN,
		MULTISLICER_PROGRESS_MAX,
		MULTISLICER_PROGRESS_MIN,
		MULTISLICER_PROGRESS_MAX,
		MULTISLICER_PROGRESS_DFLT,
		PF_Precision_HUNDREDTHS,
		0,
		0,
		PROGRESS_DISK_ID);

	// Number of slices parameter
	AEFX_CLR_STRUCT(def);
	PF_ADD_SLIDER(STR(StrID_Slices_Param_Name),
		MULTISLICER_SLICES_MIN,
		MULTISLICER_SLICES_MAX,
		MULTISLICER_SLICES_MIN,
		MULTISLICER_SLICES_MAX,
		MULTISLICER_SLICES_DFLT,
		SLICES_DISK_ID);

	// Minimum slice width parameter
	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX(STR(StrID_MinWidth_Param_Name),
		MULTISLICER_WIDTH_MIN,
		MULTISLICER_WIDTH_MAX,
		MULTISLICER_WIDTH_MIN,
		MULTISLICER_WIDTH_MAX,
		MULTISLICER_WIDTH_MIN,
		PF_Precision_HUNDREDTHS,
		0,
		0,
		MIN_WIDTH_DISK_ID);

	// Maximum slice width parameter
	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX(STR(StrID_MaxWidth_Param_Name),
		MULTISLICER_WIDTH_MIN,
		MULTISLICER_WIDTH_MAX,
		MULTISLICER_WIDTH_MIN,
		MULTISLICER_WIDTH_MAX,
		MULTISLICER_WIDTH_DFLT,
		PF_Precision_HUNDREDTHS,
		0,
		0,
		MAX_WIDTH_DISK_ID);

	// Seed for randomness
	AEFX_CLR_STRUCT(def);
	PF_ADD_SLIDER(STR(StrID_Seed_Param_Name),
		MULTISLICER_SEED_MIN,
		MULTISLICER_SEED_MAX,
		MULTISLICER_SEED_MIN,
		MULTISLICER_SEED_MAX,
		MULTISLICER_SEED_DFLT,
		SEED_DISK_ID);

	// Fade parameter
	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX(STR(StrID_Fade_Param_Name),
		MULTISLICER_FADE_MIN,
		MULTISLICER_FADE_MAX,
		MULTISLICER_FADE_MIN,
		MULTISLICER_FADE_MAX,
		MULTISLICER_FADE_DFLT,
		PF_Precision_HUNDREDTHS,
		0,
		0,
		FADE_DISK_ID);

	// Fade width parameter
	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX(STR(StrID_FadeWidth_Param_Name),
		MULTISLICER_FADE_WIDTH_MIN,
		MULTISLICER_FADE_WIDTH_MAX,
		MULTISLICER_FADE_WIDTH_MIN,
		MULTISLICER_FADE_WIDTH_MAX,
		MULTISLICER_FADE_WIDTH_DFLT,
		PF_Precision_HUNDREDTHS,
		0,
		0,
		FADE_WIDTH_DISK_ID);

	// Virtual buffer checkbox
	AEFX_CLR_STRUCT(def);
	PF_ADD_CHECKBOX(STR(StrID_VirtualBuffer_Param_Name),
		"",
		FALSE,
		0,
		VIRTUAL_BUFFER_DISK_ID);

	out_data->num_params = MULTISLICER_NUM_PARAMS;

	return err;
}

// Calculate random value for consistent slice patterns
static float GetRandomValue(A_long seed, A_long index) {
	// Simple hash function to generate pseudo-random value
	A_long hash = ((seed * 1099087) + (index * 2654435761)) & 0x7FFFFFFF;
	return (float)hash / (float)0x7FFFFFFF;
}

// Rotate a point around another point
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

// Function to process a single slice of the image
// Composite function for virtual buffer
static PF_Err CompositePixel(
	void* refcon,
	A_long x,
	A_long y,
	PF_Pixel* src,
	PF_Pixel* dst)
{
	PF_Err err = PF_Err_NONE;

	// Simple alpha blending
	if (src->alpha == 0) {
		// Source is fully transparent, keep destination
		return err;
	}
	else if (src->alpha == PF_MAX_CHAN8) {
		// Source is fully opaque, replace destination
		*dst = *src;
	}
	else {
		// Blend based on alpha
		float srcAlpha = (float)src->alpha / PF_MAX_CHAN8;
		float dstAlpha = 1.0f - srcAlpha;

		dst->red = (A_u_char)(src->red * srcAlpha + dst->red * dstAlpha);
		dst->green = (A_u_char)(src->green * srcAlpha + dst->green * dstAlpha);
		dst->blue = (A_u_char)(src->blue * srcAlpha + dst->blue * dstAlpha);
		dst->alpha = MAX(src->alpha, dst->alpha);
	}

	return err;
}

static PF_Err ProcessSlice(
	void* refcon,
	A_long x,
	A_long y,
	PF_Pixel* in,
	PF_Pixel* out)
{
	PF_Err err = PF_Err_NONE;
	SliceInfo* sliceInfoP = (SliceInfo*)refcon;

	if (!sliceInfoP) {
		return err;
	}

	// Point in the original coordinate system
	float rotatedX = x;
	float rotatedY = y;

	// Rotate point to slice space
	RotatePoint(sliceInfoP->centerX, sliceInfoP->centerY,
		rotatedX, rotatedY,
		sliceInfoP->angleCos, -sliceInfoP->angleSin);

	// Calculate distance along the slice direction
	float distAlongAngle = rotatedX - sliceInfoP->sliceStart;

	// Check if this pixel is in the current slice
	if (distAlongAngle >= 0 && distAlongAngle <= sliceInfoP->sliceWidth) {
		// Calculate offset based on progress
		float offset = sliceInfoP->sliceOffset * sliceInfoP->progress;

		// Calculate source coordinates
		float srcX = x - offset * sliceInfoP->angleSin;
		float srcY = y + offset * sliceInfoP->angleCos;

		// Bounds checking
		if (srcX >= 0 && srcX < sliceInfoP->width &&
			srcY >= 0 && srcY < sliceInfoP->height) {

			// Calculate and apply fade if needed
			float fade = 1.0f;
			if (sliceInfoP->fade > 0) {
				// Calculate fade based on distance from center
				float distFromCenter = ABS(sliceInfoP->centerX - rotatedX);
				float normalizedDist = distFromCenter / (sliceInfoP->width * 0.5f);
				float fadeAmount = normalizedDist * sliceInfoP->fade;
				fade = CLAMP(1.0f - fadeAmount, 0.0f, 1.0f);

				// Apply fade width
				if (sliceInfoP->fadeWidth > 0) {
					// Get distance from slice edge
					float edgeDist = MIN(distAlongAngle, sliceInfoP->sliceWidth - distAlongAngle);
					float edgeRatio = MIN(edgeDist / (sliceInfoP->fadeWidth * sliceInfoP->sliceWidth), 1.0f);
					fade *= edgeRatio;
				}
			}

			// Get the source pixel
			A_long srcIndex = ((A_long)srcY * sliceInfoP->rowbytes) + ((A_long)srcX * sizeof(PF_Pixel));
			PF_Pixel* srcPix = (PF_Pixel*)((char*)sliceInfoP->srcData + srcIndex);

			// Apply the fade to alpha and copy the pixel
			out->alpha = (A_u_char)(srcPix->alpha * fade);
			out->red = srcPix->red;
			out->green = srcPix->green;
			out->blue = srcPix->blue;
		}
		else {
			// Out of bounds, make transparent
			out->alpha = 0;
			out->red = 0;
			out->green = 0;
			out->blue = 0;
		}
	}
	else {
		// Not in this slice, make transparent
		out->alpha = 0;
		out->red = 0;
		out->green = 0;
		out->blue = 0;
	}

	return err;
}

// Function to handle 16-bit pixels
static PF_Err ProcessSlice16(
	void* refcon,
	A_long x,
	A_long y,
	PF_Pixel16* in,
	PF_Pixel16* out)
{
	PF_Err err = PF_Err_NONE;
	SliceInfo* sliceInfoP = (SliceInfo*)refcon;

	if (!sliceInfoP) {
		return err;
	}

	// Point in the original coordinate system
	float rotatedX = x;
	float rotatedY = y;

	// Rotate point to slice space
	RotatePoint(sliceInfoP->centerX, sliceInfoP->centerY,
		rotatedX, rotatedY,
		sliceInfoP->angleCos, -sliceInfoP->angleSin);

	// Calculate distance along the slice direction
	float distAlongAngle = rotatedX - sliceInfoP->sliceStart;

	// Check if this pixel is in the current slice
	if (distAlongAngle >= 0 && distAlongAngle <= sliceInfoP->sliceWidth) {
		// Calculate offset based on progress
		float offset = sliceInfoP->sliceOffset * sliceInfoP->progress;

		// Calculate source coordinates
		float srcX = x - offset * sliceInfoP->angleSin;
		float srcY = y + offset * sliceInfoP->angleCos;

		// Bounds checking
		if (srcX >= 0 && srcX < sliceInfoP->width &&
			srcY >= 0 && srcY < sliceInfoP->height) {

			// Calculate and apply fade if needed
			float fade = 1.0f;
			if (sliceInfoP->fade > 0) {
				// Calculate fade based on distance from center
				float distFromCenter = ABS(sliceInfoP->centerX - rotatedX);
				float normalizedDist = distFromCenter / (sliceInfoP->width * 0.5f);
				float fadeAmount = normalizedDist * sliceInfoP->fade;
				fade = CLAMP(1.0f - fadeAmount, 0.0f, 1.0f);

				// Apply fade width
				if (sliceInfoP->fadeWidth > 0) {
					// Get distance from slice edge
					float edgeDist = MIN(distAlongAngle, sliceInfoP->sliceWidth - distAlongAngle);
					float edgeRatio = MIN(edgeDist / (sliceInfoP->fadeWidth * sliceInfoP->sliceWidth), 1.0f);
					fade *= edgeRatio;
				}
			}

			// Get the source pixel
			A_long srcIndex = ((A_long)srcY * sliceInfoP->rowbytes) + ((A_long)srcX * sizeof(PF_Pixel16));
			PF_Pixel16* srcPix = (PF_Pixel16*)((char*)sliceInfoP->srcData + srcIndex);

			// Apply the fade to alpha and copy the pixel
			out->alpha = (A_u_short)(srcPix->alpha * fade);
			out->red = srcPix->red;
			out->green = srcPix->green;
			out->blue = srcPix->blue;
		}
		else {
			// Out of bounds, make transparent
			out->alpha = 0;
			out->red = 0;
			out->green = 0;
			out->blue = 0;
		}
	}
	else {
		// Not in this slice, make transparent
		out->alpha = 0;
		out->red = 0;
		out->green = 0;
		out->blue = 0;
	}

	return err;
}

static PF_Err
Render(
	PF_InData* in_data,
	PF_OutData* out_data,
	PF_ParamDef* params[],
	PF_LayerDef* output)
{
	PF_Err				err = PF_Err_NONE;
	AEGP_SuiteHandler	suites(in_data->pica_basicP);
	PF_EffectWorld* inputP = &params[MULTISLICER_INPUT]->u.ld;
	PF_EffectWorld* outputP = output;

	// Extract parameters
	A_long angle = params[MULTISLICER_ANGLE]->u.ad.value;
	float progress = params[MULTISLICER_PROGRESS]->u.fs_d.value / 100.0f;
	A_long numSlices = params[MULTISLICER_SLICES]->u.sd.value;
	float minWidth = params[MULTISLICER_MIN_WIDTH]->u.fs_d.value;
	float maxWidth = params[MULTISLICER_MAX_WIDTH]->u.fs_d.value;
	A_long seed = params[MULTISLICER_SEED]->u.sd.value;
	float fade = params[MULTISLICER_FADE]->u.fs_d.value / 100.0f;
	float fadeWidth = params[MULTISLICER_FADE_WIDTH]->u.fs_d.value / 100.0f;
	A_long useVirtualBuffer = params[MULTISLICER_VIRTUAL_BUFFER]->u.bd.value;

	// Get image dimensions
	A_long width = inputP->width;
	A_long height = inputP->height;
	A_long centerX = width / 2;
	A_long centerY = height / 2;

	// Calculate angle in radians
	float angleRad = (float)angle * PF_RAD_PER_DEGREE;
	float angleCos = cosf(angleRad);
	float angleSin = sinf(angleRad);

	// Determine slice distribution along the angle
	float sliceLength = MAX(width * ABS(angleCos), height * ABS(angleSin)) * 2.0f;
	float totalWidth = sliceLength * 1.2f; // Add some margin
	float sliceSpacing = totalWidth / numSlices;

	// Create a temp world if using virtual buffer
	PF_EffectWorld tempWorld;

	if (useVirtualBuffer) {
		// Notice the correct format for PF_NEW_WORLD, which is a macro:
		// effect_ref, width, height, flags, world_ptr
		ERR(PF_NEW_WORLD(in_data->effect_ref, width, height,
			PF_NewWorldFlag_NONE, &tempWorld));
	}

	// Clear the output
	ERR(PF_FILL(NULL, NULL, output));

	// For MFR compatibility, account for downsample
	float downsample_factor_x = (float)in_data->downsample_x.den / (float)in_data->downsample_x.num;
	float downsample_factor_y = (float)in_data->downsample_y.den / (float)in_data->downsample_y.num;

	// Process each slice - using random order for AviUtl MultiSlicer_P style
	for (A_long i = 0; i < numSlices; i++) {
		// Set up slice info with MultiSlicer_P style randomness
		SliceInfo sliceInfo;
		sliceInfo.srcData = inputP->data;
		sliceInfo.rowbytes = inputP->rowbytes;
		sliceInfo.width = width;
		sliceInfo.height = height;
		sliceInfo.centerX = centerX;
		sliceInfo.centerY = centerY;
		sliceInfo.angleCos = angleCos;
		sliceInfo.angleSin = angleSin;
		sliceInfo.progress = progress;
		sliceInfo.fade = fade;
		sliceInfo.fadeWidth = fadeWidth;

		// Calculate slice properties with randomness based on seed
		float randomFactor = GetRandomValue(seed, i);
		float sliceWidth = minWidth + randomFactor * (maxWidth - minWidth);

		// Use randomness for offset too, just like MultiSlicer_P
		float offsetRandomness = GetRandomValue(seed, i + numSlices);
		float sliceOffset = (i - numSlices / 2.0f) * (20.0f * downsample_factor_x + offsetRandomness * 15.0f);

		// Position slices with slight randomness in layout
		float posRandomness = GetRandomValue(seed, i + numSlices * 2);
		float sliceStart = -totalWidth / 2.0f + i * sliceSpacing + posRandomness * sliceSpacing * 0.2f;

		// Set remaining slice info
		sliceInfo.sliceStart = sliceStart;
		sliceInfo.sliceWidth = sliceSpacing * sliceWidth;
		sliceInfo.sliceOffset = sliceOffset;

		// Create destination for this slice
		PF_EffectWorld* destWorld = outputP;

		// If using virtual buffer, render to temp world first
		if (useVirtualBuffer) {
			destWorld = &tempWorld;
			ERR(PF_FILL(NULL, NULL, destWorld));
		}

		// Process this slice with appropriate pixel depth handling
		if (PF_WORLD_IS_DEEP(inputP)) {
			ERR(suites.Iterate16Suite1()->iterate(
				in_data,
				0,                // progress base
				height,           // progress final
				inputP,           // src 
				NULL,             // area - null for all pixels
				(void*)&sliceInfo,// refcon - our custom data
				ProcessSlice16,   // pixel function
				destWorld));      // dest
		}
		else {
			ERR(suites.Iterate8Suite1()->iterate(
				in_data,
				0,                // progress base
				height,           // progress final
				inputP,           // src 
				NULL,             // area - null for all pixels
				(void*)&sliceInfo,// refcon - our custom data
				ProcessSlice,     // pixel function
				destWorld));      // dest
		}

		// If using virtual buffer, composite temp world onto output
		if (useVirtualBuffer) {
			// Handle both 8-bit and 16-bit color depths
			if (PF_WORLD_IS_DEEP(output)) {
				// 16-bit composition
				for (int y = 0; y < height; y++) {
					PF_Pixel16* tempRow = (PF_Pixel16*)((char*)tempWorld.data + y * tempWorld.rowbytes);
					PF_Pixel16* outputRow = (PF_Pixel16*)((char*)output->data + y * output->rowbytes);

					for (int x = 0; x < width; x++) {
						PF_Pixel16 tempPix = tempRow[x];

						// Only copy if alpha is not zero
						if (tempPix.alpha > 0) {
							outputRow[x] = tempPix;
						}
					}
				}
			}
			else {
				// 8-bit composition
				for (int y = 0; y < height; y++) {
					PF_Pixel* tempRow = (PF_Pixel*)((char*)tempWorld.data + y * tempWorld.rowbytes);
					PF_Pixel* outputRow = (PF_Pixel*)((char*)output->data + y * output->rowbytes);

					for (int x = 0; x < width; x++) {
						PF_Pixel tempPix = tempRow[x];

						// Only copy if alpha is not zero
						if (tempPix.alpha > 0) {
							outputRow[x] = tempPix;
						}
					}
				}
			}
		}
	}

	// Dispose temp world if needed
	if (useVirtualBuffer) {
		// Correct format for PF_DISPOSE_WORLD is also a macro:
		// effect_ref, world_ptr
		ERR(PF_DISPOSE_WORLD(in_data->effect_ref, &tempWorld));
	}

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
		"ADBE MultiSlicer", // Match Name
		"Sample Plug-ins", // Category
		AE_RESERVED_INFO, // Reserved Info
		"EffectMain",	// Entry point
		"https://www.adobe.com");	// support URL

	return result;
}


PF_Err
EffectMain(
	PF_Cmd			cmd,
	PF_InData* in_data,
	PF_OutData* out_data,
	PF_ParamDef* params[],
	PF_LayerDef* output,
	void* extra)
{
	PF_Err		err = PF_Err_NONE;

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