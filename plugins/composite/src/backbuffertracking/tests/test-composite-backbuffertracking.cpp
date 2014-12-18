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
#include <boost/bind.hpp>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <core/region.h>
#include <core/size.h>

#include "backbuffertracking.h"

using ::testing::NiceMock;
using ::testing::_;
using ::testing::AtLeast;

namespace bt = compiz::composite::buffertracking;

namespace
{
class MockAgeingDamageBufferObserver :
    public bt::AgeingDamageBufferObserver
{
    public:

	MOCK_METHOD1 (observe, void (bt::DamageAgeTracking &));
	MOCK_METHOD1 (unobserve, void (bt::DamageAgeTracking &));
};

bool alwaysDirty ()
{
    return true;
}


class BackbufferTracking :
    public ::testing::Test
{
    public:

	BackbufferTracking () :
	    screen (1000, 1000),
	    roster ()
	{
	}

	virtual void SetUp ()
	{
	    SetupRoster ();
	}

	virtual void SetupRoster ()
	{
	    roster.reset (new bt::FrameRoster (screen,
					       niceTracker,
					       boost::bind (alwaysDirty)));
	}

    protected:

	CompSize                           screen;
	NiceMock <MockAgeingDamageBufferObserver> niceTracker;
	bt::FrameRoster::Ptr               roster;
};

class BackbufferTrackingCallbacks :
    public BackbufferTracking
{
    public:

	BackbufferTrackingCallbacks () :
	    shouldDamage (false)
	{
	}

    protected:

	void allowDamage (bool allow)
	{
	    shouldDamage = allow;
	}

    private:

	virtual void SetupRoster ()
	{
	    roster.reset (new bt::FrameRoster (screen,
					       niceTracker,
					       boost::bind (&BackbufferTrackingCallbacks::shouldDamageCallback,
							    this, _1)));
	}

	bool shouldDamageCallback (const CompRegion &)
	{
	    return shouldDamage;
	}

	bool shouldDamage;
};
}

std::ostream &
operator<< (std::ostream &os, const CompRegion &reg)
{
    os << "Region with Bounding Rectangle : " <<
	  reg.boundingRect ().x () << " " <<
	  reg.boundingRect ().y () << " " <<
	  reg.boundingRect ().width () << " " <<
	  reg.boundingRect ().height () << std::endl;
    CompRect::vector rects (reg.rects ());
    for (CompRect::vector::iterator it = rects.begin ();
	 it != rects.end ();
	 it++)
	os << " - subrectangle: " <<
	      (*it).x () << " " <<
	      (*it).y () << " " <<
	      (*it).width () << " " <<
	      (*it).height () << " " << std::endl;

    return os;
}

TEST (BackbufferTrackingConstruction, CreateAddsToObserverList)
{
    MockAgeingDamageBufferObserver mockAgeingDamageBufferObserver;

    /* We can't verify the argument here,
     * but we can verify the function call */
    EXPECT_CALL (mockAgeingDamageBufferObserver, observe (_));
    EXPECT_CALL (mockAgeingDamageBufferObserver, unobserve (_)).Times (AtLeast (0));
    bt::FrameRoster roster (CompSize (),
			    mockAgeingDamageBufferObserver,
			    boost::bind (alwaysDirty));
}

TEST (BackbufferTrackingConstruction, DestroyRemovesFromObserverList)
{
    MockAgeingDamageBufferObserver mockAgeingDamageBufferObserver;

    /* We can't verify the argument here,
     * but we can verify the function call */
    EXPECT_CALL (mockAgeingDamageBufferObserver, observe (_)).Times (AtLeast (0));
    EXPECT_CALL (mockAgeingDamageBufferObserver, unobserve (_));
    bt::FrameRoster roster (CompSize (),
			    mockAgeingDamageBufferObserver,
			    boost::bind (alwaysDirty));
}

TEST_F (BackbufferTrackingCallbacks, TrackIntoCurrentIfCallbackTrue)
{
    allowDamage (true);
    CompRegion damage (100, 100, 100, 100);
    roster->dirtyAreaOnCurrentFrame (damage);
    EXPECT_EQ (damage, roster->currentFrameDamage ());
}

TEST_F (BackbufferTrackingCallbacks, NoTrackIntoCurrentIfCallbackFalse)
{
    allowDamage (false);
    CompRegion damage (100, 100, 100, 100);
    roster->dirtyAreaOnCurrentFrame (damage);
    EXPECT_EQ (emptyRegion, roster->currentFrameDamage ());
}

TEST_F (BackbufferTracking, DirtyAreaSubtraction)
{
    CompRegion dirty (100, 100, 100, 100);
    CompRegion obscured (150, 150, 50, 50);
    roster->dirtyAreaOnCurrentFrame (dirty);
    roster->subtractObscuredArea (obscured);
    EXPECT_EQ (dirty - obscured, roster->currentFrameDamage ());
}

TEST_F (BackbufferTracking, DirtyAreaForAgeZeroAll)
{
    EXPECT_EQ (CompRegion (0, 0, screen.width (), screen.height ()),
	       roster->damageForFrameAge (0));
}

TEST_F (BackbufferTracking, DirtyAreaAllForOlderThanTrackedAge)
{
    EXPECT_EQ (CompRegion (0, 0, screen.width (), screen.height ()),
	       roster->damageForFrameAge (1));
}

TEST_F (BackbufferTracking, NoDirtyAreaForLastFrame)
{
    CompRegion all (0, 0, screen.width (), screen.height ());
    roster->dirtyAreaOnCurrentFrame (all);
    roster->incrementFrameAges ();
    EXPECT_EQ (emptyRegion, roster->damageForFrameAge (1));
}

TEST_F (BackbufferTracking, DirtyAreaIfMoreSinceLastFrame)
{
    CompRegion all (0, 0, screen.width (), screen.height ());
    roster->dirtyAreaOnCurrentFrame (all);
    roster->incrementFrameAges ();
    roster->dirtyAreaOnCurrentFrame(all);
    EXPECT_EQ (all, roster->damageForFrameAge (1));
}

TEST_F (BackbufferTracking, AddOverdrawRegionForLastFrame)
{
    CompRegion all (0, 0, screen.width (), screen.height ());
    roster->dirtyAreaOnCurrentFrame (all);
    roster->incrementFrameAges ();
    roster->incrementFrameAges ();
    roster->overdrawRegionOnPaintingFrame (all);
    EXPECT_EQ (all, roster->damageForFrameAge (2));
}

TEST_F (BackbufferTracking, TwoFramesAgo)
{
    CompRegion all (0, 0, screen.width (), screen.height ());
    CompRegion topleft (0, 0, screen.width () / 2, screen.height () / 2);

    roster->dirtyAreaOnCurrentFrame (all);
    roster->incrementFrameAges ();
    roster->dirtyAreaOnCurrentFrame (topleft);
    roster->incrementFrameAges ();
    EXPECT_EQ (topleft, roster->damageForFrameAge (2));
}

TEST_F (BackbufferTracking, TwoFramesAgoCulmulative)
{
    CompRegion all (0, 0, screen.width (), screen.height ());
    CompRegion topleft (0, 0, screen.width () / 2, screen.height () / 2);
    CompRegion topright (0, screen.width () / 2,
			 screen.width () / 2,
			 screen.height () / 2);

    roster->dirtyAreaOnCurrentFrame (all);
    roster->incrementFrameAges ();
    roster->dirtyAreaOnCurrentFrame (topleft);
    roster->dirtyAreaOnCurrentFrame (topright);
    roster->incrementFrameAges ();
    EXPECT_EQ (topleft + topright, roster->damageForFrameAge (2));
}

TEST_F (BackbufferTracking, ThreeFramesAgo)
{
    CompRegion all (0, 0, screen.width (), screen.height ());
    CompRegion topleft (0, 0, screen.width () / 2, screen.height () / 2);
    CompRegion bottomright (screen.width () / 2,
			    screen.height () / 2,
			    screen.width () / 2,
			    screen.height () / 2);

    roster->dirtyAreaOnCurrentFrame (all);
    roster->incrementFrameAges ();
    roster->dirtyAreaOnCurrentFrame (topleft);
    roster->incrementFrameAges ();
    roster->dirtyAreaOnCurrentFrame (bottomright);
    roster->incrementFrameAges ();

    EXPECT_EQ (topleft + bottomright, roster->damageForFrameAge (3));
}

/* These are more or less functional tests from this point forward
 * just checking a number of different situations */

TEST_F (BackbufferTracking, ThreeFramesAgoWithFourFrames)
{
    CompRegion all (0, 0, screen.width (), screen.height ());
    CompRegion topleft (0, 0, screen.width () / 2, screen.height () / 2);
    CompRegion bottomright (screen.width () / 2,
			    screen.height () / 2,
			    screen.width () / 2,
			    screen.height () / 2);
    CompRegion topright (screen.width () / 2,
			 0,
			 screen.width () / 2,
			 screen.height () / 2);

    roster->dirtyAreaOnCurrentFrame (all);
    roster->incrementFrameAges ();
    roster->dirtyAreaOnCurrentFrame (topleft);
    roster->incrementFrameAges ();
    roster->dirtyAreaOnCurrentFrame (bottomright);
    roster->incrementFrameAges ();
    roster->dirtyAreaOnCurrentFrame (topright);
    roster->incrementFrameAges ();

    EXPECT_EQ (topright + bottomright, roster->damageForFrameAge (3));
}

TEST_F (BackbufferTracking, ThreeFramesAgoWithFourFramesAndOverlap)
{
    CompRegion all (0, 0, screen.width (), screen.height ());
    CompRegion topleft (0, 0, screen.width () / 2, screen.height () / 2);
    CompRegion bottomright (screen.width () / 2,
			    screen.height () / 2,
			    screen.width () / 2,
			    screen.height () / 2);
    CompRegion topright (screen.width () / 2,
			 0,
			 screen.width () / 2,
			 screen.height () / 2);

    roster->dirtyAreaOnCurrentFrame (all);
    roster->incrementFrameAges ();
    roster->dirtyAreaOnCurrentFrame (topleft);
    roster->incrementFrameAges ();
    roster->dirtyAreaOnCurrentFrame (bottomright);
    roster->incrementFrameAges ();
    roster->dirtyAreaOnCurrentFrame (bottomright);
    roster->incrementFrameAges ();

    EXPECT_EQ (bottomright, roster->damageForFrameAge (3));
}

TEST_F (BackbufferTracking, AllDamageForExceedingMaxTrackedFrames)
{
    CompRegion all (0, 0, screen.width (), screen.height ());
    CompRegion damage (0, 0, 1, 1);

    roster->dirtyAreaOnCurrentFrame (all);

    for (unsigned int i = 0; i < bt::FrameRoster::NUM_TRACKED_FRAMES - 1; ++i)
    {
	roster->incrementFrameAges ();
	roster->dirtyAreaOnCurrentFrame (damage);
    }

    EXPECT_EQ (all, roster->damageForFrameAge (bt::FrameRoster::NUM_TRACKED_FRAMES + 1));
}

TEST_F (BackbufferTracking, DamageForMaxTrackedFrame)
{
    CompRegion all (0, 0, screen.width (), screen.height ());
    CompRegion damage (0, 0, 1, 1);

    roster->dirtyAreaOnCurrentFrame (all);

    for (unsigned int i = 0; i < bt::FrameRoster::NUM_TRACKED_FRAMES + 1; ++i)
    {
	roster->incrementFrameAges ();
	roster->dirtyAreaOnCurrentFrame (damage);
    }

    EXPECT_EQ (all, roster->damageForFrameAge (bt::FrameRoster::NUM_TRACKED_FRAMES));
}

class MockDamageAgeTracking :
    public bt::DamageAgeTracking
{
    public:

	typedef boost::shared_ptr <MockDamageAgeTracking> Ptr;

	MOCK_METHOD0 (incrementFrameAges, void ());
	MOCK_METHOD1 (dirtyAreaOnCurrentFrame, void (const CompRegion &));
	MOCK_METHOD1 (subtractObscuredArea, void (const CompRegion &));
	MOCK_METHOD1 (overdrawRegionOnPaintingFrame, void (const CompRegion &));
};

class AgeingDamageBuffers :
    public ::testing::Test
{
    public:

	AgeingDamageBuffers ()
	{
	    ageing.observe (mockDamageAgeTracker);
	}

	MockDamageAgeTracking   mockDamageAgeTracker;
	bt::AgeingDamageBuffers ageing;
};

TEST_F (AgeingDamageBuffers, IncrementAgesOnValidRosters)
{
    EXPECT_CALL (mockDamageAgeTracker, incrementFrameAges ());
    ageing.incrementAges ();
}

TEST_F (AgeingDamageBuffers, DirtyAreaOnValidRosters)
{
    CompRegion dirtyArea (100, 100, 100, 100);
    EXPECT_CALL (mockDamageAgeTracker, dirtyAreaOnCurrentFrame (dirtyArea));
    ageing.markAreaDirty (dirtyArea);
}

TEST_F (AgeingDamageBuffers, SubtractObscuredAreaOnValidRosters)
{
    CompRegion obscuredArea (100, 100, 100, 100);
    EXPECT_CALL (mockDamageAgeTracker, subtractObscuredArea (obscuredArea));
    ageing.subtractObscuredArea (obscuredArea);
}

TEST_F (AgeingDamageBuffers, AddOverdrawAreaOnValidRosters)
{
    CompRegion overdrawArea (100, 100, 100, 100);
    EXPECT_CALL (mockDamageAgeTracker, overdrawRegionOnPaintingFrame (overdrawArea));
    ageing.markAreaDirtyOnLastFrame (overdrawArea);
}

TEST_F (AgeingDamageBuffers, IncrementAgesOnInvalidRosters)
{
    EXPECT_CALL (mockDamageAgeTracker, incrementFrameAges ()).Times (0);
    ageing.unobserve (mockDamageAgeTracker);
    ageing.incrementAges ();
}

TEST_F (AgeingDamageBuffers, DirtyAreaOnInvalidRosters)
{
    EXPECT_CALL (mockDamageAgeTracker, dirtyAreaOnCurrentFrame (_)).Times (0);
    ageing.unobserve (mockDamageAgeTracker);
    ageing.markAreaDirty (emptyRegion);
}

TEST_F (AgeingDamageBuffers, SubtractObscuredAreaOnInvalidRosters)
{
    EXPECT_CALL (mockDamageAgeTracker, subtractObscuredArea (_)).Times (0);
    ageing.unobserve (mockDamageAgeTracker);
    ageing.subtractObscuredArea (emptyRegion);
}

TEST_F (AgeingDamageBuffers, AddOverdrawAreaOnInvalidRosters)
{
    EXPECT_CALL (mockDamageAgeTracker, overdrawRegionOnPaintingFrame (_)).Times (0);
    ageing.unobserve (mockDamageAgeTracker);
    ageing.markAreaDirtyOnLastFrame (emptyRegion);
}


