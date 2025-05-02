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

    This plugin slices an image into multiple parts with customizable properties.
    It allows random division and shifting in random directions.

    Revision History

    Version     Change                                              Engineer   Date
    =======     ======                                              ========   ======
    1.0         Initial implementation                              yourname   04/28/2025

*/

#include "MultiSlicer.h"
#include <stdlib.h>
#include <math.h>

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

    // Support 16-bit, multiprocessing, and Multi-Frame Rendering
    out_data->out_flags = PF_OutFlag_DEEP_COLOR_AWARE;
    out_data->out_flags |= PF_OutFlag_PIX_INDEPENDENT;
    out_data->out_flags |= PF_OutFlag_SEND_UPDATE_PARAMS_UI;

    // Enable Multi-Frame Rendering support
    out_data->out_flags2 = 0x08000000; // PF_OutFlag2_SUPPORTS_THREADED_RENDERING

    return PF_Err_NONE;
}
static PF_Err
ParamsSetup(
    PF_InData* in_data,
    PF_OutData* out_data,
    PF_ParamDef* params[],
    PF_LayerDef* output)
{
    PF_Err      err = PF_Err_NONE;
    PF_ParamDef def;

    AEFX_CLR_STRUCT(def);

    // Angle parameter - determines the direction of slicing
    PF_ADD_ANGLE(STR(StrID_Angle_Param_Name),
        MULTISLICER_ANGLE_DFLT,
        ANGLE_DISK_ID);

    // Shift parameter - controls how much the slices move, in pixels
    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX(STR(StrID_Shift_Param_Name),
        -10000,
        10000,
        -500,
        500,
        0,
        PF_Precision_INTEGER,
        0,
        0,
        SHIFT_DISK_ID);

    // Width parameter - controls the display width of split image from 0-100%
    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX(STR(StrID_Width_Param_Name),
        0,
        100,
        0,
        100,
        100,
        PF_Precision_TENTHS,  // Use TENTHS for decimal precision
        0,
        0,
        WIDTH_DISK_ID);

    // Number of slices parameter
    AEFX_CLR_STRUCT(def);
    PF_ADD_SLIDER(STR(StrID_Slices_Param_Name),
        1,
        1000,
        1,
        50,
        10,
        SLICES_DISK_ID);

    // Seed for randomness
    AEFX_CLR_STRUCT(def);
    PF_ADD_SLIDER(STR(StrID_Seed_Param_Name),
        0,
        10000,
        0,
        500,
        0,
        SEED_DISK_ID);

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

// Determine if a point is inside a visible portion of a slice
static bool IsPointInSlice(
    float x, float y,
    const SliceInfo* sliceInfoP)
{
    // Rotate point to slice space
    float rotatedX = x;
    float rotatedY = y;

    RotatePoint(sliceInfoP->centerX, sliceInfoP->centerY,
        rotatedX, rotatedY,
        sliceInfoP->angleCos, -sliceInfoP->angleSin);

    // Calculate the center of the slice
    float sliceCenter = sliceInfoP->sliceStart + (sliceInfoP->sliceWidth / 2.0f);

    // Calculate the half-width of the visible portion
    // This is always proportional to the Width parameter
    float halfVisibleWidth = (sliceInfoP->sliceWidth / 2.0f) * sliceInfoP->widthScale;

    // Calculate visible boundaries from the center
    float leftVisible = sliceCenter - halfVisibleWidth;
    float rightVisible = sliceCenter + halfVisibleWidth;

    // Check if the point is inside the visible region
    // Use a much smaller epsilon to avoid precision issues
    const float epsilon = 0.0001f;

    return (rotatedX >= leftVisible - epsilon && rotatedX <= rightVisible + epsilon);
}
// Get pixel color from the source at given coordinates with bounds checking
static PF_Pixel GetSourcePixel(
    float srcX, float srcY,
    const SliceInfo* sliceInfoP)
{
    PF_Pixel result = { 0, 0, 0, 0 }; // Default to transparent black

    // Bounds checking
    if (srcX >= 0 && srcX < sliceInfoP->width &&
        srcY >= 0 && srcY < sliceInfoP->height) {

        // Calculate the index in the source data
        A_long srcIndex = ((A_long)srcY * sliceInfoP->rowbytes) + ((A_long)srcX * sizeof(PF_Pixel));
        PF_Pixel* srcPix = (PF_Pixel*)((char*)sliceInfoP->srcData + srcIndex);

        // Copy the pixel
        result = *srcPix;
    }

    return result;
}

// Get 16-bit pixel color from the source at given coordinates with bounds checking
static PF_Pixel16 GetSourcePixel16(
    float srcX, float srcY,
    const SliceInfo* sliceInfoP)
{
    PF_Pixel16 result = { 0, 0, 0, 0 }; // Default to transparent black

    // Bounds checking
    if (srcX >= 0 && srcX < sliceInfoP->width &&
        srcY >= 0 && srcY < sliceInfoP->height) {

        // Calculate the index in the source data
        A_long srcIndex = ((A_long)srcY * sliceInfoP->rowbytes) + ((A_long)srcX * sizeof(PF_Pixel16));
        PF_Pixel16* srcPix = (PF_Pixel16*)((char*)sliceInfoP->srcData + srcIndex);

        // Copy the pixel
        result = *srcPix;
    }

    return result;
}

static PF_Err
ProcessMultiSlice(
    void* refcon,
    A_long x,
    A_long y,
    PF_Pixel* in,
    PF_Pixel* out)
{
    PF_Err err = PF_Err_NONE;
    SliceInfo* sliceInfosArray = (SliceInfo*)refcon;

    // Handle full width case - don't use a floating-point comparison
    // Instead, check if we're very close to full width
    if (sliceInfosArray[0].shiftAmount < 0.001f && sliceInfosArray[0].widthScale >= 0.9999f) {
        // Full width with no shift - just copy the original pixel
        *out = *in;
        return err;
    }

    // Start with transparent pixel
    out->alpha = 0;
    out->red = 0;
    out->green = 0;
    out->blue = 0;

    // Check if point is inside any slice
    for (int i = 0; i < sliceInfosArray[0].numSlices; i++) {
        SliceInfo* currentSlice = &sliceInfosArray[i];

        // Skip slices with effectively zero width
        if (currentSlice->widthScale <= 0.0001f) continue;

        if (IsPointInSlice(x, y, currentSlice)) {
            float offsetPixels = currentSlice->shiftAmount * currentSlice->shiftRandomFactor * currentSlice->shiftDirection;

            // Use consistent perpendicular direction calculation
            float shiftDirX = currentSlice->angleSin; // perpendicular X component
            float shiftDirY = -currentSlice->angleCos; // perpendicular Y component

            // Apply the shift to get source pixel coordinates
            float srcX = x + shiftDirX * offsetPixels;
            float srcY = y + shiftDirY * offsetPixels;

            // Get the source pixel
            PF_Pixel srcPixel = GetSourcePixel(srcX, srcY, currentSlice);

            // Only use this pixel if it's not fully transparent
            if (srcPixel.alpha > 0) {
                *out = srcPixel;
                return err; // Use the first valid slice we find
            }
        }
    }

    return err;
}

// Function to process a given (x,y) pixel for slice effects (16-bit)
static PF_Err
ProcessMultiSlice16(
    void* refcon,
    A_long x,
    A_long y,
    PF_Pixel16* in,
    PF_Pixel16* out)
{
    PF_Err err = PF_Err_NONE;
    SliceInfo* sliceInfosArray = (SliceInfo*)refcon;

    // Handle full width case - don't use a floating-point comparison
    // Instead, check if we're very close to full width
    if (sliceInfosArray[0].shiftAmount < 0.001f && sliceInfosArray[0].widthScale >= 0.9999f) {
        // Full width with no shift - just copy the original pixel
        *out = *in;
        return err;
    }

    // Start with transparent pixel
    out->alpha = 0;
    out->red = 0;
    out->green = 0;
    out->blue = 0;

    // Check if point is inside any slice
    for (int i = 0; i < sliceInfosArray[0].numSlices; i++) {
        SliceInfo* currentSlice = &sliceInfosArray[i];

        // Skip slices with effectively zero width
        if (currentSlice->widthScale <= 0.0001f) continue;

        if (IsPointInSlice(x, y, currentSlice)) {
            float offsetPixels = currentSlice->shiftAmount * currentSlice->shiftRandomFactor * currentSlice->shiftDirection;

            // Use consistent perpendicular direction calculation
            float shiftDirX = currentSlice->angleSin; // perpendicular X component
            float shiftDirY = -currentSlice->angleCos; // perpendicular Y component

            // Apply the shift to get source pixel coordinates
            float srcX = x + shiftDirX * offsetPixels;
            float srcY = y + shiftDirY * offsetPixels;

            // Get the source pixel
            PF_Pixel16 srcPixel = GetSourcePixel16(srcX, srcY, currentSlice);

            // Only use this pixel if it's not fully transparent
            if (srcPixel.alpha > 0) {
                *out = srcPixel;
                return err; // Use the first valid slice we find
            }
        }
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
    PF_Err              err = PF_Err_NONE;
    AEGP_SuiteHandler   suites(in_data->pica_basicP);
    PF_EffectWorld* inputP = &params[MULTISLICER_INPUT]->u.ld;
    PF_EffectWorld* outputP = output;

    // Extract parameters with proper bit-shift for angle
    A_long angle_long = params[MULTISLICER_ANGLE]->u.ad.value >> 16;
    float shiftRaw = params[MULTISLICER_SHIFT]->u.fs_d.value;
    float width = params[MULTISLICER_WIDTH]->u.fs_d.value / 100.0f;
    A_long numSlices = params[MULTISLICER_SLICES]->u.sd.value;
    A_long seed = params[MULTISLICER_SEED]->u.sd.value;

    // Calculate downsampling factors for composition display resolution
    float downsize_x = static_cast<float>(in_data->downsample_x.den) / static_cast<float>(in_data->downsample_x.num);
    float downsize_y = static_cast<float>(in_data->downsample_y.den) / static_cast<float>(in_data->downsample_y.num);

    // Adjust shift amount based on composition display resolution
    float resolution_factor = min(downsize_x, downsize_y);
    float shiftAmount = fabsf(shiftRaw) / resolution_factor;

    // Properly define shift direction based on sign of the shift parameter
    float shiftDirection = (shiftRaw >= 0) ? 1.0f : -1.0f;

    // Fast path for identity case
    if (shiftAmount < 0.001f && fabsf(width - 1.0f) < 0.001f) {
        // Just copy the input to output
        ERR(suites.WorldTransformSuite1()->copy_hq(
            in_data->effect_ref,
            inputP,
            output,
            NULL,
            NULL));
        return err;
    }

    // Get image dimensions
    A_long imageWidth = inputP->width;
    A_long imageHeight = inputP->height;
    A_long centerX = imageWidth / 2;
    A_long centerY = imageHeight / 2;

    // Calculate angle in radians
    float angleRad = (float)angle_long * PF_RAD_PER_DEGREE;
    float angleCos = cosf(angleRad);
    float angleSin = sinf(angleRad);

    // Determine slice distribution along the angle
    float sliceLength = 2.0f * MAX(
        imageWidth * fabsf(angleCos) + imageHeight * fabsf(angleSin),
        imageHeight * fabsf(angleCos) + imageWidth * fabsf(angleSin)
    );

    // Create a handle for our slice info array
    PF_Handle sliceInfosHandle = suites.HandleSuite1()->host_new_handle(numSlices * sizeof(SliceInfo));
    if (!sliceInfosHandle) {
        return PF_Err_OUT_OF_MEMORY;
    }

    // Lock the handle to get a pointer to the memory
    SliceInfo* sliceInfos = *((SliceInfo**)sliceInfosHandle);
    if (!sliceInfos) {
        suites.HandleSuite1()->host_dispose_handle(sliceInfosHandle);
        return PF_Err_OUT_OF_MEMORY;
    }

    // IMPROVED: Generate a randomized baseline for slicing
    // First, create a set of dividing points
    PF_Handle divPointsHandle = suites.HandleSuite1()->host_new_handle((numSlices + 1) * sizeof(float));
    if (!divPointsHandle) {
        suites.HandleSuite1()->host_dispose_handle(sliceInfosHandle);
        return PF_Err_OUT_OF_MEMORY;
    }

    float* divPoints = *((float**)divPointsHandle);
    if (!divPoints) {
        suites.HandleSuite1()->host_dispose_handle(divPointsHandle);
        suites.HandleSuite1()->host_dispose_handle(sliceInfosHandle);
        return PF_Err_OUT_OF_MEMORY;
    }

    // Start with boundaries of the total slice area
    divPoints[0] = -sliceLength / 2.0f;
    divPoints[numSlices] = sliceLength / 2.0f;

    // Generate a random value for the baseline offset
    float baselineOffset = (GetRandomValue(seed, 12345) - 0.5f) * sliceLength * 0.2f;

    // Generate division points between slices with random spacing
    // First, generate uniform divisions
    float equalSpacing = sliceLength / numSlices;

    // Then add randomization to these divisions
    for (A_long i = 1; i < numSlices; i++) {
        // Calculate position as if uniform
        float uniformPos = divPoints[0] + (i * equalSpacing);

        // Add randomization - more pronounced for higher values of numSlices
        float randomFactor = 0.6f - (0.4f * i / numSlices); // Higher slices get less randomization
        float randomOffset = (GetRandomValue(seed, i * 3779 + 1259) - 0.5f) * equalSpacing * randomFactor;

        // Set the division point with randomization and global offset
        divPoints[i] = uniformPos + randomOffset + baselineOffset;
    }

    // Ensure divisions are strictly increasing
    for (A_long i = 1; i < numSlices; i++) {
        if (divPoints[i] <= divPoints[i - 1] + (equalSpacing * 0.15f)) { // Minimum 15% of equal spacing
            divPoints[i] = divPoints[i - 1] + (equalSpacing * 0.15f);
        }
    }


    // Fill the array with slice information
    for (A_long i = 0; i < numSlices; i++) {
        // Basic information common to all slices
        sliceInfos[i].srcData = inputP->data;
        sliceInfos[i].rowbytes = inputP->rowbytes;
        sliceInfos[i].width = imageWidth;
        sliceInfos[i].height = imageHeight;
        sliceInfos[i].centerX = centerX;
        sliceInfos[i].centerY = centerY;
        sliceInfos[i].angleCos = angleCos;
        sliceInfos[i].angleSin = angleSin;
        sliceInfos[i].numSlices = numSlices;

        // Apply width scaling linearly with no special cases or branches
        sliceInfos[i].widthScale = width;
        sliceInfos[i].shiftAmount = shiftAmount;

        // Set slice position properties from our division points
        float sliceStart = divPoints[i];
        float sliceWidth = divPoints[i + 1] - divPoints[i];

        sliceInfos[i].sliceStart = sliceStart;
        sliceInfos[i].sliceWidth = sliceWidth;

        // Apply randomization to shift properties as before
        A_long dirSeed = (seed * 17 + i * 31) & 0x7FFF;
        A_long factorSeed = (seed * 23 + i * 41) & 0x7FFF;

        float randomDir = (GetRandomValue(dirSeed, 0) > 0.5f) ? 1.0f : -1.0f;
        float randomShiftFactor = 0.5f + GetRandomValue(factorSeed, 0) * 1.0f;

        sliceInfos[i].shiftDirection = shiftDirection * randomDir;
        sliceInfos[i].shiftRandomFactor = randomShiftFactor;
    }

    // Free the division points handle
    suites.HandleSuite1()->host_dispose_handle(divPointsHandle);

    // Process the entire image (operates on all slices in a single pass)
    if (PF_WORLD_IS_DEEP(inputP)) {
        ERR(suites.Iterate16Suite1()->iterate(
            in_data,
            0,                  // progress base
            imageHeight,        // progress final
            inputP,             // src
            NULL,               // area - null for all pixels
            (void*)sliceInfos,  // refcon - our slices array
            ProcessMultiSlice16,// pixel function
            outputP));          // dest
    }
    else {
        ERR(suites.Iterate8Suite1()->iterate(
            in_data,
            0,                  // progress base
            imageHeight,        // progress final
            inputP,             // src
            NULL,               // area - null for all pixels
            (void*)sliceInfos,  // refcon - our slices array
            ProcessMultiSlice,  // pixel function
            outputP));          // dest
    }

    // Free the SliceInfo array
    suites.HandleSuite1()->host_dispose_handle(sliceInfosHandle);

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
        "EffectMain", // Entry point
        "https://www.adobe.com"); // support URL

    return result;
}


PF_Err
EffectMain(
    PF_Cmd          cmd,
    PF_InData* in_data,
    PF_OutData* out_data,
    PF_ParamDef* params[],
    PF_LayerDef* output,
    void* extra)
{
    PF_Err      err = PF_Err_NONE;

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