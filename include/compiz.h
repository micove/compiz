/*
 * Copyright © 2007 Novell, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Novell, Inc. not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior permission.
 * Novell, Inc. makes no representations about the suitability of this
 * software for any purpose. It is provided "as is" without express or
 * implied warranty.
 *
 * NOVELL, INC. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL NOVELL, INC. BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: David Reveman <davidr@novell.com>
 */

#ifndef _COMPIZ_H
#define _COMPIZ_H

#include <compiz-common.h>

#include <string>
#include <list>
#include <cstdarg>

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY (x)

#define RESTRICT_VALUE(value, min, max)				     \
    (((value) < (min)) ? (min): ((value) > (max)) ? (max) : (value))

#define MOD(a,b) ((a) < 0 ? ((b) - ((-(a) - 1) % (b))) - 1 : (a) % (b))

#define TIMEVALDIFF(tv1, tv2)						   \
    ((tv1)->tv_sec == (tv2)->tv_sec || (tv1)->tv_usec >= (tv2)->tv_usec) ? \
    ((((tv1)->tv_sec - (tv2)->tv_sec) * 1000000) +			   \
     ((tv1)->tv_usec - (tv2)->tv_usec)) / 1000 :			   \
    ((((tv1)->tv_sec - 1 - (tv2)->tv_sec) * 1000000) +			   \
     (1000000 + (tv1)->tv_usec - (tv2)->tv_usec)) / 1000

#define TIMESPECDIFF(ts1, ts2)						   \
    ((ts1)->tv_sec == (ts2)->tv_sec || (ts1)->tv_nsec >= (ts2)->tv_nsec) ? \
    ((((ts1)->tv_sec - (ts2)->tv_sec) * 1000000) +			   \
     ((ts1)->tv_nsec - (ts2)->tv_nsec)) / 1000000 :			   \
    ((((ts1)->tv_sec - 1 - (ts2)->tv_sec) * 1000000) +			   \
     (1000000 + (ts1)->tv_nsec - (ts2)->tv_nsec)) / 1000000

#define MULTIPLY_USHORT(us1, us2)		 \
    (((GLuint) (us1) * (GLuint) (us2)) / 0xffff)

#define DEG2RAD (M_PI / 180.0f)

#if defined(HAVE_SCANDIR_POSIX)
  // POSIX (2008) defines the comparison function like this:
  #define scandir(a,b,c,d) scandir((a), (b), (c), (int(*)(const dirent **, const dirent **))(d));
#else
  #define scandir(a,b,c,d) scandir((a), (b), (c), (int(*)(const void*,const void*))(d));
#endif

typedef std::string CompString;
typedef std::list<CompString> CompStringList;

CompString compPrintf (const char *format, ...);
CompString compPrintf (const char *format, va_list ap);



typedef enum {
    CompLogLevelFatal = 0,
    CompLogLevelError,
    CompLogLevelWarn,
    CompLogLevelInfo,
    CompLogLevelDebug
} CompLogLevel;

void
compLogMessage (const char   *componentName,
		CompLogLevel level,
		const char   *format,
		...);

const char *
logLevelToString (CompLogLevel level);

extern char       *programName;
extern char       **programArgv;
extern int        programArgc;

#endif
