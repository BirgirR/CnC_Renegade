/*
**	Command & Conquer Renegade(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// regexpr.cpp

//
// Implemented over the C++ standard library's <regex>.
//
// This was written against GNU regex, which the source release does not
// include and which is GPL in its own right. The class was the only thing in
// the tree that needed it, and nothing in the tree uses the class, so it had
// been excluded from the build entirely -- which in turn kept the game
// executable out of the default target, for one file nobody calls.
//
// The behaviour it had is preserved rather than modernised:
//
//   * The GNU syntax flags it set -- NO_BK_PARENS, NO_BK_BRACES, NO_BK_VBAR,
//     INTERVALS, CHAR_CLASSES, CONTEXT_INDEP_ANCHORS and OPS -- describe POSIX
//     extended regular expressions, which is std::regex::extended.
//
//   * Match used re_match, not re_search. re_match anchors at the start of the
//     string and returns how many characters matched, so a match had to begin
//     at position zero but did not have to reach the end. match_continuous is
//     that same rule. A search would quietly accept far more.
//
//   * A zero character match is a match, which both agree on.
//

#include "always.h"
#include "regexpr.h"
#include "wwstring.h"
#include <assert.h>
#include <regex>


/*
** Definition of private DataStruct for RegularExpressionClass
*/

struct RegularExpressionClass::DataStruct
{
	DataStruct (void)
	:	IsValid(false)
	{
	}

	void ClearExpression (void)
	{
		//	std::regex owns its own memory, so there is nothing to free here the
		//	way there was with regfree.
		ExprString = "";
		IsValid = false;
	}

	// The regular expression that has been compiled.
	StringClass	ExprString;

	// Compiled form, used during matching. Only meaningful when IsValid.
	std::regex	Compiled;

	// True if Compiled is valid.
	bool			IsValid;
};


/*
** RegularExpressionClass Implementation
*/

RegularExpressionClass::RegularExpressionClass (const char *expression)
:	Data(0)
{
	// Allocate our private members.
	Data = new DataStruct;
	assert(Data);

	// Compile the expression if we were given one.
	if (expression)
		Compile(expression);
}


RegularExpressionClass::RegularExpressionClass (const RegularExpressionClass &copy)
:	Data(0)
{
	// Allocate our private members.
	Data = new DataStruct;
	assert(Data);

	// Compile the expression if the given object had one.
	if (copy.Is_Valid())
	{
		Compile(copy.Data->ExprString);
		assert(Is_Valid());
	}
}


RegularExpressionClass::~RegularExpressionClass (void)
{
	delete Data;
	Data = 0;
}


bool RegularExpressionClass::Compile (const char *expression)
{
	assert(Data);
	assert(expression);

	// Clear any existing expression data. This makes it safe to
	// call Compile() twice on one object.
	Data->ClearExpression();

	if (expression == 0)
		return false;

	//	A malformed expression is reported by throwing, where GNU regex returned
	//	an error string. Either way the answer to the caller is false, and the
	//	object is left invalid rather than half compiled.
	try
	{
		Data->Compiled.assign(expression, std::regex::extended);
	}
	catch (const std::regex_error &)
	{
		return false;
	}

	Data->IsValid = true;
	Data->ExprString = expression;
	return true;
}


bool RegularExpressionClass::Is_Valid (void) const
{
	assert(Data);
	return Data->IsValid;
}


bool RegularExpressionClass::Match (const char *string) const
{
	assert(Data);

	// If we have no valid compiled expression, we can't match Jack.
	if (!Data->IsValid || string == 0)
		return false;

	//	match_continuous is re_match's rule: the match has to start at the
	//	beginning of the string, and may end anywhere at or after it.
	return std::regex_search(string, Data->Compiled,
									 std::regex_constants::match_continuous);
}


/*
** Operators
*/

RegularExpressionClass & RegularExpressionClass::operator = (const RegularExpressionClass &rhs)
{
	// Check for assignment to self.
	if (*this == rhs)
		return *this;

	// Assign that object to this one.
	assert(rhs.Data);
	Compile(rhs.Data->ExprString);
	assert(Is_Valid());

	// Return this object.
	return *this;
}


bool RegularExpressionClass::operator == (const RegularExpressionClass &rhs) const
{
	// Two RegularExpressionClass objects are equivalent if they both
	// have the same validity state, and if that state is 'true' both
	// of their expressions are the same.

	// Check validity states for equality.
	if (Is_Valid() != rhs.Is_Valid())
		return false;

	// If they're valid, check their expressions.
	if (Is_Valid())
	{
		// The objects are not equivalent if their expression strings
		// don't match.
		if (Data->ExprString != rhs.Data->ExprString)
			return false;
	}
	return true;
}


//	Not inline, unlike the original: the header declares it, so a definition
//	marked inline in this file gives it no external linkage and the first
//	caller anywhere else fails to link.
bool RegularExpressionClass::operator != (const RegularExpressionClass &rhs) const
{
	return !(*this == rhs);
}
