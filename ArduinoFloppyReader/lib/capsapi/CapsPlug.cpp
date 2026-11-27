// Simple caps plug-in support on Win32, should fit most users
//
// You may want to link with the libray instead, if your product fully complies
// with the license.
// If using the library directly without the plug-in, define CAPS_USER, and include CapsLib.h
// functions are the same with the plug-in, but their name start with "CAPS", not "Caps"
// CAPSInit does not have any parameters, CapsInit gets the library name
//
// Comment out stdafx.h if your project does not use the MSVC precompiled headers
//
// www.caps-project.org
//
// Modified for basic cross-platform support

#include "Comtype.h"
#include "CapsAPI.h"
#include "CapsPlug.h"


#ifdef _WIN32
#include "windows.h"

#define CALLING_CONVENTION WINAPI
#define GETFUNC GetProcAddress

HMODULE capi = NULL;


#else
#include <dlfcn.h>

#define CALLING_CONVENTION
#define GETFUNC dlsym

void* capi = NULL;
#endif



// Call through the exact prototype. Calling via a (...) pointer breaks on Apple arm64,
// where variadic arguments are passed on the stack but the library expects registers.
#define CAPSFN(idx, ret, ...) ((ret (*)(__VA_ARGS__))cpr[idx].proc)

CapsProc cpr[]= {
	"CAPSInit", NULL,
	"CAPSExit", NULL,
	"CAPSAddImage", NULL,
	"CAPSRemImage", NULL,
	"CAPSLockImage", NULL,
	"CAPSUnlockImage", NULL,
	"CAPSLoadImage", NULL,
	"CAPSGetImageInfo", NULL,
	"CAPSLockTrack", NULL,
	"CAPSUnlockTrack", NULL,
	"CAPSUnlockAllTracks", NULL,
	"CAPSGetPlatformName", NULL,
	"CAPSLockImageMemory", NULL,
	"CAPSGetInfo", NULL,
	NULL, NULL
};



// start caps image support
SDWORD CapsInit()
{
	if (capi)
		return imgeOk;

#ifdef WIN32
	capi = LoadLibrary(L"CAPSImg.dll");
#else
#ifdef __APPLE__
	// dlopen() on macOS doesn't search /usr/local/lib or Homebrew's prefix for a bare library name,
	// and DYLD_* variables aren't passed to apps started from Finder, so try the usual places explicitly.
	capi = dlopen("/usr/local/lib/libcapsimage.so", RTLD_NOW);
	if (!capi) capi = dlopen("/opt/homebrew/lib/libcapsimage.so", RTLD_NOW);
	if (!capi) capi = dlopen("/usr/local/lib/libcapsimage.dylib", RTLD_NOW);
	if (!capi) capi = dlopen("/opt/homebrew/lib/libcapsimage.dylib", RTLD_NOW);
	if (!capi) capi = dlopen("libcapsimage.so.5", RTLD_NOW);
#else
 	capi = dlopen("libcapsimage.so.5", RTLD_NOW);
#endif
	if (!capi) capi = dlopen("libcapsimage.so.5", RTLD_NOW);
	if (!capi) capi = dlopen("libcapsimage.so.5.1", RTLD_NOW);
	if (!capi) capi = dlopen("libcapsimage.so.4", RTLD_NOW);
	if (!capi) capi = dlopen("libcapsimage.so.4.2", RTLD_NOW);
	if (!capi) capi = dlopen("libcapsimage.so", RTLD_NOW);
#endif
	if (!capi)
		return imgeUnsupported;

	for (int prc=0; cpr[prc].name; prc++)
		cpr[prc].proc= GETFUNC(capi, cpr[prc].name);

	SDWORD res=cpr[0].proc ? CAPSFN(0, SDWORD, void)() : imgeUnsupported;

	return res;
}

// stop caps image support
SDWORD CapsExit()
{
	SDWORD res=cpr[1].proc ? CAPSFN(1, SDWORD, void)() : imgeUnsupported;

	if (capi) {
#ifdef _WIN32
		FreeLibrary(capi);
#else
		dlclose(capi);
#endif
		capi=NULL;
	}

	for (int prc=0; cpr[prc].name; prc++)
		cpr[prc].proc=NULL;

	return res;
}

// add image container
SDWORD CapsAddImage()
{
	SDWORD res=cpr[2].proc ? CAPSFN(2, SDWORD, void)() : -1;

	return res;
}

// delete image container
SDWORD CapsRemImage(SDWORD id)
{
	SDWORD res=cpr[3].proc ? CAPSFN(3, SDWORD, SDWORD)(id) : -1;

	return res;
}

// lock image
SDWORD CapsLockImage(SDWORD id, PCHAR name)
{
	SDWORD res=cpr[4].proc ? CAPSFN(4, SDWORD, SDWORD, PCHAR)(id, name) : imgeUnsupported;

	return res;
}

// unlock image
SDWORD CapsUnlockImage(SDWORD id)
{
	SDWORD res=cpr[5].proc ? CAPSFN(5, SDWORD, SDWORD)(id) : imgeUnsupported;

	return res;
}

// load and decode complete image
SDWORD CapsLoadImage(SDWORD id, UDWORD flag)
{
	SDWORD res=cpr[6].proc ? CAPSFN(6, SDWORD, SDWORD, UDWORD)(id, flag) : imgeUnsupported;

	return res;
}

// get image information
SDWORD CapsGetImageInfo(PCAPSIMAGEINFO pi, SDWORD id)
{
	SDWORD res=cpr[7].proc ? CAPSFN(7, SDWORD, PCAPSIMAGEINFO, SDWORD)(pi, id) : imgeUnsupported;

	return res;
}

SDWORD CapsGetInfo(void* si, SDWORD id, UDWORD cylinder, UDWORD head, UDWORD inftype, UDWORD infid) {
	SDWORD res = cpr[13].proc ? CAPSFN(13, SDWORD, void*, SDWORD, UDWORD, UDWORD, UDWORD, UDWORD)(si, id, cylinder, head, inftype, infid) : imgeUnsupported;

	return res;
}

// load and decode track, or return with the cache
SDWORD CapsLockTrack(PCAPSTRACKINFO pi, SDWORD id, UDWORD cylinder, UDWORD head, UDWORD flag)
{
	SDWORD res=cpr[8].proc ? CAPSFN(8, SDWORD, PCAPSTRACKINFO, SDWORD, UDWORD, UDWORD, UDWORD)(pi, id, cylinder, head, flag) : imgeUnsupported;

	return res;
}

// remove track from cache
SDWORD CapsUnlockTrack(SDWORD id, UDWORD cylinder, UDWORD head)
{
	SDWORD res=cpr[9].proc ? CAPSFN(9, SDWORD, SDWORD, UDWORD, UDWORD)(id, cylinder, head) : imgeUnsupported;

	return res;
}

// remove all tracks from cache
SDWORD CapsUnlockAllTracks(SDWORD id)
{
	SDWORD res=cpr[10].proc ? CAPSFN(10, SDWORD, SDWORD)(id) : imgeUnsupported;

	return res;
}

// get platform name
PCHAR CapsGetPlatformName(UDWORD pid)
{
	PCHAR res=cpr[11].proc ? CAPSFN(11, PCHAR, UDWORD)(pid) : NULL;

	return res;
}

// lock memory mapped image
SDWORD CapsLockImageMemory(SDWORD id, PUBYTE buffer, UDWORD length, UDWORD flag)
{
	SDWORD res=cpr[12].proc ? CAPSFN(12, SDWORD, SDWORD, PUBYTE, UDWORD, UDWORD)(id, buffer, length, flag) : imgeUnsupported;

	return res;
}

