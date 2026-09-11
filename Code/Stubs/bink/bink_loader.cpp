/*
**	Binds the Bink entry points to the retail binkw32.dll at runtime.
**
**	Renegade shipped binkw32.dll beside the executable, so anyone who owns the
**	game already has a licensed copy of the decoder. Resolving it with
**	LoadLibrary/GetProcAddress -- the same trick ww3d2 uses for
**	Direct3DCreate9 -- gives bit-exact playback of the original movies without
**	an SDK to build against and without redistributing anything of RAD's.
**
**	The DLL is optional. Without it the movies are decoded by the vendored
**	libbinkdec instead -- see the Vendored decoder section below. If that is
**	compiled out too, BinkOpen fails
**	and BINKMovieClass reports every movie as already complete
**	(BINKMovie.cpp:355), so intros are skipped rather than hung. That is why
**	nothing here is fatal.
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
#ifdef RENEGADE_BINK_DECODER
		Bink_Log("binkw32.dll not found (error %lu); decoding with libbinkdec.",
					::GetLastError());
#else
		Bink_Log("binkw32.dll not found (error %lu); movies will be skipped.",
					::GetLastError());
#endif
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


#ifdef RENEGADE_BINK_DECODER

//-----------------------------------------------------------------------------
//	Vendored decoder
//
//	Used when binkw32.dll is not present. libbinkdec (Code/ThirdParty) decodes
//	Bink 1 itself, so the movies play from the game's data with nothing
//	proprietary involved.
//
//	Two things have to be bridged. The engine drives a decode/copy/advance cycle
//	across three calls, while libbinkdec has one call that decodes and advances
//	together, so the decoded frame is held here between them. And it hands back
//	YUV planes where the engine asks for RGB565, so the conversion is ours.
//
//	Playback timing is ours too: the retail DLL paced itself and reported
//	through BinkWait, and nothing else in the player keeps a clock.
//-----------------------------------------------------------------------------

#include "BinkDecoder.h"

#include <mmsystem.h>
#include <xaudio2.h>

/*
**	Frees each buffer once XAudio2 is done with it. The buffers are handed over
**	one per frame and the voice owns them until then, so this is where they go.
**	Called on the audio thread.
*/
struct MovieVoiceCallback : public IXAudio2VoiceCallback
{
	STDMETHOD_(void, OnBufferEnd)			(void *context)	{ delete [] (unsigned char *)context; }
	STDMETHOD_(void, OnBufferStart)		(void *)				{}
	STDMETHOD_(void, OnLoopEnd)			(void *)				{}
	STDMETHOD_(void, OnStreamEnd)			(void)				{}
	STDMETHOD_(void, OnVoiceProcessingPassEnd)	(void)		{}
	STDMETHOD_(void, OnVoiceProcessingPassStart)	(UINT32)	{}
	STDMETHOD_(void, OnVoiceError)		(void *, HRESULT)	{}
};

static MovieVoiceCallback _MovieCallback;

struct DecoderMovie
{
	BINK				Header;			// must be first: the engine holds a HBINK
	BinkHandle		Handle;
	YUVbuffer		Frame;
	bool				FrameValid;		// decoded and not yet advanced past
	DWORD				StartTicks;
	bool				Started;			// clock runs from the first frame asked for
	unsigned			FrameMs;			// only a fallback; see Decoder_Wait

	//	Audio, when the movie has a track. XAudio2 is created here rather than
	//	borrowed from WWAudio: this module links nothing from the engine, and in
	//	the default build WWAudio is the retail Miles DLL with no XAudio2 in it
	//	at all.
	IXAudio2 *					Audio;
	IXAudio2MasteringVoice *	Master;
	IXAudio2SourceVoice *		Voice;
	bool							HasAudio;
	unsigned						AudioBytes;		// idealBufferSize, in bytes
	unsigned						SampleRate;

	DecoderMovie(void)
		: FrameValid(false), StartTicks(0), Started(false), FrameMs(33),
		  Audio(NULL), Master(NULL), Voice(NULL), HasAudio(false),
		  AudioBytes(0), SampleRate(0)
	{
		memset(&Header, 0, sizeof(Header));
		memset(&Frame, 0, sizeof(Frame));
	}
};

static DecoderMovie *_Decoded = NULL;		// one movie plays at a time

/*
**	BT.601 limited range, which is what Bink encodes. The engine wants 5:6:5, so
**	the fixed-point maths only needs to be accurate to five bits of red and blue
**	-- rounding beyond that is thrown away by the pack.
*/
static inline unsigned short Yuv_To_Rgb565(int y, int u, int v)
{
	y = (y - 16) * 298;
	u -= 128;
	v -= 128;

	int r = (y + 409 * v + 128) >> 8;
	int g = (y - 100 * u - 208 * v + 128) >> 8;
	int b = (y + 516 * u + 128) >> 8;

	if (r < 0) r = 0; else if (r > 255) r = 255;
	if (g < 0) g = 0; else if (g > 255) g = 255;
	if (b < 0) b = 0; else if (b > 255) b = 255;

	return (unsigned short)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

/*
**	Convert the held frame into the caller's buffer. The chroma planes are half
**	resolution in each direction, so each pair of rows and columns shares one U
**	and V sample.
*/
static void Decoder_Copy_565(DecoderMovie *movie, void *dest, S32 dest_pitch,
									  U32 dest_height)
{
	const ImagePlane &plane_y = movie->Frame[0];
	const ImagePlane &plane_u = movie->Frame[1];
	const ImagePlane &plane_v = movie->Frame[2];

	if (plane_y.data == NULL || plane_u.data == NULL || plane_v.data == NULL) {
		return;
	}

	U32 height = movie->Header.Height;
	if (dest_height < height) height = dest_height;

	const U32 width = movie->Header.Width;

	for (U32 y = 0; y < height; ++y) {

		const unsigned char *row_y = plane_y.data + (size_t)y * plane_y.pitch;
		const unsigned char *row_u = plane_u.data + (size_t)(y / 2) * plane_u.pitch;
		const unsigned char *row_v = plane_v.data + (size_t)(y / 2) * plane_v.pitch;

		unsigned short *out = (unsigned short *)((unsigned char *)dest + (size_t)y * dest_pitch);

		for (U32 x = 0; x < width; ++x) {
			out[x] = Yuv_To_Rgb565(row_y[x], row_u[x / 2], row_v[x / 2]);
		}
	}
}

/*
**	Give the movie a voice to play its soundtrack through, if it has one.
**
**	Failure at any step is not fatal: HasAudio stays false and the movie plays
**	silently, which is what this path did before the audio was written.
*/
static void Decoder_Open_Audio(DecoderMovie *movie)
{
	if (Bink_GetNumAudioTracks(movie->Handle) == 0) {
		return;
	}

	const AudioInfo info = Bink_GetAudioTrackDetails(movie->Handle, 0);
	if (info.sampleRate == 0 || info.nChannels == 0 || info.idealBufferSize == 0) {
		return;
	}

	if (FAILED(::XAudio2Create(&movie->Audio, 0, XAUDIO2_DEFAULT_PROCESSOR))) {
		Bink_Log("   XAudio2Create failed; the movie will be silent.");
		return;
	}
	if (FAILED(movie->Audio->CreateMasteringVoice(&movie->Master))) {
		Bink_Log("   no mastering voice; the movie will be silent.");
		return;
	}

	WAVEFORMATEX fmt;
	memset(&fmt, 0, sizeof(fmt));
	fmt.wFormatTag      = WAVE_FORMAT_PCM;
	fmt.nChannels       = (WORD)info.nChannels;
	fmt.nSamplesPerSec  = info.sampleRate;
	fmt.wBitsPerSample  = 16;
	fmt.nBlockAlign     = (WORD)(info.nChannels * 2);
	fmt.nAvgBytesPerSec = info.sampleRate * fmt.nBlockAlign;

	if (FAILED(movie->Audio->CreateSourceVoice(&movie->Voice, &fmt, 0,
															 XAUDIO2_DEFAULT_FREQ_RATIO,
															 &_MovieCallback))) {
		Bink_Log("   no source voice; the movie will be silent.");
		return;
	}

	movie->Voice->Start(0);
	movie->AudioBytes = info.idealBufferSize;
	movie->SampleRate = info.sampleRate;
	movie->HasAudio   = true;
}

static void Decoder_Close_Audio(DecoderMovie *movie)
{
	if (movie->Voice != NULL) {
		movie->Voice->Stop(0);
		movie->Voice->FlushSourceBuffers();		// frees the queued buffers via the callback
		movie->Voice->DestroyVoice();
		movie->Voice = NULL;
	}
	if (movie->Master != NULL) {
		movie->Master->DestroyVoice();
		movie->Master = NULL;
	}
	if (movie->Audio != NULL) {
		movie->Audio->Release();
		movie->Audio = NULL;
	}
	movie->HasAudio = false;
}

static HBINK Decoder_Open(const char *name)
{
	if (_Decoded != NULL) {			// the player only runs one at a time
		return 0;
	}

	DecoderMovie *movie = new DecoderMovie;
	movie->Handle = Bink_Open(name);
	if (!movie->Handle.isValid) {
		delete movie;
		Bink_Log("libbinkdec could not open %s", (name != NULL) ? name : "(null)");
		return 0;
	}

	unsigned width = 0, height = 0;
	Bink_GetFrameSize(movie->Handle, width, height);

	const float rate = Bink_GetFrameRate(movie->Handle);

	movie->Header.Width        = width;
	movie->Header.Height       = height;
	movie->Header.Frames       = Bink_GetNumFrames(movie->Handle);
	movie->Header.FrameNum     = 1;			// Bink counts frames from one
	movie->Header.LastFrameNum = 0;
	//	The engine divides these, so give it the rate as a ratio over 1000 rather
	//	than rounding a float to whole frames per second.
	movie->Header.FrameRate    = (U32)(rate * 1000.0f + 0.5f);
	movie->Header.FrameRateDiv = 1000;
	movie->FrameMs             = (rate > 0.0f) ? (unsigned)(1000.0f / rate) : 33;

	Decoder_Open_Audio(movie);

	_Decoded = movie;

	Bink_Log("libbinkdec playing %s: %ux%u, %u frames, %.2f fps, audio %s", name,
				width, height, movie->Header.Frames, rate,
				movie->HasAudio ? "on" : "none");

	return &movie->Header;
}

static void Decoder_Close(void)
{
	if (_Decoded == NULL) return;

	Decoder_Close_Audio(_Decoded);
	Bink_Close(_Decoded->Handle);
	delete _Decoded;
	_Decoded = NULL;
}

/*
**	Non-zero means "not yet".
**
**	The clock starts here rather than at open. Opening a movie is followed by
**	texture setup before the player draws anything, and timing from the open
**	meant several frames were already overdue by the time the first one was
**	asked for -- the movie then raced through them as fast as the render loop
**	ran, which is what made playback look fast at the start.
**
**	The due time is computed from the frame number each call, not accumulated,
**	so a slow frame is caught up rather than pushing everything after it late.
**	It is worked out from the rate as a ratio: at 15fps a frame is 66.67ms, and
**	rounding that down to 66 runs the movie one percent fast all the way
**	through.
*/
static S32 Decoder_Wait(void)
{
	if (_Decoded == NULL) return 1;

	if (!_Decoded->Started) {
		_Decoded->StartTicks = ::timeGetTime();
		_Decoded->Started = true;
		return 0;					// the first frame is due immediately
	}

	const unsigned rate = _Decoded->Header.FrameRate;
	const unsigned div  = _Decoded->Header.FrameRateDiv;

	/*
	**	With a soundtrack, the audio is the clock. Pacing video on the wall clock
	**	instead lets the two drift apart over a movie's length, and a minute of
	**	drift against speech is obvious in a way a few late frames are not.
	**
	**	The queue running dry is the exception: SamplesPlayed stops advancing and
	**	would hold the video forever, so that falls back to the wall clock.
	*/
	if (_Decoded->HasAudio && rate > 0) {

		XAUDIO2_VOICE_STATE state;
		memset(&state, 0, sizeof(state));
		_Decoded->Voice->GetState(&state);

		if (state.BuffersQueued > 0) {
			const unsigned __int64 due_samples =
				((unsigned __int64)(_Decoded->Header.FrameNum - 1) * _Decoded->SampleRate * div) / rate;

			return (state.SamplesPlayed >= due_samples) ? 0 : 1;
		}
	}

	const DWORD elapsed = ::timeGetTime() - _Decoded->StartTicks;

	DWORD due;
	if (rate > 0) {
		due = (DWORD)(((unsigned __int64)(_Decoded->Header.FrameNum - 1) * 1000ui64 * div) / rate);
	} else {
		due = (_Decoded->Header.FrameNum - 1) * _Decoded->FrameMs;
	}

	return (elapsed >= due) ? 0 : 1;
}

static void Decoder_Do_Frame(void)
{
	if (_Decoded == NULL || _Decoded->FrameValid) return;

	Bink_GetNextFrame(_Decoded->Handle, _Decoded->Frame);
	_Decoded->FrameValid = true;

	/*
	**	Each decoded frame carries its own slice of the soundtrack, so the audio
	**	is queued as the video is decoded and needs no clock of its own. The
	**	voice owns the buffer until it has played it, which is why the callback
	**	does the freeing.
	*/
	if (_Decoded->HasAudio) {

		unsigned char *buffer = new unsigned char[_Decoded->AudioBytes];
		const unsigned bytes = Bink_GetAudioData(_Decoded->Handle, 0, (int16_t *)buffer);

		if (bytes > 0 && bytes <= _Decoded->AudioBytes) {
			XAUDIO2_BUFFER submit;
			memset(&submit, 0, sizeof(submit));
			submit.AudioBytes = bytes;
			submit.pAudioData = buffer;
			submit.pContext   = buffer;

			if (FAILED(_Decoded->Voice->SubmitSourceBuffer(&submit))) {
				delete [] buffer;
			}
		} else {
			delete [] buffer;
		}
	}
}

static void Decoder_Next_Frame(void)
{
	if (_Decoded == NULL) return;

	//	libbinkdec advanced when it decoded, so this only moves the counters the
	//	engine reads and marks the held frame spent.
	_Decoded->Header.LastFrameNum = _Decoded->Header.FrameNum;
	if (_Decoded->Header.FrameNum < _Decoded->Header.Frames) {
		_Decoded->Header.FrameNum++;
	}
	_Decoded->FrameValid = false;
}

#endif // RENEGADE_BINK_DECODER
//-----------------------------------------------------------------------------
//	Entry points
//-----------------------------------------------------------------------------

/*
**	Each entry point prefers the retail DLL and falls through to the vendored
**	decoder when it is absent. Only one of the two is ever in play for a given
**	run, so no handle can reach the wrong implementation.
*/
#ifdef RENEGADE_BINK_DECODER
#define BINK_FALLBACK(expr)	do { expr; } while (0)
#else
#define BINK_FALLBACK(expr)	do { } while (0)
#endif

HBINK __stdcall BinkOpen(const char *name, U32 flags)
{
	if (!Load_Bink()) {
		BINK_FALLBACK(return Decoder_Open(name));
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
	if (bnk == 0) return;

	if (Real_Close != NULL) {
		Real_Close(bnk);
		return;
	}
	BINK_FALLBACK(Decoder_Close());
}

S32 __stdcall BinkWait(HBINK bnk)
{
	// Non-zero means "not time for the next frame yet".
	if (bnk == 0) {
		return 1;
	}
	if (Real_Wait != NULL) {
		return Real_Wait(bnk);
	}
	BINK_FALLBACK(return Decoder_Wait());
	return 1;
}

S32 __stdcall BinkDoFrame(HBINK bnk)
{
	if (bnk == 0) {
		return 0;
	}
	if (Real_Do_Frame != NULL) {
		return Real_Do_Frame(bnk);
	}
	BINK_FALLBACK(Decoder_Do_Frame());
	return 0;
}

void __stdcall BinkNextFrame(HBINK bnk)
{
	if (bnk == 0) return;

	if (Real_Next_Frame != NULL) {
		Real_Next_Frame(bnk);
		return;
	}
	BINK_FALLBACK(Decoder_Next_Frame());
}

S32 __stdcall BinkCopyToBuffer(HBINK bnk, void *dest, S32 destpitch, U32 destheight,
										 U32 destx, U32 desty, U32 flags)
{
	if (bnk == 0 || dest == NULL) {
		return 0;
	}
	if (Real_Copy_To_Buffer != NULL) {
		return Real_Copy_To_Buffer(bnk, dest, destpitch, destheight, destx, desty, flags);
	}

	//	destx/desty are always zero from BINKMovie.cpp, which copies the whole
	//	frame and then tiles it across textures itself.
	BINK_FALLBACK(Decoder_Copy_565(_Decoded, dest, destpitch, destheight));
	return 0;
}

/*
**	BINKMovie::Init calls this once, before any movie is opened, which is where
**	Bink requires the sound system to be chosen.
**
**	binkw32.dll does not export BinkSoundUseDirectSound at all: in the real SDK
**	that name is a wrapper around BinkSetSoundSystem(BinkOpenDirectSound, ds),
**	so the wrapper is reproduced here. WaveOut is the fallback for the same
**	reason it exists in the SDK -- it needs no device object and no DirectSound.
**
**	The vendored decoder has nothing to answer here, and needs nothing: it owns
**	its own XAudio2 voice and queues the soundtrack as it decodes, rather than
**	being handed a sound system the way Bink is.
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
