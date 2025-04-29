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
    1.0.1       Fixed parameter mismatch and improved stability     yourname   05/01/2025

*/

#include "MultiSlicer.h"
#include <stdlib.h>
#include <math.h>
#include <string.h> // For memset and other memory functions

static PF_Err
About(
    PF_InData* in_data,
    PF_OutData* out_data,
    PF_ParamDef* params[],
    PF_LayerDef* output)
{
    AEGP_SuiteHandler suites(in_data->pica_basicP);

    // Safety check for null pointers
    if (!in_data || !out_data) {
        return PF_Err_INTERNAL_STRUCT_DAMAGED;
    }

    try {
        suites.ANSICallbacksSuite1()->sprintf(out_data->return_msg,
            "%s v%d.%d.%d\r%s",
            STR(StrID_Name),
            MAJOR_VERSION,
            MINOR_VERSION,
            BUG_VERSION,
            STR(StrID_Description));
    }
    catch (...) {
        return PF_Err_INTERNAL_STRUCT_DAMAGED;
    }

    return PF_Err_NONE;
}

static PF_Err
GlobalSetup(
    PF_InData* in_data,
    PF_OutData* out_data,
    PF_ParamDef* params[],
    PF_LayerDef* output)
{
    // Safety check for null pointers
    if (!in_data || !out_data) {
        return PF_Err_INTERNAL_STRUCT_DAMAGED;
    }

    // IMPORTANT: Use PF_VERSION macro to set version correctly
    out_data->my_version = PF_VERSION(MAJOR_VERSION,
        MINOR_VERSION,
        BUG_VERSION,
        STAGE_VERSION,
        BUILD_VERSION);

    // Set these flags exactly as they appear in PiPL
    // The hex value 0x06000400 matches the PiPL file
    out_data->out_flags = 0;  // Reset to zero first
    out_data->out_flags |= PF_OutFlag_DEEP_COLOR_AWARE;
    out_data->out_flags |= PF_OutFlag_PIX_INDEPENDENT;
    out_data->out_flags |= PF_OutFlag_USE_OUTPUT_EXTENT;

    // IMPORTANT: Do not add PF_OutFlag_SEND_UPDATE_PARAMS_UI here
    // as it's not included in the PiPL resource

    // Enable Multi-Frame Rendering support
    out_data->out_flags2 = PF_OutFlag2_SUPPORTS_THREADED_RENDERING;

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

    // Safety check for null pointers
    if (!in_data || !out_data || !params) {
        return PF_Err_INTERNAL_STRUCT_DAMAGED;
    }

    try {
        // Clear the parameter definition before use
        AEFX_CLR_STRUCT(def);

        // Angle parameter - determines the direction of slicing
        PF_ADD_ANGLE(STR(StrID_Angle_Param_Name),
            MULTISLICER_ANGLE_DFLT,
            ANGLE_DISK_ID);

        // Shift parameter - controls how much the slices move, in pixels
        AEFX_CLR_STRUCT(def);
        PF_ADD_FLOAT_SLIDERX(STR(StrID_Shift_Param_Name),
            MULTISLICER_SHIFT_MIN,    // Valid min value
            MULTISLICER_SHIFT_MAX,    // Valid max value
            -500,                     // Display min value
            500,                      // Display max value
            MULTISLICER_SHIFT_DFLT,   // Default value
            PF_Precision_TENTHS,      // Show 1 decimal place for finer control
            0,
            0,
            SHIFT_DISK_ID);

        // Width parameter - controls the display width of split image
        AEFX_CLR_STRUCT(def);
        PF_ADD_FLOAT_SLIDERX(STR(StrID_Width_Param_Name),
            MULTISLICER_WIDTH_MIN,
            MULTISLICER_WIDTH_MAX,
            MULTISLICER_WIDTH_MIN,
            MULTISLICER_WIDTH_MAX,
            MULTISLICER_WIDTH_DFLT,
            PF_Precision_TENTHS,     // Show 1 decimal place for finer control
            0,
            0,
            WIDTH_DISK_ID);

        // Number of slices parameter
        AEFX_CLR_STRUCT(def);
        PF_ADD_SLIDER(STR(StrID_Slices_Param_Name),
            MULTISLICER_SLICES_MIN,
            MULTISLICER_SLICES_MAX,
            MULTISLICER_SLICES_MIN,
            MULTISLICER_SLICES_MAX,
            MULTISLICER_SLICES_DFLT,
            SLICES_DISK_ID);

        // Seed for randomness
        AEFX_CLR_STRUCT(def);
        PF_ParamFlags flags = PF_ParamFlag_CANNOT_TIME_VARY | PF_ParamFlag_CANNOT_INTERP;
        PF_ADD_SLIDER(STR(StrID_Seed_Param_Name),
            MULTISLICER_SEED_MIN,
            MULTISLICER_SEED_MAX,
            MULTISLICER_SEED_MIN,
            MULTISLICER_SEED_MAX,
            MULTISLICER_SEED_DFLT,
            SEED_DISK_ID);
        params[MULTISLICER_SEED]->flags |= flags;

        // Set the total number of parameters - IMPORTANT: Must match the enum MULTISLICER_NUM_PARAMS
        out_data->num_params = MULTISLICER_NUM_PARAMS;
    }
    catch (PF_Err& thrown_err) {
        err = thrown_err;
    }
    catch (...) {
        err = PF_Err_INTERNAL_STRUCT_DAMAGED;
    }

    return err;
}

// Calculate deterministic random value for consistent slice patterns
static float GetRandomValue(A_long seed, A_long index) {
    // Input validation to prevent crashes
    if (index < 0) index = 0;

    // More complex hash function for better distribution
    A_long hash = ((seed * 1099087) + (index * 2654435761U)) & 0x7FFFFFFF;
    float result = (float)hash / (float)0x7FFFFFFF;

    // Apply some additional transformation for more appealing randomness
    result = fabsf(sinf(result * 12.9898f) * 43758.5453f);
    result = result - floorf(result);

    // Safety check - ensure result is valid and in range
    if (isnan(result) || isinf(result)) {
        result = 0.5f; // Fallback to a safe value
    }

    return CLAMP(result, 0.0f, 1.0f);
}

// Rotate a point around another point - with improved safety
static void RotatePoint(
    float centerX, float centerY,
    float& x, float& y,
    float angleCos, float angleSin)
{
    // Validate angle components to avoid NaN or inf
    if (isnan(angleCos) || isinf(angleCos)) angleCos = 1.0f;
    if (isnan(angleSin) || isinf(angleSin)) angleSin = 0.0f;

    float dx = x - centerX;
    float dy = y - centerY;

    // Rotate the point
    float newX = dx * angleCos - dy * angleSin + centerX;
    float newY = dx * angleSin + dy * angleCos + centerY;

    // Validate results
    if (!isnan(newX) && !isinf(newX)) x = newX;
    if (!isnan(newY) && !isinf(newY)) y = newY;
}

// Check if a point is within a slice - with robust error checking
static bool IsPointInSlice(
    float x, float y,
    const SliceInfo* sliceInfoP)
{
    // Validate slice info
    if (!sliceInfoP) return false;

    // Basic validation of slice width
    if (sliceInfoP->sliceWidth <= 0.0f) return false;
    if (sliceInfoP->widthScale <= 0.0f) return false;

    // Rotate point to slice space - with safety validations
    float rotatedX = x;
    float rotatedY = y;

    RotatePoint(sliceInfoP->centerX, sliceInfoP->centerY,
        rotatedX, rotatedY,
        sliceInfoP->angleCos, -sliceInfoP->angleSin);

    // Calculate the center of the slice
    float sliceCenter = sliceInfoP->sliceStart + sliceInfoP->sliceWidth / 2.0f;

    // Validate slice center
    if (isnan(sliceCenter) || isinf(sliceCenter)) return false;

    // Calculate distance from the center of the slice
    float distFromCenter = fabsf(rotatedX - sliceCenter);

    // Calculate the maximum allowable distance based on scaled width
    float maxDist = (sliceInfoP->sliceWidth / 2.0f) * sliceInfoP->widthScale;

    // Validate max distance
    if (isnan(maxDist) || isinf(maxDist) || maxDist < 0) return false;

    // Return true if the point is within the slice
    return (distFromCenter <= maxDist);
}

// Get pixel color from the source at given coordinates with bounds checking
static PF_Pixel GetSourcePixel(
    float srcX, float srcY,
    const SliceInfo* sliceInfoP)
{
    PF_Pixel result = { 0, 0, 0, 0 }; // Default to transparent black

    // Validate parameters
    if (!sliceInfoP || !sliceInfoP->srcData) return result;
    if (sliceInfoP->width <= 0 || sliceInfoP->height <= 0 || sliceInfoP->rowbytes <= 0) return result;

    // Round to nearest pixel
    A_long pixelX = (A_long)(srcX + 0.5f);
    A_long pixelY = (A_long)(srcY + 0.5f);

    // Bounds checking
    if (pixelX >= 0 && pixelX < sliceInfoP->width &&
        pixelY >= 0 && pixelY < sliceInfoP->height) {

        // Calculate the offset in memory
        A_long srcOffset = pixelY * sliceInfoP->rowbytes;
        A_long pixOffset = pixelX * sizeof(PF_Pixel);

        // Safety check to prevent overflow
        if (srcOffset < 0 ||
            pixOffset < 0 ||
            srcOffset >= sliceInfoP->height * sliceInfoP->rowbytes ||
            pixOffset >= sliceInfoP->width * sizeof(PF_Pixel)) {
            return result;
        }

        A_long totalOffset = srcOffset + pixOffset;
        // Final safety check
        if (totalOffset < 0 ||
            totalOffset >= sliceInfoP->height * sliceInfoP->rowbytes ||
            totalOffset + sizeof(PF_Pixel) > sliceInfoP->height * sliceInfoP->rowbytes) {
            return result;
        }

        // Get the pixel
        PF_Pixel* srcPix = (PF_Pixel*)((char*)sliceInfoP->srcData + totalOffset);
        if (srcPix) {
            result = *srcPix;
        }
    }

    return result;
}

// Get 16-bit pixel color from source with strong bounds checking
static PF_Pixel16 GetSourcePixel16(
    float srcX, float srcY,
    const SliceInfo* sliceInfoP)
{
    PF_Pixel16 result = { 0, 0, 0, 0 }; // Default to transparent black

    // Validate parameters
    if (!sliceInfoP || !sliceInfoP->srcData) return result;
    if (sliceInfoP->width <= 0 || sliceInfoP->height <= 0 || sliceInfoP->rowbytes <= 0) return result;

    // Round to nearest pixel
    A_long pixelX = (A_long)(srcX + 0.5f);
    A_long pixelY = (A_long)(srcY + 0.5f);

    // Bounds checking
    if (pixelX >= 0 && pixelX < sliceInfoP->width &&
        pixelY >= 0 && pixelY < sliceInfoP->height) {

        // Calculate the offset in memory
        A_long srcOffset = pixelY * sliceInfoP->rowbytes;
        A_long pixOffset = pixelX * sizeof(PF_Pixel16);

        // Safety check to prevent overflow
        if (srcOffset < 0 ||
            pixOffset < 0 ||
            srcOffset >= sliceInfoP->height * sliceInfoP->rowbytes ||
            pixOffset >= sliceInfoP->width * sizeof(PF_Pixel16)) {
            return result;
        }

        A_long totalOffset = srcOffset + pixOffset;
        // Final safety check
        if (totalOffset < 0 ||
            totalOffset >= sliceInfoP->height * sliceInfoP->rowbytes ||
            totalOffset + sizeof(PF_Pixel16) > sliceInfoP->height * sliceInfoP->rowbytes) {
            return result;
        }

        // Get the pixel
        PF_Pixel16* srcPix = (PF_Pixel16*)((char*)sliceInfoP->srcData + totalOffset);
        if (srcPix) {
            result = *srcPix;
        }
    }

    return result;
}

// Function to process a given (x,y) pixel for slice effects (8-bit) - with error handling
static PF_Err
ProcessMultiSlice(
    void* refcon,
    A_long x,
    A_long y,
    PF_Pixel* in,
    PF_Pixel* out)
{
    PF_Err err = PF_Err_NONE;

    // Safety check for nullptr
    if (!refcon || !in || !out) {
        if (out) {
            out->alpha = 0;
            out->red = 0;
            out->green = 0;
            out->blue = 0;
        }
        return err;
    }

    SliceInfo* sliceInfosArray = (SliceInfo*)refcon;

    // Check for valid array
    if (!sliceInfosArray) {
        // Copy input to output as a failsafe
        *out = *in;
        return err;
    }

    // Get number of slices (with validation)
    A_long numSlices = sliceInfosArray[0].numSlices;
    if (numSlices <= 0 || numSlices > MULTISLICER_SLICES_MAX) { // Use constant for safety
        *out = *in;
        return err;
    }

    // Check if we're in identity mode (no shift)
    if (fabsf(sliceInfosArray[0].shiftAmount) < 0.001f && sliceInfosArray[0].widthScale > 0.999f) {
        // No shift and full width - just copy the original pixel
        *out = *in;
        return err;
    }

    // Start with transparent pixel
    out->alpha = 0;
    out->red = 0;
    out->green = 0;
    out->blue = 0;

    // Check if point is inside any slice
    bool foundSlice = false;
    for (int i = 0; i < numSlices; i++) {
        SliceInfo* currentSlice = &sliceInfosArray[i];

        // Validate current slice
        if (!currentSlice) continue;

        // Skip slices with zero width
        if (currentSlice->widthScale <= 0.001f) continue;

        if (IsPointInSlice(x, y, currentSlice)) {
            // Calculate the shift amount in pixels
            float offsetPixels = currentSlice->shiftAmount *
                currentSlice->shiftRandomFactor *
                currentSlice->shiftDirection;

            // Validate offset
            if (isnan(offsetPixels) || isinf(offsetPixels)) {
                offsetPixels = 0.0f;
            }

            // Check for extremely large shifts that could cause problems
            offsetPixels = CLAMP(offsetPixels, -1000.0f, 1000.0f);

            // Calculate source coordinates based on the shift
            float srcX = x - offsetPixels * currentSlice->angleSin;
            float srcY = y + offsetPixels * currentSlice->angleCos;

            // Get the source pixel (with validation)
            PF_Pixel srcPixel = GetSourcePixel(srcX, srcY, currentSlice);

            // Only use this pixel if it's not fully transparent
            if (srcPixel.alpha > 0) {
                *out = srcPixel;
                foundSlice = true;
                break; // Use the first valid slice we find
            }
        }
    }

    // If no slice was found, keep the pixel transparent
    return err;
}

// Function to process a given (x,y) pixel for slice effects (16-bit) - with error handling
static PF_Err
ProcessMultiSlice16(
    void* refcon,
    A_long x,
    A_long y,
    PF_Pixel16* in,
    PF_Pixel16* out)
{
    PF_Err err = PF_Err_NONE;

    // Safety check for nullptr
    if (!refcon || !in || !out) {
        if (out) {
            out->alpha = 0;
            out->red = 0;
            out->green = 0;
            out->blue = 0;
        }
        return err;
    }

    SliceInfo* sliceInfosArray = (SliceInfo*)refcon;

    // Check for valid array
    if (!sliceInfosArray) {
        // Copy input to output as a failsafe
        *out = *in;
        return err;
    }

    // Get number of slices (with validation)
    A_long numSlices = sliceInfosArray[0].numSlices;
    if (numSlices <= 0 || numSlices > MULTISLICER_SLICES_MAX) { // Use constant for safety
        *out = *in;
        return err;
    }

    // Check if we're in identity mode (no shift)
    if (fabsf(sliceInfosArray[0].shiftAmount) < 0.001f && sliceInfosArray[0].widthScale > 0.999f) {
        // No shift and full width - just copy the original pixel
        *out = *in;
        return err;
    }

    // Start with transparent pixel
    out->alpha = 0;
    out->red = 0;
    out->green = 0;
    out->blue = 0;

    // Check if point is inside any slice
    bool foundSlice = false;
    for (int i = 0; i < numSlices; i++) {
        SliceInfo* currentSlice = &sliceInfosArray[i];

        // Validate current slice
        if (!currentSlice) continue;

        // Skip slices with zero width
        if (currentSlice->widthScale <= 0.001f) continue;

        if (IsPointInSlice(x, y, currentSlice)) {
            // Calculate the shift amount in pixels
            float offsetPixels = currentSlice->shiftAmount *
                currentSlice->shiftRandomFactor *
                currentSlice->shiftDirection;

            // Validate offset
            if (isnan(offsetPixels) || isinf(offsetPixels)) {
                offsetPixels = 0.0f;
            }

            // Check for extremely large shifts that could cause problems
            offsetPixels = CLAMP(offsetPixels, -1000.0f, 1000.0f);

            // Calculate source coordinates based on the shift
            float srcX = x - offsetPixels * currentSlice->angleSin;
            float srcY = y + offsetPixels * currentSlice->angleCos;

            // Get the source pixel (with validation)
            PF_Pixel16 srcPixel = GetSourcePixel16(srcX, srcY, currentSlice);

            // Only use this pixel if it's not fully transparent
            if (srcPixel.alpha > 0) {
                *out = srcPixel;
                foundSlice = true;
                break; // Use the first valid slice we find
            }
        }
    }

    // If no slice was found, keep the pixel transparent
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

    // Validate input parameters
    if (!in_data || !out_data || !params || !output) {
        return PF_Err_INTERNAL_STRUCT_DAMAGED;
    }

    PF_EffectWorld* inputP = &params[MULTISLICER_INPUT]->u.ld;
    PF_EffectWorld* outputP = output;

    // Safety check for input and output worlds
    if (!inputP || !outputP) {
        return PF_Err_INTERNAL_STRUCT_DAMAGED;
    }

    try {
        // Extract parameters with validation
        A_long angle = params[MULTISLICER_ANGLE]->u.ad.value;
        float shiftRaw = params[MULTISLICER_SHIFT]->u.fs_d.value;
        float width = params[MULTISLICER_WIDTH]->u.fs_d.value / 100.0f;
        A_long numSlices = params[MULTISLICER_SLICES]->u.sd.value;
        A_long seed = params[MULTISLICER_SEED]->u.sd.value;

        // Validate parameters for safety
        numSlices = CLAMP(numSlices, MULTISLICER_SLICES_MIN, MULTISLICER_SLICES_MAX);
        width = CLAMP(width, 0.0f, 1.0f);
        seed = CLAMP(seed, MULTISLICER_SEED_MIN, MULTISLICER_SEED_MAX);
        shiftRaw = CLAMP(shiftRaw, MULTISLICER_SHIFT_MIN, MULTISLICER_SHIFT_MAX);

        // Determine shift direction based on sign
        float shiftDirection = (shiftRaw >= 0) ? 1.0f : -1.0f;

        // Calculate downsampling factors for composition display resolution
        float downsize_x = 1.0f;
        float downsize_y = 1.0f;

        // Safely extract downsample values
        if (in_data->downsample_x.num > 0 && in_data->downsample_x.den > 0) {
            downsize_x = static_cast<float>(in_data->downsample_x.den) /
                static_cast<float>(in_data->downsample_x.num);
        }

        if (in_data->downsample_y.num > 0 && in_data->downsample_y.den > 0) {
            downsize_y = static_cast<float>(in_data->downsample_y.den) /
                static_cast<float>(in_data->downsample_y.num);
        }

        // Validate downsampling factors
        downsize_x = CLAMP(downsize_x, 0.1f, 10.0f);
        downsize_y = CLAMP(downsize_y, 0.1f, 10.0f);

        // Adjust shift amount based on composition display resolution
        // Using minimum of x and y factors to maintain proportions
        float resolution_factor = MIN(downsize_x, downsize_y);
        resolution_factor = CLAMP(resolution_factor, 0.1f, 10.0f);  // Extra safety

        float shiftAmount = fabsf(shiftRaw) / resolution_factor;

        // Get image dimensions with safety checks
        A_long imageWidth = inputP->width;
        A_long imageHeight = inputP->height;

        // Check for unreasonably large dimensions
        if (imageWidth <= 0 || imageHeight <= 0 ||
            imageWidth > MULTISLICER_MAX_DIMENSION ||
            imageHeight > MULTISLICER_MAX_DIMENSION) {
            // Simply copy input to output
            ERR(suites.WorldTransformSuite1()->copy_hq(
                in_data->effect_ref,
                inputP,
                output,
                NULL,
                NULL));
            return PF_Err_NONE;
        }

        // Fast path for identity case (no effect)
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

        // Calculate center coordinates
        A_long centerX = imageWidth / 2;
        A_long centerY = imageHeight / 2;

        // Calculate angle in radians with validation
        float angleRad = (float)angle * PF_RAD_PER_DEGREE;

        // Validate angle
        if (isnan(angleRad) || isinf(angleRad)) {
            angleRad = 0.0f;
        }

        float angleCos = cosf(angleRad);
        float angleSin = sinf(angleRad);

        // Validate trig values for safety
        if (isnan(angleCos) || isinf(angleCos)) angleCos = 1.0f;
        if (isnan(angleSin) || isinf(angleSin)) angleSin = 0.0f;

        // Determine slice distribution along the angle
        float sliceLength = 2.0f * MAX(
            imageWidth * fabsf(angleCos) + imageHeight * fabsf(angleSin),
            imageHeight * fabsf(angleCos) + imageWidth * fabsf(angleSin)
        );

        // Ensure a minimum slice length
        sliceLength = MAX(sliceLength, 1.0f);

        float sliceSpacing = sliceLength / (numSlices > 0 ? numSlices : 1); // Avoid division by zero
        // Fixed handle allocation and locking code
        PF_Handle sliceInfosHandle = NULL;
        SliceInfo* sliceInfos = NULL;

        try {
            // Create a handle for our slice info array
            sliceInfosHandle = suites.HandleSuite1()->host_new_handle(numSlices * sizeof(SliceInfo));
            if (!sliceInfosHandle) {
                return PF_Err_OUT_OF_MEMORY;
            }

            // Cast the handle to get the pointer to our data - CORRECT APPROACH FOR AE SDK
            sliceInfos = reinterpret_cast<SliceInfo*>(*sliceInfosHandle);
            if (!sliceInfos) {
                suites.HandleSuite1()->host_dispose_handle(sliceInfosHandle);
                return PF_Err_OUT_OF_MEMORY;
            }

            // Initialize all memory to zero first
            memset(sliceInfos, 0, numSlices * sizeof(SliceInfo));

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
                sliceInfos[i].widthScale = width;
                sliceInfos[i].shiftAmount = shiftAmount;

                // Generate randomness for this slice with robust validation
                float randomPos = (GetRandomValue(seed, i + numSlices * 3) - 0.5f) * 0.3f; // -0.15 to 0.15
                float randomWidth = GetRandomValue(seed, i) * 0.5f + 0.75f;   // 0.75 to 1.25
                float randomDir = (GetRandomValue(seed, i + numSlices) > 0.5f) ? 1.0f : -1.0f;

                // Get a random factor for shift amount (0.5 to 2.5)
                float randomShiftFactor = GetRandomValue(seed, i + numSlices * 4) * 2.0f + 0.5f;

                // Validate random values
                randomPos = isnan(randomPos) || isinf(randomPos) ? 0.0f : randomPos;
                randomWidth = isnan(randomWidth) || isinf(randomWidth) ? 1.0f : randomWidth;
                randomShiftFactor = isnan(randomShiftFactor) || isinf(randomShiftFactor) ? 1.0f : randomShiftFactor;

                // Set slice properties with randomization
                sliceInfos[i].sliceStart = -sliceLength / 2.0f + i * sliceSpacing + randomPos * sliceSpacing;
                sliceInfos[i].sliceWidth = sliceSpacing * randomWidth;
                sliceInfos[i].shiftDirection = shiftDirection * randomDir;
                sliceInfos[i].shiftRandomFactor = randomShiftFactor;

                // Validate slice properties
                if (isnan(sliceInfos[i].sliceStart) || isinf(sliceInfos[i].sliceStart))
                    sliceInfos[i].sliceStart = 0.0f;

                if (isnan(sliceInfos[i].sliceWidth) || isinf(sliceInfos[i].sliceWidth) || sliceInfos[i].sliceWidth <= 0.0f)
                    sliceInfos[i].sliceWidth = sliceSpacing;
            }

            // Process the entire image
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
        }
        catch (...) {
            // Ensure we don't leak memory
            if (sliceInfosHandle) {
                suites.HandleSuite1()->host_dispose_handle(sliceInfosHandle);
            }
            throw; // Re-throw the exception
        }

        // Free the SliceInfo array
        if (sliceInfosHandle) {
            suites.HandleSuite1()->host_dispose_handle(sliceInfosHandle);
        }
    }
    catch (PF_Err& thrown_err) {
        err = thrown_err;
    }
    catch (...) {
        // For any other exception, fall back to copying input to output
        ERR(suites.WorldTransformSuite1()->copy_hq(
            in_data->effect_ref,
            inputP,
            output,
            NULL,
            NULL));
        err = PF_Err_INTERNAL_STRUCT_DAMAGED;
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

    try {
        result = PF_REGISTER_EFFECT_EXT2(
            inPtr,
            inPluginDataCallBackPtr,
            "MultiSlicer",            // Name
            "ADBE MultiSlicer",       // Match Name
            "Sample Plug-ins",        // Category
            AE_RESERVED_INFO,         // Reserved Info
            "EffectMain",             // Entry point
            "https://www.adobe.com"); // support URL
    }
    catch (...) {
        result = PF_Err_INTERNAL_STRUCT_DAMAGED;
    }

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

    // Basic validation of input parameters
    if (cmd == PF_Cmd_RENDER && (!in_data || !out_data || !params || !output)) {
        return PF_Err_INVALID_CALLBACK;
    }

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
    catch (...) {
        // Handle any unexpected exceptions
        err = PF_Err_INTERNAL_STRUCT_DAMAGED;
    }

    return err;
}