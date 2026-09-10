/*
**	Binds the Miles Sound System API to the retail mss32.dll at runtime.
**
**	Renegade shipped Miles 6 with the game -- mss32.dll, its .m3d 3D providers
**	and Mp3dec.asi -- so anyone who owns Renegade already has a licensed copy of
**	the sound system this engine was written against. Resolving it with
**	LoadLibrary/GetProcAddress, exactly as Code/Stubs/bink does for binkw32.dll,
**	gives the original audio in full: effects, 3D positioning, EAX reverb and
**	the streamed MP3 music that mss_xaudio2.cpp cannot play.
**
**	Nothing is redistributed and nothing is needed to build. Without the DLL
**	every entry point degrades to the silent no-op the stub always was, so the
**	game still runs -- see -DRENEGADE_MILES_RUNTIME=OFF for the self-contained
**	XAudio2 backend instead, which plays effects but no music.
**
**	Most of this file is mechanical: 75 of the 80 entry points are one-line
**	forwards, generated from mss.h and the DLL's export table so that the
**	__stdcall decorations ("_AIL_open_stream@12") match the declared signatures
**	byte for byte rather than being typed out by hand.
**
**	Three of the names in our mss.h were invented when nothing existed to check
**	them against, and the DLL calls them something else:
**
**	    AIL_3D_object_user_data     -> AIL_3D_user_data
**	    AIL_set_3D_object_user_data -> AIL_set_3D_user_data
**	    AIL_3D_open_listener        -> AIL_open_3D_listener
**
**	The five entry points that carry an HDIGDRIVER are hand-written, because
**	the engine reads a field out of that structure (WWAudio.cpp:316 tests
**	m_Driver2D->emulated_ds) and our DIG_DRIVER is a one-field invention that
**	will not match Miles' real layout. Handing the engine a shim of our own and
**	swapping in the real handle on the way back into the DLL keeps that
**	dereference reading something we defined. See Driver_Shim below.
*/

#include "mss.h"

#include <windows.h>
#include <stdio.h>
#include <stdarg.h>

//-----------------------------------------------------------------------------
//	Logging
//
//	This module links nothing from the engine, so it cannot use WWDEBUG_SAY.
//	It shares _audio.txt with the XAudio2 backend: only one of the two is ever
//	compiled in, and "why is there no sound" is answered in the same place
//	either way.
//-----------------------------------------------------------------------------

static void Miles_Log(const char *fmt, ...)
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

//-----------------------------------------------------------------------------
//	Imported entry points
//-----------------------------------------------------------------------------

typedef S32 (AILEXPORT *AIL_startup_Type)(void);
typedef void (AILEXPORT *AIL_shutdown_Type)(void);
typedef S32 (AILEXPORT *AIL_set_preference_Type)(U32 number, S32 value);
typedef char * (AILEXPORT *AIL_last_error_Type)(void);
typedef void (AILEXPORT *AIL_serve_Type)(void);
typedef void (AILEXPORT *AIL_lock_Type)(void);
typedef void (AILEXPORT *AIL_unlock_Type)(void);
typedef void (AILEXPORT *AIL_set_file_callbacks_Type)(AIL_file_open_callback opencb, AIL_file_close_callback closecb, AIL_file_seek_callback seekcb, AIL_file_read_callback readcb);
typedef S32 (AILEXPORT *AIL_WAV_info_Type)(void const *data, AILSOUNDINFO *info);
typedef S32 (AILEXPORT *AIL_waveOutOpen_Type)(HDIGDRIVER *drvr, HWAVEOUT *lphWaveOut, S32 wDeviceID, LPWAVEFORMAT lpFormat);
typedef void (AILEXPORT *AIL_waveOutClose_Type)(HDIGDRIVER drvr);
typedef void (AILEXPORT *AIL_stop_timer_Type)(HTIMER timer);
typedef void (AILEXPORT *AIL_release_timer_handle_Type)(HTIMER timer);
typedef HSAMPLE (AILEXPORT *AIL_allocate_sample_handle_Type)(HDIGDRIVER dig);
typedef void (AILEXPORT *AIL_release_sample_handle_Type)(HSAMPLE S);
typedef void (AILEXPORT *AIL_init_sample_Type)(HSAMPLE S);
typedef S32 (AILEXPORT *AIL_set_named_sample_file_Type)(HSAMPLE S, char const *file_type_suffix, void const *file_image, S32 file_image_size, S32 block);
typedef void (AILEXPORT *AIL_start_sample_Type)(HSAMPLE S);
typedef void (AILEXPORT *AIL_stop_sample_Type)(HSAMPLE S);
typedef void (AILEXPORT *AIL_resume_sample_Type)(HSAMPLE S);
typedef void (AILEXPORT *AIL_end_sample_Type)(HSAMPLE S);
typedef void (AILEXPORT *AIL_set_sample_volume_Type)(HSAMPLE S, S32 volume);
typedef S32 (AILEXPORT *AIL_sample_volume_Type)(HSAMPLE S);
typedef void (AILEXPORT *AIL_set_sample_pan_Type)(HSAMPLE S, S32 pan);
typedef S32 (AILEXPORT *AIL_sample_pan_Type)(HSAMPLE S);
typedef void (AILEXPORT *AIL_set_sample_playback_rate_Type)(HSAMPLE S, S32 rate);
typedef S32 (AILEXPORT *AIL_sample_playback_rate_Type)(HSAMPLE S);
typedef void (AILEXPORT *AIL_set_sample_loop_count_Type)(HSAMPLE S, S32 loops);
typedef S32 (AILEXPORT *AIL_sample_loop_count_Type)(HSAMPLE S);
typedef void (AILEXPORT *AIL_set_sample_ms_position_Type)(HSAMPLE S, S32 milliseconds);
typedef void (AILEXPORT *AIL_sample_ms_position_Type)(HSAMPLE S, S32 *total_ms, S32 *current_ms);
typedef void (AILEXPORT *AIL_set_sample_user_data_Type)(HSAMPLE S, U32 index, S32 value);
typedef S32 (AILEXPORT *AIL_sample_user_data_Type)(HSAMPLE S, U32 index);
typedef void (AILEXPORT *AIL_set_sample_processor_Type)(HSAMPLE S, U32 pipeline_stage, HPROVIDER provider);
typedef HSTREAM (AILEXPORT *AIL_open_stream_Type)(HDIGDRIVER dig, char const *filename, S32 stream_mem);
typedef HSTREAM (AILEXPORT *AIL_open_stream_by_sample_Type)(HDIGDRIVER dig, HSAMPLE S, char const *filename, S32 stream_mem);
typedef void (AILEXPORT *AIL_close_stream_Type)(HSTREAM stream);
typedef void (AILEXPORT *AIL_start_stream_Type)(HSTREAM stream);
typedef void (AILEXPORT *AIL_pause_stream_Type)(HSTREAM stream, S32 onoff);
typedef void (AILEXPORT *AIL_set_stream_volume_Type)(HSTREAM stream, S32 volume);
typedef S32 (AILEXPORT *AIL_stream_volume_Type)(HSTREAM stream);
typedef void (AILEXPORT *AIL_set_stream_pan_Type)(HSTREAM stream, S32 pan);
typedef S32 (AILEXPORT *AIL_stream_pan_Type)(HSTREAM stream);
typedef void (AILEXPORT *AIL_set_stream_playback_rate_Type)(HSTREAM stream, S32 rate);
typedef S32 (AILEXPORT *AIL_stream_playback_rate_Type)(HSTREAM stream);
typedef void (AILEXPORT *AIL_set_stream_loop_count_Type)(HSTREAM stream, S32 count);
typedef S32 (AILEXPORT *AIL_stream_loop_count_Type)(HSTREAM stream);
typedef void (AILEXPORT *AIL_set_stream_loop_block_Type)(HSTREAM stream, S32 loop_start_offset, S32 loop_end_offset);
typedef void (AILEXPORT *AIL_set_stream_ms_position_Type)(HSTREAM stream, S32 milliseconds);
typedef void (AILEXPORT *AIL_stream_ms_position_Type)(HSTREAM stream, S32 *total_ms, S32 *current_ms);
typedef S32 (AILEXPORT *AIL_enumerate_3D_providers_Type)(HPROENUM *next, HPROVIDER *dest, char **name);
typedef S32 (AILEXPORT *AIL_open_3D_provider_Type)(HPROVIDER lib);
typedef void (AILEXPORT *AIL_close_3D_provider_Type)(HPROVIDER lib);
typedef S32 (AILEXPORT *AIL_enumerate_filters_Type)(HPROENUM *next, HPROVIDER *dest, char **name);
typedef void (AILEXPORT *AIL_set_filter_sample_preference_Type)(HSAMPLE S, char const *preference_name, void const *value);
typedef void (AILEXPORT *AIL_set_3D_speaker_type_Type)(HPROVIDER lib, S32 speaker_type);
typedef H3DPOBJECT (AILEXPORT *AIL_3D_open_listener_Type)(HPROVIDER lib);
typedef H3DSAMPLE (AILEXPORT *AIL_allocate_3D_sample_handle_Type)(HPROVIDER lib);
typedef void (AILEXPORT *AIL_release_3D_sample_handle_Type)(H3DSAMPLE S);
typedef S32 (AILEXPORT *AIL_set_3D_sample_file_Type)(H3DSAMPLE S, void const *file_image);
typedef void (AILEXPORT *AIL_start_3D_sample_Type)(H3DSAMPLE S);
typedef void (AILEXPORT *AIL_stop_3D_sample_Type)(H3DSAMPLE S);
typedef void (AILEXPORT *AIL_resume_3D_sample_Type)(H3DSAMPLE S);
typedef void (AILEXPORT *AIL_end_3D_sample_Type)(H3DSAMPLE S);
typedef void (AILEXPORT *AIL_set_3D_position_Type)(H3DPOBJECT obj, F32 X, F32 Y, F32 Z);
typedef void (AILEXPORT *AIL_set_3D_velocity_vector_Type)(H3DPOBJECT obj, F32 dX, F32 dY, F32 dZ);
typedef void (AILEXPORT *AIL_set_3D_orientation_Type)(H3DPOBJECT obj, F32 X_face, F32 Y_face, F32 Z_face, F32 X_up, F32 Y_up, F32 Z_up);
typedef void (AILEXPORT *AIL_set_3D_sample_volume_Type)(H3DSAMPLE S, S32 volume);
typedef S32 (AILEXPORT *AIL_3D_sample_volume_Type)(H3DSAMPLE S);
typedef void (AILEXPORT *AIL_set_3D_sample_playback_rate_Type)(H3DSAMPLE S, S32 rate);
typedef S32 (AILEXPORT *AIL_3D_sample_playback_rate_Type)(H3DSAMPLE S);
typedef void (AILEXPORT *AIL_set_3D_sample_loop_count_Type)(H3DSAMPLE S, U32 loops);
typedef U32 (AILEXPORT *AIL_3D_sample_loop_count_Type)(H3DSAMPLE S);
typedef void (AILEXPORT *AIL_set_3D_sample_offset_Type)(H3DSAMPLE S, U32 offset);
typedef U32 (AILEXPORT *AIL_3D_sample_offset_Type)(H3DSAMPLE S);
typedef U32 (AILEXPORT *AIL_3D_sample_length_Type)(H3DSAMPLE S);
typedef void (AILEXPORT *AIL_set_3D_sample_distances_Type)(H3DSAMPLE S, F32 max_dist, F32 min_dist);
typedef void (AILEXPORT *AIL_set_3D_sample_effects_level_Type)(H3DSAMPLE S, F32 effects_level);
typedef void (AILEXPORT *AIL_set_3D_object_user_data_Type)(H3DPOBJECT obj, U32 index, S32 value);
typedef S32 (AILEXPORT *AIL_3D_object_user_data_Type)(H3DPOBJECT obj, U32 index);

static AIL_startup_Type Real_AIL_startup = NULL;
static AIL_shutdown_Type Real_AIL_shutdown = NULL;
static AIL_set_preference_Type Real_AIL_set_preference = NULL;
static AIL_last_error_Type Real_AIL_last_error = NULL;
static AIL_serve_Type Real_AIL_serve = NULL;
static AIL_lock_Type Real_AIL_lock = NULL;
static AIL_unlock_Type Real_AIL_unlock = NULL;
static AIL_set_file_callbacks_Type Real_AIL_set_file_callbacks = NULL;
static AIL_WAV_info_Type Real_AIL_WAV_info = NULL;
static AIL_waveOutOpen_Type Real_AIL_waveOutOpen = NULL;
static AIL_waveOutClose_Type Real_AIL_waveOutClose = NULL;
static AIL_stop_timer_Type Real_AIL_stop_timer = NULL;
static AIL_release_timer_handle_Type Real_AIL_release_timer_handle = NULL;
static AIL_allocate_sample_handle_Type Real_AIL_allocate_sample_handle = NULL;
static AIL_release_sample_handle_Type Real_AIL_release_sample_handle = NULL;
static AIL_init_sample_Type Real_AIL_init_sample = NULL;
static AIL_set_named_sample_file_Type Real_AIL_set_named_sample_file = NULL;
static AIL_start_sample_Type Real_AIL_start_sample = NULL;
static AIL_stop_sample_Type Real_AIL_stop_sample = NULL;
static AIL_resume_sample_Type Real_AIL_resume_sample = NULL;
static AIL_end_sample_Type Real_AIL_end_sample = NULL;
static AIL_set_sample_volume_Type Real_AIL_set_sample_volume = NULL;
static AIL_sample_volume_Type Real_AIL_sample_volume = NULL;
static AIL_set_sample_pan_Type Real_AIL_set_sample_pan = NULL;
static AIL_sample_pan_Type Real_AIL_sample_pan = NULL;
static AIL_set_sample_playback_rate_Type Real_AIL_set_sample_playback_rate = NULL;
static AIL_sample_playback_rate_Type Real_AIL_sample_playback_rate = NULL;
static AIL_set_sample_loop_count_Type Real_AIL_set_sample_loop_count = NULL;
static AIL_sample_loop_count_Type Real_AIL_sample_loop_count = NULL;
static AIL_set_sample_ms_position_Type Real_AIL_set_sample_ms_position = NULL;
static AIL_sample_ms_position_Type Real_AIL_sample_ms_position = NULL;
static AIL_set_sample_user_data_Type Real_AIL_set_sample_user_data = NULL;
static AIL_sample_user_data_Type Real_AIL_sample_user_data = NULL;
static AIL_set_sample_processor_Type Real_AIL_set_sample_processor = NULL;
static AIL_open_stream_Type Real_AIL_open_stream = NULL;
static AIL_open_stream_by_sample_Type Real_AIL_open_stream_by_sample = NULL;
static AIL_close_stream_Type Real_AIL_close_stream = NULL;
static AIL_start_stream_Type Real_AIL_start_stream = NULL;
static AIL_pause_stream_Type Real_AIL_pause_stream = NULL;
static AIL_set_stream_volume_Type Real_AIL_set_stream_volume = NULL;
static AIL_stream_volume_Type Real_AIL_stream_volume = NULL;
static AIL_set_stream_pan_Type Real_AIL_set_stream_pan = NULL;
static AIL_stream_pan_Type Real_AIL_stream_pan = NULL;
static AIL_set_stream_playback_rate_Type Real_AIL_set_stream_playback_rate = NULL;
static AIL_stream_playback_rate_Type Real_AIL_stream_playback_rate = NULL;
static AIL_set_stream_loop_count_Type Real_AIL_set_stream_loop_count = NULL;
static AIL_stream_loop_count_Type Real_AIL_stream_loop_count = NULL;
static AIL_set_stream_loop_block_Type Real_AIL_set_stream_loop_block = NULL;
static AIL_set_stream_ms_position_Type Real_AIL_set_stream_ms_position = NULL;
static AIL_stream_ms_position_Type Real_AIL_stream_ms_position = NULL;
static AIL_enumerate_3D_providers_Type Real_AIL_enumerate_3D_providers = NULL;
static AIL_open_3D_provider_Type Real_AIL_open_3D_provider = NULL;
static AIL_close_3D_provider_Type Real_AIL_close_3D_provider = NULL;
static AIL_enumerate_filters_Type Real_AIL_enumerate_filters = NULL;
static AIL_set_filter_sample_preference_Type Real_AIL_set_filter_sample_preference = NULL;
static AIL_set_3D_speaker_type_Type Real_AIL_set_3D_speaker_type = NULL;
static AIL_3D_open_listener_Type Real_AIL_3D_open_listener = NULL;
static AIL_allocate_3D_sample_handle_Type Real_AIL_allocate_3D_sample_handle = NULL;
static AIL_release_3D_sample_handle_Type Real_AIL_release_3D_sample_handle = NULL;
static AIL_set_3D_sample_file_Type Real_AIL_set_3D_sample_file = NULL;
static AIL_start_3D_sample_Type Real_AIL_start_3D_sample = NULL;
static AIL_stop_3D_sample_Type Real_AIL_stop_3D_sample = NULL;
static AIL_resume_3D_sample_Type Real_AIL_resume_3D_sample = NULL;
static AIL_end_3D_sample_Type Real_AIL_end_3D_sample = NULL;
static AIL_set_3D_position_Type Real_AIL_set_3D_position = NULL;
static AIL_set_3D_velocity_vector_Type Real_AIL_set_3D_velocity_vector = NULL;
static AIL_set_3D_orientation_Type Real_AIL_set_3D_orientation = NULL;
static AIL_set_3D_sample_volume_Type Real_AIL_set_3D_sample_volume = NULL;
static AIL_3D_sample_volume_Type Real_AIL_3D_sample_volume = NULL;
static AIL_set_3D_sample_playback_rate_Type Real_AIL_set_3D_sample_playback_rate = NULL;
static AIL_3D_sample_playback_rate_Type Real_AIL_3D_sample_playback_rate = NULL;
static AIL_set_3D_sample_loop_count_Type Real_AIL_set_3D_sample_loop_count = NULL;
static AIL_3D_sample_loop_count_Type Real_AIL_3D_sample_loop_count = NULL;
static AIL_set_3D_sample_offset_Type Real_AIL_set_3D_sample_offset = NULL;
static AIL_3D_sample_offset_Type Real_AIL_3D_sample_offset = NULL;
static AIL_3D_sample_length_Type Real_AIL_3D_sample_length = NULL;
static AIL_set_3D_sample_distances_Type Real_AIL_set_3D_sample_distances = NULL;
static AIL_set_3D_sample_effects_level_Type Real_AIL_set_3D_sample_effects_level = NULL;
static AIL_set_3D_object_user_data_Type Real_AIL_set_3D_object_user_data = NULL;
static AIL_3D_object_user_data_Type Real_AIL_3D_object_user_data = NULL;

static HMODULE MilesDll = NULL;
static bool    LoadAttempted = false;

/*
**	The engine dereferences its HDIGDRIVER (WWAudio.cpp:316) to decide between
**	DirectSound and WaveOut. Miles' real DIG_DRIVER is a large structure whose
**	layout we have no way to know, so the engine is given this shim instead and
**	the real handle is swapped back in at the five entry points that take one.
**
**	emulated_ds stays 0, i.e. "not emulated", which keeps the engine on its
**	preferred DirectSound path. Reporting the real flag would mean pinning down
**	its offset inside a structure we cannot see; reporting a wrong one would
**	silently drop the game to WaveOut.
*/
static DIG_DRIVER Driver_Shim = { 0 };
static HDIGDRIVER Real_Driver = NULL;

static HDIGDRIVER To_Real_Driver(HDIGDRIVER drvr)
{
	// Anything that is not our shim is passed through untouched.
	return (drvr == &Driver_Shim) ? Real_Driver : drvr;
}

/*
**	Resolve the DLL and every entry point once. Called from AIL_startup, which
**	Miles requires first and which WWAudio duly calls first (WWAudio.cpp:184).
**	Every thunk still null-checks, so an out-of-order call is a silent no-op
**	rather than a crash.
*/
static bool Load_Miles(void)
{
	if (LoadAttempted) {
		return MilesDll != NULL;
	}
	LoadAttempted = true;

	// Plain name, so the copy beside renegade.exe wins the search order.
	MilesDll = ::LoadLibraryA("mss32.dll");
	if (MilesDll == NULL) {
		Miles_Log("mss32.dll not found (error %lu); the game will be silent.",
					::GetLastError());
		return false;
	}

	Real_AIL_startup = (AIL_startup_Type)::GetProcAddress(MilesDll, "_AIL_startup@0");
	Real_AIL_shutdown = (AIL_shutdown_Type)::GetProcAddress(MilesDll, "_AIL_shutdown@0");
	Real_AIL_set_preference = (AIL_set_preference_Type)::GetProcAddress(MilesDll, "_AIL_set_preference@8");
	Real_AIL_last_error = (AIL_last_error_Type)::GetProcAddress(MilesDll, "_AIL_last_error@0");
	Real_AIL_serve = (AIL_serve_Type)::GetProcAddress(MilesDll, "_AIL_serve@0");
	Real_AIL_lock = (AIL_lock_Type)::GetProcAddress(MilesDll, "_AIL_lock@0");
	Real_AIL_unlock = (AIL_unlock_Type)::GetProcAddress(MilesDll, "_AIL_unlock@0");
	Real_AIL_set_file_callbacks = (AIL_set_file_callbacks_Type)::GetProcAddress(MilesDll, "_AIL_set_file_callbacks@16");
	Real_AIL_WAV_info = (AIL_WAV_info_Type)::GetProcAddress(MilesDll, "_AIL_WAV_info@8");
	Real_AIL_waveOutOpen = (AIL_waveOutOpen_Type)::GetProcAddress(MilesDll, "_AIL_waveOutOpen@16");
	Real_AIL_waveOutClose = (AIL_waveOutClose_Type)::GetProcAddress(MilesDll, "_AIL_waveOutClose@4");
	Real_AIL_stop_timer = (AIL_stop_timer_Type)::GetProcAddress(MilesDll, "_AIL_stop_timer@4");
	Real_AIL_release_timer_handle = (AIL_release_timer_handle_Type)::GetProcAddress(MilesDll, "_AIL_release_timer_handle@4");
	Real_AIL_allocate_sample_handle = (AIL_allocate_sample_handle_Type)::GetProcAddress(MilesDll, "_AIL_allocate_sample_handle@4");
	Real_AIL_release_sample_handle = (AIL_release_sample_handle_Type)::GetProcAddress(MilesDll, "_AIL_release_sample_handle@4");
	Real_AIL_init_sample = (AIL_init_sample_Type)::GetProcAddress(MilesDll, "_AIL_init_sample@4");
	Real_AIL_set_named_sample_file = (AIL_set_named_sample_file_Type)::GetProcAddress(MilesDll, "_AIL_set_named_sample_file@20");
	Real_AIL_start_sample = (AIL_start_sample_Type)::GetProcAddress(MilesDll, "_AIL_start_sample@4");
	Real_AIL_stop_sample = (AIL_stop_sample_Type)::GetProcAddress(MilesDll, "_AIL_stop_sample@4");
	Real_AIL_resume_sample = (AIL_resume_sample_Type)::GetProcAddress(MilesDll, "_AIL_resume_sample@4");
	Real_AIL_end_sample = (AIL_end_sample_Type)::GetProcAddress(MilesDll, "_AIL_end_sample@4");
	Real_AIL_set_sample_volume = (AIL_set_sample_volume_Type)::GetProcAddress(MilesDll, "_AIL_set_sample_volume@8");
	Real_AIL_sample_volume = (AIL_sample_volume_Type)::GetProcAddress(MilesDll, "_AIL_sample_volume@4");
	Real_AIL_set_sample_pan = (AIL_set_sample_pan_Type)::GetProcAddress(MilesDll, "_AIL_set_sample_pan@8");
	Real_AIL_sample_pan = (AIL_sample_pan_Type)::GetProcAddress(MilesDll, "_AIL_sample_pan@4");
	Real_AIL_set_sample_playback_rate = (AIL_set_sample_playback_rate_Type)::GetProcAddress(MilesDll, "_AIL_set_sample_playback_rate@8");
	Real_AIL_sample_playback_rate = (AIL_sample_playback_rate_Type)::GetProcAddress(MilesDll, "_AIL_sample_playback_rate@4");
	Real_AIL_set_sample_loop_count = (AIL_set_sample_loop_count_Type)::GetProcAddress(MilesDll, "_AIL_set_sample_loop_count@8");
	Real_AIL_sample_loop_count = (AIL_sample_loop_count_Type)::GetProcAddress(MilesDll, "_AIL_sample_loop_count@4");
	Real_AIL_set_sample_ms_position = (AIL_set_sample_ms_position_Type)::GetProcAddress(MilesDll, "_AIL_set_sample_ms_position@8");
	Real_AIL_sample_ms_position = (AIL_sample_ms_position_Type)::GetProcAddress(MilesDll, "_AIL_sample_ms_position@12");
	Real_AIL_set_sample_user_data = (AIL_set_sample_user_data_Type)::GetProcAddress(MilesDll, "_AIL_set_sample_user_data@12");
	Real_AIL_sample_user_data = (AIL_sample_user_data_Type)::GetProcAddress(MilesDll, "_AIL_sample_user_data@8");
	Real_AIL_set_sample_processor = (AIL_set_sample_processor_Type)::GetProcAddress(MilesDll, "_AIL_set_sample_processor@12");
	Real_AIL_open_stream = (AIL_open_stream_Type)::GetProcAddress(MilesDll, "_AIL_open_stream@12");
	Real_AIL_open_stream_by_sample = (AIL_open_stream_by_sample_Type)::GetProcAddress(MilesDll, "_AIL_open_stream_by_sample@16");
	Real_AIL_close_stream = (AIL_close_stream_Type)::GetProcAddress(MilesDll, "_AIL_close_stream@4");
	Real_AIL_start_stream = (AIL_start_stream_Type)::GetProcAddress(MilesDll, "_AIL_start_stream@4");
	Real_AIL_pause_stream = (AIL_pause_stream_Type)::GetProcAddress(MilesDll, "_AIL_pause_stream@8");
	Real_AIL_set_stream_volume = (AIL_set_stream_volume_Type)::GetProcAddress(MilesDll, "_AIL_set_stream_volume@8");
	Real_AIL_stream_volume = (AIL_stream_volume_Type)::GetProcAddress(MilesDll, "_AIL_stream_volume@4");
	Real_AIL_set_stream_pan = (AIL_set_stream_pan_Type)::GetProcAddress(MilesDll, "_AIL_set_stream_pan@8");
	Real_AIL_stream_pan = (AIL_stream_pan_Type)::GetProcAddress(MilesDll, "_AIL_stream_pan@4");
	Real_AIL_set_stream_playback_rate = (AIL_set_stream_playback_rate_Type)::GetProcAddress(MilesDll, "_AIL_set_stream_playback_rate@8");
	Real_AIL_stream_playback_rate = (AIL_stream_playback_rate_Type)::GetProcAddress(MilesDll, "_AIL_stream_playback_rate@4");
	Real_AIL_set_stream_loop_count = (AIL_set_stream_loop_count_Type)::GetProcAddress(MilesDll, "_AIL_set_stream_loop_count@8");
	Real_AIL_stream_loop_count = (AIL_stream_loop_count_Type)::GetProcAddress(MilesDll, "_AIL_stream_loop_count@4");
	Real_AIL_set_stream_loop_block = (AIL_set_stream_loop_block_Type)::GetProcAddress(MilesDll, "_AIL_set_stream_loop_block@12");
	Real_AIL_set_stream_ms_position = (AIL_set_stream_ms_position_Type)::GetProcAddress(MilesDll, "_AIL_set_stream_ms_position@8");
	Real_AIL_stream_ms_position = (AIL_stream_ms_position_Type)::GetProcAddress(MilesDll, "_AIL_stream_ms_position@12");
	Real_AIL_enumerate_3D_providers = (AIL_enumerate_3D_providers_Type)::GetProcAddress(MilesDll, "_AIL_enumerate_3D_providers@12");
	Real_AIL_open_3D_provider = (AIL_open_3D_provider_Type)::GetProcAddress(MilesDll, "_AIL_open_3D_provider@4");
	Real_AIL_close_3D_provider = (AIL_close_3D_provider_Type)::GetProcAddress(MilesDll, "_AIL_close_3D_provider@4");
	Real_AIL_enumerate_filters = (AIL_enumerate_filters_Type)::GetProcAddress(MilesDll, "_AIL_enumerate_filters@12");
	Real_AIL_set_filter_sample_preference = (AIL_set_filter_sample_preference_Type)::GetProcAddress(MilesDll, "_AIL_set_filter_sample_preference@12");
	Real_AIL_set_3D_speaker_type = (AIL_set_3D_speaker_type_Type)::GetProcAddress(MilesDll, "_AIL_set_3D_speaker_type@8");
	Real_AIL_3D_open_listener = (AIL_3D_open_listener_Type)::GetProcAddress(MilesDll, "_AIL_open_3D_listener@4");
	Real_AIL_allocate_3D_sample_handle = (AIL_allocate_3D_sample_handle_Type)::GetProcAddress(MilesDll, "_AIL_allocate_3D_sample_handle@4");
	Real_AIL_release_3D_sample_handle = (AIL_release_3D_sample_handle_Type)::GetProcAddress(MilesDll, "_AIL_release_3D_sample_handle@4");
	Real_AIL_set_3D_sample_file = (AIL_set_3D_sample_file_Type)::GetProcAddress(MilesDll, "_AIL_set_3D_sample_file@8");
	Real_AIL_start_3D_sample = (AIL_start_3D_sample_Type)::GetProcAddress(MilesDll, "_AIL_start_3D_sample@4");
	Real_AIL_stop_3D_sample = (AIL_stop_3D_sample_Type)::GetProcAddress(MilesDll, "_AIL_stop_3D_sample@4");
	Real_AIL_resume_3D_sample = (AIL_resume_3D_sample_Type)::GetProcAddress(MilesDll, "_AIL_resume_3D_sample@4");
	Real_AIL_end_3D_sample = (AIL_end_3D_sample_Type)::GetProcAddress(MilesDll, "_AIL_end_3D_sample@4");
	Real_AIL_set_3D_position = (AIL_set_3D_position_Type)::GetProcAddress(MilesDll, "_AIL_set_3D_position@16");
	Real_AIL_set_3D_velocity_vector = (AIL_set_3D_velocity_vector_Type)::GetProcAddress(MilesDll, "_AIL_set_3D_velocity_vector@16");
	Real_AIL_set_3D_orientation = (AIL_set_3D_orientation_Type)::GetProcAddress(MilesDll, "_AIL_set_3D_orientation@28");
	Real_AIL_set_3D_sample_volume = (AIL_set_3D_sample_volume_Type)::GetProcAddress(MilesDll, "_AIL_set_3D_sample_volume@8");
	Real_AIL_3D_sample_volume = (AIL_3D_sample_volume_Type)::GetProcAddress(MilesDll, "_AIL_3D_sample_volume@4");
	Real_AIL_set_3D_sample_playback_rate = (AIL_set_3D_sample_playback_rate_Type)::GetProcAddress(MilesDll, "_AIL_set_3D_sample_playback_rate@8");
	Real_AIL_3D_sample_playback_rate = (AIL_3D_sample_playback_rate_Type)::GetProcAddress(MilesDll, "_AIL_3D_sample_playback_rate@4");
	Real_AIL_set_3D_sample_loop_count = (AIL_set_3D_sample_loop_count_Type)::GetProcAddress(MilesDll, "_AIL_set_3D_sample_loop_count@8");
	Real_AIL_3D_sample_loop_count = (AIL_3D_sample_loop_count_Type)::GetProcAddress(MilesDll, "_AIL_3D_sample_loop_count@4");
	Real_AIL_set_3D_sample_offset = (AIL_set_3D_sample_offset_Type)::GetProcAddress(MilesDll, "_AIL_set_3D_sample_offset@8");
	Real_AIL_3D_sample_offset = (AIL_3D_sample_offset_Type)::GetProcAddress(MilesDll, "_AIL_3D_sample_offset@4");
	Real_AIL_3D_sample_length = (AIL_3D_sample_length_Type)::GetProcAddress(MilesDll, "_AIL_3D_sample_length@4");
	Real_AIL_set_3D_sample_distances = (AIL_set_3D_sample_distances_Type)::GetProcAddress(MilesDll, "_AIL_set_3D_sample_distances@12");
	Real_AIL_set_3D_sample_effects_level = (AIL_set_3D_sample_effects_level_Type)::GetProcAddress(MilesDll, "_AIL_set_3D_sample_effects_level@8");
	Real_AIL_set_3D_object_user_data = (AIL_set_3D_object_user_data_Type)::GetProcAddress(MilesDll, "_AIL_set_3D_user_data@12");
	Real_AIL_3D_object_user_data = (AIL_3D_object_user_data_Type)::GetProcAddress(MilesDll, "_AIL_3D_user_data@8");

	// AIL_startup is the one entry point that must exist; if it does not, this
	// is not the DLL we think it is.
	if (Real_AIL_startup == NULL) {
		Miles_Log("mss32.dll loaded but exports no AIL_startup; the game will be silent.");
		::FreeLibrary(MilesDll);
		MilesDll = NULL;
		return false;
	}

	Miles_Log("mss32.dll bound: the retail Miles Sound System is in use.");
	return true;
}

//-----------------------------------------------------------------------------
//	Driver handle entry points
//
//	These five are the reason this file is not generated wholesale: each has to
//	translate between the shim the engine holds and the handle Miles issued.
//-----------------------------------------------------------------------------

S32 AILEXPORT AIL_waveOutOpen(HDIGDRIVER *drvr, HWAVEOUT *lphWaveOut, S32 wDeviceID,
										LPWAVEFORMAT lpFormat)
{
	if (Real_AIL_waveOutOpen == NULL) return (S32)-1;

	HDIGDRIVER opened = NULL;
	S32 result = Real_AIL_waveOutOpen(&opened, lphWaveOut, wDeviceID, lpFormat);

	if (result == AIL_NO_ERROR && opened != NULL) {
		Real_Driver = opened;
		if (drvr != NULL) *drvr = &Driver_Shim;
	} else if (drvr != NULL) {
		*drvr = NULL;
	}
	return result;
}

void AILEXPORT AIL_waveOutClose(HDIGDRIVER drvr)
{
	if (Real_AIL_waveOutClose != NULL) Real_AIL_waveOutClose(To_Real_Driver(drvr));
	if (drvr == &Driver_Shim) Real_Driver = NULL;
}

HSAMPLE AILEXPORT AIL_allocate_sample_handle(HDIGDRIVER dig)
{
	if (Real_AIL_allocate_sample_handle == NULL) return (HSAMPLE)0;
	return Real_AIL_allocate_sample_handle(To_Real_Driver(dig));
}

HSTREAM AILEXPORT AIL_open_stream(HDIGDRIVER dig, char const *file_name, S32 stream_mem)
{
	if (Real_AIL_open_stream == NULL) return (HSTREAM)0;

	HSTREAM stream = Real_AIL_open_stream(To_Real_Driver(dig), file_name, stream_mem);
	Miles_Log("AIL_open_stream(%s) -> %s", (file_name != NULL) ? file_name : "(null)",
				(stream != NULL) ? "ok" : "FAILED");
	return stream;
}

HSTREAM AILEXPORT AIL_open_stream_by_sample(HDIGDRIVER dig, HSAMPLE S, char const *file_name,
														  S32 stream_mem)
{
	if (Real_AIL_open_stream_by_sample == NULL) return (HSTREAM)0;

	HSTREAM stream = Real_AIL_open_stream_by_sample(To_Real_Driver(dig), S, file_name, stream_mem);
	Miles_Log("AIL_open_stream_by_sample(%s) -> %s", (file_name != NULL) ? file_name : "(null)",
				(stream != NULL) ? "ok" : "FAILED");
	return stream;
}

//-----------------------------------------------------------------------------
//	Generated forwards
//-----------------------------------------------------------------------------

/*
**	Miles requires this first, so it is where the DLL is resolved.
*/
S32 AILEXPORT AIL_startup(void)
{
	if (!Load_Miles()) return (S32)-1;
	return Real_AIL_startup();
}

void AILEXPORT AIL_shutdown(void)
{
	if (Real_AIL_shutdown != NULL) Real_AIL_shutdown();
}

S32 AILEXPORT AIL_set_preference(U32 number, S32 value)
{
	if (Real_AIL_set_preference == NULL) return (S32)0;
	return Real_AIL_set_preference(number, value);
}

char * AILEXPORT AIL_last_error(void)
{
	if (Real_AIL_last_error == NULL) return (char *)0;
	return Real_AIL_last_error();
}

void AILEXPORT AIL_serve(void)
{
	if (Real_AIL_serve != NULL) Real_AIL_serve();
}

void AILEXPORT AIL_lock(void)
{
	if (Real_AIL_lock != NULL) Real_AIL_lock();
}

void AILEXPORT AIL_unlock(void)
{
	if (Real_AIL_unlock != NULL) Real_AIL_unlock();
}

void AILEXPORT AIL_set_file_callbacks(AIL_file_open_callback opencb, AIL_file_close_callback closecb, AIL_file_seek_callback seekcb, AIL_file_read_callback readcb)
{
	if (Real_AIL_set_file_callbacks != NULL) Real_AIL_set_file_callbacks(opencb, closecb, seekcb, readcb);
}

S32 AILEXPORT AIL_WAV_info(void const *data, AILSOUNDINFO *info)
{
	if (Real_AIL_WAV_info == NULL) return (S32)0;
	return Real_AIL_WAV_info(data, info);
}

void AILEXPORT AIL_stop_timer(HTIMER timer)
{
	if (Real_AIL_stop_timer != NULL) Real_AIL_stop_timer(timer);
}

void AILEXPORT AIL_release_timer_handle(HTIMER timer)
{
	if (Real_AIL_release_timer_handle != NULL) Real_AIL_release_timer_handle(timer);
}

void AILEXPORT AIL_release_sample_handle(HSAMPLE S)
{
	if (Real_AIL_release_sample_handle != NULL) Real_AIL_release_sample_handle(S);
}

void AILEXPORT AIL_init_sample(HSAMPLE S)
{
	if (Real_AIL_init_sample != NULL) Real_AIL_init_sample(S);
}

S32 AILEXPORT AIL_set_named_sample_file(HSAMPLE S, char const *file_type_suffix, void const *file_image, S32 file_image_size, S32 block)
{
	if (Real_AIL_set_named_sample_file == NULL) return (S32)0;
	return Real_AIL_set_named_sample_file(S, file_type_suffix, file_image, file_image_size, block);
}

void AILEXPORT AIL_start_sample(HSAMPLE S)
{
	if (Real_AIL_start_sample != NULL) Real_AIL_start_sample(S);
}

void AILEXPORT AIL_stop_sample(HSAMPLE S)
{
	if (Real_AIL_stop_sample != NULL) Real_AIL_stop_sample(S);
}

void AILEXPORT AIL_resume_sample(HSAMPLE S)
{
	if (Real_AIL_resume_sample != NULL) Real_AIL_resume_sample(S);
}

void AILEXPORT AIL_end_sample(HSAMPLE S)
{
	if (Real_AIL_end_sample != NULL) Real_AIL_end_sample(S);
}

void AILEXPORT AIL_set_sample_volume(HSAMPLE S, S32 volume)
{
	if (Real_AIL_set_sample_volume != NULL) Real_AIL_set_sample_volume(S, volume);
}

S32 AILEXPORT AIL_sample_volume(HSAMPLE S)
{
	if (Real_AIL_sample_volume == NULL) return (S32)0;
	return Real_AIL_sample_volume(S);
}

void AILEXPORT AIL_set_sample_pan(HSAMPLE S, S32 pan)
{
	if (Real_AIL_set_sample_pan != NULL) Real_AIL_set_sample_pan(S, pan);
}

S32 AILEXPORT AIL_sample_pan(HSAMPLE S)
{
	if (Real_AIL_sample_pan == NULL) return (S32)0;
	return Real_AIL_sample_pan(S);
}

void AILEXPORT AIL_set_sample_playback_rate(HSAMPLE S, S32 rate)
{
	if (Real_AIL_set_sample_playback_rate != NULL) Real_AIL_set_sample_playback_rate(S, rate);
}

S32 AILEXPORT AIL_sample_playback_rate(HSAMPLE S)
{
	if (Real_AIL_sample_playback_rate == NULL) return (S32)0;
	return Real_AIL_sample_playback_rate(S);
}

void AILEXPORT AIL_set_sample_loop_count(HSAMPLE S, S32 loops)
{
	if (Real_AIL_set_sample_loop_count != NULL) Real_AIL_set_sample_loop_count(S, loops);
}

S32 AILEXPORT AIL_sample_loop_count(HSAMPLE S)
{
	if (Real_AIL_sample_loop_count == NULL) return (S32)0;
	return Real_AIL_sample_loop_count(S);
}

void AILEXPORT AIL_set_sample_ms_position(HSAMPLE S, S32 milliseconds)
{
	if (Real_AIL_set_sample_ms_position != NULL) Real_AIL_set_sample_ms_position(S, milliseconds);
}

void AILEXPORT AIL_sample_ms_position(HSAMPLE S, S32 *total_ms, S32 *current_ms)
{
	if (Real_AIL_sample_ms_position != NULL) Real_AIL_sample_ms_position(S, total_ms, current_ms);
}

void AILEXPORT AIL_set_sample_user_data(HSAMPLE S, U32 index, S32 value)
{
	if (Real_AIL_set_sample_user_data != NULL) Real_AIL_set_sample_user_data(S, index, value);
}

S32 AILEXPORT AIL_sample_user_data(HSAMPLE S, U32 index)
{
	if (Real_AIL_sample_user_data == NULL) return (S32)0;
	return Real_AIL_sample_user_data(S, index);
}

void AILEXPORT AIL_set_sample_processor(HSAMPLE S, U32 pipeline_stage, HPROVIDER provider)
{
	if (Real_AIL_set_sample_processor != NULL) Real_AIL_set_sample_processor(S, pipeline_stage, provider);
}

void AILEXPORT AIL_close_stream(HSTREAM stream)
{
	if (Real_AIL_close_stream != NULL) Real_AIL_close_stream(stream);
}

void AILEXPORT AIL_start_stream(HSTREAM stream)
{
	if (Real_AIL_start_stream != NULL) Real_AIL_start_stream(stream);
}

void AILEXPORT AIL_pause_stream(HSTREAM stream, S32 onoff)
{
	if (Real_AIL_pause_stream != NULL) Real_AIL_pause_stream(stream, onoff);
}

void AILEXPORT AIL_set_stream_volume(HSTREAM stream, S32 volume)
{
	if (Real_AIL_set_stream_volume != NULL) Real_AIL_set_stream_volume(stream, volume);
}

S32 AILEXPORT AIL_stream_volume(HSTREAM stream)
{
	if (Real_AIL_stream_volume == NULL) return (S32)0;
	return Real_AIL_stream_volume(stream);
}

void AILEXPORT AIL_set_stream_pan(HSTREAM stream, S32 pan)
{
	if (Real_AIL_set_stream_pan != NULL) Real_AIL_set_stream_pan(stream, pan);
}

S32 AILEXPORT AIL_stream_pan(HSTREAM stream)
{
	if (Real_AIL_stream_pan == NULL) return (S32)0;
	return Real_AIL_stream_pan(stream);
}

void AILEXPORT AIL_set_stream_playback_rate(HSTREAM stream, S32 rate)
{
	if (Real_AIL_set_stream_playback_rate != NULL) Real_AIL_set_stream_playback_rate(stream, rate);
}

S32 AILEXPORT AIL_stream_playback_rate(HSTREAM stream)
{
	if (Real_AIL_stream_playback_rate == NULL) return (S32)0;
	return Real_AIL_stream_playback_rate(stream);
}

void AILEXPORT AIL_set_stream_loop_count(HSTREAM stream, S32 count)
{
	if (Real_AIL_set_stream_loop_count != NULL) Real_AIL_set_stream_loop_count(stream, count);
}

S32 AILEXPORT AIL_stream_loop_count(HSTREAM stream)
{
	if (Real_AIL_stream_loop_count == NULL) return (S32)0;
	return Real_AIL_stream_loop_count(stream);
}

void AILEXPORT AIL_set_stream_loop_block(HSTREAM stream, S32 loop_start_offset, S32 loop_end_offset)
{
	if (Real_AIL_set_stream_loop_block != NULL) Real_AIL_set_stream_loop_block(stream, loop_start_offset, loop_end_offset);
}

void AILEXPORT AIL_set_stream_ms_position(HSTREAM stream, S32 milliseconds)
{
	if (Real_AIL_set_stream_ms_position != NULL) Real_AIL_set_stream_ms_position(stream, milliseconds);
}

void AILEXPORT AIL_stream_ms_position(HSTREAM stream, S32 *total_ms, S32 *current_ms)
{
	if (Real_AIL_stream_ms_position != NULL) Real_AIL_stream_ms_position(stream, total_ms, current_ms);
}

S32 AILEXPORT AIL_enumerate_3D_providers(HPROENUM *next, HPROVIDER *dest, char **name)
{
	if (Real_AIL_enumerate_3D_providers == NULL) return (S32)0;
	return Real_AIL_enumerate_3D_providers(next, dest, name);
}

S32 AILEXPORT AIL_open_3D_provider(HPROVIDER lib)
{
	if (Real_AIL_open_3D_provider == NULL) return (S32)0;
	return Real_AIL_open_3D_provider(lib);
}

void AILEXPORT AIL_close_3D_provider(HPROVIDER lib)
{
	if (Real_AIL_close_3D_provider != NULL) Real_AIL_close_3D_provider(lib);
}

S32 AILEXPORT AIL_enumerate_filters(HPROENUM *next, HPROVIDER *dest, char **name)
{
	if (Real_AIL_enumerate_filters == NULL) return (S32)0;
	return Real_AIL_enumerate_filters(next, dest, name);
}

void AILEXPORT AIL_set_filter_sample_preference(HSAMPLE S, char const *preference_name, void const *value)
{
	if (Real_AIL_set_filter_sample_preference != NULL) Real_AIL_set_filter_sample_preference(S, preference_name, value);
}

void AILEXPORT AIL_set_3D_speaker_type(HPROVIDER lib, S32 speaker_type)
{
	if (Real_AIL_set_3D_speaker_type != NULL) Real_AIL_set_3D_speaker_type(lib, speaker_type);
}

H3DPOBJECT AILEXPORT AIL_3D_open_listener(HPROVIDER lib)
{
	if (Real_AIL_3D_open_listener == NULL) return (H3DPOBJECT)0;
	return Real_AIL_3D_open_listener(lib);
}

H3DSAMPLE AILEXPORT AIL_allocate_3D_sample_handle(HPROVIDER lib)
{
	if (Real_AIL_allocate_3D_sample_handle == NULL) return (H3DSAMPLE)0;
	return Real_AIL_allocate_3D_sample_handle(lib);
}

void AILEXPORT AIL_release_3D_sample_handle(H3DSAMPLE S)
{
	if (Real_AIL_release_3D_sample_handle != NULL) Real_AIL_release_3D_sample_handle(S);
}

S32 AILEXPORT AIL_set_3D_sample_file(H3DSAMPLE S, void const *file_image)
{
	if (Real_AIL_set_3D_sample_file == NULL) return (S32)0;
	return Real_AIL_set_3D_sample_file(S, file_image);
}

void AILEXPORT AIL_start_3D_sample(H3DSAMPLE S)
{
	if (Real_AIL_start_3D_sample != NULL) Real_AIL_start_3D_sample(S);
}

void AILEXPORT AIL_stop_3D_sample(H3DSAMPLE S)
{
	if (Real_AIL_stop_3D_sample != NULL) Real_AIL_stop_3D_sample(S);
}

void AILEXPORT AIL_resume_3D_sample(H3DSAMPLE S)
{
	if (Real_AIL_resume_3D_sample != NULL) Real_AIL_resume_3D_sample(S);
}

void AILEXPORT AIL_end_3D_sample(H3DSAMPLE S)
{
	if (Real_AIL_end_3D_sample != NULL) Real_AIL_end_3D_sample(S);
}

void AILEXPORT AIL_set_3D_position(H3DPOBJECT obj, F32 X, F32 Y, F32 Z)
{
	if (Real_AIL_set_3D_position != NULL) Real_AIL_set_3D_position(obj, X, Y, Z);
}

void AILEXPORT AIL_set_3D_velocity_vector(H3DPOBJECT obj, F32 dX, F32 dY, F32 dZ)
{
	if (Real_AIL_set_3D_velocity_vector != NULL) Real_AIL_set_3D_velocity_vector(obj, dX, dY, dZ);
}

void AILEXPORT AIL_set_3D_orientation(H3DPOBJECT obj, F32 X_face, F32 Y_face, F32 Z_face, F32 X_up, F32 Y_up, F32 Z_up)
{
	if (Real_AIL_set_3D_orientation != NULL) Real_AIL_set_3D_orientation(obj, X_face, Y_face, Z_face, X_up, Y_up, Z_up);
}

void AILEXPORT AIL_set_3D_sample_volume(H3DSAMPLE S, S32 volume)
{
	if (Real_AIL_set_3D_sample_volume != NULL) Real_AIL_set_3D_sample_volume(S, volume);
}

S32 AILEXPORT AIL_3D_sample_volume(H3DSAMPLE S)
{
	if (Real_AIL_3D_sample_volume == NULL) return (S32)0;
	return Real_AIL_3D_sample_volume(S);
}

void AILEXPORT AIL_set_3D_sample_playback_rate(H3DSAMPLE S, S32 rate)
{
	if (Real_AIL_set_3D_sample_playback_rate != NULL) Real_AIL_set_3D_sample_playback_rate(S, rate);
}

S32 AILEXPORT AIL_3D_sample_playback_rate(H3DSAMPLE S)
{
	if (Real_AIL_3D_sample_playback_rate == NULL) return (S32)0;
	return Real_AIL_3D_sample_playback_rate(S);
}

void AILEXPORT AIL_set_3D_sample_loop_count(H3DSAMPLE S, U32 loops)
{
	if (Real_AIL_set_3D_sample_loop_count != NULL) Real_AIL_set_3D_sample_loop_count(S, loops);
}

U32 AILEXPORT AIL_3D_sample_loop_count(H3DSAMPLE S)
{
	if (Real_AIL_3D_sample_loop_count == NULL) return (U32)0;
	return Real_AIL_3D_sample_loop_count(S);
}

void AILEXPORT AIL_set_3D_sample_offset(H3DSAMPLE S, U32 offset)
{
	if (Real_AIL_set_3D_sample_offset != NULL) Real_AIL_set_3D_sample_offset(S, offset);
}

U32 AILEXPORT AIL_3D_sample_offset(H3DSAMPLE S)
{
	if (Real_AIL_3D_sample_offset == NULL) return (U32)0;
	return Real_AIL_3D_sample_offset(S);
}

U32 AILEXPORT AIL_3D_sample_length(H3DSAMPLE S)
{
	if (Real_AIL_3D_sample_length == NULL) return (U32)0;
	return Real_AIL_3D_sample_length(S);
}

void AILEXPORT AIL_set_3D_sample_distances(H3DSAMPLE S, F32 max_dist, F32 min_dist)
{
	if (Real_AIL_set_3D_sample_distances != NULL) Real_AIL_set_3D_sample_distances(S, max_dist, min_dist);
}

void AILEXPORT AIL_set_3D_sample_effects_level(H3DSAMPLE S, F32 effects_level)
{
	if (Real_AIL_set_3D_sample_effects_level != NULL) Real_AIL_set_3D_sample_effects_level(S, effects_level);
}

void AILEXPORT AIL_set_3D_object_user_data(H3DPOBJECT obj, U32 index, S32 value)
{
	if (Real_AIL_set_3D_object_user_data != NULL) Real_AIL_set_3D_object_user_data(obj, index, value);
}

S32 AILEXPORT AIL_3D_object_user_data(H3DPOBJECT obj, U32 index)
{
	if (Real_AIL_3D_object_user_data == NULL) return (S32)0;
	return Real_AIL_3D_object_user_data(obj, index);
}

