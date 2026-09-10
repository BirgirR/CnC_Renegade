/*
**	No-op implementation of the Miles Sound System 6 stub declared in mss.h.
**
**	Link this in place of mss32.lib to get a building, running, SILENT game.
**	Nothing here produces audio -- the point is to unblock compilation and
**	linking so the remaining port work is reachable, and to give a real audio
**	backend (OpenAL, XAudio2, miniaudio) a single file to grow into.
**
**	Handles are handed out as small non-NULL sentinels because the calling code
**	in WWAudio treats NULL as allocation failure and will otherwise thrash
**	retrying. They are never dereferenced here.
*/

#include "mss.h"

namespace {

// Distinct non-NULL sentinels; never dereferenced.
char Sample_Sentinel;
char Sample3D_Sentinel;
char Stream_Sentinel;
// Real instance, not a sentinel: the game reads emulated_ds through this.
DIG_DRIVER Driver_Instance = { 0 };
char Listener_Sentinel;

char No_Error[] = "";

} // namespace

//-----------------------------------------------------------------------------
// System
//-----------------------------------------------------------------------------

S32   AILEXPORT AIL_startup        (void)                    { return 1; }
void  AILEXPORT AIL_shutdown       (void)                    {}
S32   AILEXPORT AIL_set_preference (U32, S32)                { return 0; }
char *AILEXPORT AIL_last_error     (void)                    { return No_Error; }
void  AILEXPORT AIL_serve          (void)                    {}
void  AILEXPORT AIL_lock           (void)                    {}
void  AILEXPORT AIL_unlock         (void)                    {}

void  AILEXPORT AIL_set_file_callbacks (AIL_file_open_callback,
                                        AIL_file_close_callback,
                                        AIL_file_seek_callback,
                                        AIL_file_read_callback) {}

// Reported as "not a WAV" so callers fall back to their own handling rather
// than trusting zeroed format fields.
S32   AILEXPORT AIL_WAV_info (void const *, AILSOUNDINFO *)  { return 0; }

//-----------------------------------------------------------------------------
// Digital driver
//-----------------------------------------------------------------------------

S32 AILEXPORT AIL_waveOutOpen (HDIGDRIVER *drvr, HWAVEOUT *, S32, LPWAVEFORMAT)
{
   if (drvr != 0) {
      *drvr = &Driver_Instance;
   }
   return AIL_NO_ERROR;
}

void AILEXPORT AIL_waveOutClose (HDIGDRIVER) {}

//-----------------------------------------------------------------------------
// Timers
//-----------------------------------------------------------------------------

void AILEXPORT AIL_stop_timer           (HTIMER) {}
void AILEXPORT AIL_release_timer_handle (HTIMER) {}

//-----------------------------------------------------------------------------
// 2D samples
//-----------------------------------------------------------------------------

HSAMPLE AILEXPORT AIL_allocate_sample_handle (HDIGDRIVER)
{
   return reinterpret_cast<HSAMPLE>(&Sample_Sentinel);
}

void AILEXPORT AIL_release_sample_handle (HSAMPLE) {}
void AILEXPORT AIL_init_sample           (HSAMPLE) {}

S32  AILEXPORT AIL_set_named_sample_file (HSAMPLE, char const *, void const *, S32, S32)
{
   return 1;
}

void AILEXPORT AIL_start_sample  (HSAMPLE) {}
void AILEXPORT AIL_stop_sample   (HSAMPLE) {}
void AILEXPORT AIL_resume_sample (HSAMPLE) {}
void AILEXPORT AIL_end_sample    (HSAMPLE) {}

void AILEXPORT AIL_set_sample_volume        (HSAMPLE, S32) {}
S32  AILEXPORT AIL_sample_volume            (HSAMPLE)      { return 0; }
void AILEXPORT AIL_set_sample_pan           (HSAMPLE, S32) {}
S32  AILEXPORT AIL_sample_pan               (HSAMPLE)      { return 64; }
void AILEXPORT AIL_set_sample_playback_rate (HSAMPLE, S32) {}
S32  AILEXPORT AIL_sample_playback_rate     (HSAMPLE)      { return 44100; }
void AILEXPORT AIL_set_sample_loop_count    (HSAMPLE, S32) {}
S32  AILEXPORT AIL_sample_loop_count        (HSAMPLE)      { return 0; }
void AILEXPORT AIL_set_sample_ms_position   (HSAMPLE, S32) {}

void AILEXPORT AIL_sample_ms_position (HSAMPLE, S32 *total_ms, S32 *current_ms)
{
   if (total_ms   != 0) *total_ms   = 0;
   if (current_ms != 0) *current_ms = 0;
}

void AILEXPORT AIL_set_sample_user_data (HSAMPLE, U32, S32) {}
S32  AILEXPORT AIL_sample_user_data     (HSAMPLE, U32)      { return 0; }

void AILEXPORT AIL_set_sample_processor (HSAMPLE, U32, HPROVIDER) {}

//-----------------------------------------------------------------------------
// Streams
//-----------------------------------------------------------------------------

HSTREAM AILEXPORT AIL_open_stream (HDIGDRIVER, char const *, S32)
{
   return reinterpret_cast<HSTREAM>(&Stream_Sentinel);
}

HSTREAM AILEXPORT AIL_open_stream_by_sample (HDIGDRIVER, HSAMPLE, char const *, S32)
{
   return reinterpret_cast<HSTREAM>(&Stream_Sentinel);
}

void AILEXPORT AIL_close_stream (HSTREAM)      {}
void AILEXPORT AIL_start_stream (HSTREAM)      {}
void AILEXPORT AIL_pause_stream (HSTREAM, S32) {}

void AILEXPORT AIL_set_stream_volume        (HSTREAM, S32) {}
S32  AILEXPORT AIL_stream_volume            (HSTREAM)      { return 0; }
void AILEXPORT AIL_set_stream_pan           (HSTREAM, S32) {}
S32  AILEXPORT AIL_stream_pan               (HSTREAM)      { return 64; }
void AILEXPORT AIL_set_stream_playback_rate (HSTREAM, S32) {}
S32  AILEXPORT AIL_stream_playback_rate     (HSTREAM)      { return 44100; }
void AILEXPORT AIL_set_stream_loop_count    (HSTREAM, S32) {}
S32  AILEXPORT AIL_stream_loop_count        (HSTREAM)      { return 0; }
void AILEXPORT AIL_set_stream_loop_block    (HSTREAM, S32, S32) {}
void AILEXPORT AIL_set_stream_ms_position   (HSTREAM, S32) {}

void AILEXPORT AIL_stream_ms_position (HSTREAM, S32 *total_ms, S32 *current_ms)
{
   if (total_ms   != 0) *total_ms   = 0;
   if (current_ms != 0) *current_ms = 0;
}

//-----------------------------------------------------------------------------
// Providers
//
// Enumeration reports "no more providers" immediately, so the game falls back
// to 2D audio instead of waiting on a 3D device that will never appear.
//-----------------------------------------------------------------------------

S32 AILEXPORT AIL_enumerate_3D_providers (HPROENUM *, HPROVIDER *, char **)
{
   return 0;
}

S32   AILEXPORT AIL_open_3D_provider  (HPROVIDER) { return M3D_NOERR; }
void  AILEXPORT AIL_close_3D_provider (HPROVIDER) {}

S32 AILEXPORT AIL_enumerate_filters (HPROENUM *, HPROVIDER *, char **)
{
   return 0;
}

void AILEXPORT AIL_set_filter_sample_preference (HSAMPLE, char const *, void const *) {}
void AILEXPORT AIL_set_3D_speaker_type          (HPROVIDER, S32) {}

//-----------------------------------------------------------------------------
// 3D samples and listener
//-----------------------------------------------------------------------------

H3DPOBJECT AILEXPORT AIL_3D_open_listener (HPROVIDER)
{
   return reinterpret_cast<H3DPOBJECT>(&Listener_Sentinel);
}

H3DSAMPLE AILEXPORT AIL_allocate_3D_sample_handle (HPROVIDER)
{
   return reinterpret_cast<H3DSAMPLE>(&Sample3D_Sentinel);
}

void AILEXPORT AIL_release_3D_sample_handle (H3DSAMPLE) {}

S32  AILEXPORT AIL_set_3D_sample_file (H3DSAMPLE, void const *) { return 1; }

void AILEXPORT AIL_start_3D_sample  (H3DSAMPLE) {}
void AILEXPORT AIL_stop_3D_sample   (H3DSAMPLE) {}
void AILEXPORT AIL_resume_3D_sample (H3DSAMPLE) {}
void AILEXPORT AIL_end_3D_sample    (H3DSAMPLE) {}

void AILEXPORT AIL_set_3D_position        (H3DPOBJECT, F32, F32, F32) {}
void AILEXPORT AIL_set_3D_velocity_vector (H3DPOBJECT, F32, F32, F32) {}
void AILEXPORT AIL_set_3D_orientation     (H3DPOBJECT, F32, F32, F32, F32, F32, F32) {}

void AILEXPORT AIL_set_3D_sample_volume        (H3DSAMPLE, S32) {}
S32  AILEXPORT AIL_3D_sample_volume            (H3DSAMPLE)      { return 0; }
void AILEXPORT AIL_set_3D_sample_playback_rate (H3DSAMPLE, S32) {}
S32  AILEXPORT AIL_3D_sample_playback_rate     (H3DSAMPLE)      { return 44100; }
void AILEXPORT AIL_set_3D_sample_loop_count    (H3DSAMPLE, U32) {}
U32  AILEXPORT AIL_3D_sample_loop_count        (H3DSAMPLE)      { return 0; }
void AILEXPORT AIL_set_3D_sample_offset        (H3DSAMPLE, U32) {}
U32  AILEXPORT AIL_3D_sample_offset            (H3DSAMPLE)      { return 0; }
U32  AILEXPORT AIL_3D_sample_length            (H3DSAMPLE)      { return 0; }
void AILEXPORT AIL_set_3D_sample_distances     (H3DSAMPLE, F32, F32) {}
void AILEXPORT AIL_set_3D_sample_effects_level (H3DSAMPLE, F32) {}

void AILEXPORT AIL_set_3D_object_user_data (H3DPOBJECT, U32, S32) {}
S32  AILEXPORT AIL_3D_object_user_data     (H3DPOBJECT, U32)      { return 0; }
