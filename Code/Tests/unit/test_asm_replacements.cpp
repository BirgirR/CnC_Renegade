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

#endif	// _M_IX86
