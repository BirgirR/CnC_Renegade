/*
**	Stub replacement for the GameSpy MD5 header.
**	Only MD5Digest() is referenced (CDKeyAuth.cpp:150). The stub produces a
**	deterministic but meaningless digest; nothing can validate against it.
*/
#ifndef GS_MD5_H
#define GS_MD5_H
#ifdef __cplusplus
extern "C" {
#endif
void MD5Digest(unsigned char *input, unsigned int len, char *output);
#ifdef __cplusplus
}
#endif
#endif
