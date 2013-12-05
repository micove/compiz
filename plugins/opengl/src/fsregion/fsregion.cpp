/*
 * Compiz opengl plugin, FullscreenRegion class
 *
 * Copyright (c) 2012 Canonical Ltd.
 * Author: Daniel van Vugt <daniel.van.vugt@canonical.com>
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

#include "fsregion.h"

namespace compiz {
namespace opengl {

FullscreenRegion::FullscreenRegion (const CompRect &output) :
    untouched (output),
    orig (output),
    allOutputs (output)
{
}

FullscreenRegion::FullscreenRegion (const CompRect &output,
                                    const CompRegion &all) :
    untouched (output),
    orig (output),
    allOutputs (all)
{
}

bool
FullscreenRegion::isCoveredBy (const CompRegion &region, WinFlags flags)
{
    bool fullscreen = false;

    if (!(flags & (Desktop | Alpha | NoOcclusionDetection)) &&
        region == untouched &&
        region == orig)
    {
	fullscreen = true;
    }

    untouched -= region;

    return fullscreen;
}

bool
FullscreenRegion::allowRedirection (const CompRegion &region)
{
    /* Don't allow existing unredirected windows that cover this
     * region to be redirected again as they were probably unredirected
     * on another monitor
     * Also be careful to not allow unredirection of offscreen windows
     * (outside of allOutputs).
     */
    return region.intersects (orig) || !region.intersects (allOutputs);
}

} // namespace opengl
} // namespace compiz
