/*
**	d3dx_lite -- the D3DX surface Renegade actually uses, declared without any
**	DirectX SDK.
**
**	D3DX has never shipped in the Windows SDK, in any version. Since the engine
**	only wants three types, four filter constants and nine functions, declaring
**	them here removes the last reason to fetch anything from the 2001 SDK: with
**	the renderer on D3D9, d3d9.h/d3d9types.h/d3d9caps.h all come from the
**	Windows SDK and this tree needs no external headers at all.
**
**	The type layouts match D3DX's exactly, because the engine reinterpret_casts
**	its own Matrix4 and Vector3 onto them (sortingrenderer.cpp:332-333).
**	Matrix4 is Vector4 Row[4], i.e. 16 contiguous floats, which is D3DXMATRIX.
*/

#ifndef D3DX_LITE_H
#define D3DX_LITE_H

#include <d3d9.h>

//-----------------------------------------------------------------------------
//	Filter flags. Values match D3DX so existing call sites keep their meaning.
//-----------------------------------------------------------------------------

#define D3DX_FILTER_NONE			(1 << 0)
#define D3DX_FILTER_POINT			(2 << 0)
#define D3DX_FILTER_LINEAR			(3 << 0)
#define D3DX_FILTER_TRIANGLE		(4 << 0)
#define D3DX_FILTER_BOX				(5 << 0)

#define D3DX_DEFAULT				((UINT) -1)

//-----------------------------------------------------------------------------
//	Types
//-----------------------------------------------------------------------------

typedef struct D3DXVECTOR3
{
	D3DXVECTOR3() {}
	D3DXVECTOR3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

	operator float* ()					{ return &x; }
	operator const float* () const	{ return &x; }

	float x, y, z;
} D3DXVECTOR3;

typedef struct D3DXVECTOR4
{
	D3DXVECTOR4() {}
	D3DXVECTOR4(float _x, float _y, float _z, float _w) : x(_x), y(_y), z(_z), w(_w) {}

	// sortingrenderer.cpp reads the result of D3DXVec3Transform by index.
	float &operator [] (int i)					{ return (&x)[i]; }
	const float &operator [] (int i) const	{ return (&x)[i]; }

	operator float* ()					{ return &x; }
	operator const float* () const	{ return &x; }

	float x, y, z, w;
} D3DXVECTOR4;

typedef struct D3DXMATRIX
{
	D3DXMATRIX() {}

	float &operator () (int row, int col)				{ return m[row][col]; }
	const float &operator () (int row, int col) const	{ return m[row][col]; }

	operator float* ()					{ return &_11; }
	operator const float* () const	{ return &_11; }

	// Declared out of line; defined in d3dx_lite.cpp via D3DXMatrixMultiply so
	// there is exactly one implementation of the maths.
	D3DXMATRIX operator * (const D3DXMATRIX &rhs) const;

	//	The nameless union/struct is what gives D3DXMATRIX its dual _11.._44 and
	//	m[4][4] views, and the engine reinterpret_casts its own Matrix4 onto this
	//	layout, so the extension is deliberate rather than incidental. Scoped
	//	rather than suppressed build-wide.
#pragma warning(push)
#pragma warning(disable: 4201)		// nonstandard extension: nameless struct/union
	union {
		struct {
			float _11, _12, _13, _14;
			float _21, _22, _23, _24;
			float _31, _32, _33, _34;
			float _41, _42, _43, _44;
		};
		float m[4][4];
	};
#pragma warning(pop)
} D3DXMATRIX;

//-----------------------------------------------------------------------------
//	Functions
//
//	These keep D3DX's signatures and calling convention so the engine's existing
//	call sites are unchanged. See d3dx_lite.cpp for behaviour notes.
//-----------------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

D3DXMATRIX*  WINAPI D3DXMatrixMultiply  (D3DXMATRIX *pOut, const D3DXMATRIX *pM1,
													  const D3DXMATRIX *pM2);
D3DXMATRIX*  WINAPI D3DXMatrixTranspose (D3DXMATRIX *pOut, const D3DXMATRIX *pM);
D3DXVECTOR4* WINAPI D3DXVec3Transform   (D3DXVECTOR4 *pOut, const D3DXVECTOR3 *pV,
													  const D3DXMATRIX *pM);

UINT    WINAPI D3DXGetFVFVertexSize (DWORD FVF);
HRESULT WINAPI D3DXGetErrorStringA  (HRESULT hr, LPSTR pBuffer, UINT BufferLen);

HRESULT WINAPI D3DXCreateTexture (LPDIRECT3DDEVICE9 pDevice, UINT Width, UINT Height,
											 UINT MipLevels, DWORD Usage, D3DFORMAT Format,
											 D3DPOOL Pool, LPDIRECT3DTEXTURE9 *ppTexture);

HRESULT WINAPI D3DXFilterTexture (LPDIRECT3DBASETEXTURE9 pBaseTexture,
											 const PALETTEENTRY *pPalette, UINT SrcLevel, DWORD Filter);

// Unreachable in this tree (see d3dx_lite.cpp). The only call site passes NULL
// for both the image-info and palette arguments, so they are typed as void* and
// D3DXIMAGE_INFO never has to be declared.
HRESULT WINAPI D3DXCreateTextureFromFileExA (LPDIRECT3DDEVICE9 pDevice, LPCSTR pSrcFile,
															UINT Width, UINT Height, UINT MipLevels,
															DWORD Usage, D3DFORMAT Format, D3DPOOL Pool,
															DWORD Filter, DWORD MipFilter, D3DCOLOR ColorKey,
															void *pSrcInfo, void *pPalette,
															LPDIRECT3DTEXTURE9 *ppTexture);

HRESULT WINAPI D3DXLoadSurfaceFromSurface (LPDIRECT3DSURFACE9 pDestSurface,
														 const PALETTEENTRY *pDestPalette,
														 const RECT *pDestRect,
														 LPDIRECT3DSURFACE9 pSrcSurface,
														 const PALETTEENTRY *pSrcPalette,
														 const RECT *pSrcRect,
														 DWORD Filter, D3DCOLOR ColorKey);

#ifdef __cplusplus
}
#endif

#endif // D3DX_LITE_H
