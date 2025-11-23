#define NOMINMAX
#include "MultiSlicer.h"

#include <vector>
#include <algorithm>
#include <cmath>
#include <thread>
#include <atomic>
#include <random>

// -----------------------------------------------------------------------------
// Constants & Helpers
// -----------------------------------------------------------------------------

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

template <typename T>
static inline T Clamp(T val, T minVal, T maxVal) {
    return std::max(minVal, std::min(val, maxVal));
}

// -----------------------------------------------------------------------------
// Pixel Traits
// -----------------------------------------------------------------------------

template <typename PixelT>
struct MultiSlicerPixelTraits;

template <>
struct MultiSlicerPixelTraits<PF_Pixel> {
    using ChannelType = A_u_char;
    static constexpr float MAX_VAL = 255.0f;
    static inline float ToFloat(ChannelType v) { return static_cast<float>(v); }
    static inline ChannelType FromFloat(float v) { return static_cast<ChannelType>(Clamp(v, 0.0f, MAX_VAL) + 0.5f); }
};

template <>
struct MultiSlicerPixelTraits<PF_Pixel16> {
    using ChannelType = A_u_short;
    static constexpr float MAX_VAL = 32768.0f;
    static inline float ToFloat(ChannelType v) { return static_cast<float>(v); }
    static inline ChannelType FromFloat(float v) { return static_cast<ChannelType>(Clamp(v, 0.0f, MAX_VAL) + 0.5f); }
};

template <>
struct MultiSlicerPixelTraits<PF_PixelFloat> {
    using ChannelType = PF_FpShort;
    static constexpr float MAX_VAL = 1.0f;
    static inline float ToFloat(ChannelType v) { return static_cast<float>(v); }
    static inline ChannelType FromFloat(float v) { return static_cast<ChannelType>(v); }
};

// -----------------------------------------------------------------------------
// Slice Structure
// -----------------------------------------------------------------------------

struct Slice {
    float start; // Start U coordinate
    float end;   // End U coordinate
    float shift; // Shift amount in U direction
};

// -----------------------------------------------------------------------------
// Sampling
// -----------------------------------------------------------------------------

template <typename Pixel>
static inline Pixel SampleBilinear(const A_u_char *base_ptr,
                                   A_long rowbytes,
                                   float xf,
                                   float yf,
                                   int width,
                                   int height)
{
    // Clamp coordinates to valid range
    xf = Clamp(xf, 0.0f, static_cast<float>(width - 1));
    yf = Clamp(yf, 0.0f, static_cast<float>(height - 1));

    const int x0 = static_cast<int>(xf);
    const int y0 = static_cast<int>(yf);
    const int x1 = std::min(x0 + 1, width - 1);
    const int y1 = std::min(y0 + 1, height - 1);

    const float tx = xf - static_cast<float>(x0);
    const float ty = yf - static_cast<float>(y0);

    const Pixel *row0 = reinterpret_cast<const Pixel *>(base_ptr + static_cast<A_long>(y0) * rowbytes);
    const Pixel *row1 = reinterpret_cast<const Pixel *>(base_ptr + static_cast<A_long>(y1) * rowbytes);

    const Pixel &p00 = row0[x0];
    const Pixel &p10 = row0[x1];
    const Pixel &p01 = row1[x0];
    const Pixel &p11 = row1[x1];

    auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };

    Pixel result;
    result.alpha = MultiSlicerPixelTraits<Pixel>::FromFloat(lerp(lerp(MultiSlicerPixelTraits<Pixel>::ToFloat(p00.alpha), MultiSlicerPixelTraits<Pixel>::ToFloat(p10.alpha), tx),
                                                      lerp(MultiSlicerPixelTraits<Pixel>::ToFloat(p01.alpha), MultiSlicerPixelTraits<Pixel>::ToFloat(p11.alpha), tx), ty));
    result.red = MultiSlicerPixelTraits<Pixel>::FromFloat(lerp(lerp(MultiSlicerPixelTraits<Pixel>::ToFloat(p00.red), MultiSlicerPixelTraits<Pixel>::ToFloat(p10.red), tx),
                                                    lerp(MultiSlicerPixelTraits<Pixel>::ToFloat(p01.red), MultiSlicerPixelTraits<Pixel>::ToFloat(p11.red), tx), ty));
    result.green = MultiSlicerPixelTraits<Pixel>::FromFloat(lerp(lerp(MultiSlicerPixelTraits<Pixel>::ToFloat(p00.green), MultiSlicerPixelTraits<Pixel>::ToFloat(p10.green), tx),
                                                      lerp(MultiSlicerPixelTraits<Pixel>::ToFloat(p01.green), MultiSlicerPixelTraits<Pixel>::ToFloat(p11.green), tx), ty));
    result.blue = MultiSlicerPixelTraits<Pixel>::FromFloat(lerp(lerp(MultiSlicerPixelTraits<Pixel>::ToFloat(p00.blue), MultiSlicerPixelTraits<Pixel>::ToFloat(p10.blue), tx),
                                                     lerp(MultiSlicerPixelTraits<Pixel>::ToFloat(p01.blue), MultiSlicerPixelTraits<Pixel>::ToFloat(p11.blue), tx), ty));
    return result;
}

// -----------------------------------------------------------------------------
// Rendering
// -----------------------------------------------------------------------------

template <typename Pixel>
static PF_Err RenderGeneric(PF_InData* in_data, PF_OutData* out_data, PF_ParamDef* params[], PF_LayerDef* output) {
    PF_EffectWorld* input = &params[MULTISLICER_INPUT]->u.ld;
    
    const int width = output->width;
    const int height = output->height;
    
    if (width <= 0 || height <= 0) return PF_Err_NONE;

    const A_u_char* input_base = reinterpret_cast<const A_u_char*>(input->data);
    A_u_char* output_base = reinterpret_cast<A_u_char*>(output->data);
    const A_long input_rowbytes = input->rowbytes;
    const A_long output_rowbytes = output->rowbytes;

    // Parameters
    float angle_deg = static_cast<float>(params[MULTISLICER_ANGLE]->u.ad.value >> 16);
    float shift_master = static_cast<float>(params[MULTISLICER_SHIFT]->u.fs_d.value);
    float width_master = static_cast<float>(params[MULTISLICER_WIDTH]->u.fs_d.value); // Not used? Maybe slice width randomness?
    int num_slices = params[MULTISLICER_SLICES]->u.sd.value;
    int seed = params[MULTISLICER_SEED]->u.sd.value;

    float angle_rad = angle_deg * (static_cast<float>(M_PI) / 180.0f);
    float cs = std::cos(angle_rad);
    float sn = std::sin(angle_rad);

    // Coordinate system:
    // u = x * cs + y * sn
    // v = -x * sn + y * cs
    // Slicing happens along U axis.

    // Calculate bounds in U
    // Corners: (0,0), (w,0), (0,h), (w,h)
    float u0 = 0 * cs + 0 * sn;
    float u1 = width * cs + 0 * sn;
    float u2 = 0 * cs + height * sn;
    float u3 = width * cs + height * sn;
    float min_u = std::min({u0, u1, u2, u3});
    float max_u = std::max({u0, u1, u2, u3});
    float total_u = max_u - min_u;

    // Generate Slices
    std::vector<Slice> slices;
    slices.reserve(num_slices);

    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> dist_width(0.5f, 1.5f); // Random width variation
    std::uniform_real_distribution<float> dist_shift(-1.0f, 1.0f); // Random shift direction/amount

    float current_u = min_u;
    float avg_width = total_u / num_slices;

    for (int i = 0; i < num_slices; ++i) {
        float w = avg_width;
        if (width_master > 0) {
             // Use width_master to control randomness? 
             // Or width_master is % of coverage?
             // Let's assume width_master controls randomness intensity.
             // 0 = equal width, 100 = highly random.
             float r = dist_width(rng);
             float factor = width_master / 100.0f;
             w = avg_width * (1.0f - factor + r * factor);
        }
        
        // Adjust last slice to fit exactly?
        // Better: generate all, then normalize.
        slices.push_back({0, w, 0}); // Temp start/end
    }

    // Normalize widths
    float sum_w = 0;
    for (const auto& s : slices) sum_w += s.end; // .end holds width temporarily
    float scale = total_u / sum_w;
    
    current_u = min_u;
    for (auto& s : slices) {
        float w = s.end * scale;
        s.start = current_u;
        s.end = current_u + w;
        current_u += w;
        
        // Random shift
        float r_shift = dist_shift(rng);
        s.shift = r_shift * shift_master;
    }
    // Ensure last slice covers everything
    if (!slices.empty()) slices.back().end = max_u + 1.0f; // Padding

    // Multi-threading
    int num_threads = std::max(1u, std::thread::hardware_concurrency());
    std::vector<std::thread> threads;

    auto process_rows = [&](int start_y, int end_y) {
        for (int y = start_y; y < end_y; ++y) {
            Pixel* out_row = reinterpret_cast<Pixel*>(output_base + y * output_rowbytes);
            
            // Start of row in U, V
            float u = 0 * cs + y * sn;
            float v = -0 * sn + y * cs;
            
            // Increments per pixel
            float du = cs;
            float dv = -sn;

            // Find initial slice
            // Binary search or linear scan? Linear scan from 0 is fine if we track it.
            // Or just find once.
            int slice_idx = 0;
            // Optimization: if du > 0, we move forward. If du < 0, backward.
            // Let's just find the slice for the first pixel.
            
            // Simple linear search for start
            for (int i = 0; i < num_slices; ++i) {
                if (u >= slices[i].start && u < slices[i].end) {
                    slice_idx = i;
                    break;
                }
            }
            // Handle out of bounds (u < min_u or u > max_u)
            if (u < slices[0].start) slice_idx = 0;
            if (u >= slices.back().end) slice_idx = num_slices - 1;

            for (int x = 0; x < width; ++x) {
                // Update slice index
                // While u is outside current slice, move.
                // Note: u can decrease if cs < 0.
                
                while (slice_idx < num_slices - 1 && u >= slices[slice_idx].end) {
                    slice_idx++;
                }
                while (slice_idx > 0 && u < slices[slice_idx].start) {
                    slice_idx--;
                }
                
                // Apply shift
                float shift = slices[slice_idx].shift;
                float u_shifted = u - shift;
                float v_shifted = v; // No shift in V? Or shift along slice direction? Usually along slice (U).
                
                // Map back to X, Y
                // x = u*cs - v*sn
                // y = u*sn + v*cs
                float src_x = u_shifted * cs - v_shifted * sn;
                float src_y = u_shifted * sn + v_shifted * cs;
                
                out_row[x] = SampleBilinear<Pixel>(input_base, input_rowbytes, src_x, src_y, input->width, input->height);
                
                u += du;
                v += dv;
            }
        }
    };

    int rows_per_thread = (height + num_threads - 1) / num_threads;
    for (int i = 0; i < num_threads; ++i) {
        int start = i * rows_per_thread;
        int end = std::min(start + rows_per_thread, height);
        if (start < end) threads.emplace_back(process_rows, start, end);
    }
    for (auto& t : threads) t.join();

    return PF_Err_NONE;
}

static PF_Err Render(PF_InData* in_data, PF_OutData* out_data, PF_ParamDef* params[], PF_LayerDef* output) {
    int bpp = (output->width > 0) ? (output->rowbytes / output->width) : 0;
    if (bpp == sizeof(PF_PixelFloat)) {
        return RenderGeneric<PF_PixelFloat>(in_data, out_data, params, output);
    } else if (bpp == sizeof(PF_Pixel16)) {
        return RenderGeneric<PF_Pixel16>(in_data, out_data, params, output);
    } else {
        return RenderGeneric<PF_Pixel>(in_data, out_data, params, output);
    }
}

static PF_Err
About(PF_InData* in_data, PF_OutData* out_data, PF_ParamDef* params[], PF_LayerDef* output)
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
GlobalSetup(PF_InData* in_data, PF_OutData* out_data, PF_ParamDef* params[], PF_LayerDef* output)
{
    out_data->my_version = PF_VERSION(MAJOR_VERSION, MINOR_VERSION, BUG_VERSION, STAGE_VERSION, BUILD_VERSION);
    out_data->out_flags = PF_OutFlag_DEEP_COLOR_AWARE | PF_OutFlag_PIX_INDEPENDENT;
    out_data->out_flags2 = PF_OutFlag2_FLOAT_COLOR_AWARE | PF_OutFlag2_SUPPORTS_SMART_RENDER | PF_OutFlag2_SUPPORTS_THREADED_RENDERING;
    return PF_Err_NONE;
}

static void UnionLRect(const PF_LRect* src, PF_LRect* dst) {
    if (src->left < dst->left) dst->left = src->left;
    if (src->top < dst->top) dst->top = src->top;
    if (src->right > dst->right) dst->right = src->right;
    if (src->bottom > dst->bottom) dst->bottom = src->bottom;
}

static PF_Err
PreRender(
    PF_InData* in_data,
    PF_OutData* out_data,
    void* extraP)
{
    PF_Err err = PF_Err_NONE;
    PF_PreRenderExtra_Local* extra = (PF_PreRenderExtra_Local*)extraP;
    PF_RenderRequest req = extra->input->output_request;
    PF_CheckoutResult in_result;

    // Checkout input
    ERR(extra->cb->checkout_layer(in_data->effect_ref,
        MULTISLICER_INPUT,
        MULTISLICER_INPUT,
        &req,
        in_data->current_time,
        in_data->time_step,
        in_data->time_scale,
        &in_result));

    // Set result rects
    if (!err) {
        UnionLRect(&in_result.result_rect, &extra->output->result_rect);
        UnionLRect(&in_result.max_result_rect, &extra->output->max_result_rect);
    }

    return err;
}

static PF_Err
SmartRender(
    PF_InData* in_data,
    PF_OutData* out_data,
    void* extraP)
{
    PF_Err err = PF_Err_NONE;
    PF_SmartRenderExtra_Local* extra = (PF_SmartRenderExtra_Local*)extraP;
    PF_EffectWorld* input_world = NULL;
    PF_EffectWorld* output_world = NULL;
    
    AEGP_SuiteHandler suites(in_data->pica_basicP);
    PF_WorldSuite2* wsP = NULL;
    ERR(suites.SPBasicSuite()->AcquireSuite(kPFWorldSuite, kPFWorldSuiteVersion2, (const void**)&wsP));

    if (!err) {
        // Checkout input/output
        ERR((extra->cb->checkout_layer_pixels(in_data->effect_ref, MULTISLICER_INPUT, &input_world)));
        ERR(extra->cb->checkout_output(in_data->effect_ref, &output_world));
    }

    if (!err && input_world && output_world) {
        // Checkout parameters
        PF_ParamDef p[MULTISLICER_NUM_PARAMS];
        PF_ParamDef* pp[MULTISLICER_NUM_PARAMS];
        
        // Input
        AEFX_CLR_STRUCT(p[MULTISLICER_INPUT]);
        p[MULTISLICER_INPUT].u.ld = *input_world;
        pp[MULTISLICER_INPUT] = &p[MULTISLICER_INPUT];

        // Params
        for (int i = 1; i < MULTISLICER_NUM_PARAMS; ++i) {
            PF_Checkout_Value(in_data, out_data, i, in_data->current_time, in_data->time_step, in_data->time_scale, &p[i]);
            pp[i] = &p[i];
        }

        // Call Render
        int bpp = (output_world->width > 0) ? (output_world->rowbytes / output_world->width) : 0;
        if (bpp == sizeof(PF_PixelFloat)) {
            err = RenderGeneric<PF_PixelFloat>(in_data, out_data, pp, output_world);
        } else if (bpp == sizeof(PF_Pixel16)) {
            err = RenderGeneric<PF_Pixel16>(in_data, out_data, pp, output_world);
        } else {
            err = RenderGeneric<PF_Pixel>(in_data, out_data, pp, output_world);
        }
        
        // Checkin params
        for (int i = 1; i < MULTISLICER_NUM_PARAMS; ++i) {
            PF_Checkin_Param(in_data, out_data, i, &p[i]);
        }
    }
    
    if (wsP) suites.SPBasicSuite()->ReleaseSuite(kPFWorldSuite, kPFWorldSuiteVersion2);

    return err;
}

extern "C" DllExport
PF_Err PluginDataEntryFunction2(PF_PluginDataPtr inPtr,
    PF_PluginDataCB2 inPluginDataCallBackPtr,
    SPBasicSuite * inSPBasicSuitePtr,
    const char* inHostName,
    const char* inHostVersion)
{
    PF_Err result = PF_Err_INVALID_CALLBACK;
    result = PF_REGISTER_EFFECT_EXT2(
        inPtr,
        inPluginDataCallBackPtr,
        "MultiSlicer", // Name
        "361do MultiSlicer", // Match Name
        "361do_plugins", // Category
        AE_RESERVED_INFO,
        "EffectMain",
        "https://github.com/rebuildup/Ae_MultiSlicer");
    return result;
}

extern "C" DllExport
PF_Err EffectMain(PF_Cmd cmd,
    PF_InData * in_data,
    PF_OutData * out_data,
    PF_ParamDef * params[],
    PF_LayerDef * output,
    void* extra)
{
    PF_Err err = PF_Err_NONE;
    try {
        switch (cmd) {
        case PF_Cmd_ABOUT: err = About(in_data, out_data, params, output); break;
        case PF_Cmd_GLOBAL_SETUP: err = GlobalSetup(in_data, out_data, params, output); break;
        case PF_Cmd_PARAMS_SETUP: err = ParamsSetup(in_data, out_data, params, output); break;
        case PF_Cmd_RENDER: err = Render(in_data, out_data, params, output); break;
        case PF_Cmd_SMART_PRE_RENDER: err = PreRender(in_data, out_data, extra); break;
        case PF_Cmd_SMART_RENDER: err = SmartRender(in_data, out_data, extra); break;
        default: break;
        }
    }
    catch (...) {
        err = PF_Err_INTERNAL_STRUCT_DAMAGED;
    }
    return err;
}
