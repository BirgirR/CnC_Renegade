/*
**	No-op implementation of the Bink stub declared in Bink.h.
**
**	BinkOpen fails, so BINKMovieClass never gets a handle and reports every
**	movie as already complete. The remaining entry points exist only to satisfy
**	the linker and are unreachable while BinkOpen returns NULL.
*/

#include "Bink.h"

HBINK __stdcall BinkOpen(const char *, U32)
{
   // Reporting failure is the whole design: see the header.
   return 0;
}

void __stdcall BinkClose     (HBINK) {}
S32  __stdcall BinkWait      (HBINK) { return 1; }
S32  __stdcall BinkDoFrame   (HBINK) { return 0; }
void __stdcall BinkNextFrame (HBINK) {}

S32 __stdcall BinkCopyToBuffer(HBINK, void *, S32, U32, U32, U32, U32)
{
   return 0;
}

S32 __stdcall BinkSoundUseDirectSound(void *)
{
   return 1;
}
