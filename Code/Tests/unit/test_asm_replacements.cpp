/*
**	Inline assembly that was replaced by C and intrinsics, checked against the
**	original. Each reference below is the assembly as it was, and each test runs
**	it beside the replacement over a sweep of inputs and requires the same bits.
**
**	The x87 sequences are run at both precisions the game sees: 53-bit, the
**	default and what any thread has before the Direct3D device exists, and
**	24-bit, which Direct3D sets on the thread that creates the device.
*/

#include "wwtest.h"
#include "wwmath.h"
#include "cpudetect.h"
#include "mpu.h"
#include "dx8wrapper.h"
#include "lcw.h"
#include "blitblit.h"
#include "rlerle.h"

#include <float.h>
#include <string.h>
#include <intrin.h>

#if defined(_M_IX86)

namespace
{

unsigned int Bits(float f)				{ unsigned int b; memcpy(&b, &f, 4); return b; }
float From_Bits(unsigned int b)		{ float f; memcpy(&f, &b, 4); return f; }

//	Runs body at each x87 precision, restoring the control word afterwards.
template<class Body> void At_Each_Precision(Body body)
{
	static const unsigned int precisions[] = { _PC_53, _PC_24 };
	unsigned int saved;
	_controlfp_s(&saved, 0, 0);
	for (int i = 0; i < 2; ++i) {
		unsigned int unused;
		_controlfp_s(&unused, precisions[i], _MCW_PC);
		body();
	}
	unsigned int unused;
	_controlfp_s(&unused, saved & _MCW_PC, _MCW_PC);
}

//-----------------------------------------------------------------------------
//	The original assembly
//-----------------------------------------------------------------------------

__declspec(noinline) long Ref_Float_To_Long(float f)
{
	long i;
	__asm {
		fld [f]
		fistp [i]
	}
	return i;
}

__declspec(noinline) long Ref_Double_To_Long(double f)
{
	long retval;
	__asm fld	qword ptr [f]
	__asm fistp dword ptr [retval]
	return retval;
}

__declspec(noinline) float Ref_Sqrt(float val)
{
	float retval;
	__asm {
		fld [val]
		fsqrt
		fstp [retval]
	}
	return retval;
}

//	DX8Wrapper::Convert_Color(const Vector3&, float), as of 2cdca14.
__declspec(noinline) unsigned int Ref_Convert_Color(const Vector3& color, float alpha)
{
	const float scale = 255.0;
	unsigned int col;
	__asm
	{
		push	esi
		push	edi
		sub	esp,20

		fwait
		fstcw		[esp+16]
		mov		eax,[esp+16]
		mov		edi,eax
		and		eax,~(1024|2048)
		or			eax,(1024|2048)
		sub		edi,eax
		jz			skip
		mov		[esp],eax
		fldcw		[esp]
skip:
		mov	esi,dword ptr color
		fld	dword ptr[scale]

		fld	dword ptr[esi]
		fld	dword ptr[esi+4]
		fld	dword ptr[esi+8]
		fld	dword ptr[alpha]
		fld	st(4)
		fmul	st(4),st
		fmul	st(3),st
		fmul	st(2),st
		fmulp	st(1),st
		fistp	dword ptr[esp+0]
		fistp	dword ptr[esp+4]
		fistp	dword ptr[esp+8]
		fistp	dword ptr[esp+12]
		mov	ecx,[esp]
		mov	eax,[esp+4]
		mov	edx,[esp+8]
		mov	esi,[esp+12]
		shl	ecx,24
		shl	esi,16
		shl	edx,8
		or		eax,ecx
		or		eax,esi
		or		eax,edx

		fstp	st(0)

		cmp	edi,0
		je		not_changed
		fwait
		fldcw	[esp+16];
not_changed:
		add	esp,20
		pop	edi
		pop	esi

		mov	col,eax
	}
	return col;
}

//	One lane of DX8Wrapper::Clamp_Color's CMOV path.
__declspec(noinline) unsigned int Ref_Clamp_Bits(unsigned int bits)
{
	unsigned int result;
	__asm
	{
		mov	edx,0x3f800000
		mov	edi,[bits]
		mov	ebx,edi
		sar	edi,31
		not	edi
		and	edi,ebx
		cmp	edi,edx
		cmovnb edi,edx
		mov	[result],edi
	}
	return result;
}

__declspec(noinline) void Ref_CPUID(unsigned leaf, unsigned regs[4])
{
	unsigned a, b, c, d;
	__asm
	{
		pushad
		mov		eax,[leaf]
		xor		ebx,ebx
		xor		ecx,ecx
		xor		edx,edx
		cpuid
		mov		[a],eax
		mov		[b],ebx
		mov		[c],ecx
		mov		[d],edx
		popad
	}
	regs[0] = a; regs[1] = b; regs[2] = c; regs[3] = d;
}

//	Float bit patterns worth sweeping: every stride-th one, plus each special.
template<class Check> void Sweep_Floats(unsigned int stride, Check check)
{
	for (unsigned long long b = 0; b <= 0xffffffffull; b += stride) {
		check(From_Bits((unsigned int)b));
	}
	static const unsigned int specials[] = {
		0x00000000, 0x80000000, 0x00000001, 0x007fffff, 0x00800000,
		0x3f000000, 0x3f800000, 0x3fc00000, 0x7f7fffff, 0x7f800000,
		0xff800000, 0x7fc00000, 0xffc00000, 0x7f800001, 0x4effffff,
		0x4f000000, 0xcf000000, 0xcf000001, 0x4b000000, 0x4b7fffff,
	};
	for (int i = 0; i < (int)(sizeof(specials) / sizeof(specials[0])); ++i) {
		check(From_Bits(specials[i]));
	}
}


//	LCW_Comp as it was: wwlib/lcw.cpp before the C port.
__declspec(noinline) int Ref_LCW_Comp(void const * source, void * dest, int datasize)
{
	int retval = 0;
#ifdef _WINDOWS
	long inlen = 0;
	long a1stdest = 0;
	long a1stsrc = 0;
	long lenoff = 0;
	long ndest = 0;
	long count = 0;
	long matchoff = 0;
	long end_of_data =0;
#ifdef _DEBUG
	inlen = inlen;
	a1stdest = a1stdest;
	a1stsrc = a1stsrc;
	lenoff = lenoff;
	ndest = ndest;
	count = count;
	matchoff = matchoff;
	end_of_data = end_of_data;
#endif

	__asm {
		cld			// make sure all string commands are forward
		mov	edi,[dest]
		mov	esi,[source]
		mov	edx,[datasize]		// get length of data to compress

// compress data to the following codes in the format b = byte, w = word
// n = byte code pulled from compressed data
//   Bit field of n		command		description
// n=0xxxyyyy,yyyyyyyy		short run	back y bytes and run x+3
// n=10xxxxxx,n1,n2,...,nx+1	med length	copy the next x+1 bytes
// n=11xxxxxx,w1			med run		run x+3 bytes from offset w1
// n=11111111,w1,w2		long run	run w1 bytes from offset w2
// n=10000000			end		end of data reached

		mov	ebx,esi
		add	ebx,edx
		mov	[end_of_data],ebx
		mov	[inlen],1	//; set the in-length flag
		mov	[a1stdest],edi	//; save original dest offset for size calc
		mov	[a1stsrc],esi	//; save offset of first byte of data
		mov	[lenoff],edi	//; save the offset of the legth of this len
		sub	eax,eax
		mov	al,081h		//; the first byte is always a len
		stosb			//; write out a len of 1
		lodsb			//; get the byte
		stosb			//; save it
	}

loopstart:
	__asm {
		mov	[ndest],edi	//; save offset of compressed data
		mov	edi,[a1stsrc]	//; get the offset to the first byte of data
		mov	[count],1	//; set the count of run to 0
	}
searchloop:
	__asm {
		sub	eax,eax
		mov	al,[esi]	//; get the current byte of data
		cmp	al,[esi+64]
		jne	short notrunlength

		mov	ebx,edi

		mov	edi,esi
		mov	ecx,[end_of_data]
		sub	ecx,edi
		repe	scasb
		dec	edi
		mov	ecx,edi
		sub	ecx,esi
		cmp	ecx,65
		jb	short notlongenough

		mov	[inlen],0	//; clear the in-length flag
//		mov	[DWORD PTR inlen],0	//; clear the in-length flag
		mov	esi,edi
		mov	edi,[ndest]	//; get the offset of our compressed data

		mov	ah,al
		mov	al,0FEh
		stosb
		xchg	ecx,eax
		stosw
		mov	al,ch
		stosb

		mov	[ndest],edi	//; save offset of compressed data
		mov	edi,ebx
		jmp	searchloop
	}
notlongenough:
	__asm {
		mov	edi,ebx
	}
notrunlength:
oploop:
	__asm {
		mov	ecx,esi		//; get the address of the last byte +1
		sub	ecx,edi		//; get the total number of bytes left to comp
		jz	short searchdone

		repne	scasb		//; look for a match
		jne	short searchdone	//; if we don't find one we're done

		mov	ebx,[count]
		mov	ah,[esi+ebx-1]
		cmp	ah,[edi+ebx-2]

		jne	oploop

		mov	edx,esi		//; save this spot for the next search
		mov	ebx,edi		//; save this spot for the length calc
		dec	edi		//; back up one for compare
		mov	ecx,[end_of_data]		//; get the end of data
		sub	ecx,esi		//; sub current source for max len

		repe	cmpsb		//; see how many bytes match

		jne	short notend	//; if found mismatch then di - bx = match count

		inc	edi		//; else cx = 0 and di + 1 - bx = match count
	}
notend:
	__asm {
		mov	esi,edx		//; restore si
		mov	eax,edi		//; get the dest
		sub	eax,ebx		//; sub the start for total bytes that match
		mov	edi,ebx		//; restore dest
		cmp	eax,[count]	//; see if its better than before
		jb	searchloop	//; if not keep looking

		mov	[count],eax	//; if so keep the count
		dec	ebx		//; back it up for the actual match offset
		mov	[matchoff],ebx //; save the offset for later
		jmp	searchloop	//; loop until we searched it all
	}
searchdone:
	__asm {
		mov	ecx,[count]	//; get the count of the longest run
		mov	edi,[ndest]	//; get the offset of our compressed data
		cmp	ecx,2		//; see if its not enough run to matter
		jbe	short lenin		//; if its 0,1, or 2 its too small

		cmp	ecx,10		//; if not, see if it would fit in a short
		ja	short medrun	//; if not, see if its a medium run

		mov	eax,esi		//; if its short get the current address
		sub	eax,[matchoff] //; sub the offset of the match
		cmp	eax,0FFFh	//; if its less than 12 bits its a short
		ja	short medrun	//; if its not, its a medium
	}
//shortrun:
	__asm {
		sub	ebx,ebx
		mov	bl,cl		//; get the length (3-10)
		sub	bl,3		//; sub 3 for a 3 bit number 0-7
		shl	bl,4		//; shift it left 4
		add	ah,bl		//; add in the length for the high nibble
		xchg	ah,al		//; reverse the bytes for a word store
		jmp	short srunnxt	//; do the run fixup code
	}
medrun:
	__asm {
		cmp	ecx,64		//; see if its a short run
		ja	short longrun	//; if not, oh well at least its long

		sub	cl,3		//; back down 3 to keep it in 6 bits
		or	cl,0C0h		//; the highest bits are always on
		mov	al,cl		//; put it in al for the stosb
		stosb			//; store it
		jmp	short medrunnxt //; do the run fixup code
	}
lenin:
	__asm {
		cmp	[inlen],0	//; is it doing a length?
//		cmp	[DWORD PTR inlen],0	//; is it doing a length?
		jnz	short len	//; if so, skip code
	}
lenin1:
	__asm {
		mov	[lenoff],edi	//; save the length code offset
		mov	al,80h		//; set the length to 0
		stosb			//; save it
	}
len:
	__asm {
		mov	ebx,[lenoff]	//; get the offset of the length code
		cmp	[ebx],0BFh	//; see if its maxed out
//		cmp	[BYTE PTR ebx],0BFh	//; see if its maxed out
		je	lenin1	//; if so put out a new len code
	}
//stolen:
	__asm {
		inc	[ebx] //; inc the count code
//		inc	[BYTE PTR ebx] //; inc the count code
		lodsb			//; get the byte
		stosb			//; store it
		mov	[inlen],1	//; we are now in a length so save it
//		mov	[DWORD PTR inlen],1	//; we are now in a length so save it
		jmp	short nxt	//; do the next code
	}
longrun:
	__asm {
		mov	al,0ffh		//; its a long so set a code of FF
		stosb			//; store it

		mov	eax,[count]	//; send out the count
		stosw			//; store it
	}
medrunnxt:
	__asm {
		mov	eax,[matchoff] //; get the offset
		sub	eax,[a1stsrc]	//; make it relative tot he start of data
	}
srunnxt:
	__asm {
		stosw			//; store it
		//; this code common to all runs
		add	esi,[count]	//; add in the length of the run to the source
		mov	[inlen],0	//; set the in leght flag to false
//		mov	[DWORD PTR inlen],0	//; set the in leght flag to false
	}
nxt:
	__asm {
		cmp	esi,[end_of_data]		//; see if we did the whole pic
		jae	short outofhere		//; if so, cool! were done

		jmp	loopstart
	}
outofhere:
	__asm {
		mov	ax,080h		//; remember to send an end of data code
		stosb			//; store it
		mov	eax,edi		//; get the last compressed address
		sub	eax,[a1stdest]	//; sub the first for the compressed size
		mov	[retval],eax
	}
#endif
	return(retval);
}


//	The blitter specialisations as they were in blitblit.h, with the class's
//	tables passed in.
__declspec(noinline) void Ref_BlitTrans8(void * dest, void const * source, int len)
{
	__asm {
		mov	esi,[source]
		mov	edi,[dest]
		mov	ecx,[len]
		dec	edi
		inc	ecx
	}
again:
	__asm {
		dec	ecx
		jz		fini
		mov	al,[esi]
		inc	edi
		inc	esi
		test	al,al
		jz		again
		mov	[edi],al
		jmp	again
	}
fini:;
}

__declspec(noinline) void Ref_BlitTransXlat16(void * dest, void const * source, int len, unsigned short const * xlator)
{
	__asm {
		mov	ebx,[xlator]
		mov	ecx,[len]
		inc	ecx
		mov	edi,[dest]
		sub	edi,2
		mov	esi,[source]
		xor	eax,eax
	}
again:
	__asm {
		dec	ecx
		jz		over
		add	edi,2
		mov	al,[esi]
		inc	esi
		or		al,al
		jz		again
		mov	dx,[ebx+eax*2]
		mov	[edi],dx
		jmp	again
	}
over:;
}

//	BlitTransRemapXlat and BlitTransZRemapXlat had the same body.
__declspec(noinline) void Ref_BlitTransRemapXlat16(void * dest, void const * source, int len, unsigned char const * remapper, unsigned short const * translator)
{
	__asm {
		mov	ecx,[len]
		mov	edi,[dest]
		sub	edi,2
		mov	esi,[source]
		mov	ebx,[remapper]
		mov	edx,[translator]
		xor	eax,eax
	}
again:
	__asm {
		dec	ecx
		jz		over
		add	edi,2
		xor	eax,eax
		lodsb
		or		al,al
		jz		again
		mov	al,[ebx+eax]
		mov	ax,[edx+eax*2]
		mov	[edi],ax
		jmp	again
	}
over:;
}

__declspec(noinline) void Ref_BlitPlainXlat16(void * dest, void const * source, int len, unsigned short const * remapper)
{
	__asm {
		mov	ebx,[remapper]
		mov	ecx,[len]
		mov	esi,[source]
		mov	edi,[dest]
		sub	edi,2
	}
again:
	__asm {
		xor	eax,eax
		add	edi,2
		mov	al,[esi]
		inc	esi
		mov	ax,[ebx+eax*2]
		mov	[edi],ax
		dec	ecx
		jnz	again
	}
}

//	The RLE specialisations from rlerle.h. RemapXlat and ZRemapXlat differed only
//	in how they fetched the remap table; with remapper NULL this is
//	RLEBlitTransXlat, which had no remap step.
__declspec(noinline) void Ref_RLEBlit16(void * dest, void const * source, int len, int leadskip, unsigned char const * remapper, unsigned short const * transtable)
{
	if (remapper == NULL) {
		__asm {
			mov	ecx,[len]
			mov	edi,[dest]
			mov	esi,[source]
			mov	ebx,[transtable]
			mov	edx,[leadskip]
			xor	eax,eax
		}
moreskip1:
		__asm {
			test	edx,edx
			jle	nomoreskip1
			dec	edx
			lodsb
			test	al,al
			jnz	moreskip1
			lodsb
			sub	edx,eax
			inc	edx
			jmp	moreskip1
		}
nomoreskip1:
		__asm {
			neg	edx
			sub	ecx,edx
			lea	edi,[edi+edx*2]
		}
moredata1:
		__asm {
			xor	eax,eax
			or	ecx,ecx
			jle	fini1
			lodsb
			test	al,al
			jz	transparent1
			mov	ax,[ebx+eax*2]
			dec	ecx
			stosw
			jmp	moredata1
		}
transparent1:
		__asm {
			lodsb
			lea	edi,[edi+eax*2]
			sub	ecx,eax
			jmp	moredata1
		}
fini1:
		return;
	}

	__asm {
		mov	ecx,[len]
		mov	edi,[dest]
		mov	esi,[source]
		mov	ebx,[remapper]
		mov	edx,[leadskip]
		xor	eax,eax
	}
moreskip2:
	__asm {
		test	edx,edx
		jle	nomoreskip2
		dec	edx
		lodsb
		test	al,al
		jnz	moreskip2
		lodsb
		sub	edx,eax
		inc	edx
		jmp	moreskip2
	}
nomoreskip2:
	__asm {
		neg	edx
		sub	ecx,edx
		lea	edi,[edi+edx*2]
		mov	edx,[transtable]
	}
moredata2:
	__asm {
		xor	eax,eax
		or	ecx,ecx
		jle	fini2
		lodsb
		test	al,al
		jz	transparent2
		mov	al,[ebx+eax]
		mov	ax,[edx+eax*2]
		dec	ecx
		stosw
		jmp	moredata2
	}
transparent2:
	__asm {
		lodsb
		lea	edi,[edi+eax*2]
		sub	ecx,eax
		jmp	moredata2
	}
fini2:;
}

} // namespace

//-----------------------------------------------------------------------------
//	WWMath
//-----------------------------------------------------------------------------

WWTEST(AsmReplacement, FloatToLong)
{
	int mismatches = 0;
	Sweep_Floats(251, [&](float f) {
		if (WWMath::Float_To_Long(f) != Ref_Float_To_Long(f)) ++mismatches;
	});
	//	Rounding at the halves is where a truncating replacement would differ.
	for (int k = -70000; k <= 70000; ++k) {
		float halves[] = { k + 0.5f, k - 0.5f, k + 0.49999997f, k + 0.50000006f };
		for (int i = 0; i < 4; ++i) {
			if (WWMath::Float_To_Long(halves[i]) != Ref_Float_To_Long(halves[i])) ++mismatches;
		}
	}
	CHECK_EQ(mismatches, 0);
}

WWTEST(AsmReplacement, DoubleToLong)
{
	int mismatches = 0;
	for (int k = -300000; k <= 300000; ++k) {
		double values[] = { k + 0.5, k - 0.5, k * 1.0000001, k * 3037.000499, k + 0.4999999999 };
		for (int i = 0; i < 5; ++i) {
			if (WWMath::Float_To_Long(values[i]) != Ref_Double_To_Long(values[i])) ++mismatches;
		}
	}
	double specials[] = { 0.0, -0.0, 2147483647.0, 2147483647.5, 2147483648.0, -2147483648.5,
								 1e300, -1e300, DBL_MAX, -DBL_MAX };
	for (int i = 0; i < 10; ++i) {
		if (WWMath::Float_To_Long(specials[i]) != Ref_Double_To_Long(specials[i])) ++mismatches;
	}
	CHECK_EQ(mismatches, 0);
}

WWTEST(AsmReplacement, Sqrt)
{
	int mismatches = 0;
	At_Each_Precision([&]() {
		Sweep_Floats(97, [&](float f) {
			if (Bits(WWMath::Sqrt(f)) != Bits(Ref_Sqrt(f))) ++mismatches;
		});
	});
	CHECK_EQ(mismatches, 0);
}

//-----------------------------------------------------------------------------
//	DX8Wrapper colour conversion
//-----------------------------------------------------------------------------

WWTEST(AsmReplacement, ConvertColor)
{
	int mismatches = 0;
	At_Each_Precision([&]() {
		//	One channel at a time over [0,1], every 13th float.
		for (unsigned int b = 0; b <= 0x3f800000u; b += 13) {
			float f = From_Bits(b);
			Vector3 c(f, 0.25f, 0.75f);
			if (DX8Wrapper::Convert_Color(c, 0.5f) != Ref_Convert_Color(c, 0.5f)) ++mismatches;
		}
		//	Every channel at and around each k/255, where truncation is decided.
		for (int k = 0; k <= 255; ++k) {
			unsigned int centre = Bits(k / 255.0f);
			for (int d = -64; d <= 64; ++d) {
				float f = From_Bits(centre + d);
				if (k == 0 && d < 0) f = -From_Bits((unsigned int)-d);
				Vector3 cs[] = { Vector3(f, 0, 0), Vector3(0, f, 0), Vector3(0, 0, f) };
				for (int i = 0; i < 3; ++i) {
					if (DX8Wrapper::Convert_Color(cs[i], 1.0f) != Ref_Convert_Color(cs[i], 1.0f)) ++mismatches;
				}
				if (DX8Wrapper::Convert_Color(Vector3(1, 1, 1), f) != Ref_Convert_Color(Vector3(1, 1, 1), f)) ++mismatches;
			}
		}
		//	Out of range, which callers are told not to pass but some do.
		float wild[] = { -1.0f, -0.001f, 1.001f, 1.5f, 2.0f, 255.0f, 300.0f, 65535.0f };
		for (int i = 0; i < 8; ++i) {
			Vector3 c(wild[i], wild[i], wild[i]);
			if (DX8Wrapper::Convert_Color(c, wild[i]) != Ref_Convert_Color(c, wild[i])) ++mismatches;
		}
	});
	CHECK_EQ(mismatches, 0);
}

WWTEST(AsmReplacement, ClampColor)
{
	int mismatches = 0;
	Sweep_Floats(101, [&](float f) {
		Vector4 v(f, f, f, f);
		DX8Wrapper::Clamp_Color(v);
		unsigned int expected = Ref_Clamp_Bits(Bits(f));
		for (int i = 0; i < 4; ++i) {
			if (Bits(v[i]) != expected) ++mismatches;
		}
	});
	CHECK_EQ(mismatches, 0);
}

//-----------------------------------------------------------------------------
//	CPU detection
//-----------------------------------------------------------------------------

WWTEST(AsmReplacement, CPUID)
{
	CHECK(CPUDetectClass::Has_CPUID_Instruction());

	unsigned max_basic[4], max_ext[4];
	Ref_CPUID(0, max_basic);
	Ref_CPUID(0x80000000u, max_ext);

	int mismatches = 0;
	unsigned leaves[] = { 0, 1, 2, 4, 7, 0xb, 0x80000000u, 0x80000001u, 0x80000002u,
								 0x80000003u, 0x80000004u, 0x80000006u, 0x80000008u };
	for (int i = 0; i < (int)(sizeof(leaves) / sizeof(leaves[0])); ++i) {
		unsigned leaf = leaves[i];
		if (leaf < 0x80000000u ? leaf > max_basic[0] : leaf > max_ext[0]) continue;

		unsigned expected[4], actual[4];
		Ref_CPUID(leaf, expected);
		CHECK(CPUDetectClass::CPUID(actual[0], actual[1], actual[2], actual[3], leaf));
		//	Leaf 1 EBX carries the APIC id of whichever core ran it.
		if (leaf == 1) { expected[1] &= 0x00ffffff; actual[1] &= 0x00ffffff; }
		if (memcmp(expected, actual, sizeof(expected)) != 0) ++mismatches;
	}
	CHECK_EQ(mismatches, 0);
}

WWTEST(AsmReplacement, RdtscAdvances)
{
	unsigned __int64 a = __rdtsc();
	unsigned long high;
	unsigned long low = Get_CPU_Clock(high);
	unsigned __int64 b = ((unsigned __int64)high << 32) | low;
	CHECK(b >= a);
	CHECK(CPUDetectClass::Get_Processor_Speed() > 0);
}


//-----------------------------------------------------------------------------
//	LCW compression
//-----------------------------------------------------------------------------

WWTEST(AsmReplacement, LcwComp)
{
	//	The original reads up to 64 bytes past the end of its input, so every
	//	buffer carries zeroed slack; its output never depended on those bytes.
	static unsigned char src[70000 + 128], expected[90000], actual[90000], back[70000];
	unsigned int seed = 12345;
	int mismatches = 0, round_trip_failures = 0, cases = 0;

	for (int kind = 0; kind < 9; ++kind) {
		for (int trial = 0; trial < 40; ++trial) {
			int len = (trial < 30) ? 2 + trial * trial * 7 : 2 + (int)((seed = seed * 1103515245 + 12345) % 20000);
			memset(src, 0, sizeof(src));
			for (int i = 0; i < len; ++i) {
				seed = seed * 1103515245 + 12345;
				unsigned int r = seed >> 16;
				switch (kind) {
					case 0: src[i] = (unsigned char)r; break;											// noise
					case 1: src[i] = (unsigned char)(trial & 7); break;								// one run
					case 2: src[i] = (unsigned char)((i / (3 + trial)) & 3); break;			// short runs
					case 3: src[i] = (unsigned char)("abcdefgh"[i % (1 + trial % 8)]); break;	// repeats
					case 4: src[i] = (r % 100 < 85) ? 0 : (unsigned char)r; break;			// sparse
					case 5: src[i] = (unsigned char)((i / 70) * 13); break;						// fills of 70
					case 6: src[i] = (i % 300 < 200) ? 0x55 : (unsigned char)r; break;		// fills and noise
					case 7: src[i] = (unsigned char)(i < len / 2 ? r : src[i - len / 2]); break;	// far repeat
					case 8: src[i] = (unsigned char)" etaoinshrdlu\n"[r % 14]; break;		// text-like
				}
			}
			int n = LCW_Comp(src, actual, len);
			int m = Ref_LCW_Comp(src, expected, len);
			++cases;
			if (n != m || memcmp(actual, expected, n) != 0) ++mismatches;
			if (LCW_Uncomp(actual, back, len) != (int)len || memcmp(back, src, len) != 0) ++round_trip_failures;
		}
	}
	CHECK(cases == 360);
	CHECK_EQ(mismatches, 0);
	CHECK_EQ(round_trip_failures, 0);
}


//-----------------------------------------------------------------------------
//	Blitters
//-----------------------------------------------------------------------------

WWTEST(AsmReplacement, Blitters)
{
	static unsigned char src[400];
	static unsigned char dst8a[400], dst8b[400];
	static unsigned short dst16a[400], dst16b[400];
	static unsigned short xlat[256];
	static unsigned char remap[256];
	unsigned char const * remap_ptr = remap;

	unsigned int seed = 777;
	for (int i = 0; i < 256; ++i) {
		seed = seed * 1103515245 + 12345;
		xlat[i] = (unsigned short)(seed >> 8);
		seed = seed * 1103515245 + 12345;
		remap[i] = (unsigned char)(seed >> 16);
	}

	BlitTrans<unsigned char>						trans8;
	BlitTransXlat<unsigned short>					trans_xlat(xlat);
	BlitPlainXlat<unsigned short>					plain_xlat(xlat);
	BlitTransRemapXlat<unsigned short>			remap_xlat(remap, xlat);
	BlitTransZRemapXlat<unsigned short>			zremap_xlat(&remap_ptr, xlat);

	struct Fill {
		static void Do(unsigned char * a8, unsigned char * b8, unsigned short * a16, unsigned short * b16) {
			for (int i = 0; i < 400; ++i) {
				a8[i] = b8[i] = (unsigned char)(i * 7);
				a16[i] = b16[i] = (unsigned short)(i * 31);
			}
		}
	};

	int mismatches = 0, short_by_one = 0;
	for (int trial = 0; trial < 4000; ++trial) {
		int len = 1 + trial % 300;
		for (int i = 0; i < len; ++i) {
			seed = seed * 1103515245 + 12345;
			src[i] = ((seed >> 16) % 3 == 0) ? 0 : (unsigned char)(seed >> 8);
		}

		Fill::Do(dst8a, dst8b, dst16a, dst16b);
		Ref_BlitTrans8(dst8a, src, len);
		trans8.BlitForward(dst8b, src, len);
		if (memcmp(dst8a, dst8b, sizeof(dst8a)) != 0) ++mismatches;

		Fill::Do(dst8a, dst8b, dst16a, dst16b);
		Ref_BlitTransXlat16(dst16a, src, len, xlat);
		trans_xlat.BlitForward(dst16b, src, len);
		if (memcmp(dst16a, dst16b, sizeof(dst16a)) != 0) ++mismatches;

		Fill::Do(dst8a, dst8b, dst16a, dst16b);
		Ref_BlitPlainXlat16(dst16a, src, len, xlat);
		plain_xlat.BlitForward(dst16b, src, len);
		if (memcmp(dst16a, dst16b, sizeof(dst16a)) != 0) ++mismatches;

		//	The two remapping versions never drew the last pixel: the assembly
		//	given len matches the template given len - 1.
		Fill::Do(dst8a, dst8b, dst16a, dst16b);
		Ref_BlitTransRemapXlat16(dst16a, src, len, remap, xlat);
		remap_xlat.BlitForward(dst16b, src, len - 1);
		if (memcmp(dst16a, dst16b, sizeof(dst16a)) != 0) ++short_by_one;

		Fill::Do(dst8a, dst8b, dst16a, dst16b);
		Ref_BlitTransRemapXlat16(dst16a, src, len, remap, xlat);
		zremap_xlat.BlitForward(dst16b, src, len - 1);
		if (memcmp(dst16a, dst16b, sizeof(dst16a)) != 0) ++short_by_one;
	}
	CHECK_EQ(mismatches, 0);
	CHECK_EQ(short_by_one, 0);
}

WWTEST(AsmReplacement, RLEBlitters)
{
	static unsigned char rle[1200];
	static unsigned short dsta[600], dstb[600];
	static unsigned short xlat[256];
	static unsigned char remap[256];
	unsigned char const * remap_ptr = remap;

	unsigned int seed = 4242;
	for (int i = 0; i < 256; ++i) {
		seed = seed * 1103515245 + 12345;
		xlat[i] = (unsigned short)(seed >> 8);
		seed = seed * 1103515245 + 12345;
		remap[i] = (unsigned char)(seed >> 16);
	}

	RLEBlitTransXlat<unsigned short>				xlat_blit(xlat);
	RLEBlitTransRemapXlat<unsigned short>		remap_blit(remap, xlat);
	RLEBlitTransZRemapXlat<unsigned short>		zremap_blit(&remap_ptr, xlat);

	int mismatches = 0;
	for (int trial = 0; trial < 4000; ++trial) {
		//	An RLE row: pixels, and zero followed by a transparent run length.
		int total = 0, n = 0;
		int target = 1 + trial % 400;
		while (total < target) {
			seed = seed * 1103515245 + 12345;
			if ((seed >> 16) % 4 == 0) {
				int run = 1 + (int)((seed >> 8) % 20);
				if (total + run > target) run = target - total;
				rle[n++] = 0;
				rle[n++] = (unsigned char)run;
				total += run;
			} else {
				rle[n++] = (unsigned char)(1 + (seed >> 8) % 255);
				total += 1;
			}
		}
		seed = seed * 1103515245 + 12345;
		int leadskip = (int)((seed >> 16) % (unsigned)total);
		int len = total - leadskip;

		for (int k = 0; k < 3; ++k) {
			for (int i = 0; i < 600; ++i) dsta[i] = dstb[i] = (unsigned short)(i * 31);
			if (k == 0) {
				Ref_RLEBlit16(dsta, rle, len, leadskip, NULL, xlat);
				xlat_blit.Blit(dstb, rle, len, leadskip);
			} else if (k == 1) {
				Ref_RLEBlit16(dsta, rle, len, leadskip, remap, xlat);
				remap_blit.Blit(dstb, rle, len, leadskip);
			} else {
				Ref_RLEBlit16(dsta, rle, len, leadskip, remap, xlat);
				zremap_blit.Blit(dstb, rle, len, leadskip);
			}
			if (memcmp(dsta, dstb, sizeof(dsta)) != 0) ++mismatches;
		}
	}
	CHECK_EQ(mismatches, 0);
}

#endif	// _M_IX86
