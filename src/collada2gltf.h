#pragma once

#include <atlbase.h>
#include <atlcom.h>
#include <comdef.h>
//#include <msdbg.h> 


#ifdef COLLADA2GLTF_dll_EXPORTS
#define TESTFUNCDLL_API __declspec(dllexport) 
#else
#define TESTFUNCDLL_API __declspec(dllimport) 
#endif

extern "C" {
	TESTFUNCDLL_API int collada2gltf(int dwCount, char** Members);
}

