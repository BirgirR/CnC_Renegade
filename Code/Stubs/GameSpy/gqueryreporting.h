/*
**	Stub replacement for the GameSpy Query & Reporting SDK header.
**
**	GameSpy's services were shut down in 2014 and the SDK was never
**	redistributed. Eleven Commando translation units include one of these
**	headers, which is enough to block the whole executable from linking.
**
**	The surface the game actually touches is tiny -- four qr_* entry points here
**	and five gcd_* ones in gcdkeyserver.h -- so stubbing costs far less than
**	sourcing a community-preserved SDK, and loses only the server browser and
**	CD-key authentication rather than the game.
**
**	Swap in UniSpySDK or a preserved GameSpy SDK to restore master-server
**	listing, and point it at a replacement master such as CnCNet's renmaster.
*/

#ifndef GQUERYREPORTING_H
#define GQUERYREPORTING_H

// The real SDK header chains to nonport.h, and callers lean on it for the C
// library declarations that arrive that way (GameSpy_QnR.cpp calls time()).
#include "nonport.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
   e_qrnoerror = 0,
   e_qrwsockerror,
   e_qrbinderror,
   e_qrdnserror,
   e_qrconnerror
} qr_error_t;

typedef struct qr_implementation_s *qr_t;

// Fills outbuf with at most maxlen bytes of key\value server state.
typedef void (*qr_querycallback_t)(char *outbuf, int maxlen, void *userdata);

qr_error_t qr_init(qr_t *qrec,
                   const char *ip,
                   int baseport,
                   const char *gamename,
                   const char *secret_key,
                   qr_querycallback_t qr_basic_callback,
                   qr_querycallback_t qr_info_callback,
                   qr_querycallback_t qr_rules_callback,
                   qr_querycallback_t qr_players_callback,
                   void *userdata);

void qr_process_queries(qr_t qrec);
void qr_send_exiting(qr_t qrec);
void qr_shutdown(qr_t qrec);

// Master-server list management. Declared with forward-declared socket types so
// this header does not have to pick a winsock version for its includers.
struct sockaddr_in;
struct hostent;

int  get_master_count(void);
void clear_master_list(void);
void add_master(struct sockaddr_in *addr);
int  get_sockaddrin(const char *host, int port, struct sockaddr_in *saddr,
                    struct hostent **savehent);

#ifdef __cplusplus
}
#endif

#endif // GQUERYREPORTING_H
