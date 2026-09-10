/*
**	d3dx_lite -- replacements for the nine D3DX entry points Renegade actually
**	calls, so the build needs no D3DX library and no DirectX SDK at all.
**
**	The D3DX8 helper library is the last piece of the 2001 DirectX SDK the game
**	could not do without. The source references 531 D3DX symbols, but almost all
**	of them are the inline vector/matrix types in d3dx8math.h, which cost
**	nothing at link time. Only these nine are real exported functions, and the
**	linker is the authority on that list -- it produced it when d3dx8.lib was
**	removed.
**
**	Behaviour notes, where "what D3DX did" matters to the caller:
**
**	  - D3DXCreateTexture is documented to substitute the closest supported
**	    format when the requested one is unavailable, and dx8wrapper.cpp:2077
**	    explicitly relies on that. The fallback walk below preserves it.
**	  - D3DXCreateTextureFromFileExA is unreachable: the only caller would be
**	    DX8Wrapper::_Create_DX8_Texture(const char*, ...) (dx8wrapper.h:311),
**	    which nothing calls. Textures arrive from .mix archives through the
**	    engine's own TARGA.CPP and ddsfile.cpp. It is implemented as a clean
**	    failure rather than removed, so the engine keeps compiling unchanged.
**
**	Compressed (DXT) surfaces are not resampled here. Nothing asks: DDS files
**	arrive with their mip chains already built, so the filtering paths only ever
**	see uncompressed formats. Those calls fail loudly rather than silently
**	producing garbage.
*/

#include <windows.h>
#include "d3dx_lite.h"

namespace
{

//-----------------------------------------------------------------------------
//	Pixel format handling
//
//	Everything is funnelled through a straight 8-bit BGRA intermediate. That is
//	lossless for every format below and keeps the resamplers format-agnostic.
//-----------------------------------------------------------------------------

struct Pixel
{
	unsigned char b, g, r, a;
};

unsigned Bytes_Per_Pixel(D3DFORMAT format)
{
	switch (format) {
		case D3DFMT_A8R8G8B8:
		case D3DFMT_X8R8G8B8:	return 4;
		case D3DFMT_R8G8B8:		return 3;
		case D3DFMT_R5G6B5:
		case D3DFMT_X1R5G5B5:
		case D3DFMT_A1R5G5B5:
		case D3DFMT_A4R4G4B4:
		case D3DFMT_X4R4G4B4:
		case D3DFMT_A8L8:			return 2;
		case D3DFMT_A8:
		case D3DFMT_L8:			return 1;
		default:						return 0;		// unsupported / compressed
	}
}

// Expand an n-bit channel to 8 bits by bit replication, so 0x1F -> 0xFF.
inline unsigned char Expand(unsigned value, unsigned bits)
{
	unsigned shifted = value << (8 - bits);
	return (unsigned char)(shifted | (shifted >> bits));
}

Pixel Decode(D3DFORMAT format, const unsigned char *src)
{
	Pixel p = { 0, 0, 0, 255 };

	switch (format) {
		case D3DFMT_A8R8G8B8:
			p.b = src[0]; p.g = src[1]; p.r = src[2]; p.a = src[3];
			break;

		case D3DFMT_X8R8G8B8:
			p.b = src[0]; p.g = src[1]; p.r = src[2];
			break;

		case D3DFMT_R8G8B8:
			p.b = src[0]; p.g = src[1]; p.r = src[2];
			break;

		case D3DFMT_R5G6B5: {
			unsigned short v = *(const unsigned short *)src;
			p.r = Expand((v >> 11) & 0x1F, 5);
			p.g = Expand((v >>  5) & 0x3F, 6);
			p.b = Expand( v        & 0x1F, 5);
			break;
		}

		case D3DFMT_X1R5G5B5:
		case D3DFMT_A1R5G5B5: {
			unsigned short v = *(const unsigned short *)src;
			p.r = Expand((v >> 10) & 0x1F, 5);
			p.g = Expand((v >>  5) & 0x1F, 5);
			p.b = Expand( v        & 0x1F, 5);
			if (format == D3DFMT_A1R5G5B5) {
				p.a = (v & 0x8000) ? 255 : 0;
			}
			break;
		}

		case D3DFMT_A4R4G4B4:
		case D3DFMT_X4R4G4B4: {
			unsigned short v = *(const unsigned short *)src;
			p.r = Expand((v >> 8) & 0xF, 4);
			p.g = Expand((v >> 4) & 0xF, 4);
			p.b = Expand( v       & 0xF, 4);
			if (format == D3DFMT_A4R4G4B4) {
				p.a = Expand((v >> 12) & 0xF, 4);
			}
			break;
		}

		case D3DFMT_A8L8:
			p.r = p.g = p.b = src[0];
			p.a = src[1];
			break;

		case D3DFMT_L8:
			p.r = p.g = p.b = src[0];
			break;

		case D3DFMT_A8:
			p.r = p.g = p.b = 255;
			p.a = src[0];
			break;

		default:
			break;
	}

	return p;
}

void Encode(D3DFORMAT format, unsigned char *dst, const Pixel &p)
{
	switch (format) {
		case D3DFMT_A8R8G8B8:
			dst[0] = p.b; dst[1] = p.g; dst[2] = p.r; dst[3] = p.a;
			break;

		case D3DFMT_X8R8G8B8:
			dst[0] = p.b; dst[1] = p.g; dst[2] = p.r; dst[3] = 0xFF;
			break;

		case D3DFMT_R8G8B8:
			dst[0] = p.b; dst[1] = p.g; dst[2] = p.r;
			break;

		case D3DFMT_R5G6B5:
			*(unsigned short *)dst = (unsigned short)
				(((p.r >> 3) << 11) | ((p.g >> 2) << 5) | (p.b >> 3));
			break;

		case D3DFMT_X1R5G5B5:
			*(unsigned short *)dst = (unsigned short)
				(((p.r >> 3) << 10) | ((p.g >> 3) << 5) | (p.b >> 3));
			break;

		case D3DFMT_A1R5G5B5:
			*(unsigned short *)dst = (unsigned short)
				((p.a >= 128 ? 0x8000 : 0) |
				 ((p.r >> 3) << 10) | ((p.g >> 3) << 5) | (p.b >> 3));
			break;

		case D3DFMT_A4R4G4B4:
			*(unsigned short *)dst = (unsigned short)
				(((p.a >> 4) << 12) | ((p.r >> 4) << 8) | ((p.g >> 4) << 4) | (p.b >> 4));
			break;

		case D3DFMT_X4R4G4B4:
			*(unsigned short *)dst = (unsigned short)
				(((p.r >> 4) << 8) | ((p.g >> 4) << 4) | (p.b >> 4));
			break;

		case D3DFMT_A8L8:
			// Luminance from the same weights D3DX uses.
			dst[0] = (unsigned char)((p.r * 77 + p.g * 151 + p.b * 28) >> 8);
			dst[1] = p.a;
			break;

		case D3DFMT_L8:
			dst[0] = (unsigned char)((p.r * 77 + p.g * 151 + p.b * 28) >> 8);
			break;

		case D3DFMT_A8:
			dst[0] = p.a;
			break;

		default:
			break;
	}
}

//-----------------------------------------------------------------------------
//	Resampling
//-----------------------------------------------------------------------------

inline Pixel Sample(const unsigned char *base, int pitch, D3DFORMAT format,
						  unsigned bpp, int x, int y)
{
	return Decode(format, base + (y * pitch) + (x * bpp));
}

// Point or bilinear resample of one rectangle into another.
void Resample(unsigned char *dst_base, int dst_pitch, D3DFORMAT dst_format,
				  int dst_x, int dst_y, int dst_w, int dst_h,
				  const unsigned char *src_base, int src_pitch, D3DFORMAT src_format,
				  int src_x, int src_y, int src_w, int src_h,
				  bool bilinear)
{
	unsigned dst_bpp = Bytes_Per_Pixel(dst_format);
	unsigned src_bpp = Bytes_Per_Pixel(src_format);

	for (int y = 0; y < dst_h; ++y) {
		unsigned char *dst_row = dst_base + ((dst_y + y) * dst_pitch) + (dst_x * dst_bpp);

		for (int x = 0; x < dst_w; ++x) {
			Pixel out;

			if (!bilinear || (src_w == dst_w && src_h == dst_h)) {
				int sx = (dst_w == src_w) ? x : (x * src_w) / dst_w;
				int sy = (dst_h == src_h) ? y : (y * src_h) / dst_h;
				out = Sample(src_base, src_pitch, src_format, src_bpp,
								 src_x + sx, src_y + sy);
			} else {
				// Sample at the centre of the destination texel.
				float fx = ((x + 0.5f) * src_w) / dst_w - 0.5f;
				float fy = ((y + 0.5f) * src_h) / dst_h - 0.5f;
				if (fx < 0.0f) fx = 0.0f;
				if (fy < 0.0f) fy = 0.0f;

				int x0 = (int)fx, y0 = (int)fy;
				int x1 = (x0 + 1 < src_w) ? x0 + 1 : x0;
				int y1 = (y0 + 1 < src_h) ? y0 + 1 : y0;
				float tx = fx - x0, ty = fy - y0;

				Pixel p00 = Sample(src_base, src_pitch, src_format, src_bpp, src_x + x0, src_y + y0);
				Pixel p10 = Sample(src_base, src_pitch, src_format, src_bpp, src_x + x1, src_y + y0);
				Pixel p01 = Sample(src_base, src_pitch, src_format, src_bpp, src_x + x0, src_y + y1);
				Pixel p11 = Sample(src_base, src_pitch, src_format, src_bpp, src_x + x1, src_y + y1);

				float w00 = (1 - tx) * (1 - ty), w10 = tx * (1 - ty);
				float w01 = (1 - tx) * ty,       w11 = tx * ty;

				out.b = (unsigned char)(p00.b * w00 + p10.b * w10 + p01.b * w01 + p11.b * w11 + 0.5f);
				out.g = (unsigned char)(p00.g * w00 + p10.g * w10 + p01.g * w01 + p11.g * w11 + 0.5f);
				out.r = (unsigned char)(p00.r * w00 + p10.r * w10 + p01.r * w01 + p11.r * w11 + 0.5f);
				out.a = (unsigned char)(p00.a * w00 + p10.a * w10 + p01.a * w01 + p11.a * w11 + 0.5f);
			}

			Encode(dst_format, dst_row, out);
			dst_row += dst_bpp;
		}
	}
}

} // namespace

//-----------------------------------------------------------------------------
//	Math
//-----------------------------------------------------------------------------

// All three write through a temporary: the engine calls Transpose with pOut ==
// pM (sortingrenderer.cpp:521), and Multiply is reached via operator*.

// Declared in d3dx_lite.h; routed through D3DXMatrixMultiply so there is a
// single implementation of the maths.
D3DXMATRIX D3DXMATRIX::operator * (const D3DXMATRIX &rhs) const
{
	D3DXMATRIX out;
	D3DXMatrixMultiply(&out, this, &rhs);
	return out;
}

D3DXMATRIX* WINAPI D3DXMatrixMultiply(D3DXMATRIX *pOut, const D3DXMATRIX *pM1,
												  const D3DXMATRIX *pM2)
{
	if (pOut == NULL || pM1 == NULL || pM2 == NULL) return pOut;

	D3DXMATRIX tmp;
	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			tmp.m[i][j] = pM1->m[i][0] * pM2->m[0][j]
						   + pM1->m[i][1] * pM2->m[1][j]
						   + pM1->m[i][2] * pM2->m[2][j]
						   + pM1->m[i][3] * pM2->m[3][j];
		}
	}

	*pOut = tmp;
	return pOut;
}

D3DXMATRIX* WINAPI D3DXMatrixTranspose(D3DXMATRIX *pOut, const D3DXMATRIX *pM)
{
	if (pOut == NULL || pM == NULL) return pOut;

	D3DXMATRIX tmp;
	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			tmp.m[i][j] = pM->m[j][i];
		}
	}

	*pOut = tmp;
	return pOut;
}

// D3DX uses the row-vector convention: out = (v, 1) * M.
D3DXVECTOR4* WINAPI D3DXVec3Transform(D3DXVECTOR4 *pOut, const D3DXVECTOR3 *pV,
												  const D3DXMATRIX *pM)
{
	if (pOut == NULL || pV == NULL || pM == NULL) return pOut;

	float x = pV->x, y = pV->y, z = pV->z;

	D3DXVECTOR4 tmp;
	tmp.x = x * pM->_11 + y * pM->_21 + z * pM->_31 + pM->_41;
	tmp.y = x * pM->_12 + y * pM->_22 + z * pM->_32 + pM->_42;
	tmp.z = x * pM->_13 + y * pM->_23 + z * pM->_33 + pM->_43;
	tmp.w = x * pM->_14 + y * pM->_24 + z * pM->_34 + pM->_44;

	*pOut = tmp;
	return pOut;
}

//-----------------------------------------------------------------------------
//	Vertex format
//-----------------------------------------------------------------------------

UINT WINAPI D3DXGetFVFVertexSize(DWORD FVF)
{
	UINT size = 0;

	switch (FVF & D3DFVF_POSITION_MASK) {
		case D3DFVF_XYZ:		size += 3 * sizeof(float); break;
		case D3DFVF_XYZRHW:	size += 4 * sizeof(float); break;
		case D3DFVF_XYZB1:	size += 4 * sizeof(float); break;
		case D3DFVF_XYZB2:	size += 5 * sizeof(float); break;
		case D3DFVF_XYZB3:	size += 6 * sizeof(float); break;
		case D3DFVF_XYZB4:	size += 7 * sizeof(float); break;
		case D3DFVF_XYZB5:	size += 8 * sizeof(float); break;
		default:					break;
	}

	if (FVF & D3DFVF_NORMAL)		size += 3 * sizeof(float);
	if (FVF & D3DFVF_PSIZE)			size += sizeof(float);
	if (FVF & D3DFVF_DIFFUSE)		size += sizeof(DWORD);
	if (FVF & D3DFVF_SPECULAR)		size += sizeof(DWORD);

	// Each texture stage carries a 2-bit size code; the default (0) is 2 floats.
	UINT tex_count = (FVF & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT;
	for (UINT i = 0; i < tex_count; ++i) {
		switch ((FVF >> (16 + i * 2)) & 0x3) {
			case 0:	size += 2 * sizeof(float); break;	// D3DFVF_TEXTUREFORMAT2
			case 1:	size += 3 * sizeof(float); break;	// D3DFVF_TEXTUREFORMAT3
			case 2:	size += 4 * sizeof(float); break;	// D3DFVF_TEXTUREFORMAT4
			case 3:	size += 1 * sizeof(float); break;	// D3DFVF_TEXTUREFORMAT1
		}
	}

	return size;
}

//-----------------------------------------------------------------------------
//	Diagnostics
//-----------------------------------------------------------------------------

HRESULT WINAPI D3DXGetErrorStringA(HRESULT hr, LPSTR pBuffer, UINT BufferLen)
{
	if (pBuffer == NULL || BufferLen == 0) return D3DERR_INVALIDCALL;

	const char *text = NULL;

	switch (hr) {
		case D3D_OK:									text = "D3D_OK"; break;
		case D3DERR_WRONGTEXTUREFORMAT:			text = "D3DERR_WRONGTEXTUREFORMAT"; break;
		case D3DERR_UNSUPPORTEDCOLOROPERATION:	text = "D3DERR_UNSUPPORTEDCOLOROPERATION"; break;
		case D3DERR_UNSUPPORTEDCOLORARG:			text = "D3DERR_UNSUPPORTEDCOLORARG"; break;
		case D3DERR_UNSUPPORTEDALPHAOPERATION:	text = "D3DERR_UNSUPPORTEDALPHAOPERATION"; break;
		case D3DERR_UNSUPPORTEDALPHAARG:			text = "D3DERR_UNSUPPORTEDALPHAARG"; break;
		case D3DERR_TOOMANYOPERATIONS:			text = "D3DERR_TOOMANYOPERATIONS"; break;
		case D3DERR_CONFLICTINGTEXTUREFILTER:	text = "D3DERR_CONFLICTINGTEXTUREFILTER"; break;
		case D3DERR_UNSUPPORTEDFACTORVALUE:		text = "D3DERR_UNSUPPORTEDFACTORVALUE"; break;
		case D3DERR_CONFLICTINGRENDERSTATE:		text = "D3DERR_CONFLICTINGRENDERSTATE"; break;
		case D3DERR_UNSUPPORTEDTEXTUREFILTER:	text = "D3DERR_UNSUPPORTEDTEXTUREFILTER"; break;
		case D3DERR_CONFLICTINGTEXTUREPALETTE:	text = "D3DERR_CONFLICTINGTEXTUREPALETTE"; break;
		case D3DERR_DRIVERINTERNALERROR:			text = "D3DERR_DRIVERINTERNALERROR"; break;
		case D3DERR_NOTFOUND:						text = "D3DERR_NOTFOUND"; break;
		case D3DERR_MOREDATA:						text = "D3DERR_MOREDATA"; break;
		case D3DERR_DEVICELOST:						text = "D3DERR_DEVICELOST"; break;
		case D3DERR_DEVICENOTRESET:				text = "D3DERR_DEVICENOTRESET"; break;
		case D3DERR_NOTAVAILABLE:					text = "D3DERR_NOTAVAILABLE"; break;
		case D3DERR_OUTOFVIDEOMEMORY:				text = "D3DERR_OUTOFVIDEOMEMORY"; break;
		case D3DERR_INVALIDDEVICE:					text = "D3DERR_INVALIDDEVICE"; break;
		case D3DERR_INVALIDCALL:					text = "D3DERR_INVALIDCALL"; break;
		case D3DERR_DRIVERINVALIDCALL:			text = "D3DERR_DRIVERINVALIDCALL"; break;
		case E_OUTOFMEMORY:							text = "E_OUTOFMEMORY"; break;
		case E_INVALIDARG:							text = "E_INVALIDARG"; break;
		case E_NOTIMPL:								text = "E_NOTIMPL"; break;
		case E_FAIL:									text = "E_FAIL"; break;
		default:											text = NULL; break;
	}

	if (text != NULL) {
		lstrcpynA(pBuffer, text, BufferLen);
	} else {
		char scratch[64];
		wsprintfA(scratch, "HRESULT 0x%08lX", (unsigned long)hr);
		lstrcpynA(pBuffer, scratch, BufferLen);
	}

	return D3D_OK;
}

//-----------------------------------------------------------------------------
//	Texture creation
//-----------------------------------------------------------------------------

HRESULT WINAPI D3DXCreateTexture(LPDIRECT3DDEVICE9 pDevice, UINT Width, UINT Height,
											UINT MipLevels, DWORD Usage, D3DFORMAT Format,
											D3DPOOL Pool, LPDIRECT3DTEXTURE9 *ppTexture)
{
	if (pDevice == NULL || ppTexture == NULL) return D3DERR_INVALIDCALL;
	*ppTexture = NULL;

	D3DCAPS9 caps;
	HRESULT hr = pDevice->GetDeviceCaps(&caps);
	if (FAILED(hr)) return hr;

	if (Width  == 0) Width  = 1;
	if (Height == 0) Height = 1;

	// Round up to a power of two unless the device says it does not need it.
	if ((caps.TextureCaps & D3DPTEXTURECAPS_POW2) != 0 &&
		 (caps.TextureCaps & D3DPTEXTURECAPS_NONPOW2CONDITIONAL) == 0) {
		UINT w = 1; while (w < Width)  w <<= 1; Width  = w;
		UINT h = 1; while (h < Height) h <<= 1; Height = h;
	}

	if ((caps.TextureCaps & D3DPTEXTURECAPS_SQUAREONLY) != 0) {
		if (Width > Height) Height = Width; else Width = Height;
	}

	if (caps.MaxTextureWidth  != 0 && Width  > caps.MaxTextureWidth)  Width  = caps.MaxTextureWidth;
	if (caps.MaxTextureHeight != 0 && Height > caps.MaxTextureHeight) Height = caps.MaxTextureHeight;

	// The engine depends on the documented behaviour of substituting the nearest
	// supported format when the requested one is unavailable (dx8wrapper.cpp:2077).
	IDirect3D9 *d3d = NULL;
	if (SUCCEEDED(pDevice->GetDirect3D(&d3d)) && d3d != NULL) {
		D3DDEVICE_CREATION_PARAMETERS params;
		D3DDISPLAYMODE mode;

		if (SUCCEEDED(pDevice->GetCreationParameters(&params)) &&
			 SUCCEEDED(pDevice->GetDisplayMode(0, &mode))) {

			static const D3DFORMAT with_alpha[] = {
				D3DFMT_A8R8G8B8, D3DFMT_A1R5G5B5, D3DFMT_A4R4G4B4, D3DFMT_UNKNOWN
			};
			static const D3DFORMAT without_alpha[] = {
				D3DFMT_X8R8G8B8, D3DFMT_R8G8B8, D3DFMT_R5G6B5, D3DFMT_X1R5G5B5, D3DFMT_UNKNOWN
			};

			bool wants_alpha = (Format == D3DFMT_A8R8G8B8 || Format == D3DFMT_A1R5G5B5 ||
									  Format == D3DFMT_A4R4G4B4 || Format == D3DFMT_A8 ||
									  Format == D3DFMT_A8L8     || Format == D3DFMT_DXT2 ||
									  Format == D3DFMT_DXT3     || Format == D3DFMT_DXT4 ||
									  Format == D3DFMT_DXT5);

			if (FAILED(d3d->CheckDeviceFormat(params.AdapterOrdinal, params.DeviceType,
														 mode.Format, Usage, D3DRTYPE_TEXTURE, Format))) {
				const D3DFORMAT *candidates = wants_alpha ? with_alpha : without_alpha;
				for (int i = 0; candidates[i] != D3DFMT_UNKNOWN; ++i) {
					if (SUCCEEDED(d3d->CheckDeviceFormat(params.AdapterOrdinal, params.DeviceType,
																	 mode.Format, Usage, D3DRTYPE_TEXTURE,
																	 candidates[i]))) {
						Format = candidates[i];
						break;
					}
				}
			}
		}

		d3d->Release();
	}

	// D3D9's CreateTexture takes a trailing shared-handle parameter.
	return pDevice->CreateTexture(Width, Height, MipLevels, Usage, Format, Pool,
											  ppTexture, NULL);
}

HRESULT WINAPI D3DXCreateTextureFromFileExA(LPDIRECT3DDEVICE9, LPCSTR,
														  UINT, UINT, UINT,
														  DWORD, D3DFORMAT, D3DPOOL,
														  DWORD, DWORD, D3DCOLOR,
														  void *, void *,
														  LPDIRECT3DTEXTURE9 *ppTexture)
{
	// Unreachable in this tree -- see the note at the top of the file. Loading
	// image files is the engine's own job (TARGA.CPP, ddsfile.cpp), so rather
	// than reimplement a decoder nothing calls, this fails cleanly.
	if (ppTexture != NULL) *ppTexture = NULL;
	return D3DERR_NOTAVAILABLE;
}

//-----------------------------------------------------------------------------
//	Surface copy / convert
//-----------------------------------------------------------------------------

HRESULT WINAPI D3DXLoadSurfaceFromSurface(LPDIRECT3DSURFACE9 pDestSurface,
														const PALETTEENTRY * /*pDestPalette*/,
														const RECT *pDestRect,
														LPDIRECT3DSURFACE9 pSrcSurface,
														const PALETTEENTRY * /*pSrcPalette*/,
														const RECT *pSrcRect,
														DWORD Filter,
														D3DCOLOR /*ColorKey*/)
{
	if (pDestSurface == NULL || pSrcSurface == NULL) return D3DERR_INVALIDCALL;

	D3DSURFACE_DESC dst_desc, src_desc;
	HRESULT hr = pDestSurface->GetDesc(&dst_desc);
	if (FAILED(hr)) return hr;
	hr = pSrcSurface->GetDesc(&src_desc);
	if (FAILED(hr)) return hr;

	if (Bytes_Per_Pixel(dst_desc.Format) == 0 || Bytes_Per_Pixel(src_desc.Format) == 0) {
		// Compressed or otherwise unhandled. Nothing in the engine hits this.
		return D3DERR_WRONGTEXTUREFORMAT;
	}

	RECT dst_r = { 0, 0, (LONG)dst_desc.Width, (LONG)dst_desc.Height };
	RECT src_r = { 0, 0, (LONG)src_desc.Width, (LONG)src_desc.Height };
	if (pDestRect != NULL) dst_r = *pDestRect;
	if (pSrcRect  != NULL) src_r = *pSrcRect;

	// Reject anything that would read or write outside the locked surfaces.
	// The real D3DX validates its rectangles; Resample() below indexes straight
	// off the base pointer and would happily run past the end without this.
	if (dst_r.left < 0 || dst_r.top < 0 ||
		 dst_r.right  > (LONG)dst_desc.Width || dst_r.bottom > (LONG)dst_desc.Height ||
		 src_r.left < 0 || src_r.top < 0 ||
		 src_r.right  > (LONG)src_desc.Width || src_r.bottom > (LONG)src_desc.Height) {
		return D3DERR_INVALIDCALL;
	}

	int dst_w = dst_r.right - dst_r.left, dst_h = dst_r.bottom - dst_r.top;
	int src_w = src_r.right - src_r.left, src_h = src_r.bottom - src_r.top;
	if (dst_w <= 0 || dst_h <= 0 || src_w <= 0 || src_h <= 0) return D3DERR_INVALIDCALL;

	D3DLOCKED_RECT dst_lock, src_lock;
	hr = pSrcSurface->LockRect(&src_lock, NULL, D3DLOCK_READONLY);
	if (FAILED(hr)) return hr;

	hr = pDestSurface->LockRect(&dst_lock, NULL, 0);
	if (FAILED(hr)) {
		pSrcSurface->UnlockRect();
		return hr;
	}

	bool bilinear = ((Filter & 0x7) != D3DX_FILTER_NONE) && (src_w != dst_w || src_h != dst_h);

	Resample((unsigned char *)dst_lock.pBits, dst_lock.Pitch, dst_desc.Format,
				dst_r.left, dst_r.top, dst_w, dst_h,
				(const unsigned char *)src_lock.pBits, src_lock.Pitch, src_desc.Format,
				src_r.left, src_r.top, src_w, src_h,
				bilinear);

	pDestSurface->UnlockRect();
	pSrcSurface->UnlockRect();
	return D3D_OK;
}

//-----------------------------------------------------------------------------
//	Mip chain generation
//-----------------------------------------------------------------------------

HRESULT WINAPI D3DXFilterTexture(LPDIRECT3DBASETEXTURE9 pBaseTexture,
										   const PALETTEENTRY * /*pPalette*/,
										   UINT SrcLevel, DWORD Filter)
{
	if (pBaseTexture == NULL) return D3DERR_INVALIDCALL;

	// Only 2D textures are filtered in this engine.
	if (pBaseTexture->GetType() != D3DRTYPE_TEXTURE) return D3DERR_INVALIDCALL;

	IDirect3DTexture9 *texture = (IDirect3DTexture9 *)pBaseTexture;

	DWORD levels = texture->GetLevelCount();
	if (SrcLevel == D3DX_DEFAULT) SrcLevel = 0;
	if (SrcLevel + 1 >= levels) return D3D_OK;		// nothing below the source level

	D3DSURFACE_DESC desc;
	HRESULT hr = texture->GetLevelDesc(SrcLevel, &desc);
	if (FAILED(hr)) return hr;

	if (Bytes_Per_Pixel(desc.Format) == 0) {
		// DDS files arrive with their mip chains already built, so the engine
		// never asks us to downsample a compressed texture.
		return D3DERR_WRONGTEXTUREFORMAT;
	}

	bool bilinear = ((Filter & 0x7) != D3DX_FILTER_NONE);

	for (DWORD level = SrcLevel; level + 1 < levels; ++level) {
		D3DSURFACE_DESC src_desc, dst_desc;
		if (FAILED(texture->GetLevelDesc(level, &src_desc))) break;
		if (FAILED(texture->GetLevelDesc(level + 1, &dst_desc))) break;

		D3DLOCKED_RECT src_lock, dst_lock;
		if (FAILED(texture->LockRect(level, &src_lock, NULL, D3DLOCK_READONLY))) break;

		if (FAILED(texture->LockRect(level + 1, &dst_lock, NULL, 0))) {
			texture->UnlockRect(level);
			break;
		}

		Resample((unsigned char *)dst_lock.pBits, dst_lock.Pitch, dst_desc.Format,
					0, 0, (int)dst_desc.Width, (int)dst_desc.Height,
					(const unsigned char *)src_lock.pBits, src_lock.Pitch, src_desc.Format,
					0, 0, (int)src_desc.Width, (int)src_desc.Height,
					bilinear);

		texture->UnlockRect(level + 1);
		texture->UnlockRect(level);
	}

	return D3D_OK;
}
