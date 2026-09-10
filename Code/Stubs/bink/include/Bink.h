/*
**	Stub replacement for the RAD Bink Video SDK header (Bink.h).
**
**	Bink is proprietary and is not distributed with the Renegade source release.
**	Only Code/BinkMovie needs it, and only for the eight entry points declared
**	below, so this header plus bink_stub.cpp lets that library build and link.
**
**	The stub deliberately makes BinkOpen() fail. BINKMovieClass already treats a
**	null handle as "movie finished" (BINKMovie.cpp:355 -- `if (!Bink) return
**	true;`) and its constructor returns early on NULL, so every movie is skipped
**	cleanly instead of hanging the front end. Nothing else has to pretend.
**
**	Replace with the real SDK, or reimplement over FFmpeg, for actual playback.
*/

#ifndef BINK_H
#define BINK_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BINK_TYPES_DEFINED
#define BINK_TYPES_DEFINED
typedef unsigned int   U32;
typedef signed int     S32;
typedef void          *BINKPTR;
#endif

// Surface / copy flags. Only these two are referenced (BINKMovie.cpp:289).
#define BINKSURFACE565      3
#define BINKCOPYNOSCALING   0x80000000

// Only the fields the player actually reads are modelled. The real BINK struct
// is much larger; nothing here is ever dereferenced because BinkOpen returns
// NULL, but the layout has to compile.
typedef struct BINK {
   U32 Width;
   U32 Height;
   U32 Frames;
   U32 FrameNum;
   U32 FrameRate;
   U32 FrameRateDiv;
} BINK, *HBINK;

HBINK __stdcall BinkOpen                (const char *name, U32 flags);
void  __stdcall BinkClose               (HBINK bnk);
S32   __stdcall BinkWait                (HBINK bnk);
S32   __stdcall BinkDoFrame             (HBINK bnk);
void  __stdcall BinkNextFrame           (HBINK bnk);
S32   __stdcall BinkCopyToBuffer        (HBINK bnk, void *dest, S32 destpitch,
                                         U32 destheight, U32 destx, U32 desty,
                                         U32 flags);
S32   __stdcall BinkSoundUseDirectSound (void *ds);

#ifdef __cplusplus
}
#endif

#endif // BINK_H
