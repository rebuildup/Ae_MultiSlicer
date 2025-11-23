#include "MultiSlicer.h"

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

	// Simple passthrough for now
	PF_COPY(&params[0]->u.ld, output, NULL, NULL);

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
