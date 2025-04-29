#include "AEConfig.h"
#include "AE_EffectVers.h"

#ifndef AE_OS_WIN
	#include <AE_General.r>
#endif

// Must match the values in MultiSlicer.h exactly
#define MAJOR_VERSION    1
#define MINOR_VERSION    0
#define BUG_VERSION      1
#define STAGE_VERSION    PF_Stage_DEVELOP
#define BUILD_VERSION    1
	
resource 'PiPL' (16000) {
	{	/* array properties: 12 elements */
		/* [1] */
		Kind {
			AEEffect
		},
		/* [2] */
		Name {
			"MultiSlicer"
		},
		/* [3] */
		Category {
			"Sample Plug-ins"
		},
#ifdef AE_OS_WIN
	#ifdef AE_PROC_INTELx64
		CodeWin64X86 {"EffectMain"},
	#endif
#else
	#ifdef AE_OS_MAC
		CodeMacIntel64 {"EffectMain"},
		CodeMacARM64 {"EffectMain"},
	#endif
#endif
		/* [6] */
		AE_PiPL_Version {
			2,
			0
		},
		/* [7] */
		AE_Effect_Spec_Version {
			PF_PLUG_IN_VERSION,
			PF_PLUG_IN_SUBVERS
		},
		/* [8] */
		AE_Effect_Version {
			65537 /* Correctly set to 1.0.1 (1<<16 | 0<<8 | 1) */
		},
		/* [9] */
		AE_Effect_Info_Flags {
			0
		},
		/* [10] */
		AE_Effect_Global_OutFlags {
			0x06000400 /* DEEP_COLOR_AWARE | PIX_INDEPENDENT | USE_OUTPUT_EXTENT */
		},
		/* [11] */
		AE_Effect_Global_OutFlags_2 {
			0x08000000 /* PF_OutFlag2_SUPPORTS_THREADED_RENDERING */
		},
		/* [12] */
		AE_Effect_Match_Name {
			"ADBE MultiSlicer"
		},
		/* [13] */
		AE_Reserved_Info {
			0
		},
		/* [14] */
		AE_Effect_Support_URL {
			"https://www.adobe.com"
		}
	}
};