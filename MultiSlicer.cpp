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
#include <math.h>

// π の定義 (M_PI が定義されていない場合のため)
// Define π (in case M_PI is not defined)
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// 透明とみなす最小アルファ値
// Minimum alpha value considered non-transparent
#define ALPHA_THRESHOLD 5

// シード値から擬似乱数系列を生成するための構造体
// Structure for generating a pseudo-random sequence from a seed value
typedef struct {
    A_long seed;
    A_long divCount;
    A_long height;
    A_long width;
    A_long topEdge;      // 上端（全体）
    A_long bottomEdge;   // 下端（全体）
    double* positions;   // 分割位置を保存する配列（0〜1の値）
} SliceInfo;

// 擬似乱数生成関数（より複雑な要素を加えて、異なるシードが異なる結果を作る）
// Pseudo-random number generation function (with more complexity to ensure different seeds produce different results)
static double generateRandom(A_long seed, A_long index) {
    // シード値にバリエーションを持たせるために複数の変換を適用
    // Apply multiple transformations to add variation to the seed value
    double value1 = sin((seed * 0.87 + index * 24.37) * M_PI) * M_PI;
    double value2 = cos((seed * 1.23 + index * 18.53) * M_PI) * M_PI;
    double value3 = sin((seed * 2.31 + index * 12.91 + value1 * 0.1) * M_PI) * M_PI;

    // 複数の乱数値を組み合わせる
    // Combine multiple random values
    double finalValue = (value1 + value2 + value3) / 3.0;

    // 0〜1の範囲に正規化
    // Normalize to 0-1 range
    return fabs(finalValue - floor(finalValue));
}

// レイヤーの透明でない領域の上端と下端を検出する関数
// Function to detect top and bottom edges of non-transparent areas in the layer
static PF_Err detectLayerEdges(PF_EffectWorld* inputP, SliceInfo* sliceInfoP) {
    PF_Err err = PF_Err_NONE;

    // 初期値設定（上端は最大値、下端は最小値）
    // Initialize values (top edge to maximum, bottom edge to minimum)
    sliceInfoP->topEdge = sliceInfoP->height; // 初期値は高さの最大値
    sliceInfoP->bottomEdge = 0;               // 初期値は0

    // 8bpcと16bpcのケースを処理
    // Handle 8bpc and 16bpc cases
    if (PF_WORLD_IS_DEEP(inputP)) {
        // 16bpcの場合
        // For 16bpc
        PF_Pixel16* pixelP;

        for (A_long y = 0; y < sliceInfoP->height; y++) {
            pixelP = (PF_Pixel16*)((char*)inputP->data + y * inputP->rowbytes);

            for (A_long x = 0; x < sliceInfoP->width; x++) {
                if (pixelP[x].alpha > ALPHA_THRESHOLD) {
                    // 上端は最小のY値
                    // Top edge is the minimum Y value
                    if (y < sliceInfoP->topEdge) {
                        sliceInfoP->topEdge = y;
                    }

                    // 下端は最大のY値
                    // Bottom edge is the maximum Y value
                    if (y > sliceInfoP->bottomEdge) {
                        sliceInfoP->bottomEdge = y;
                    }
                }
            }
        }
    }
    else {
        // 8bpcの場合
        // For 8bpc
        PF_Pixel8* pixelP;

        for (A_long y = 0; y < sliceInfoP->height; y++) {
            pixelP = (PF_Pixel8*)((char*)inputP->data + y * inputP->rowbytes);

            for (A_long x = 0; x < sliceInfoP->width; x++) {
                if (pixelP[x].alpha > ALPHA_THRESHOLD) {
                    // 上端は最小のY値
                    // Top edge is the minimum Y value
                    if (y < sliceInfoP->topEdge) {
                        sliceInfoP->topEdge = y;
                    }

                    // 下端は最大のY値
                    // Bottom edge is the maximum Y value
                    if (y > sliceInfoP->bottomEdge) {
                        sliceInfoP->bottomEdge = y;
                    }
                }
            }
        }
    }

    // 透明な層の場合のエッジを修正
    // Adjust edges for completely transparent layers
    if (sliceInfoP->topEdge == sliceInfoP->height) {
        sliceInfoP->topEdge = 0;
        sliceInfoP->bottomEdge = sliceInfoP->height - 1;
    }

    return err;
}

// 分割位置を計算する関数
// Function to calculate slice positions
static PF_Err calculateSlicePositions(SliceInfo* sliceInfoP) {
    PF_Err err = PF_Err_NONE;

    // メモリ確保
    // Allocate memory
    sliceInfoP->positions = (double*)malloc(sliceInfoP->divCount * sizeof(double));
    if (!sliceInfoP->positions) {
        return PF_Err_OUT_OF_MEMORY;
    }

    // 各分割位置を計算する
    // Calculate each slice position
    for (A_long i = 0; i < sliceInfoP->divCount; i++) {
        // シード値と位置から一意の乱数を生成
        // Generate unique random number from seed and position
        double random = generateRandom(sliceInfoP->seed, i);

        // 実数値として保存（0〜1の範囲）
        // Save as real value (0-1 range)
        sliceInfoP->positions[i] = random;
    }

    return err;
}

// 8bpcピクセル処理関数
// 8bpc pixel processing function
static PF_Err
SliceFunc8(
    void* refcon,
    A_long      xL,
    A_long      yL,
    PF_Pixel8* inP,
    PF_Pixel8* outP)
{
    PF_Err      err = PF_Err_NONE;
    SliceInfo* sliceInfoP = (SliceInfo*)refcon;

    if (!sliceInfoP) return err;

    // 参照線を描画するかどうかの判定
    // Determine whether to draw a reference line
    A_Boolean drawLine = FALSE;

    // 現在のピクセルが透明でない場合のみ処理
    // Only process if current pixel is not transparent
    if (inP->alpha > ALPHA_THRESHOLD) {
        // 層の非透明部分の範囲内かチェック
        // Check if within the range of non-transparent part of the layer
        if (yL >= sliceInfoP->topEdge && yL <= sliceInfoP->bottomEdge) {
            // 層全体の高さを基準にした位置
            // Position based on the entire height of the layer
            A_long edgeHeight = sliceInfoP->bottomEdge - sliceInfoP->topEdge + 1;

            // 各分割位置において、スケーリングされたY座標が一致するかをチェック
            // Check if scaled Y coordinate matches any slice position
            for (A_long i = 0; i < sliceInfoP->divCount; i++) {
                // 位置をピクセル位置に変換（上端〜下端の間）
                // Convert position to pixel position (between top and bottom edge)
                A_long lineY = sliceInfoP->topEdge + (A_long)(sliceInfoP->positions[i] * edgeHeight);

                // 現在の座標がライン上にあるかどうか
                // Check if current coordinate is on the line
                if (lineY == yL) {
                    drawLine = TRUE;
                    break;
                }
            }
        }
    }

    // テスト用に白い線を描画
    // Draw white line for testing
    if (drawLine) {
        outP->alpha = 255;
        outP->red = 255;
        outP->green = 255;
        outP->blue = 255;
    }
    else {
        // 線以外の部分はそのまま入力をコピー
        // Copy input for areas other than lines
        *outP = *inP;
    }

    return err;
}

// 16bpcピクセル処理関数
// 16bpc pixel processing function
static PF_Err
SliceFunc16(
    void* refcon,
    A_long      xL,
    A_long      yL,
    PF_Pixel16* inP,
    PF_Pixel16* outP)
{
    PF_Err      err = PF_Err_NONE;
    SliceInfo* sliceInfoP = (SliceInfo*)refcon;

    if (!sliceInfoP) return err;

    // 参照線を描画するかどうかの判定
    // Determine whether to draw a reference line
    A_Boolean drawLine = FALSE;

    // 現在のピクセルが透明でない場合のみ処理
    // Only process if current pixel is not transparent
    if (inP->alpha > ALPHA_THRESHOLD) {
        // 層の非透明部分の範囲内かチェック
        // Check if within the range of non-transparent part of the layer
        if (yL >= sliceInfoP->topEdge && yL <= sliceInfoP->bottomEdge) {
            // 層全体の高さを基準にした位置
            // Position based on the entire height of the layer
            A_long edgeHeight = sliceInfoP->bottomEdge - sliceInfoP->topEdge + 1;

            // 各分割位置において、スケーリングされたY座標が一致するかをチェック
            // Check if scaled Y coordinate matches any slice position
            for (A_long i = 0; i < sliceInfoP->divCount; i++) {
                // 位置をピクセル位置に変換（上端〜下端の間）
                // Convert position to pixel position (between top and bottom edge)
                A_long lineY = sliceInfoP->topEdge + (A_long)(sliceInfoP->positions[i] * edgeHeight);

                // 現在の座標がライン上にあるかどうか
                // Check if current coordinate is on the line
                if (lineY == yL) {
                    drawLine = TRUE;
                    break;
                }
            }
        }
    }

    // テスト用に白い線を描画
    // Draw white line for testing
    if (drawLine) {
        outP->alpha = PF_MAX_CHAN16;
        outP->red = PF_MAX_CHAN16;
        outP->green = PF_MAX_CHAN16;
        outP->blue = PF_MAX_CHAN16;
    }
    else {
        // 線以外の部分はそのまま入力をコピー
        // Copy input for areas other than lines
        *outP = *inP;
    }

    return err;
}

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

    out_data->out_flags = PF_OutFlag_DEEP_COLOR_AWARE;	// just 16bpc, not 32bpc

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

    // シフト量を定義する
    // Define Shift Amount parameter
    PF_ADD_FLOAT_SLIDERX(STR(StrID_Shift_Param_Name),
        SLICE_SHIFT_MIN,
        SLICE_SHIFT_MAX,
        SLICE_SHIFT_MIN,
        SLICE_SHIFT_MAX,
        SLICE_SHIFT_DFLT,
        PF_Precision_TENTHS,
        0,
        0,
        SHIFT_DISK_ID);

    AEFX_CLR_STRUCT(def);

    // 角度を定義する
    // Define Angle parameter
    PF_ADD_ANGLE(STR(StrID_Angle_Param_Name),
        SLICE_ANGLE_DFLT,
        ANGLE_DISK_ID);

    AEFX_CLR_STRUCT(def);

    // 幅を定義する
    // Define Width parameter (as percentage)
    PF_ADD_PERCENT(STR(StrID_Width_Param_Name),
        SLICE_WIDTH_DFLT,
        WIDTH_DISK_ID);

    AEFX_CLR_STRUCT(def);

    // 分割数を定義する
    // Define Division Count parameter
    PF_ADD_SLIDER(STR(StrID_DivCount_Param_Name),
        SLICE_DIV_COUNT_MIN,
        SLICE_DIV_COUNT_MAX,
        SLICE_DIV_COUNT_MIN,
        SLICE_DIV_COUNT_MAX,
        SLICE_DIV_COUNT_DFLT,
        DIV_COUNT_DISK_ID);

    AEFX_CLR_STRUCT(def);

    // シード値を定義する
    // Define Seed parameter
    PF_ADD_SLIDER(STR(StrID_Seed_Param_Name),
        SLICE_SEED_MIN,
        SLICE_SEED_MAX,
        SLICE_SEED_MIN,
        SLICE_SEED_MAX,
        SLICE_SEED_DFLT,
        SEED_DISK_ID);

    out_data->num_params = SKELETON_NUM_PARAMS;

    return err;
}

// ユーザーパラメータ変更の処理関数
// Function to handle user parameter changes
static PF_Err
UserChangedParam(
    PF_InData* in_data,
    PF_OutData* out_data,
    PF_ParamDef* params[],
    PF_LayerDef* output,
    const PF_UserChangedParamExtra* which_hitP)
{
    PF_Err err = PF_Err_NONE;

    // 変更されたパラメータがDivision CountまたはSeedの場合
    // If the changed parameter is Division Count or Seed
    if (which_hitP->param_index == SLICE_DIV_COUNT || which_hitP->param_index == SLICE_SEED) {
        PF_ParamDef param_copy;
        AEFX_CLR_STRUCT(param_copy);

        // パラメータのコピーを取得
        // Get a copy of the parameter
        ERR(PF_CHECKOUT_PARAM(in_data, which_hitP->param_index, in_data->current_time, in_data->time_step, in_data->time_scale, &param_copy));

        if (!err) {
            // 現在の値を取得
            // Get current value
            A_long current_value = param_copy.u.sd.value;

            // 10の倍数に丸める
            // Round to multiple of 10
            A_long rounded_value = ((current_value + 5) / 10) * 10;

            // 値の範囲を確認
            // Check value range
            if (rounded_value < param_copy.u.sd.valid_min) {
                rounded_value = param_copy.u.sd.valid_min;
            }
            else if (rounded_value > param_copy.u.sd.valid_max) {
                rounded_value = param_copy.u.sd.valid_max;
            }

            // 値が変わった場合は更新
            // Update if value changed
            if (rounded_value != current_value) {
                param_copy.u.sd.value = rounded_value;
                ERR(PF_CHECKIN_PARAM(in_data, &param_copy));
            }
            else {
                ERR(PF_CHECKIN_PARAM(in_data, 0));
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
    SliceInfo           sliceInfo;
    A_long              linesL = 0;

    // パラメータを取得
    // Get parameters
    A_long divCount = params[SLICE_DIV_COUNT]->u.sd.value;
    A_long seed = params[SLICE_SEED]->u.sd.value;

    // SliceInfoを初期化
    // Initialize SliceInfo
    sliceInfo.seed = seed;
    sliceInfo.divCount = divCount;
    sliceInfo.positions = NULL;
    sliceInfo.width = output->width;
    sliceInfo.height = output->height;

    // レイヤーの透明でない領域のエッジを検出
    // Detect edges of non-transparent areas in the layer
    ERR(detectLayerEdges(&params[SKELETON_INPUT]->u.ld, &sliceInfo));

    // 分割位置を計算
    // Calculate slice positions
    if (!err) {
        ERR(calculateSlicePositions(&sliceInfo));
    }

    // レンダリング
    // Rendering
    if (!err) {
        linesL = output->extent_hint.bottom - output->extent_hint.top;

        if (PF_WORLD_IS_DEEP(output)) {
            ERR(suites.Iterate16Suite2()->iterate(
                in_data,
                0,                              // progress base
                linesL,                         // progress final
                &params[SKELETON_INPUT]->u.ld,  // src 
                NULL,                           // area - null for all pixels
                (void*)&sliceInfo,              // refcon - custom data pointer
                SliceFunc16,                    // pixel function pointer
                output));
        }
        else {
            ERR(suites.Iterate8Suite2()->iterate(
                in_data,
                0,                              // progress base
                linesL,                         // progress final
                &params[SKELETON_INPUT]->u.ld,  // src 
                NULL,                           // area - null for all pixels
                (void*)&sliceInfo,              // refcon - custom data pointer
                SliceFunc8,                     // pixel function pointer
                output));
        }
    }

    // メモリ解放
    // Free memory
    if (sliceInfo.positions) {
        free(sliceInfo.positions);
        sliceInfo.positions = NULL;
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

        case PF_Cmd_USER_CHANGED_PARAM:
            err = UserChangedParam(
                in_data,
                out_data,
                params,
                output,
                (const PF_UserChangedParamExtra*)extra);
            break;
        }
    }
    catch (PF_Err& thrown_err) {
        err = thrown_err;
    }
    return err;
}