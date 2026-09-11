# Translation units excluded from the build when RENEGADE_EXCLUDE_UNPORTED is ON
# (the default). Configure with -DRENEGADE_EXCLUDE_UNPORTED=OFF to put them back
# and see the real diagnostics.
#
# The list is empty: every translation unit in the tree now compiles under
# modern MSVC. The last entry was Code/wwlib/regexpr.cpp, which needed the GNU
# regex library that the source release does not include; it is now implemented
# over the standard library's <regex> instead.
#
# The machinery is kept rather than deleted. It is the natural place to park a
# file that stops compiling during a larger change, and RENEGADE_EXCLUDE_UNPORTED
# still reports what it skipped, so nothing goes missing quietly.

set(RENEGADE_UNPORTED_SOURCES
)
