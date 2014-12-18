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
#include <deque>
#include <core/size.h>
#include <core/region.h>
#include "backbuffertracking.h"
#include <cstdio>

namespace bt = compiz::composite::buffertracking;

class bt::FrameRoster::Private
{
    public:

	Private (const CompSize                                 &size,
		 bt::AgeingDamageBufferObserver                 &observer,
		 const bt::FrameRoster::AreaShouldBeMarkedDirty &shouldBeMarkedDirty) :
	    screenSize (size),
	    observer (observer),
	    shouldBeMarkedDirty (shouldBeMarkedDirty),
	    oldFrames (1)
	{
	}

	CompSize                                 screenSize;
	bt::AgeingDamageBufferObserver           &observer;
	bt::FrameRoster::AreaShouldBeMarkedDirty shouldBeMarkedDirty;
	std::deque <CompRegion>                  oldFrames;
};

bt::FrameRoster::FrameRoster (const CompSize                 &size,
			      bt::AgeingDamageBufferObserver &tracker,
			      const AreaShouldBeMarkedDirty  &shouldBeMarkedDirty) :
    priv (new bt::FrameRoster::Private (size,
					tracker,
					shouldBeMarkedDirty))
{
    priv->observer.observe (*this);
}

bt::FrameRoster::~FrameRoster ()
{
    priv->observer.unobserve (*this);
}

CompRegion
bt::FrameRoster::damageForFrameAge (unsigned int age)
{
    if (!age)
	return CompRegion (0, 0,
			   priv->screenSize.width (),
			   priv->screenSize.height ());

    if (age >= priv->oldFrames.size ())
	return CompRegion (0, 0,
			   priv->screenSize.width (),
			   priv->screenSize.height ());

    CompRegion accumulatedDamage;

    while (age--)
    {
	unsigned int frameNum = (priv->oldFrames.size () - age) - 1;
	accumulatedDamage += priv->oldFrames[frameNum];
    }

    return accumulatedDamage;
}

void
bt::FrameRoster::dirtyAreaOnCurrentFrame (const CompRegion &r)
{
    if (priv->shouldBeMarkedDirty (r))
	(*priv->oldFrames.rbegin ()) += r;
}

void
bt::FrameRoster::subtractObscuredArea (const CompRegion &r)
{
    (*priv->oldFrames.rbegin ()) -= r;
}

void
bt::FrameRoster::overdrawRegionOnPaintingFrame (const CompRegion &r)
{
    assert (priv->oldFrames.size () > 1);
    std::deque <CompRegion>::reverse_iterator it = priv->oldFrames.rbegin ();
    ++it;
    (*it) += r;
}

void
bt::FrameRoster::incrementFrameAges ()
{
    priv->oldFrames.push_back (CompRegion ());

    /* Get rid of old frames */
    if (priv->oldFrames.size () > NUM_TRACKED_FRAMES)
	priv->oldFrames.pop_front ();
}

const CompRegion &
bt::FrameRoster::currentFrameDamage ()
{
    return *priv->oldFrames.rbegin ();
}

class bt::AgeingDamageBuffers::Private
{
    public:

	std::vector <bt::DamageAgeTracking *> damageAgeTrackers;
};

bt::AgeingDamageBuffers::AgeingDamageBuffers () :
    priv (new bt::AgeingDamageBuffers::Private ())
{
}

void
bt::AgeingDamageBuffers::observe (bt::DamageAgeTracking &damageAgeTracker)
{
    priv->damageAgeTrackers.push_back (&damageAgeTracker);
}

void
bt::AgeingDamageBuffers::unobserve (bt::DamageAgeTracking &damageAgeTracker)
{
    std::vector <bt::DamageAgeTracking *>::iterator it =
	std::find (priv->damageAgeTrackers.begin (),
		   priv->damageAgeTrackers.end (),
		   &damageAgeTracker);

    if (it != priv->damageAgeTrackers.end ())
	priv->damageAgeTrackers.erase (it);
}

void
bt::AgeingDamageBuffers::incrementAges ()
{
    for (std::vector <bt::DamageAgeTracking *>::iterator it =
	 priv->damageAgeTrackers.begin ();
	 it != priv->damageAgeTrackers.end ();
	 ++it)
    {
	bt::DamageAgeTracking *tracker = *it;

	tracker->incrementFrameAges ();
    }
}

void
bt::AgeingDamageBuffers::markAreaDirty (const CompRegion &reg)
{
    for (std::vector <bt::DamageAgeTracking *>::iterator it =
	 priv->damageAgeTrackers.begin ();
	 it != priv->damageAgeTrackers.end ();
	 ++it)
    {
	bt::DamageAgeTracking *tracker = *it;

	tracker->dirtyAreaOnCurrentFrame (reg);
    }
}

void
bt::AgeingDamageBuffers::subtractObscuredArea (const CompRegion &reg)
{
    for (std::vector <bt::DamageAgeTracking *>::iterator it =
	 priv->damageAgeTrackers.begin ();
	 it != priv->damageAgeTrackers.end ();
	 ++it)
    {
	bt::DamageAgeTracking *tracker = *it;

	tracker->subtractObscuredArea (reg);
    }
}

void
bt::AgeingDamageBuffers::markAreaDirtyOnLastFrame (const CompRegion &reg)
{
    for (std::vector <bt::DamageAgeTracking *>::iterator it =
	 priv->damageAgeTrackers.begin ();
	 it != priv->damageAgeTrackers.end ();
	 ++it)
    {
	bt::DamageAgeTracking *tracker = *it;

	tracker->overdrawRegionOnPaintingFrame (reg);
    }
}
