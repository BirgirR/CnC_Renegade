# Translation units excluded from the build when RENEGADE_EXCLUDE_UNPORTED is ON
# (the default). Configure with -DRENEGADE_EXCLUDE_UNPORTED=OFF to put them back
# and see the real diagnostics.
#
# Everything else in the tree now compiles under modern MSVC. What remains here
# is blocked by a genuinely missing third-party library rather than by anything
# in the code.

set(RENEGADE_UNPORTED_SOURCES

    # GNU regex is GPL and simply absent from the source release; there is no
    # POSIX regex in the MSVC runtime to substitute. Nothing in the tree
    # references RegularExpressionClass -- regexpr.h/.cpp are self-contained and
    # unused -- so excluding this costs the build nothing.
    #
    # To restore it: drop a gnu_regex.h/.c into Code/wwlib, or reimplement
    # RegularExpressionClass over C++11 <regex>.
    Code/wwlib/regexpr.cpp
)
