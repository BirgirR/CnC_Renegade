/*
**	Binds the Bink entry points to the retail binkw32.dll at runtime.
**
**	Renegade shipped binkw32.dll beside the executable, so anyone who owns the
**	game already has a licensed copy of the decoder. Resolving it with
**	LoadLibrary/GetProcAddress -- the same trick ww3d2 uses for
**	Direct3DCreate9 -- gives bit-exact playback of the original movies without
**	an SDK to build against and without redistributing anything of RAD's.
**
**	The DLL is optional. When it is missing, or too old to export what we need,
**	BinkOpen fails and BINKMovieClass reports every movie as already complete
**	(BINKMovie.cpp:355), which is the behaviour this file replaced: intros are
**	skipped rather than hung. That is why nothing here is fatal.
**
**	binkw32.dll is 32-bit, which is no constraint -- so is this build.
**
**	The exports are __stdcall and therefore decorated, so GetProcAddress needs
**	"_BinkOpen@8" rather than "BinkOpen". The byte counts are part of the name
**	and have to match the signature exactly.
*/

#include "Bink.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>

//-----------------------------------------------------------------------------
//	The imported surface
//-----------------------------------------------------------------------------

typedef HBINK (__stdcall *BinkOpen_Type)               (const char *name, U32 flags);
typedef void  (__stdcall *BinkClose_Type)              (HBINK bnk);
typedef S32   (__stdcall *BinkWait_Type)               (HBINK bnk);
typedef S32   (__stdcall *BinkDoFrame_Type)            (HBINK bnk);
typedef void  (__stdcall *BinkNextFrame_Type)          (HBINK bnk);
typedef S32   (__stdcall *BinkCopyToBuffer_Type)       (HBINK bnk, void *dest, S32 destpitch,
                                                        U32 destheight, U32 destx, U32 desty,
                                                        U32 flags);
typedef S32   (__stdcall *BinkSetSoundSystem_Type)     (void *open, U32 param);

static HMODULE                  BinkDll             = NULL;
static bool                     LoadAttempted       = false;

static BinkOpen_Type            Real_Open           = NULL;
static BinkClose_Type           Real_Close          = NULL;
static BinkWait_Type            Real_Wait           = NULL;
static BinkDoFrame_Type         Real_Do_Frame       = NULL;
static BinkNextFrame_Type       Real_Next_Frame     = NULL;
static BinkCopyToBuffer_Type    Real_Copy_To_Buffer = NULL;
static BinkSetSoundSystem_Type  Real_Set_Sound      = NULL;

// Passed to BinkSetSoundSystem rather than called by us, so its signature does
// not matter here -- only its address.
static void *Real_Open_Direct_Sound = NULL;
static void *Real_Open_Wave_Out     = NULL;

//-----------------------------------------------------------------------------

/*
**	This module stands in for a third-party library and links nothing from the
**	engine, so it cannot use WWDEBUG_SAY. Whether the movies played is otherwise
**	invisible -- a missing DLL looks exactly like a movie that already finished --
**	so the binding decision is recorded here, next to WWAudio's _audio.txt.
*/
static void Bink_Log(const char *fmt, ...)
{
	FILE *f = fopen("_bink.txt", "at");
	if (f == NULL) return;

	va_list args;
	va_start(args, fmt);
	vfprintf(f, fmt, args);
	va_end(args);

	fputc('\n', f);
	fclose(f);
}

/*
**	Resolve everything once. Returns false if the DLL is absent or does not
**	export the full set, in which case every entry point below falls back.
**
**	Partial success is treated as failure on purpose: binding half the API
**	would let a movie open and then fail mid-playback, which is harder to
**	diagnose than not playing at all.
*/
static bool Load_Bink(void)
{
	if (LoadAttempted) {
		return BinkDll != NULL;
	}
	LoadAttempted = true;

	// Plain name, so the usual search order applies and the copy sitting
	// beside renegade.exe wins.
	BinkDll = ::LoadLibraryA("binkw32.dll");
	if (BinkDll == NULL) {
		Bink_Log("binkw32.dll not found (error %lu); movies will be skipped.",
					::GetLastError());
		return false;
	}

	Real_Open           = (BinkOpen_Type)          ::GetProcAddress(BinkDll, "_BinkOpen@8");
	Real_Close          = (BinkClose_Type)         ::GetProcAddress(BinkDll, "_BinkClose@4");
	Real_Wait           = (BinkWait_Type)          ::GetProcAddress(BinkDll, "_BinkWait@4");
	Real_Do_Frame       = (BinkDoFrame_Type)       ::GetProcAddress(BinkDll, "_BinkDoFrame@4");
	Real_Next_Frame     = (BinkNextFrame_Type)     ::GetProcAddress(BinkDll, "_BinkNextFrame@4");
	Real_Copy_To_Buffer = (BinkCopyToBuffer_Type)  ::GetProcAddress(BinkDll, "_BinkCopyToBuffer@28");
	Real_Set_Sound      = (BinkSetSoundSystem_Type)::GetProcAddress(BinkDll, "_BinkSetSoundSystem@8");

	Real_Open_Direct_Sound = (void *)::GetProcAddress(BinkDll, "_BinkOpenDirectSound@4");
	Real_Open_Wave_Out     = (void *)::GetProcAddress(BinkDll, "_BinkOpenWaveOut@4");

	if (Real_Open == NULL || Real_Close == NULL || Real_Wait == NULL ||
		 Real_Do_Frame == NULL || Real_Next_Frame == NULL || Real_Copy_To_Buffer == NULL) {

		Bink_Log("binkw32.dll loaded but is missing entry points; movies will be skipped.");
		::FreeLibrary(BinkDll);
		BinkDll = NULL;
		return false;
	}

	Bink_Log("binkw32.dll bound: video ok, sound system %s.",
				(Real_Set_Sound != NULL) ? "available" : "unavailable");
	return true;
}

//-----------------------------------------------------------------------------
//	Entry points
//-----------------------------------------------------------------------------

HBINK __stdcall BinkOpen(const char *name, U32 flags)
{
	if (!Load_Bink()) {
		return 0;
	}

	HBINK bnk = Real_Open(name, flags);
	if (bnk == 0) {
		Bink_Log("BinkOpen failed: %s", (name != NULL) ? name : "(null)");
	} else {
		Bink_Log("playing %s: %ux%u, %u frames, %u/%u fps", (name != NULL) ? name : "(null)",
					bnk->Width, bnk->Height, bnk->Frames, bnk->FrameRate, bnk->FrameRateDiv);
	}
	return bnk;
}

void __stdcall BinkClose(HBINK bnk)
{
	if (bnk != 0 && Real_Close != NULL) {
		Real_Close(bnk);
	}
}

S32 __stdcall BinkWait(HBINK bnk)
{
	// Non-zero means "not time for the next frame yet". With no DLL there is no
	// handle either, so this is unreachable; answering 1 keeps the player from
	// spinning if that ever changes.
	if (bnk == 0 || Real_Wait == NULL) {
		return 1;
	}
	return Real_Wait(bnk);
}

S32 __stdcall BinkDoFrame(HBINK bnk)
{
	if (bnk == 0 || Real_Do_Frame == NULL) {
		return 0;
	}
	return Real_Do_Frame(bnk);
}

void __stdcall BinkNextFrame(HBINK bnk)
{
	if (bnk != 0 && Real_Next_Frame != NULL) {
		Real_Next_Frame(bnk);
	}
}

S32 __stdcall BinkCopyToBuffer(HBINK bnk, void *dest, S32 destpitch, U32 destheight,
										 U32 destx, U32 desty, U32 flags)
{
	if (bnk == 0 || Real_Copy_To_Buffer == NULL) {
		return 0;
	}
	return Real_Copy_To_Buffer(bnk, dest, destpitch, destheight, destx, desty, flags);
}

/*
**	BINKMovie::Init calls this once, before any movie is opened, which is where
**	Bink requires the sound system to be chosen.
**
**	binkw32.dll does not export BinkSoundUseDirectSound at all: in the real SDK
**	that name is a wrapper around BinkSetSoundSystem(BinkOpenDirectSound, ds),
**	so the wrapper is reproduced here. WaveOut is the fallback for the same
**	reason it exists in the SDK -- it needs no device object and no DirectSound.
*/
S32 __stdcall BinkSoundUseDirectSound(void *ds)
{
	if (!Load_Bink() || Real_Set_Sound == NULL) {
		return 0;
	}

	if (Real_Open_Direct_Sound != NULL) {
		return Real_Set_Sound(Real_Open_Direct_Sound, (U32)(UINT_PTR)ds);
	}
	if (Real_Open_Wave_Out != NULL) {
		return Real_Set_Sound(Real_Open_Wave_Out, 0);
	}

	// Video without audio still beats no movie.
	return 0;
}
