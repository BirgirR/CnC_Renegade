/*
**	Tests for wwlib -- the engine's strings, containers and hashing.
**
**	These are the primitives every other library builds on, so a defect here is
**	both invisible and everywhere. Several of the cases below are regression
**	tests for changes made while porting this tree to modern MSVC; those are
**	marked and say what they are guarding against.
*/

#include "wwtest.h"
#include <new>			// placement new, for the post-destruction vector test

#include "wwstring.h"
#include "widestring.h"
#include "vector.h"
#include "index.h"
#include "realcrc.h"
#include "random.h"

//-----------------------------------------------------------------------------
//	StringClass
//-----------------------------------------------------------------------------

WWTEST(StringClass, ConstructAndCompare)
{
	StringClass s("hello");
	CHECK_STR_EQ((const char *)s, "hello");
	CHECK_EQ(s.Get_Length(), 5);
	CHECK(s == "hello");
	CHECK(s != "world");
}

WWTEST(StringClass, Assignment)
{
	StringClass a("first");
	StringClass b;
	b = a;
	CHECK_STR_EQ((const char *)b, "first");

	b = "second";
	CHECK_STR_EQ((const char *)b, "second");
	CHECK_EQ(b.Get_Length(), 6);

	// Assigning must not disturb the source.
	CHECK_STR_EQ((const char *)a, "first");
}

WWTEST(StringClass, Concatenate)
{
	StringClass s("abc");
	s += "def";
	CHECK_STR_EQ((const char *)s, "abcdef");
	CHECK_EQ(s.Get_Length(), 6);

	s += 'g';
	CHECK_STR_EQ((const char *)s, "abcdefg");
}

WWTEST(StringClass, Empty)
{
	StringClass s;
	CHECK_EQ(s.Get_Length(), 0);

	s = "";
	CHECK_EQ(s.Get_Length(), 0);
	CHECK(s == "");
}

WWTEST(StringClass, Format)
{
	StringClass s;
	s.Format("%s-%d", "x", 42);
	CHECK_STR_EQ((const char *)s, "x-42");
}

WWTEST(StringClass, GrowsBeyondInternalBuffer)
{
	// StringClass keeps a small inline buffer and heap-allocates past it; make
	// sure the handover is clean.
	StringClass s;
	for (int i = 0; i < 200; ++i) {
		s += "ab";
	}
	CHECK_EQ(s.Get_Length(), 400);
	CHECK(((const char *)s)[399] == 'b');
	CHECK(((const char *)s)[400] == 0);
}

//-----------------------------------------------------------------------------
//	WideStringClass
//-----------------------------------------------------------------------------

WWTEST(WideStringClass, Basics)
{
	WideStringClass w(L"wide");
	CHECK_EQ(w.Get_Length(), 4);

	WideStringClass other;
	other = w;
	CHECK_EQ(other.Get_Length(), 4);
	CHECK(other == L"wide");
}

//-----------------------------------------------------------------------------
//	DynamicVectorClass
//-----------------------------------------------------------------------------

WWTEST(DynamicVectorClass, AddAndIndex)
{
	DynamicVectorClass<int> v;
	CHECK_EQ(v.Count(), 0);

	for (int i = 0; i < 10; ++i) {
		v.Add(i * 10);
	}

	CHECK_EQ(v.Count(), 10);
	CHECK_EQ(v[0], 0);
	CHECK_EQ(v[9], 90);
}

WWTEST(DynamicVectorClass, DeleteByValue)
{
	// NB: deliberately not DynamicVectorClass<int>. With T == int, Delete(int)
	// and Delete(T const &) are genuinely ambiguous and the by-value form
	// cannot be named at all -- which is exactly why Delete_Index exists. Use a
	// T where the two overloads are distinguishable.
	DynamicVectorClass<float> v;
	v.Add(1.0f);
	v.Add(2.0f);
	v.Add(3.0f);

	CHECK(v.Delete(2.0f));		// removes the element equal to 2.0f
	CHECK_EQ(v.Count(), 2);
	CHECK_NEAR(v[0], 1.0f, 1.0e-6f);
	CHECK_NEAR(v[1], 3.0f, 1.0e-6f);
}

WWTEST(DynamicVectorClass, DeleteIndexRegression)
{
	// REGRESSION: Delete(int) and Delete(T const &) are ambiguous when T is int,
	// which broke wwnet/connect.cpp under modern MSVC. Delete_Index was added to
	// give an unambiguous by-index form. These two must behave differently on
	// the same input: index 1 removes the *second* element, value 1 removes the
	// element equal to 1.
	DynamicVectorClass<int> by_index;
	by_index.Add(10);
	by_index.Add(11);
	by_index.Add(12);
	CHECK(by_index.Delete_Index(1));
	CHECK_EQ(by_index.Count(), 2);
	CHECK_EQ(by_index[0], 10);
	CHECK_EQ(by_index[1], 12);

	// Out of range must fail rather than corrupt.
	CHECK(!by_index.Delete_Index(99));
	CHECK_EQ(by_index.Count(), 2);
}

WWTEST(DynamicVectorClass, DeleteAllAndReuse)
{
	DynamicVectorClass<int> v;
	for (int i = 0; i < 5; ++i) v.Add(i);

	v.Delete_All();
	CHECK_EQ(v.Count(), 0);

	v.Add(99);
	CHECK_EQ(v.Count(), 1);
	CHECK_EQ(v[0], 99);
}

WWTEST(DynamicVectorClass, Insert)
{
	DynamicVectorClass<int> v;
	v.Add(1);
	v.Add(3);
	v.Insert(1, 2);

	CHECK_EQ(v.Count(), 3);
	CHECK_EQ(v[0], 1);
	CHECK_EQ(v[1], 2);
	CHECK_EQ(v[2], 3);
}

WWTEST(DynamicVectorClass, GrowsPastInitialCapacity)
{
	DynamicVectorClass<int> v;
	for (int i = 0; i < 1000; ++i) {
		v.Add(i);
	}
	CHECK_EQ(v.Count(), 1000);
	CHECK_EQ(v[0], 0);
	CHECK_EQ(v[999], 999);
}

WWTEST(DynamicVectorClass, ReadsAsEmptyAfterDestruction)
{
	// REGRESSION: ~VectorClass frees the buffer and nulls Vector, but
	// ActiveCount is declared in DynamicVectorClass and nothing reset it on the
	// way out -- the virtual Clear() does, yet base-class destruction cannot
	// dispatch to a derived override. A destroyed vector was therefore
	// self-contradictory: Vector NULL, Count() still reporting the old size.
	//
	// That crashed the game on exit. NetworkObjectMgrClass::_ObjectList is a
	// static, and ~NetworkObjectClass calls Unregister_Object, so any object
	// destroyed after it during static teardown ran Find_Object's binary search
	// over a null buffer with a non-zero count.
	//
	// Placement new lets the destructor run while the storage stays readable,
	// so the post-destruction state can actually be inspected.
	alignas(DynamicVectorClass<int>) unsigned char storage[sizeof(DynamicVectorClass<int>)];

	DynamicVectorClass<int> *v = new (storage) DynamicVectorClass<int>;
	for (int i = 0; i < 10; ++i) {
		v->Add(i);
	}
	CHECK_EQ(v->Count(), 10);

	v->~DynamicVectorClass();

	// The count is what every caller checks before indexing, so this is the
	// property that has to hold: a destroyed vector looks empty, not populated.
	CHECK_EQ(v->Count(), 0);
}

//-----------------------------------------------------------------------------
//	IndexClass
//-----------------------------------------------------------------------------

WWTEST(IndexClass, AddFindRemove)
{
	// REGRESSION: IndexClass<>::Search_For_Node needed `typename` on its
	// out-of-line return type. Instantiating and exercising the lookup keeps
	// that fix honest.
	IndexClass<int, int> index;

	CHECK_EQ(index.Count(), 0);

	index.Add_Index(100, 1);
	index.Add_Index(200, 2);
	index.Add_Index(300, 3);

	CHECK_EQ(index.Count(), 3);
	CHECK(index.Is_Present(200));
	CHECK(!index.Is_Present(999));

	CHECK_EQ(index[200], 2);

	CHECK(index.Remove_Index(200));
	CHECK(!index.Is_Present(200));
	CHECK_EQ(index.Count(), 2);

	// The surviving entries must still be reachable -- this is the binary search
	// path that Search_For_Node implements.
	CHECK_EQ(index[100], 1);
	CHECK_EQ(index[300], 3);
}

WWTEST(IndexClass, ManyEntriesStaySorted)
{
	IndexClass<int, int> index;

	// Insert out of order; the index sorts lazily on first search.
	for (int i = 99; i >= 0; --i) {
		index.Add_Index(i, i * 2);
	}
	CHECK_EQ(index.Count(), 100);

	for (int i = 0; i < 100; ++i) {
		CHECK(index.Is_Present(i));
		CHECK_EQ(index[i], i * 2);
	}
}

//-----------------------------------------------------------------------------
//	CRC
//-----------------------------------------------------------------------------

WWTEST(CRC, StableAndSensitive)
{
	unsigned long a = CRC_Stringi("hello");
	unsigned long b = CRC_Stringi("hello");
	unsigned long c = CRC_Stringi("hellp");

	CHECK(a == b);			// deterministic
	CHECK(a != c);			// sensitive to a one-character change
}

//-----------------------------------------------------------------------------
//	Random
//-----------------------------------------------------------------------------

WWTEST(Random, SeedIsReproducible)
{
	Random3Class a(1234, 5678);
	Random3Class b(1234, 5678);

	// Same seed must give the same stream -- the netcode and replays depend on
	// this being true.
	for (int i = 0; i < 32; ++i) {
		CHECK_EQ((long)a(), (long)b());
	}
}
