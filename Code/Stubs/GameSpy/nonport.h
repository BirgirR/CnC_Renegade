/*
**	Stub replacement for the GameSpy portability header. The original abstracted
**	sockets and types across platforms; on Win32 the game's own headers already
**	supply everything it uses, so this is intentionally empty.
*/
#ifndef GS_NONPORT_H
#define GS_NONPORT_H

// The original abstracted sockets and types across platforms. On Win32 the
// game's own headers supply nearly everything, but callers do rely on this
// header dragging in the C library pieces GameSpy used -- time() among them.
#include <time.h>
#include <string.h>
#include <stdlib.h>

#endif
