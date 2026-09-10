/*
**	wwtest harness implementation and entry point.
**
**	Run with no arguments to run everything, or pass a suite name to run just
**	that suite:  wwtest.exe Vector3
*/

#include "wwtest.h"

#include <stdlib.h>
#include <math.h>

namespace WWTest
{

static int CurrentFailures = 0;

TestCase **Registry(void)
{
	static TestCase *head = 0;
	return &head;
}

int Register(TestCase *test)
{
	TestCase **head = Registry();

	// Append rather than push, so tests report in the order they were written.
	TestCase **tail = head;
	while (*tail != 0) {
		tail = &((*tail)->Next);
	}
	*tail = test;
	test->Next = 0;
	return 0;
}

bool Near(float a, float b, float epsilon)
{
	float diff = a - b;
	if (diff < 0.0f) diff = -diff;
	if (diff <= epsilon) return true;

	// Fall back to a relative comparison so large magnitudes are not held to an
	// absolute tolerance they can never meet.
	float mag_a = (a < 0.0f) ? -a : a;
	float mag_b = (b < 0.0f) ? -b : b;
	float larger = (mag_a > mag_b) ? mag_a : mag_b;
	return diff <= epsilon * larger;
}

void Report_Failure(const char *file, int line, const char *expr, const char *detail)
{
	++CurrentFailures;

	// Emit in a form Visual Studio and most editors can jump to.
	if (detail != 0 && detail[0] != 0) {
		printf("%s(%d): FAIL  %s   [%s]\n", file, line, expr, detail);
	} else {
		printf("%s(%d): FAIL  %s\n", file, line, expr);
	}
}

int Run_All(const char *suite_filter)
{
	int total = 0;
	int failed_tests = 0;
	int total_failures = 0;

	for (TestCase *test = *Registry(); test != 0; test = test->Next) {
		if (suite_filter != 0 && strcmp(suite_filter, test->Suite) != 0) {
			continue;
		}

		++total;
		CurrentFailures = 0;
		test->Func();

		if (CurrentFailures != 0) {
			++failed_tests;
			total_failures += CurrentFailures;
			printf("  [FAIL] %s.%s (%d check%s failed)\n",
					 test->Suite, test->Name,
					 CurrentFailures, CurrentFailures == 1 ? "" : "s");
		}
	}

	printf("\n");
	if (failed_tests == 0) {
		printf("PASSED  %d test%s\n", total, total == 1 ? "" : "s");
	} else {
		printf("FAILED  %d of %d test%s (%d check%s)\n",
				 failed_tests, total, total == 1 ? "" : "s",
				 total_failures, total_failures == 1 ? "" : "s");
	}

	return (failed_tests == 0) ? 0 : 1;
}

} // namespace WWTest


int main(int argc, char *argv[])
{
	const char *filter = (argc > 1) ? argv[1] : 0;

	if (filter != 0) {
		printf("Running suite '%s'\n\n", filter);
	}

	return WWTest::Run_All(filter);
}
