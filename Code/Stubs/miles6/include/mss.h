/*
**	Stub replacement for the RAD Miles Sound System 6 SDK header (mss.h).
**
**	The real SDK is proprietary and is not distributed with the Renegade source
**	release, yet 91 translation units pull this header in (mostly transitively
**	through WWAudio headers), so its absence blocks most of WWAudio, Combat and
**	Commando from compiling at all.
**
**	This declares just the surface the game actually uses -- the ~80 AIL_*
**	entry points, the handle types, and the few constants -- so the tree can be
**	compiled and the remaining real porting work can be found. Paired with
**	mss_stub.cpp it also links, producing a silent build.
**
**	This is NOT an audio implementation. Every entry point is a no-op. Replace
**	with a real backend (OpenAL, XAudio2, miniaudio) or with the genuine SDK.
*/

#ifndef MSS_H
#define MSS_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmsystem.h>
#include <mmreg.h>   // WAVE_FORMAT_* codes; the real mss.h exposes these too

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// Base types
//-----------------------------------------------------------------------------

#ifndef MSS_TYPES_DEFINED
#define MSS_TYPES_DEFINED
typedef signed char        S8;
typedef unsigned char      U8;
typedef signed short       S16;
typedef unsigned short     U16;
typedef signed int         S32;
typedef unsigned int       U32;
typedef float              F32;
typedef double             F64;
#endif

#define AILCALLBACK  __stdcall
#define AILEXPORT    __stdcall
// FAR comes from windows.h (minwindef.h); do not redefine it.

//-----------------------------------------------------------------------------
// Handles
//
// H3DSAMPLE and H3DPOBJECT are the same underlying type in Miles: a 3D sample
// is just a 3D object, and the listener handle is passed to the same setters.
//-----------------------------------------------------------------------------

// The game reads DIG_DRIVER::emulated_ds directly (WWAudio.cpp:316) to detect a
// DirectSound-emulated device, so this one has to be a real, complete struct.
typedef struct _DIG_DRIVER {
   S32 emulated_ds;
} DIG_DRIVER;

typedef DIG_DRIVER FAR *HDIGDRIVER;
typedef struct _SAMPLE      FAR *HSAMPLE;
typedef struct _STREAM      FAR *HSTREAM;
typedef struct _3D_OBJECT   FAR *H3DPOBJECT;
typedef H3DPOBJECT               H3DSAMPLE;

// HTIMER is an integer index, not a pointer: the game stores -1 for "no timer".
typedef S32 HTIMER;

typedef U32 HPROVIDER;
typedef U32 HPROENUM;
typedef U32 HATTRIB;

#define HPROENUM_FIRST  ((HPROENUM)0)

//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

#define AIL_NO_ERROR              0
#define M3D_NOERR                 0

#ifndef YES
#define YES                       1
#endif
#ifndef NO
#define NO                        0
#endif

// AIL_set_preference() indices
#define AIL_LOCK_PROTECTION       17
#define DIG_USE_WAVEOUT           26
#define DIG_DS_MIX_FRAGMENT_CNT   27

// File-callback seek origins
#define AIL_FILE_SEEK_BEGIN       0
#define AIL_FILE_SEEK_CURRENT     1
#define AIL_FILE_SEEK_END         2

// Speaker configurations
#define AIL_3D_2_SPEAKER          0
#define AIL_3D_HEADPHONE          1
#define AIL_3D_SURROUND           2
#define AIL_3D_4_SPEAKER          3

// Sample pipeline stages (AIL_set_sample_processor)
#define DP_FILTER                 3

// EAX reverb environment presets. Only ENVIRONMENT_GENERIC is referenced by the
// game, but the full set is cheap to carry and matches the real SDK's ordering.
#define ENVIRONMENT_GENERIC          0
#define ENVIRONMENT_PADDEDCELL       1
#define ENVIRONMENT_ROOM             2
#define ENVIRONMENT_BATHROOM         3
#define ENVIRONMENT_LIVINGROOM       4
#define ENVIRONMENT_STONEROOM        5
#define ENVIRONMENT_AUDITORIUM       6
#define ENVIRONMENT_CONCERTHALL      7
#define ENVIRONMENT_CAVE             8
#define ENVIRONMENT_ARENA            9
#define ENVIRONMENT_HANGAR          10
#define ENVIRONMENT_CARPETEDHALLWAY 11
#define ENVIRONMENT_HALLWAY         12
#define ENVIRONMENT_STONECORRIDOR   13
#define ENVIRONMENT_ALLEY           14
#define ENVIRONMENT_FOREST          15
#define ENVIRONMENT_CITY            16
#define ENVIRONMENT_MOUNTAINS       17
#define ENVIRONMENT_QUARRY          18
#define ENVIRONMENT_PLAIN           19
#define ENVIRONMENT_PARKINGLOT      20
#define ENVIRONMENT_SEWERPIPE       21
#define ENVIRONMENT_UNDERWATER      22
#define ENVIRONMENT_DRUGGED         23
#define ENVIRONMENT_DIZZY           24
#define ENVIRONMENT_PSYCHOTIC       25

//-----------------------------------------------------------------------------
// Structures
//-----------------------------------------------------------------------------

typedef struct {
   S32         format;
   void const *data_ptr;
   U32         data_len;
   U32         rate;
   S32         bits;
   S32         channels;
   U32         samples;
   U32         block_size;
   void const *initial_ptr;
} AILSOUNDINFO;

//-----------------------------------------------------------------------------
// Callback types
//-----------------------------------------------------------------------------

typedef U32  (AILCALLBACK *AIL_file_open_callback)  (char const *filename, U32 *file_handle);
typedef void (AILCALLBACK *AIL_file_close_callback) (U32 file_handle);
typedef S32  (AILCALLBACK *AIL_file_seek_callback)  (U32 file_handle, S32 offset, U32 type);
typedef U32  (AILCALLBACK *AIL_file_read_callback)  (U32 file_handle, void *buffer, U32 bytes);

typedef void (AILCALLBACK *AILSAMPLECB) (HSAMPLE sample);
typedef void (AILCALLBACK *AILTIMERCB)  (U32 user);

//-----------------------------------------------------------------------------
// System
//-----------------------------------------------------------------------------

S32   AILEXPORT AIL_startup        (void);
void  AILEXPORT AIL_shutdown       (void);
S32   AILEXPORT AIL_set_preference (U32 number, S32 value);
char *AILEXPORT AIL_last_error     (void);
void  AILEXPORT AIL_serve          (void);
void  AILEXPORT AIL_lock           (void);
void  AILEXPORT AIL_unlock         (void);

void  AILEXPORT AIL_set_file_callbacks (AIL_file_open_callback  opencb,
                                        AIL_file_close_callback closecb,
                                        AIL_file_seek_callback  seekcb,
                                        AIL_file_read_callback  readcb);

S32   AILEXPORT AIL_WAV_info (void const *data, AILSOUNDINFO *info);

//-----------------------------------------------------------------------------
// Digital driver
//-----------------------------------------------------------------------------

S32   AILEXPORT AIL_waveOutOpen  (HDIGDRIVER *drvr, HWAVEOUT *lphWaveOut,
                                  S32 wDeviceID, LPWAVEFORMAT lpFormat);
void  AILEXPORT AIL_waveOutClose (HDIGDRIVER drvr);

//-----------------------------------------------------------------------------
// Timers
//-----------------------------------------------------------------------------

void  AILEXPORT AIL_stop_timer           (HTIMER timer);
void  AILEXPORT AIL_release_timer_handle (HTIMER timer);

//-----------------------------------------------------------------------------
// 2D samples
//-----------------------------------------------------------------------------

HSAMPLE AILEXPORT AIL_allocate_sample_handle (HDIGDRIVER dig);
void    AILEXPORT AIL_release_sample_handle  (HSAMPLE S);
void    AILEXPORT AIL_init_sample            (HSAMPLE S);

S32     AILEXPORT AIL_set_named_sample_file  (HSAMPLE S, char const *file_type_suffix,
                                              void const *file_image, S32 file_image_size,
                                              S32 block);

void    AILEXPORT AIL_start_sample  (HSAMPLE S);
void    AILEXPORT AIL_stop_sample   (HSAMPLE S);
void    AILEXPORT AIL_resume_sample (HSAMPLE S);
void    AILEXPORT AIL_end_sample    (HSAMPLE S);

void    AILEXPORT AIL_set_sample_volume        (HSAMPLE S, S32 volume);
S32     AILEXPORT AIL_sample_volume            (HSAMPLE S);
void    AILEXPORT AIL_set_sample_pan           (HSAMPLE S, S32 pan);
S32     AILEXPORT AIL_sample_pan               (HSAMPLE S);
void    AILEXPORT AIL_set_sample_playback_rate (HSAMPLE S, S32 rate);
S32     AILEXPORT AIL_sample_playback_rate     (HSAMPLE S);
void    AILEXPORT AIL_set_sample_loop_count    (HSAMPLE S, S32 loops);
S32     AILEXPORT AIL_sample_loop_count        (HSAMPLE S);
void    AILEXPORT AIL_set_sample_ms_position   (HSAMPLE S, S32 milliseconds);
void    AILEXPORT AIL_sample_ms_position       (HSAMPLE S, S32 *total_ms, S32 *current_ms);
void    AILEXPORT AIL_set_sample_user_data     (HSAMPLE S, U32 index, S32 value);
S32     AILEXPORT AIL_sample_user_data         (HSAMPLE S, U32 index);

void    AILEXPORT AIL_set_sample_processor (HSAMPLE S, U32 pipeline_stage, HPROVIDER provider);

//-----------------------------------------------------------------------------
// Streams
//-----------------------------------------------------------------------------

HSTREAM AILEXPORT AIL_open_stream           (HDIGDRIVER dig, char const *filename, S32 stream_mem);
HSTREAM AILEXPORT AIL_open_stream_by_sample (HDIGDRIVER dig, HSAMPLE S,
                                             char const *filename, S32 stream_mem);
void    AILEXPORT AIL_close_stream  (HSTREAM stream);
void    AILEXPORT AIL_start_stream  (HSTREAM stream);
void    AILEXPORT AIL_pause_stream  (HSTREAM stream, S32 onoff);

void    AILEXPORT AIL_set_stream_volume        (HSTREAM stream, S32 volume);
S32     AILEXPORT AIL_stream_volume            (HSTREAM stream);
void    AILEXPORT AIL_set_stream_pan           (HSTREAM stream, S32 pan);
S32     AILEXPORT AIL_stream_pan               (HSTREAM stream);
void    AILEXPORT AIL_set_stream_playback_rate (HSTREAM stream, S32 rate);
S32     AILEXPORT AIL_stream_playback_rate     (HSTREAM stream);
void    AILEXPORT AIL_set_stream_loop_count    (HSTREAM stream, S32 count);
S32     AILEXPORT AIL_stream_loop_count        (HSTREAM stream);
void    AILEXPORT AIL_set_stream_loop_block    (HSTREAM stream, S32 loop_start_offset,
                                                S32 loop_end_offset);
void    AILEXPORT AIL_set_stream_ms_position   (HSTREAM stream, S32 milliseconds);
void    AILEXPORT AIL_stream_ms_position       (HSTREAM stream, S32 *total_ms, S32 *current_ms);

//-----------------------------------------------------------------------------
// Providers (3D drivers and filters)
//-----------------------------------------------------------------------------

S32   AILEXPORT AIL_enumerate_3D_providers (HPROENUM *next, HPROVIDER *dest, char **name);
S32   AILEXPORT AIL_open_3D_provider       (HPROVIDER lib);
void  AILEXPORT AIL_close_3D_provider      (HPROVIDER lib);
S32   AILEXPORT AIL_enumerate_filters      (HPROENUM *next, HPROVIDER *dest, char **name);
void  AILEXPORT AIL_set_filter_sample_preference (HSAMPLE S, char const *preference_name,
                                                  void const *value);
void  AILEXPORT AIL_set_3D_speaker_type    (HPROVIDER lib, S32 speaker_type);

//-----------------------------------------------------------------------------
// 3D samples and listener
//-----------------------------------------------------------------------------

H3DPOBJECT AILEXPORT AIL_3D_open_listener            (HPROVIDER lib);
H3DSAMPLE  AILEXPORT AIL_allocate_3D_sample_handle   (HPROVIDER lib);
void       AILEXPORT AIL_release_3D_sample_handle    (H3DSAMPLE S);

S32   AILEXPORT AIL_set_3D_sample_file   (H3DSAMPLE S, void const *file_image);

void  AILEXPORT AIL_start_3D_sample      (H3DSAMPLE S);
void  AILEXPORT AIL_stop_3D_sample       (H3DSAMPLE S);
void  AILEXPORT AIL_resume_3D_sample     (H3DSAMPLE S);
void  AILEXPORT AIL_end_3D_sample        (H3DSAMPLE S);

void  AILEXPORT AIL_set_3D_position        (H3DPOBJECT obj, F32 X, F32 Y, F32 Z);
void  AILEXPORT AIL_set_3D_velocity_vector (H3DPOBJECT obj, F32 dX, F32 dY, F32 dZ);
void  AILEXPORT AIL_set_3D_orientation     (H3DPOBJECT obj, F32 X_face, F32 Y_face, F32 Z_face,
                                            F32 X_up, F32 Y_up, F32 Z_up);

void  AILEXPORT AIL_set_3D_sample_volume        (H3DSAMPLE S, S32 volume);
S32   AILEXPORT AIL_3D_sample_volume            (H3DSAMPLE S);
void  AILEXPORT AIL_set_3D_sample_playback_rate (H3DSAMPLE S, S32 rate);
S32   AILEXPORT AIL_3D_sample_playback_rate     (H3DSAMPLE S);
void  AILEXPORT AIL_set_3D_sample_loop_count    (H3DSAMPLE S, U32 loops);
U32   AILEXPORT AIL_3D_sample_loop_count        (H3DSAMPLE S);
void  AILEXPORT AIL_set_3D_sample_offset        (H3DSAMPLE S, U32 offset);
U32   AILEXPORT AIL_3D_sample_offset            (H3DSAMPLE S);
U32   AILEXPORT AIL_3D_sample_length            (H3DSAMPLE S);
void  AILEXPORT AIL_set_3D_sample_distances     (H3DSAMPLE S, F32 max_dist, F32 min_dist);
void  AILEXPORT AIL_set_3D_sample_effects_level (H3DSAMPLE S, F32 effects_level);

void  AILEXPORT AIL_set_3D_object_user_data (H3DPOBJECT obj, U32 index, S32 value);
S32   AILEXPORT AIL_3D_object_user_data     (H3DPOBJECT obj, U32 index);

#ifdef __cplusplus
}
#endif

#endif // MSS_H
