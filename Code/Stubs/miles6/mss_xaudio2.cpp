/*
**	Miles Sound System API implemented on XAudio2.
**
**	The Miles 6 SDK is proprietary and long out of distribution, so the engine's
**	91 files that include mss.h had nothing real to link against. This supplies
**	the 77 AIL_* entry points WWAudio actually calls, backed by XAudio2 and
**	X3DAudio -- both part of the Windows SDK, with XAudio2_9.dll shipping in
**	Windows itself. Nothing here needs a redistributable, an SDK download or a
**	package manager, which is the point: the tree stays buildable from itself
**	plus the Windows SDK.
**
**	Design notes
**	------------
**	The engine hands us whole WAV files in memory (AIL_set_named_sample_file,
**	AIL_set_3D_sample_file) rather than streaming from disk, so decoding is ours
**	to do. Renegade's effects are IMA ADPCM -- see SoundBuffer.cpp, which
**	defaults m_Type to WAVE_FORMAT_IMA_ADPCM -- and XAudio2 plays only PCM, so
**	ADPCM is expanded to 16-bit PCM up front. Sounds are short; the memory cost
**	is a few hundred KB at worst and it keeps the mixing path simple.
**
**	Miles has no end-of-sample callback in the surface the engine uses. It finds
**	out a sound has finished by polling AIL_sample_ms_position, so that has to be
**	accurate rather than approximate; it is derived from
**	IXAudio2SourceVoice::GetState().SamplesPlayed, which is exact.
**
**	Miles scales volume 0..127 and pans 0..127 with 64 as centre. XAudio2 wants
**	linear amplitude and an output matrix, so both are converted at the edge.
*/

#include "mss.h"

#include <windows.h>
#include <xaudio2.h>
#include <x3daudio.h>
#include <mmreg.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#pragma comment(lib, "xaudio2.lib")

//-----------------------------------------------------------------------------
//	Miles scaling helpers
//-----------------------------------------------------------------------------

static const S32 MILES_VOLUME_MAX = 127;
static const S32 MILES_PAN_CENTRE = 64;

static float Miles_Volume_To_Amplitude(S32 volume)
{
	if (volume < 0) volume = 0;
	if (volume > MILES_VOLUME_MAX) volume = MILES_VOLUME_MAX;
	return (float)volume / (float)MILES_VOLUME_MAX;
}

//-----------------------------------------------------------------------------
//	IMA ADPCM
//
//	The standard IMA/DVI decoder. Each block opens with a per-channel header --
//	predictor, step index, reserved byte -- and the rest is 4-bit nibbles, low
//	nibble first. Stereo blocks interleave one 4-byte group per channel.
//-----------------------------------------------------------------------------

static const int IMA_STEP_TABLE[89] = {
	7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
	50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230,
	253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963,
	1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024,
	3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493,
	10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086,
	29794, 32767
};

static const int IMA_INDEX_TABLE[16] = {
	-1, -1, -1, -1, 2, 4, 6, 8,
	-1, -1, -1, -1, 2, 4, 6, 8
};

struct ImaChannelState
{
	int Predictor;
	int StepIndex;
};

static short Ima_Decode_Nibble(ImaChannelState &state, unsigned char nibble)
{
	int step = IMA_STEP_TABLE[state.StepIndex];

	int diff = step >> 3;
	if (nibble & 1) diff += step >> 2;
	if (nibble & 2) diff += step >> 1;
	if (nibble & 4) diff += step;
	if (nibble & 8) diff = -diff;

	int predictor = state.Predictor + diff;
	if (predictor > 32767)  predictor = 32767;
	if (predictor < -32768) predictor = -32768;
	state.Predictor = predictor;

	state.StepIndex += IMA_INDEX_TABLE[nibble & 0x0F];
	if (state.StepIndex < 0)  state.StepIndex = 0;
	if (state.StepIndex > 88) state.StepIndex = 88;

	return (short)predictor;
}

/*
**	Expand IMA ADPCM to 16-bit PCM. Returns a buffer the caller frees with
**	delete[], or NULL if the data is malformed.
*/
static short *Ima_Decode(const unsigned char *src, unsigned src_bytes,
								 int channels, int block_align, unsigned &out_samples)
{
	out_samples = 0;

	if (src == NULL || channels < 1 || channels > 2 || block_align <= 4 * channels) {
		return NULL;
	}

	const int header_bytes = 4 * channels;
	const int nibbles_per_block = (block_align - header_bytes) * 2;
	const int samples_per_block = 1 + (nibbles_per_block / channels);

	const unsigned block_count = src_bytes / (unsigned)block_align;
	if (block_count == 0) {
		return NULL;
	}

	const unsigned total = block_count * (unsigned)samples_per_block * (unsigned)channels;
	short *dest = new short[total];
	if (dest == NULL) {
		return NULL;
	}

	short *out = dest;

	for (unsigned b = 0; b < block_count; ++b) {

		const unsigned char *block = src + (size_t)b * block_align;

		ImaChannelState state[2];
		for (int c = 0; c < channels; ++c) {
			const unsigned char *hdr = block + 4 * c;
			state[c].Predictor = (short)(hdr[0] | (hdr[1] << 8));
			state[c].StepIndex = hdr[2];
			if (state[c].StepIndex > 88) state[c].StepIndex = 88;

			//	The first sample of every block is the predictor itself.
			out[c] = (short)state[c].Predictor;
		}
		out += channels;

		//	Nibbles arrive in 4-byte groups, one group per channel in turn.
		const unsigned char *data = block + header_bytes;
		const int groups = (block_align - header_bytes) / (4 * channels);

		for (int g = 0; g < groups; ++g) {
			for (int c = 0; c < channels; ++c) {
				for (int i = 0; i < 4; ++i) {
					unsigned char byte = *data++;
					short lo = Ima_Decode_Nibble(state[c], (unsigned char)(byte & 0x0F));
					short hi = Ima_Decode_Nibble(state[c], (unsigned char)(byte >> 4));
					out[(i * 2 + 0) * channels + c] = lo;
					out[(i * 2 + 1) * channels + c] = hi;
				}
			}
			out += 8 * channels;
		}
	}

	out_samples = (unsigned)(out - dest) / (unsigned)channels;
	return dest;
}

//-----------------------------------------------------------------------------
//	WAV parsing
//-----------------------------------------------------------------------------

struct WavDescription
{
	int					Format;			// WAVE_FORMAT_PCM or WAVE_FORMAT_IMA_ADPCM
	int					Channels;
	unsigned				Rate;
	int					Bits;
	int					BlockAlign;
	const unsigned char *Data;
	unsigned				DataBytes;
};

static unsigned Read_U32(const unsigned char *p)
{
	return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}

static unsigned short Read_U16(const unsigned char *p)
{
	return (unsigned short)((unsigned)p[0] | ((unsigned)p[1] << 8));
}

/*
**	How many bytes can be read from p without leaving mapped memory.
**
**	Two of the Miles entry points -- AIL_WAV_info and AIL_set_3D_sample_file --
**	are handed a bare pointer with no length, so the only size available is the
**	one written inside the file. That is a claim, and a WAV whose header claims
**	more than the buffer actually holds walks the chunk loop off the end. Asking
**	the memory manager for the extent of the containing region gives a bound
**	that is always safe, even when it is generous.
*/
static unsigned Readable_Bytes(const void *p)
{
	MEMORY_BASIC_INFORMATION mbi;
	if (VirtualQuery(p, &mbi, sizeof(mbi)) == 0) {
		return 0;
	}
	if (mbi.State != MEM_COMMIT) {
		return 0;
	}

	const DWORD readable = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
								  PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE |
								  PAGE_EXECUTE_WRITECOPY;
	if ((mbi.Protect & readable) == 0) {
		return 0;
	}

	const unsigned char *base = (const unsigned char *)mbi.BaseAddress;
	const size_t offset = (const unsigned char *)p - base;
	if (offset >= mbi.RegionSize) {
		return 0;
	}

	return (unsigned)(mbi.RegionSize - offset);
}

/*
**	Walk a RIFF/WAVE image for its 'fmt ' and 'data' chunks. Deliberately
**	tolerant of unknown chunks (LIST, fact, cue) which Renegade's assets carry.
**
**	image_bytes is what the caller knows the buffer to be; pass 0 when that is
**	genuinely unknown, as it is for the Miles entry points that take only a
**	pointer. Sizes inside the file are treated as claims to be checked, never as
**	facts: a chunk header that runs past the end of the buffer walks this loop
**	straight into unmapped memory, which is exactly what it used to do.
*/
static bool Wav_Parse(const void *image, unsigned image_bytes, WavDescription &out)
{
	if (image == NULL) return false;

	//	Smallest meaningful file: RIFF header, WAVE tag, and one chunk header.
	if (image_bytes != 0 && image_bytes < 20) return false;

	const unsigned char *p = (const unsigned char *)image;
	if (memcmp(p, "RIFF", 4) != 0 || memcmp(p + 8, "WAVE", 4) != 0) {
		return false;
	}

	//	Believe the smallest of: what the file claims, what the caller told us,
	//	and what is actually mapped.
	unsigned riff_size = Read_U32(p + 4);
	if (riff_size < 4) return false;

	unsigned limit = riff_size + 8;
	if (image_bytes != 0 && image_bytes < limit) {
		limit = image_bytes;
	}

	const unsigned mapped = Readable_Bytes(p);
	if (mapped == 0) return false;
	if (mapped < limit) {
		limit = mapped;
	}
	if (limit < 20) return false;

	const unsigned char *end = p + limit;

	memset(&out, 0, sizeof(out));
	bool have_fmt = false;

	const unsigned char *chunk = p + 12;
	while (chunk + 8 <= end) {

		const unsigned size = Read_U32(chunk + 4);
		const unsigned char *body = chunk + 8;

		//	A chunk that claims more than the buffer holds ends the walk. Done
		//	as a subtraction on the remaining bytes so it cannot overflow.
		const unsigned remaining = (unsigned)(end - body);
		if (size > remaining) {
			break;
		}

		if (memcmp(chunk, "fmt ", 4) == 0 && size >= 16) {
			out.Format     = Read_U16(body + 0);
			out.Channels   = Read_U16(body + 2);
			out.Rate       = Read_U32(body + 4);
			out.BlockAlign = Read_U16(body + 12);
			out.Bits       = Read_U16(body + 14);
			have_fmt = true;
		} else if (memcmp(chunk, "data", 4) == 0) {
			out.Data      = body;
			out.DataBytes = size;
		}

		//	Chunks are word aligned. The step is always positive, so the loop
		//	cannot stall on a zero-sized chunk.
		const unsigned step = size + (size & 1) + 8;
		chunk += step;
	}

	return have_fmt && out.Data != NULL &&
			 out.Channels > 0 && out.Channels <= 2 && out.Rate > 0;
}

//-----------------------------------------------------------------------------
//	Device
//-----------------------------------------------------------------------------

static IXAudio2 *				_XAudio2			= NULL;
static IXAudio2MasteringVoice *_MasterVoice	= NULL;
static unsigned				_MasterChannels	= 2;
static X3DAUDIO_HANDLE		_X3DAudio;
static bool						_X3DAudioReady	= false;
static S32						_LastError		= AIL_NO_ERROR;
static DIG_DRIVER				_Driver2D		= { FALSE };

//	One provider is advertised: this implementation is its own 3D provider.
static const char *			PROVIDER_NAME	= "XAudio2 3D Positional Audio";
static const HPROVIDER		PROVIDER_HANDLE = (HPROVIDER)1;

/*
**	This module cannot use WWDEBUG_SAY: it stands in for a third-party library
**	and deliberately links nothing from the engine. Audio problems are silent by
**	nature, so the few decisions that matter are recorded here instead.
*/
static void Audio_Log(const char *fmt, ...)
{
	FILE *f = fopen("_audio.txt", "at");
	if (f == NULL) return;

	va_list args;
	va_start(args, fmt);
	vfprintf(f, fmt, args);
	va_end(args);

	fputc('\n', f);
	fclose(f);
}

static bool Device_Start(void)
{
	if (_XAudio2 != NULL) {
		return true;
	}

	//	The game calls CoInitialize before audio starts (cominit.cpp), but say so
	//	explicitly rather than relying on ordering.
	HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	const bool com_owned = SUCCEEDED(hr);

	hr = XAudio2Create(&_XAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
	if (FAILED(hr)) {
		Audio_Log("XAudio2Create failed, hr=0x%08X -- no sound", (unsigned)hr);
		if (com_owned) CoUninitialize();
		_XAudio2 = NULL;
		return false;
	}

	hr = _XAudio2->CreateMasteringVoice(&_MasterVoice);
	if (FAILED(hr)) {
		Audio_Log("CreateMasteringVoice failed, hr=0x%08X -- no sound", (unsigned)hr);
		_XAudio2->Release();
		_XAudio2 = NULL;
		_MasterVoice = NULL;
		return false;
	}

	XAUDIO2_VOICE_DETAILS details;
	_MasterVoice->GetVoiceDetails(&details);
	_MasterChannels = details.InputChannels;

	Audio_Log("XAudio2 device up: %u output channels, %u Hz",
				_MasterChannels, details.InputSampleRate);

	DWORD mask = 0;
	if (SUCCEEDED(_MasterVoice->GetChannelMask(&mask)) && mask != 0) {
		if (SUCCEEDED(X3DAudioInitialize(mask, X3DAUDIO_SPEED_OF_SOUND, _X3DAudio))) {
			_X3DAudioReady = true;
			Audio_Log("X3DAudio ready, channel mask 0x%08X", (unsigned)mask);
		}
	}

	return true;
}

static void Device_Stop(void)
{
	if (_MasterVoice != NULL) {
		_MasterVoice->DestroyVoice();
		_MasterVoice = NULL;
	}
	if (_XAudio2 != NULL) {
		_XAudio2->Release();
		_XAudio2 = NULL;
	}
	_X3DAudioReady = false;
}

//-----------------------------------------------------------------------------
//	Voices
//
//	_SAMPLE and _3D_OBJECT are declared but not defined in mss.h, so the engine
//	only ever holds pointers and the layout is ours to choose. Both are the same
//	thing underneath -- a source voice plus decoded audio -- differing only in
//	whether X3DAudio drives the output matrix.
//-----------------------------------------------------------------------------

struct MilesVoice
{
	IXAudio2SourceVoice *	Voice;
	short *						Pcm;
	unsigned						PcmSamples;		// per channel
	int							Channels;
	unsigned						Rate;
	S32							Volume;			// Miles 0..127
	S32							Pan;				// Miles 0..127, 64 centre
	S32							LoopCount;		// Miles: 0 == forever
	S32							PlaybackRate;	// Hz, 0 == use the file's rate
	S32							UserData[8];
	bool							Started;

	MilesVoice(void)
		: Voice(NULL), Pcm(NULL), PcmSamples(0), Channels(0), Rate(0),
		  Volume(MILES_VOLUME_MAX), Pan(MILES_PAN_CENTRE), LoopCount(1),
		  PlaybackRate(0), Started(false)
	{
		memset(UserData, 0, sizeof(UserData));
	}
};

struct _SAMPLE : public MilesVoice
{
};

struct _3D_OBJECT : public MilesVoice
{
	//	Position and orientation are kept whether or not this object is a
	//	listener; the listener simply never gets a voice.
	X3DAUDIO_VECTOR	Position;
	X3DAUDIO_VECTOR	Velocity;
	X3DAUDIO_VECTOR	Front;
	X3DAUDIO_VECTOR	Top;
	float					MinDistance;
	float					MaxDistance;
	bool					IsListener;

	_3D_OBJECT(void)
		: MinDistance(1.0f), MaxDistance(100.0f), IsListener(false)
	{
		Position.x = Position.y = Position.z = 0.0f;
		Velocity.x = Velocity.y = Velocity.z = 0.0f;
		Front.x = 0.0f; Front.y = 0.0f; Front.z = 1.0f;
		Top.x   = 0.0f; Top.y   = 1.0f; Top.z   = 0.0f;
	}
};

//	The single listener the engine opens, needed when positioning every 3D voice.
static _3D_OBJECT *_Listener = NULL;

static void Voice_Free_Audio(MilesVoice *v)
{
	if (v->Voice != NULL) {
		v->Voice->Stop(0);
		v->Voice->FlushSourceBuffers();
		v->Voice->DestroyVoice();
		v->Voice = NULL;
	}
	delete[] v->Pcm;
	v->Pcm = NULL;
	v->PcmSamples = 0;
}

/*
**	Decode a WAV image and give the voice a source to play. Shared by the 2D and
**	3D setters, which differ only in the handle type they are given.
*/
static bool Voice_Load(MilesVoice *v, const void *image, unsigned image_bytes)
{
	if (v == NULL) return false;

	Voice_Free_Audio(v);

	WavDescription wav;
	if (!Wav_Parse(image, image_bytes, wav)) {
		return false;
	}

	short *pcm = NULL;
	unsigned samples = 0;

	if (wav.Format == WAVE_FORMAT_IMA_ADPCM) {
		pcm = Ima_Decode(wav.Data, wav.DataBytes, wav.Channels, wav.BlockAlign, samples);
	} else if (wav.Format == WAVE_FORMAT_PCM && wav.Bits == 16) {
		samples = wav.DataBytes / (unsigned)(2 * wav.Channels);
		pcm = new short[samples * (unsigned)wav.Channels];
		if (pcm != NULL) {
			memcpy(pcm, wav.Data, (size_t)samples * wav.Channels * 2);
		}
	} else if (wav.Format == WAVE_FORMAT_PCM && wav.Bits == 8) {
		//	8-bit WAV samples are unsigned; shift them into signed 16-bit.
		samples = wav.DataBytes / (unsigned)wav.Channels;
		pcm = new short[samples * (unsigned)wav.Channels];
		if (pcm != NULL) {
			for (unsigned i = 0; i < samples * (unsigned)wav.Channels; ++i) {
				pcm[i] = (short)(((int)wav.Data[i] - 128) << 8);
			}
		}
	}

	if (pcm == NULL || samples == 0) {
		delete[] pcm;
		static int _fail_count = 0;
		if (++_fail_count <= 5) {
			Audio_Log("decode failed: format=%d channels=%d rate=%u bits=%d",
						 wav.Format, wav.Channels, wav.Rate, wav.Bits);
		}
		return false;
	}

	{
		static int _ok_count = 0;
		if (++_ok_count <= 5) {
			Audio_Log("decoded ok: format=%d channels=%d rate=%u -> %u samples",
						 wav.Format, wav.Channels, wav.Rate, samples);
		}
	}

	v->Pcm        = pcm;
	v->PcmSamples = samples;
	v->Channels   = wav.Channels;
	v->Rate       = wav.Rate;

	if (_XAudio2 == NULL) {
		return false;
	}

	WAVEFORMATEX fmt;
	memset(&fmt, 0, sizeof(fmt));
	fmt.wFormatTag      = WAVE_FORMAT_PCM;
	fmt.nChannels       = (WORD)wav.Channels;
	fmt.nSamplesPerSec  = wav.Rate;
	fmt.wBitsPerSample  = 16;
	fmt.nBlockAlign     = (WORD)(wav.Channels * 2);
	fmt.nAvgBytesPerSec = wav.Rate * fmt.nBlockAlign;

	if (FAILED(_XAudio2->CreateSourceVoice(&v->Voice, &fmt))) {
		v->Voice = NULL;
		return false;
	}

	return true;
}

static void Voice_Apply_Volume(MilesVoice *v)
{
	if (v->Voice != NULL) {
		v->Voice->SetVolume(Miles_Volume_To_Amplitude(v->Volume));
	}
}

/*
**	Constant-power pan across a stereo master. Mono sources get the usual
**	sin/cos pair; anything else is left alone rather than guessed at.
*/
static void Voice_Apply_Pan(MilesVoice *v)
{
	if (v->Voice == NULL || _MasterChannels != 2 || v->Channels != 1) {
		return;
	}

	S32 pan = v->Pan;
	if (pan < 0) pan = 0;
	if (pan > MILES_VOLUME_MAX) pan = MILES_VOLUME_MAX;

	const float t = (float)pan / (float)MILES_VOLUME_MAX;		// 0 left .. 1 right
	const float angle = t * 1.57079633f;								// 0 .. pi/2

	float matrix[2];
	matrix[0] = cosf(angle);
	matrix[1] = sinf(angle);

	v->Voice->SetOutputMatrix(NULL, 1, 2, matrix);
}

static void Voice_Submit_And_Play(MilesVoice *v)
{
	if (v->Voice == NULL || v->Pcm == NULL) {
		return;
	}

	v->Voice->Stop(0);
	v->Voice->FlushSourceBuffers();

	XAUDIO2_BUFFER buffer;
	memset(&buffer, 0, sizeof(buffer));
	buffer.AudioBytes = v->PcmSamples * (unsigned)v->Channels * 2;
	buffer.pAudioData = (const BYTE *)v->Pcm;
	buffer.Flags      = XAUDIO2_END_OF_STREAM;

	//	Miles counts total plays and treats 0 as "forever"; XAudio2 counts
	//	repeats after the first, so one is subtracted.
	if (v->LoopCount == 0) {
		buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
	} else if (v->LoopCount > 1) {
		buffer.LoopCount = (UINT32)(v->LoopCount - 1);
	}

	if (buffer.LoopCount != 0) {
		buffer.LoopBegin  = 0;
		buffer.LoopLength = v->PcmSamples;
	}

	v->Voice->SubmitSourceBuffer(&buffer);

	Voice_Apply_Volume(v);
	Voice_Apply_Pan(v);

	if (v->PlaybackRate > 0 && v->Rate > 0) {
		v->Voice->SetFrequencyRatio((float)v->PlaybackRate / (float)v->Rate);
	}

	v->Voice->Start(0);
	v->Started = true;
}

static void Voice_Position_Report(MilesVoice *v, S32 *total_ms, S32 *current_ms)
{
	unsigned total = 0;
	unsigned current = 0;

	if (v != NULL && v->Rate > 0) {
		total = (unsigned)(((double)v->PcmSamples * 1000.0) / (double)v->Rate);

		if (v->Voice != NULL) {
			XAUDIO2_VOICE_STATE state;
			memset(&state, 0, sizeof(state));
			v->Voice->GetState(&state);

			//	SamplesPlayed keeps counting across loops, so fold it back into
			//	the buffer to report a position rather than a running total.
			unsigned __int64 played = state.SamplesPlayed;
			if (v->PcmSamples > 0) {
				played %= (unsigned __int64)v->PcmSamples;
			}
			current = (unsigned)((played * 1000) / v->Rate);
		}
	}

	if (total_ms   != NULL) *total_ms   = (S32)total;
	if (current_ms != NULL) *current_ms = (S32)current;
}

//-----------------------------------------------------------------------------
//	3D positioning
//-----------------------------------------------------------------------------

static void Voice_Apply_3D(_3D_OBJECT *obj)
{
	if (obj == NULL || obj->IsListener || obj->Voice == NULL) {
		return;
	}
	if (!_X3DAudioReady || _Listener == NULL || obj->Channels != 1) {
		//	Without a listener, or for a source that is already multi-channel,
		//	fall back to plain volume so the sound is still audible.
		Voice_Apply_Volume(obj);
		return;
	}

	X3DAUDIO_LISTENER listener;
	memset(&listener, 0, sizeof(listener));
	listener.Position    = _Listener->Position;
	listener.Velocity    = _Listener->Velocity;
	listener.OrientFront = _Listener->Front;
	listener.OrientTop   = _Listener->Top;

	X3DAUDIO_EMITTER emitter;
	memset(&emitter, 0, sizeof(emitter));
	emitter.Position          = obj->Position;
	emitter.Velocity          = obj->Velocity;
	emitter.OrientFront       = obj->Front;
	emitter.OrientTop         = obj->Top;
	emitter.ChannelCount      = 1;
	emitter.CurveDistanceScaler = (obj->MinDistance > 0.0f) ? obj->MinDistance : 1.0f;
	emitter.DopplerScaler     = 1.0f;

	float matrix[8];			// enough for any master layout we will meet
	memset(matrix, 0, sizeof(matrix));

	X3DAUDIO_DSP_SETTINGS dsp;
	memset(&dsp, 0, sizeof(dsp));
	dsp.SrcChannelCount  = 1;
	dsp.DstChannelCount  = _MasterChannels;
	dsp.pMatrixCoefficients = matrix;

	if (_MasterChannels > (unsigned)(sizeof(matrix) / sizeof(matrix[0]))) {
		Voice_Apply_Volume(obj);
		return;
	}

	X3DAudioCalculate(_X3DAudio, &listener, &emitter,
							X3DAUDIO_CALCULATE_MATRIX | X3DAUDIO_CALCULATE_DOPPLER,
							&dsp);

	obj->Voice->SetOutputMatrix(NULL, 1, _MasterChannels, matrix);
	obj->Voice->SetVolume(Miles_Volume_To_Amplitude(obj->Volume));

	if (obj->PlaybackRate > 0 && obj->Rate > 0) {
		obj->Voice->SetFrequencyRatio(
			dsp.DopplerFactor * ((float)obj->PlaybackRate / (float)obj->Rate));
	} else {
		obj->Voice->SetFrequencyRatio(dsp.DopplerFactor);
	}
}

//-----------------------------------------------------------------------------
//	Startup / shutdown
//-----------------------------------------------------------------------------

extern "C" {

S32 AILEXPORT AIL_startup(void)
{
	_LastError = AIL_NO_ERROR;
	return Device_Start() ? AIL_NO_ERROR : 1;
}

void AILEXPORT AIL_shutdown(void)
{
	Device_Stop();
}

char *AILEXPORT AIL_last_error(void)
{
	//	Miles answers NULL when nothing has gone wrong.
	return (_LastError == AIL_NO_ERROR) ? NULL : (char *)"XAudio2 error";
}

void AILEXPORT AIL_serve(void)
{
	//	XAudio2 mixes on its own thread; there is no per-frame servicing to do.
}

void AILEXPORT AIL_lock(void)
{
}

void AILEXPORT AIL_unlock(void)
{
}

S32 AILEXPORT AIL_set_preference(U32 /*number*/, S32 /*value*/)
{
	//	The engine only uses these to choose between DirectSound and waveOut,
	//	a distinction XAudio2 does not expose. Accepting them keeps
	//	Open_2D_Device on its first, preferred path.
	return AIL_NO_ERROR;
}

void AILEXPORT AIL_set_file_callbacks(AIL_file_open_callback  /*opencb*/,
												  AIL_file_close_callback /*closecb*/,
												  AIL_file_seek_callback  /*seekcb*/,
												  AIL_file_read_callback  /*readcb*/)
{
	//	Only streams would read through these; samples arrive already in memory.
}

S32 AILEXPORT AIL_WAV_info(void const *data, AILSOUNDINFO *info)
{
	WavDescription wav;
	if (info == NULL || !Wav_Parse(data, 0, wav)) {
		return 0;
	}

	memset(info, 0, sizeof(*info));
	info->format     = wav.Format;
	info->data_ptr   = wav.Data;
	info->data_len   = wav.DataBytes;
	info->rate       = wav.Rate;
	info->bits       = wav.Bits;
	info->channels   = wav.Channels;
	info->block_size = wav.BlockAlign;
	info->initial_ptr = data;

	if (wav.Format == WAVE_FORMAT_IMA_ADPCM) {
		const int header_bytes = 4 * wav.Channels;
		if (wav.BlockAlign > header_bytes) {
			const int per_block = 1 + (((wav.BlockAlign - header_bytes) * 2) / wav.Channels);
			info->samples = (wav.DataBytes / (unsigned)wav.BlockAlign) * (unsigned)per_block;
		}
	} else if (wav.Bits > 0) {
		info->samples = wav.DataBytes / (unsigned)((wav.Bits / 8) * wav.Channels);
	}

	return 1;
}

//-----------------------------------------------------------------------------
//	Digital driver
//-----------------------------------------------------------------------------

S32 AILEXPORT AIL_waveOutOpen(HDIGDRIVER *drvr, HWAVEOUT * /*lphWaveOut*/,
										S32 /*wDeviceID*/, LPWAVEFORMAT /*lpFormat*/)
{
	if (drvr == NULL) {
		return 1;
	}

	if (!Device_Start()) {
		*drvr = NULL;
		return 1;
	}

	//	Reported as a real device, not a DirectSound emulation, so WWAudio keeps
	//	the driver it just opened instead of retrying on waveOut.
	_Driver2D.emulated_ds = FALSE;
	*drvr = &_Driver2D;
	return AIL_NO_ERROR;
}

void AILEXPORT AIL_waveOutClose(HDIGDRIVER /*drvr*/)
{
	//	The device outlives individual open/close pairs; AIL_shutdown tears it
	//	down. WWAudio closes and reopens the 2D driver when settings change.
}

//-----------------------------------------------------------------------------
//	Timers
//-----------------------------------------------------------------------------

void AILEXPORT AIL_stop_timer(HTIMER /*timer*/)
{
}

void AILEXPORT AIL_release_timer_handle(HTIMER /*timer*/)
{
}

//-----------------------------------------------------------------------------
//	2D samples
//-----------------------------------------------------------------------------

HSAMPLE AILEXPORT AIL_allocate_sample_handle(HDIGDRIVER /*dig*/)
{
	if (_XAudio2 == NULL && !Device_Start()) {
		return NULL;
	}
	return new _SAMPLE;
}

void AILEXPORT AIL_release_sample_handle(HSAMPLE S)
{
	if (S != NULL) {
		Voice_Free_Audio(S);
		delete S;
	}
}

void AILEXPORT AIL_init_sample(HSAMPLE S)
{
	if (S == NULL) return;

	Voice_Free_Audio(S);
	S->Volume       = MILES_VOLUME_MAX;
	S->Pan          = MILES_PAN_CENTRE;
	S->LoopCount    = 1;
	S->PlaybackRate = 0;
	S->Started      = false;
	memset(S->UserData, 0, sizeof(S->UserData));
}

S32 AILEXPORT AIL_set_named_sample_file(HSAMPLE S, char const * /*file_type_suffix*/,
													 void const *file_image, S32 file_size,
													 S32 /*block*/)
{
	if (S == NULL) return 0;
	return Voice_Load(S, file_image, (file_size > 0) ? (unsigned)file_size : 0) ? 1 : 0;
}

void AILEXPORT AIL_start_sample(HSAMPLE S)
{
	if (S != NULL) Voice_Submit_And_Play(S);
}

void AILEXPORT AIL_stop_sample(HSAMPLE S)
{
	if (S != NULL && S->Voice != NULL) S->Voice->Stop(0);
}

void AILEXPORT AIL_resume_sample(HSAMPLE S)
{
	if (S != NULL && S->Voice != NULL) S->Voice->Start(0);
}

void AILEXPORT AIL_end_sample(HSAMPLE S)
{
	if (S != NULL && S->Voice != NULL) {
		S->Voice->Stop(0);
		S->Voice->FlushSourceBuffers();
		S->Started = false;
	}
}

void AILEXPORT AIL_set_sample_volume(HSAMPLE S, S32 volume)
{
	if (S == NULL) return;
	S->Volume = volume;
	Voice_Apply_Volume(S);
}

S32 AILEXPORT AIL_sample_volume(HSAMPLE S)
{
	return (S != NULL) ? S->Volume : 0;
}

void AILEXPORT AIL_set_sample_pan(HSAMPLE S, S32 pan)
{
	if (S == NULL) return;
	S->Pan = pan;
	Voice_Apply_Pan(S);
}

S32 AILEXPORT AIL_sample_pan(HSAMPLE S)
{
	return (S != NULL) ? S->Pan : MILES_PAN_CENTRE;
}

void AILEXPORT AIL_set_sample_playback_rate(HSAMPLE S, S32 rate)
{
	if (S == NULL) return;
	S->PlaybackRate = rate;
	if (S->Voice != NULL && rate > 0 && S->Rate > 0) {
		S->Voice->SetFrequencyRatio((float)rate / (float)S->Rate);
	}
}

S32 AILEXPORT AIL_sample_playback_rate(HSAMPLE S)
{
	if (S == NULL) return 0;
	return (S->PlaybackRate > 0) ? S->PlaybackRate : (S32)S->Rate;
}

void AILEXPORT AIL_set_sample_loop_count(HSAMPLE S, S32 loops)
{
	if (S != NULL) S->LoopCount = loops;
}

S32 AILEXPORT AIL_sample_loop_count(HSAMPLE S)
{
	return (S != NULL) ? S->LoopCount : 0;
}

void AILEXPORT AIL_set_sample_ms_position(HSAMPLE S, S32 milliseconds)
{
	//	XAudio2 cannot seek a submitted buffer, so restart from an offset by
	//	resubmitting. Only a rewind to zero is exercised by the engine.
	if (S == NULL || S->Voice == NULL || S->Pcm == NULL) return;
	if (milliseconds <= 0) {
		Voice_Submit_And_Play(S);
	}
}

void AILEXPORT AIL_sample_ms_position(HSAMPLE S, S32 *total_ms, S32 *current_ms)
{
	Voice_Position_Report(S, total_ms, current_ms);
}

void AILEXPORT AIL_set_sample_user_data(HSAMPLE S, U32 index, S32 value)
{
	if (S != NULL && index < 8) S->UserData[index] = value;
}

S32 AILEXPORT AIL_sample_user_data(HSAMPLE S, U32 index)
{
	return (S != NULL && index < 8) ? S->UserData[index] : 0;
}

void AILEXPORT AIL_set_sample_processor(HSAMPLE /*S*/, U32 /*pipeline_stage*/,
													 HPROVIDER /*provider*/)
{
	//	Miles pipeline filters have no XAudio2 equivalent worth emulating.
}

//-----------------------------------------------------------------------------
//	Providers
//-----------------------------------------------------------------------------

S32 AILEXPORT AIL_enumerate_3D_providers(HPROENUM *next, HPROVIDER *dest, char **name)
{
	if (next == NULL || dest == NULL || name == NULL) return 0;

	//	Exactly one provider, so anything past the first ends the enumeration.
	if (*next != HPROENUM_FIRST) {
		return 0;
	}

	*dest = PROVIDER_HANDLE;
	*name = (char *)PROVIDER_NAME;
	*next = 1;
	return 1;
}

S32 AILEXPORT AIL_open_3D_provider(HPROVIDER lib)
{
	if (lib != PROVIDER_HANDLE) return 1;
	return Device_Start() ? AIL_NO_ERROR : 1;
}

void AILEXPORT AIL_close_3D_provider(HPROVIDER /*lib*/)
{
}

S32 AILEXPORT AIL_enumerate_filters(HPROENUM * /*next*/, HPROVIDER * /*dest*/,
												char ** /*name*/)
{
	//	No filter providers. WWAudio treats an empty list as "no filtering".
	return 0;
}

void AILEXPORT AIL_set_filter_sample_preference(HSAMPLE /*S*/, char const * /*preference_name*/,
																void const * /*value*/)
{
}

void AILEXPORT AIL_set_3D_speaker_type(HPROVIDER /*lib*/, S32 /*speaker_type*/)
{
	//	XAudio2 takes the speaker layout from the endpoint, so there is nothing
	//	to override here.
}

//-----------------------------------------------------------------------------
//	3D samples and listener
//-----------------------------------------------------------------------------

H3DPOBJECT AILEXPORT AIL_3D_open_listener(HPROVIDER /*lib*/)
{
	if (_Listener == NULL) {
		_Listener = new _3D_OBJECT;
		_Listener->IsListener = true;
	}
	return _Listener;
}

H3DSAMPLE AILEXPORT AIL_allocate_3D_sample_handle(HPROVIDER /*lib*/)
{
	if (_XAudio2 == NULL && !Device_Start()) {
		return NULL;
	}
	return new _3D_OBJECT;
}

void AILEXPORT AIL_release_3D_sample_handle(H3DSAMPLE S)
{
	if (S == NULL) return;
	Voice_Free_Audio(S);
	if (S == _Listener) {
		_Listener = NULL;
	}
	delete S;
}

S32 AILEXPORT AIL_set_3D_sample_file(H3DSAMPLE S, void const *file_image)
{
	if (S == NULL) return 0;
	return Voice_Load(S, file_image, 0) ? 1 : 0;
}

void AILEXPORT AIL_start_3D_sample(H3DSAMPLE S)
{
	if (S == NULL) return;
	Voice_Submit_And_Play(S);
	Voice_Apply_3D(S);
}

void AILEXPORT AIL_stop_3D_sample(H3DSAMPLE S)
{
	if (S != NULL && S->Voice != NULL) S->Voice->Stop(0);
}

void AILEXPORT AIL_resume_3D_sample(H3DSAMPLE S)
{
	if (S != NULL && S->Voice != NULL) S->Voice->Start(0);
}

void AILEXPORT AIL_end_3D_sample(H3DSAMPLE S)
{
	if (S != NULL && S->Voice != NULL) {
		S->Voice->Stop(0);
		S->Voice->FlushSourceBuffers();
		S->Started = false;
	}
}

void AILEXPORT AIL_set_3D_position(H3DPOBJECT obj, F32 X, F32 Y, F32 Z)
{
	if (obj == NULL) return;
	obj->Position.x = X;
	obj->Position.y = Y;
	obj->Position.z = Z;
	Voice_Apply_3D(obj);
}

void AILEXPORT AIL_set_3D_velocity_vector(H3DPOBJECT obj, F32 dX, F32 dY, F32 dZ)
{
	if (obj == NULL) return;
	obj->Velocity.x = dX;
	obj->Velocity.y = dY;
	obj->Velocity.z = dZ;
	Voice_Apply_3D(obj);
}

void AILEXPORT AIL_set_3D_orientation(H3DPOBJECT obj, F32 X_face, F32 Y_face, F32 Z_face,
												  F32 X_up, F32 Y_up, F32 Z_up)
{
	if (obj == NULL) return;
	obj->Front.x = X_face;
	obj->Front.y = Y_face;
	obj->Front.z = Z_face;
	obj->Top.x   = X_up;
	obj->Top.y   = Y_up;
	obj->Top.z   = Z_up;
	Voice_Apply_3D(obj);
}

void AILEXPORT AIL_set_3D_sample_volume(H3DSAMPLE S, S32 volume)
{
	if (S == NULL) return;
	S->Volume = volume;
	Voice_Apply_3D(S);
	if (S->Voice != NULL) {
		S->Voice->SetVolume(Miles_Volume_To_Amplitude(S->Volume));
	}
}

S32 AILEXPORT AIL_3D_sample_volume(H3DSAMPLE S)
{
	return (S != NULL) ? S->Volume : 0;
}

void AILEXPORT AIL_set_3D_sample_distances(H3DSAMPLE S, F32 max_dist, F32 min_dist)
{
	if (S == NULL) return;
	S->MaxDistance = max_dist;
	S->MinDistance = min_dist;
	Voice_Apply_3D(S);
}

void AILEXPORT AIL_set_3D_sample_effects_level(H3DSAMPLE /*S*/, F32 /*level*/)
{
	//	Reverb send level. No reverb graph is built, so nothing to set.
}

void AILEXPORT AIL_set_3D_sample_loop_count(H3DSAMPLE S, U32 loops)
{
	if (S != NULL) S->LoopCount = (S32)loops;
}

U32 AILEXPORT AIL_3D_sample_loop_count(H3DSAMPLE S)
{
	return (S != NULL) ? (U32)S->LoopCount : 0;
}

void AILEXPORT AIL_set_3D_sample_playback_rate(H3DSAMPLE S, S32 rate)
{
	if (S == NULL) return;
	S->PlaybackRate = rate;
	if (S->Voice != NULL && rate > 0 && S->Rate > 0) {
		S->Voice->SetFrequencyRatio((float)rate / (float)S->Rate);
	}
}

S32 AILEXPORT AIL_3D_sample_playback_rate(H3DSAMPLE S)
{
	if (S == NULL) return 0;
	return (S->PlaybackRate > 0) ? S->PlaybackRate : (S32)S->Rate;
}

void AILEXPORT AIL_set_3D_sample_offset(H3DSAMPLE S, U32 offset)
{
	if (S != NULL && S->Voice != NULL && offset == 0) {
		Voice_Submit_And_Play(S);
		Voice_Apply_3D(S);
	}
}

U32 AILEXPORT AIL_3D_sample_offset(H3DSAMPLE S)
{
	if (S == NULL || S->Voice == NULL) return 0;

	XAUDIO2_VOICE_STATE state;
	memset(&state, 0, sizeof(state));
	S->Voice->GetState(&state);

	unsigned __int64 played = state.SamplesPlayed;
	if (S->PcmSamples > 0) {
		played %= (unsigned __int64)S->PcmSamples;
	}
	return (U32)(played * (unsigned)S->Channels * 2);
}

U32 AILEXPORT AIL_3D_sample_length(H3DSAMPLE S)
{
	if (S == NULL) return 0;
	return (U32)(S->PcmSamples * (unsigned)S->Channels * 2);
}

void AILEXPORT AIL_set_3D_object_user_data(H3DPOBJECT obj, U32 index, S32 value)
{
	if (obj != NULL && index < 8) obj->UserData[index] = value;
}

S32 AILEXPORT AIL_3D_object_user_data(H3DPOBJECT obj, U32 index)
{
	return (obj != NULL && index < 8) ? obj->UserData[index] : 0;
}

//-----------------------------------------------------------------------------
//	Streams
//
//	Music is MP3 and streamed from disk, which needs a decoder this first pass
//	does not have. The entry points stay well-behaved so the music code runs
//	without producing sound.
//-----------------------------------------------------------------------------

struct _STREAM
{
	S32 Volume;
	S32 Pan;
	S32 LoopCount;
	S32 PlaybackRate;

	_STREAM(void) : Volume(MILES_VOLUME_MAX), Pan(MILES_PAN_CENTRE),
						 LoopCount(1), PlaybackRate(0) {}
};

HSTREAM AILEXPORT AIL_open_stream(HDIGDRIVER /*dig*/, char const * /*filename*/,
											 S32 /*stream_mem*/)
{
	return new _STREAM;
}

HSTREAM AILEXPORT AIL_open_stream_by_sample(HDIGDRIVER /*dig*/, HSAMPLE /*S*/,
														  char const * /*filename*/, S32 /*stream_mem*/)
{
	return new _STREAM;
}

void AILEXPORT AIL_close_stream(HSTREAM stream)
{
	delete stream;
}

void AILEXPORT AIL_start_stream(HSTREAM /*stream*/)
{
}

void AILEXPORT AIL_pause_stream(HSTREAM /*stream*/, S32 /*onoff*/)
{
}

void AILEXPORT AIL_set_stream_volume(HSTREAM stream, S32 volume)
{
	if (stream != NULL) stream->Volume = volume;
}

S32 AILEXPORT AIL_stream_volume(HSTREAM stream)
{
	return (stream != NULL) ? stream->Volume : 0;
}

void AILEXPORT AIL_set_stream_pan(HSTREAM stream, S32 pan)
{
	if (stream != NULL) stream->Pan = pan;
}

S32 AILEXPORT AIL_stream_pan(HSTREAM stream)
{
	return (stream != NULL) ? stream->Pan : MILES_PAN_CENTRE;
}

void AILEXPORT AIL_set_stream_playback_rate(HSTREAM stream, S32 rate)
{
	if (stream != NULL) stream->PlaybackRate = rate;
}

S32 AILEXPORT AIL_stream_playback_rate(HSTREAM stream)
{
	return (stream != NULL) ? stream->PlaybackRate : 0;
}

void AILEXPORT AIL_set_stream_loop_count(HSTREAM stream, S32 count)
{
	if (stream != NULL) stream->LoopCount = count;
}

S32 AILEXPORT AIL_stream_loop_count(HSTREAM stream)
{
	return (stream != NULL) ? stream->LoopCount : 0;
}

void AILEXPORT AIL_set_stream_loop_block(HSTREAM /*stream*/, S32 /*loop_start_offset*/,
													  S32 /*loop_end_offset*/)
{
}

void AILEXPORT AIL_set_stream_ms_position(HSTREAM /*stream*/, S32 /*milliseconds*/)
{
}

void AILEXPORT AIL_stream_ms_position(HSTREAM /*stream*/, S32 *total_ms, S32 *current_ms)
{
	if (total_ms   != NULL) *total_ms   = 0;
	if (current_ms != NULL) *current_ms = 0;
}

}	// extern "C"
