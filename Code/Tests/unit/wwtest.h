/*
**	wwtest -- a deliberately tiny test harness for the Renegade engine.
**
**	This tree has spent a lot of effort getting down to a single external
**	dependency, so pulling in Catch2 or doctest to test it would be a poor
**	trade -- and both want a newer C++ dialect than the engine builds with
**	(/Zc:wchar_t-, /Zc:forScope-, /permissive). This is enough to be useful and
**	small enough to read in one sitting.
**
**	Usage:
**
**		WWTEST(Vector3, Length)
**		{
**			Vector3 v(3.0f, 4.0f, 0.0f);
**			CHECK_NEAR(v.Length(), 5.0f, 1.0e-6f);
**		}
**
**	Tests self-register, so adding a file to the target is all it takes. A
**	failing CHECK reports and continues, so one broken assumption does not hide
**	the rest of the test.
*/

#ifndef WWTEST_H
#define WWTEST_H

#include <stdio.h>
#include <string.h>

namespace WWTest
{

typedef void (*TestFunc)(void);

struct TestCase
{
	const char *	Suite;
	const char *	Name;
	TestFunc			Func;
	TestCase *		Next;
};

// Registration happens during static init, so the list head lives inside a
// function to dodge the static initialisation order problem.
TestCase **	Registry(void);
int			Register(TestCase *test);

void	Report_Failure(const char *file, int line, const char *expr, const char *detail);
int	Run_All(const char *suite_filter);

bool	Near(float a, float b, float epsilon);

} // namespace WWTest


#define WWTEST(suite, name)																				\
	static void suite##_##name##_Body(void);															\
	static WWTest::TestCase suite##_##name##_Case =													\
		{ #suite, #name, suite##_##name##_Body, 0 };													\
	static int suite##_##name##_Reg = WWTest::Register(&suite##_##name##_Case);				\
	static void suite##_##name##_Body(void)


#define CHECK(expr)																							\
	do {																											\
		if (!(expr)) {																							\
			WWTest::Report_Failure(__FILE__, __LINE__, #expr, 0);									\
		}																											\
	} while (0)

#define CHECK_EQ(actual, expected)																		\
	do {																											\
		if (!((actual) == (expected))) {																	\
			char _detail[256];																				\
			_snprintf(_detail, sizeof(_detail), "got %ld, expected %ld",							\
						 (long)(actual), (long)(expected));													\
			_detail[sizeof(_detail) - 1] = 0;																\
			WWTest::Report_Failure(__FILE__, __LINE__, #actual " == " #expected, _detail);	\
		}																											\
	} while (0)

#define CHECK_STR_EQ(actual, expected)																	\
	do {																											\
		const char *_a = (actual);																			\
		const char *_e = (expected);																		\
		if (_a == 0 || _e == 0 || strcmp(_a, _e) != 0) {												\
			char _detail[512];																				\
			_snprintf(_detail, sizeof(_detail), "got \"%s\", expected \"%s\"",					\
						 _a ? _a : "(null)", _e ? _e : "(null)");										\
			_detail[sizeof(_detail) - 1] = 0;																\
			WWTest::Report_Failure(__FILE__, __LINE__, #actual " == " #expected, _detail);	\
		}																											\
	} while (0)

#define CHECK_NEAR(actual, expected, epsilon)															\
	do {																											\
		float _a = (float)(actual);																			\
		float _e = (float)(expected);																		\
		if (!WWTest::Near(_a, _e, (float)(epsilon))) {													\
			char _detail[256];																				\
			_snprintf(_detail, sizeof(_detail), "got %.9g, expected %.9g (eps %.9g)",			\
						 _a, _e, (double)(epsilon));														\
			_detail[sizeof(_detail) - 1] = 0;																\
			WWTest::Report_Failure(__FILE__, __LINE__, #actual " ~= " #expected, _detail);	\
		}																											\
	} while (0)

#endif // WWTEST_H
