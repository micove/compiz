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
#ifndef _COMPIZ_COMPOSITE_BACKBUFFERTRACKING_H
#define _COMPIZ_COMPOSITE_BACKBUFFERTRACKING_H

#include <memory>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/function.hpp>

#include <composite/agedamagequery.h>

class CompSize;
class CompRegion;

namespace compiz
{
namespace composite
{
namespace buffertracking
{
class DamageAgeTracking
{
    public:

	virtual ~DamageAgeTracking () {};
	virtual void dirtyAreaOnCurrentFrame (const CompRegion &) = 0;
	virtual void overdrawRegionOnPaintingFrame (const CompRegion &) = 0;
	virtual void subtractObscuredArea (const CompRegion &) = 0;
	virtual void incrementFrameAges () = 0;
};

class AgeingDamageBufferObserver
{
    public:

	virtual ~AgeingDamageBufferObserver () {};
	virtual void observe (DamageAgeTracking &damageAgeTracker) = 0;
	virtual void unobserve (DamageAgeTracking &damageAgeTracker) = 0;
};

class AgeingDamageBuffers :
    public AgeingDamageBufferObserver,
    boost::noncopyable
{
    public:

	AgeingDamageBuffers ();

	void observe (DamageAgeTracking &damageAgeTracker);
	void unobserve (DamageAgeTracking &damageAgeTracker);
	void incrementAges ();
	void markAreaDirty (const CompRegion &reg);
	void markAreaDirtyOnLastFrame (const CompRegion &reg);
	void subtractObscuredArea (const CompRegion &reg);

    private:

	class Private;
	std::auto_ptr <Private> priv;
};

class FrameRoster :
    public DamageAgeTracking,
    public AgeDamageQuery,
    boost::noncopyable
{
    public:

	typedef AgeDamageQuery::AreaShouldBeMarkedDirty AreaShouldBeMarkedDirty;
	typedef boost::shared_ptr <FrameRoster> Ptr;

	FrameRoster (const CompSize                   &size,
		     AgeingDamageBufferObserver       &tracker,
		     const AreaShouldBeMarkedDirty    &shouldMarkDirty);

	~FrameRoster ();

	void dirtyAreaOnCurrentFrame (const CompRegion &);
	void overdrawRegionOnPaintingFrame (const CompRegion &);
	void subtractObscuredArea (const CompRegion &);
	void incrementFrameAges ();
	CompRegion damageForFrameAge (unsigned int);
	const CompRegion & currentFrameDamage ();

	class Private;
	std::auto_ptr <Private> priv;

	static const unsigned int NUM_TRACKED_FRAMES = 10;

	static
	FrameRoster::Ptr create (const CompSize                &size,

				 const AreaShouldBeMarkedDirty &shouldMarkDirty);

    private:

};
} // namespace buffertracking
} // namespace composite
} // namespace compiz
#endif
