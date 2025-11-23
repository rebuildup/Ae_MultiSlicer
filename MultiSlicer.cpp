#include "MultiSlicer.h"

#include <vector>
#include <algorithm>
#include <cstring>

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
	int slices = params[MULTISLICER_SLICES]->u.sd.value;
	double shift = params[MULTISLICER_SHIFT]->u.fs_d.value;
	
	// Copy input to output first
	PF_COPY(input, output, NULL, NULL);
	
	// Simple slice logic
	A_long width = output->width;
	A_long height = output->height;
	A_long rowbytes = output->rowbytes;
	
	if (slices < 1) slices = 1;
	int slice_height = height / slices;
	
	// Shift odd slices
	for (int i = 0; i < slices; i++) {
		if (i % 2 == 1) {
			int start_y = i * slice_height;
			int end_y = std::min((int)height, (i + 1) * slice_height);
			int shift_px = (int)shift;
			
			for (int y = start_y; y < end_y; y++) {
				// Shift row
				// Simple implementation: copy row to temp, write back with offset
				char *row_ptr = (char*)output->data + y * rowbytes;
				std::vector<char> temp_row(rowbytes);
				memcpy(temp_row.data(), row_ptr, rowbytes);
				
				int pixel_size = PF_WORLD_IS_DEEP(output) ? 8 : 4; // ARGB 8bit=4, 16bit=8
				int width_bytes = width * pixel_size;
				
				// Clamp shift
				int shift_bytes = shift_px * pixel_size;
				// Handle wrap around or clamp? Let's clamp/fill black for simplicity or wrap
				// Wrap:
				for (int x = 0; x < width; x++) {
					int src_x = (x - shift_px) % width;
					if (src_x < 0) src_x += width;
					
					memcpy(row_ptr + x * pixel_size, temp_row.data() + src_x * pixel_size, pixel_size);
				}
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
