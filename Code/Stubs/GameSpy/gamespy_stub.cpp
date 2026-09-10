/*
**	No-op implementation of the GameSpy stubs.
**
**	Behaviour is chosen so the game degrades to "not listed, not authenticated"
**	rather than "waiting forever":
**
**	  - qr_init reports success but registers nothing, so a server runs and is
**	    simply invisible to the master server.
**	  - gcd_authenticate_user fires its callback immediately with
**	    authenticated = 1. The alternative -- never calling back -- leaves every
**	    joining client stuck in the CD-key handshake. There is no key server to
**	    validate against, so refusing everyone would just make servers unusable.
**	    If you would rather fail closed, pass 0 instead.
*/

#include "gqueryreporting.h"
#include "gcdkeyserver.h"
#include "ghttp.h"
#include "gs_md5.h"

// -----------------------------------------------------------------------------
// Query & Reporting
// -----------------------------------------------------------------------------

qr_error_t qr_init(qr_t *qrec,
                   const char * /*ip*/,
                   int /*baseport*/,
                   const char * /*gamename*/,
                   const char * /*secret_key*/,
                   qr_querycallback_t /*basic*/,
                   qr_querycallback_t /*info*/,
                   qr_querycallback_t /*rules*/,
                   qr_querycallback_t /*players*/,
                   void * /*userdata*/)
{
   if (qrec != 0) {
      *qrec = 0;
   }
   return e_qrnoerror;
}

void qr_process_queries(qr_t) {}
void qr_send_exiting(qr_t)    {}
void qr_shutdown(qr_t)        {}

// Master-server list. Kept permanently empty: get_sockaddrin reports that it
// could not resolve the host, so the caller never reaches add_master and the
// server simply goes unlisted instead of pointing at a dead 2014 endpoint.
int  get_master_count(void)          { return 0; }
void clear_master_list(void)         {}
void add_master(struct sockaddr_in *) {}

int get_sockaddrin(const char * /*host*/, int /*port*/, struct sockaddr_in *,
                   struct hostent **savehent)
{
   if (savehent != 0) {
      *savehent = 0;
   }
   return 0;
}

// -----------------------------------------------------------------------------
// CD-key authentication
// -----------------------------------------------------------------------------

void gcd_init_qr(qr_t, int) {}
void gcd_disconnect_user(int) {}

void gcd_authenticate_user(int localid,
                           int /*userip*/,
                           const char * /*challenge*/,
                           const char * /*response*/,
                           AuthCallBackFn callback,
                           void *instance)
{
   // Answer immediately; see the note at the top of this file.
   if (callback != 0) {
      callback(localid, 1, const_cast<char *>(""), instance);
   }
}

void gcd_compute_response(char * /*cdkey*/, const char * /*challenge*/, char *response)
{
   // The real API writes a 72-character hex response plus a terminator.
   if (response != 0) {
      for (int i = 0; i < 72; ++i) {
         response[i] = '0';
      }
      response[72] = '\0';
   }
}

void gcd_think(void) {}

// -----------------------------------------------------------------------------
// HTTP / MD5
// -----------------------------------------------------------------------------

void ghttpThink(void) {}

void MD5Digest(unsigned char * /*input*/, unsigned int /*len*/, char *output)
{
   // Deterministic filler, not a digest. Nothing can validate against it, which
   // is correct: there is no key server on the other end to disagree with.
   if (output != 0) {
      for (int i = 0; i < 32; ++i) {
         output[i] = '0';
      }
      output[32] = '\0';
   }
}
