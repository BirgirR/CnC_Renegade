/*
**	Declarations for the RAD Bink Video entry points Code/BinkMovie needs.
**
**	Bink is proprietary and is not distributed with the Renegade source release,
**	so this replaces the SDK's Bink.h. It declares only the eight functions
**	BINKMovie.cpp calls and the part of the BINK structure it reads.
**
**	bink_loader.cpp binds these to binkw32.dll at runtime when the DLL is
**	present -- it ships with the retail game, so anyone who owns Renegade
**	already has it -- and makes BinkOpen fail cleanly when it is not. Nothing
**	here is needed to build: the tree still compiles against the Windows SDK
**	alone, and no part of Bink is redistributed.
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

/*
**	Surface / copy flags. Only these two are referenced (BINKMovie.cpp:289).
**
**	BINKSURFACE565 is 10, not 3. This was measured, not assumed: filling a
**	buffer with a sentinel and asking binkw32.dll to copy one frame at each
**	flag shows how wide a row it writes, and the widths land exactly on the
**	SDK's table -- 1 and 2 write 3 bytes per pixel (24, 24R), 3 and 4 write 4
**	(32, 32R), and 7 through 12 write 2 (4444, 5551, 555, 565, 655, 664).
**
**	The earlier value of 3 asked for BINKSURFACE32, so Bink wrote 32-bit BGRA
**	into a buffer BINKMovie then uploaded to an R5G6B5 texture. The movie played
**	but every frame was mangled.
*/
#define BINKSURFACE565      10
#define BINKCOPYNOSCALING   0x80000000

/*
**	The head of the real BINK structure.
**
**	These offsets are not guesswork and must not be edited casually. They were
**	read out of binkw32.dll by opening Data/Movies/R_Intro.BIK and dumping the
**	returned struct, then matching the values against the movie's own file
**	header -- 800x600, 475 frames, 15/1 fps:
**
**	    0 Width 800    12 FrameNum 1        24 FrameRateDiv 1
**	    4 Height 600   16 LastFrameNum -1   28 ReadError 0
**	    8 Frames 475   20 FrameRate 15      40 Size 8428292
**
**	LastFrameNum at offset 16 is the trap: leave it out and FrameRate reads
**	0xffffffff, which turns BINKMovie's ticks-per-frame into nonsense.
**
**	The real structure continues well past what is declared here. That is safe
**	because we only ever read through a pointer the DLL returns -- nothing in
**	this tree allocates a BINK -- but it does mean the fields below must stay in
**	this order and nothing may be inserted among them.
*/
typedef struct BINK {
   U32 Width;           // 0
   U32 Height;          // 4
   U32 Frames;          // 8
   U32 FrameNum;        // 12  frame about to be displayed, 1 based
   U32 LastFrameNum;    // 16  last frame displayed
   U32 FrameRate;       // 20  frame rate numerator
   U32 FrameRateDiv;    // 24  frame rate divisor
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
