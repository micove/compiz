/*
 * Compiz, composite plugin, GLX_EXT_buffer_age logic
 *
 * Copyright (c) 2012 Sam Spilsbury
 * Authors: Sam Spilsbury <smspillaz@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#ifndef _COMPIZ_COMPOSITE_AGEDAMAGEQUERY_H
#define _COMPIZ_COMPOSITE_AGEDAMAGEQUERY_H

#include <boost/shared_ptr.hpp>
#include <core/region.h>

namespace compiz
{
namespace composite
{
namespace buffertracking
{
class AgeDamageQuery
{
    public:

	typedef boost::shared_ptr <AgeDamageQuery> Ptr;
	typedef boost::function <bool (const CompRegion &)> AreaShouldBeMarkedDirty;

	virtual ~AgeDamageQuery () {}
	virtual CompRegion damageForFrameAge (unsigned int) = 0;
	virtual const CompRegion & currentFrameDamage () = 0;
};
}
}
}

#endif

