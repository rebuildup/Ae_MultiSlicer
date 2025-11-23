#include "MultiSlicer.h"

#include <vector>
#include <algorithm>
#include <cstring>
#include <cmath>

static PF_Err 
About (	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	AEGP_SuiteHandler suites(in_data->pica_basicP);
	
	suites.ANSICallbacksSuite1()->sprintf(	out_data->return_msg,
											"%s v%d.%d\r%s",
											"MultiSlicer", 
											MAJOR_VERSION, 
											MINOR_VERSION, 
											"Multi-slice effect");
	return PF_Err_NONE;
}

static PF_Err 
GlobalSetup (	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	out_data->my_version = PF_VERSION(	MAJOR_VERSION, 
										MINOR_VERSION,
										BUG_VERSION, 
										STAGE_VERSION, 
										BUILD_VERSION);
	
	out_data->out_flags =  PF_OutFlag_DEEP_COLOR_AWARE;
	out_data->out_flags2 = PF_OutFlag2_SUPPORTS_THREADED_RENDERING;
	
	return PF_Err_NONE;
}

static PF_Err 
ParamsSetup (	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	PF_Err		err		= PF_Err_NONE;
	PF_ParamDef	def;	

	AEFX_CLR_STRUCT(def);

	PF_ADD_ANGLE("Angle", 0, ANGLE_DISK_ID);

	AEFX_CLR_STRUCT(def);

	PF_ADD_FLOAT_SLIDERX(
		"Shift",
		-100,
		100,
		-100,
		100,
		0,
		PF_Precision_TENTHS,
		0,
		0,
		SHIFT_DISK_ID);

	AEFX_CLR_STRUCT(def);

	PF_ADD_FLOAT_SLIDERX(
		"Width",
		MULTISLICER_WIDTH_MIN,
		MULTISLICER_WIDTH_MAX,
		MULTISLICER_WIDTH_MIN,
		MULTISLICER_WIDTH_MAX,
		MULTISLICER_WIDTH_DFLT,
		PF_Precision_INTEGER,
		0,
		0,
		WIDTH_DISK_ID);

	AEFX_CLR_STRUCT(def);

	PF_ADD_SLIDER(
		"Slices",
		MULTISLICER_SLICES_MIN,
		MULTISLICER_SLICES_MAX,
		MULTISLICER_SLICES_MIN,
		MULTISLICER_SLICES_MAX,
		MULTISLICER_SLICES_DFLT,
		SLICES_DISK_ID);

	AEFX_CLR_STRUCT(def);

	PF_ADD_SLIDER(
		"Random Seed",
		MULTISLICER_SEED_MIN,
		MULTISLICER_SEED_MAX,
		MULTISLICER_SEED_MIN,
		MULTISLICER_SEED_MAX,
		MULTISLICER_SEED_DFLT,
		SEED_DISK_ID);

	out_data->num_params = MULTISLICER_NUM_PARAMS;

	return err;
}

static PF_Err
Render (
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	PF_Err				err		= PF_Err_NONE;
	AEGP_SuiteHandler	suites(in_data->pica_basicP);

	PF_EffectWorld *input = &params[MULTISLICER_INPUT]->u.ld;
	
	// Parameters
	double angle = (params[MULTISLICER_ANGLE]->u.ad.value / 65536.0) * 3.14159265358979323846 / 180.0; // Convert to radians
	double shift = params[MULTISLICER_SHIFT]->u.fs_d.value;
	double width_param = params[MULTISLICER_WIDTH]->u.fs_d.value;
	int slices = params[MULTISLICER_SLICES]->u.sd.value;
	int seed = params[MULTISLICER_SEED]->u.sd.value;
	
	// Copy input to output first
	ERR(PF_COPY(input, output, NULL, NULL));
	if (err) return err;
	
	A_long width = output->width;
	A_long height = output->height;
	A_long rowbytes = output->rowbytes;
	int pixel_size = PF_WORLD_IS_DEEP(output) ? 8 : 4;
	
	if (slices < 1) return err;
	
	// Calculate slice direction from angle
	double cos_a = cos(angle);
	double sin_a = sin(angle);
	
	// Simple LCG random number generator
	auto lcg_rand = [](int &state) -> double {
		state = (state * 1103515245 + 12345) & 0x7fffffff;
		return (double)state / 0x7fffffff;
	};
	
	// Process slices along angle direction
	double slice_spacing = (double)height / (double)slices;
	
	for (int i = 0; i < slices; i++) {
		int rng_state = seed + i;
		double random_shift = lcg_rand(rng_state) * shift * 2.0 - shift;
		double random_width = width_param * (0.5 + lcg_rand(rng_state) * 0.5);
		
		int start_y = (int)(i * slice_spacing);
		int end_y = (std::min)((int)((i + 1) * slice_spacing), (int)height);
		
		int shift_px = (int)random_shift;
		
		for (int y = start_y; y < end_y; y++) {
			if (y < 0 || y >= height) continue;
			
			char *row_ptr = (char*)output->data + y * rowbytes;
			std::vector<char> temp_row(rowbytes);
			memcpy(temp_row.data(), row_ptr, rowbytes);
			
			for (int x = 0; x < width; x++) {
				int src_x = x - shift_px;
				while (src_x < 0) src_x += width;
				while (src_x >= width) src_x -= width;
				
				memcpy(row_ptr + x * pixel_size, temp_row.data() + src_x * pixel_size, pixel_size);
			}
		}
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
		"361do MultiSlicer", // Match Name
		"361do_plugins", // Category
		AE_RESERVED_INFO,
		"EffectMain",
		"https://github.com/rebuildup/Ae_MultiSlicer");

	return result;
}


DllExport	
PF_Err 
EffectMain(
	PF_Cmd			cmd,
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output,
	void			*extra)
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
				err = GlobalSetup(	in_data,
									out_data,
									params,
									output);
				break;
				
			case PF_Cmd_PARAMS_SETUP:
				err = ParamsSetup(	in_data,
									out_data,
									params,
									output);
				break;
				
			case PF_Cmd_RENDER:
				err = Render(	in_data,
								out_data,
								params,
								output);
				break;
		}
	}
	catch(PF_Err &thrown_err){
		err = thrown_err;
	}
	return err;
}
