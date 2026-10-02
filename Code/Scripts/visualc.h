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

/*********************************************************************************************** 
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               *** 
 *********************************************************************************************** 
 *                                                                                             * 
 *                 Project Name : Command & Conquer                                            * 
 *                                                                                             * 
 *                     $Archive:: /Commando/Code/wwlib/visualc.h                              $* 
 *                                                                                             * 
 *                      $Author:: Greg_h                                                      $*
 *                                                                                             * 
 *                     $Modtime:: 5/16/01 4:11p                                               $*
 *                                                                                             * 
 *                    $Revision:: 9                                                           $*
 *                                                                                             *
 *---------------------------------------------------------------------------------------------* 
 * Functions:                                                                                  * 
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#if defined(_MSC_VER)

/**********************************************************************
**	The "bool" integral type was defined by the C++ comittee in
**	November of '94. Until the compiler supports this, use the following
**	definition.
*/
#include	"bool.h"


/*
** Make the inline depth 255
*/
#pragma inline_depth(255)


/*
**	Turn off some unneeded warnings.
**
**	Each was turned back on (and promoted to /W3) in a scratch build of the
**	whole tree; the count and verdict are beside it. Most are off by default
**	in a current MSVC or only fire at /W4, so these pragmas mainly keep a /W4
**	build (RENEGADE_ENGINE_WARNINGS) readable. C4056, C4511, C4512, C4097 and
**	C4786 were dropped: none fires any more, and the last two no longer exist.
*/

// "unreferenced inline function has been removed" Yea, so what?
// 4,031 sites, all optimizer bookkeeping.
#pragma warning(disable : 4514)

// "conversion from 'X' to 'Y', possible loss of data". Not only double to float:
// it covers every float/int narrowing. Enabled, it reports ~1,200 sites, ~800
// of them harmless int/double to float. The float to int and integer-narrowing
// sites were audited once and the real bugs fixed; re-enable it locally
// to check new code rather than trusting this.
#pragma warning(disable : 4244)

// "function not inlined" This warning is typically useless. The inline keyword
// only serves as a suggestion to the compiler and it may or may not inline a
// function on a case by case basis. No need to be told of this.
// 4,485 sites.
#pragma warning(disable : 4710)

// "'this' used in base member initializer list" Using "this" in a base member
// initializer is valid -- no need for this warning.
// 23 sites, all audited: every receiver (the boss state machines, ActionClass,
// ReferenceableClass, AABTreeIterator and the anim and shadow managers) only
// stores the pointer, and none calls back into the half-built object. Re-check
// any new one: that is the case this warning exists for.
#pragma warning(disable : 4355)

// Unreferenced local function removed.
// 2 sites: _euler_axis_j and _euler_axis_h in euler.cpp, unused members of a set
// of four. Dead, but harmless.
#pragma warning(disable : 4505)

// 'function selected for automatic inline expansion'
// 7,052 sites, all optimizer bookkeeping.
#pragma warning(disable : 4711)

// 'unreferenced formal parameter'
// 3,497 sites. Style, not correctness.
#pragma warning(disable : 4100)



#define M_E         2.71828182845904523536
#define M_LOG2E     1.44269504088896340736
#define M_LOG10E    0.434294481903251827651
#define M_LN2       0.693147180559945309417
#define M_LN10      2.30258509299404568402
#define M_PI        3.14159265358979323846
#define M_PI_2      1.57079632679489661923
#define M_PI_4      0.785398163397448309616
#define M_1_PI      0.318309886183790671538
#define M_2_PI      0.636619772367581343076
#define M_1_SQRTPI  0.564189583547756286948
#define M_2_SQRTPI  1.12837916709551257390
#define M_SQRT2     1.41421356237309504880
#define M_SQRT_2    0.707106781186547524401


#endif

