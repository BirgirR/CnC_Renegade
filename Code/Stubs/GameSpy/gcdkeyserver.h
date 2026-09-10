/*
**	Stub replacement for the GameSpy CD-key server SDK header.
**	See gqueryreporting.h for why these stubs exist.
*/

#ifndef GCDKEYSERVER_H
#define GCDKEYSERVER_H

#include "gqueryreporting.h"

#ifdef __cplusplus
extern "C" {
#endif

// The real API writes a 72-character response plus a terminator.
#define RESPONSE_SIZE 73

// Invoked with authenticated != 0 once a key has been validated.
typedef void (*AuthCallBackFn)(int localid, int authenticated, char *errmsg, void *instance);

void gcd_init_qr(qr_t qrec, int gameid);
void gcd_disconnect_user(int localid);
void gcd_authenticate_user(int localid, int userip, const char *challenge,
                           const char *response, AuthCallBackFn callback, void *instance);
void gcd_compute_response(char *cdkey, const char *challenge, char *response);
void gcd_think(void);

#ifdef __cplusplus
}
#endif

#endif // GCDKEYSERVER_H
