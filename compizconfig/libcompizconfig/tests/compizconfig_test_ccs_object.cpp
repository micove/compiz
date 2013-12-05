#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <cstdio>

#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>

#include <ccs.h>
#include <compizconfig_ccs_mocked_allocator.h>

using ::testing::_;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::Invoke;

class GoogleMockDummy;

class CCSObjectTest :
    public ::testing::Test
{
};

struct TestingObjectWrapper
{
    CCSObject object;
};

typedef void (*dummyFunc) (GoogleMockDummy *);

struct DummyInterface
{
    dummyFunc dummy;
};

struct Dummy2Interface
{
    dummyFunc dummy;
};

class GoogleMockDummyInterface
{
    public:

	virtual ~GoogleMockDummyInterface () {};

	virtual void dummyFunc () = 0;
	virtual void freeTestingObjectWrapper (TestingObjectWrapper *) = 0;
};

class GoogleMockDummy :
    public GoogleMockDummyInterface
{
    public:

	MOCK_METHOD0 (dummyFunc, void ());
	MOCK_METHOD1 (freeTestingObjectWrapper, void (TestingObjectWrapper *));
    public:

	static void thunkDummyFunc (GoogleMockDummy *mock);
};

void
GoogleMockDummy::thunkDummyFunc (GoogleMockDummy *mock)
{
    mock->dummyFunc ();
}

const struct DummyInterface SomeDummyInterface =
{
    GoogleMockDummy::thunkDummyFunc
};

#define CCS_INTERFACE_TYPE_DUMMY GET_INTERFACE_TYPE (DummyInterface)
#define CCS_INTERFACE_TYPE_DUMMY2 GET_INTERFACE_TYPE (Dummy2Interface)

INTERFACE_TYPE (DummyInterface)
INTERFACE_TYPE (Dummy2Interface)

TEST(CCSObjectTest, TestTypeAllocation)
{
    unsigned int i = CCS_INTERFACE_TYPE_DUMMY;
    unsigned int j = CCS_INTERFACE_TYPE_DUMMY;
    unsigned int k = CCS_INTERFACE_TYPE_DUMMY2;

    EXPECT_EQ (i, 1);
    EXPECT_EQ (j ,1);
    EXPECT_EQ (k, 2);
}

TEST(CCSObjectTest, InterfaceAdd)
{
    TestingObjectWrapper *to = (TestingObjectWrapper *) calloc (1, sizeof (TestingObjectWrapper));

    ccsObjectInit (to, &ccsDefaultObjectAllocator);
    ccsObjectAddInterface (to, (const CCSInterface *) &SomeDummyInterface, 1);

    EXPECT_EQ (*((CCSObject *) to)->interfaces, (const CCSInterface *) (&SomeDummyInterface));
    EXPECT_EQ (((CCSObject *) to)->n_interfaces, 1);
    EXPECT_EQ (*((CCSObject *) to)->interface_types, 1);

    ccsObjectFinalize (to);

    free (to);
}

TEST(CCSObjectTest, GetInterface)
{
    TestingObjectWrapper *to = reinterpret_cast <TestingObjectWrapper *> (calloc (1, sizeof (TestingObjectWrapper)));

    ccsObjectInit (to, &ccsDefaultObjectAllocator);
    ccsObjectAddInterface (to, reinterpret_cast <const CCSInterface *> (&SomeDummyInterface), CCS_INTERFACE_TYPE_DUMMY);
    DummyInterface *interface = GET_INTERFACE (DummyInterface, to);

    EXPECT_EQ (interface, &SomeDummyInterface);

    ccsObjectRemoveInterface (to, CCS_INTERFACE_TYPE_DUMMY);

    free (to);
}

TEST(CCSObjectDeathTest, GetInterface)
{
    TestingObjectWrapper *to = reinterpret_cast <TestingObjectWrapper *> (calloc (1, sizeof (TestingObjectWrapper)));

    ccsObjectInit (to, &ccsDefaultObjectAllocator);
    ccsObjectAddInterface (to, reinterpret_cast <const CCSInterface *> (&SomeDummyInterface), CCS_INTERFACE_TYPE_DUMMY);

    Dummy2Interface *i = NULL;
    ASSERT_DEATH ({
		      i = GET_INTERFACE (Dummy2Interface, to);
		  },
		  "Unable to find interface type*");
    EXPECT_EQ (NULL, i);  // Avoid warnings/errors about i being unused.

    ccsObjectRemoveInterface (to, CCS_INTERFACE_TYPE_DUMMY);
    free (to);
}

TEST(CCSObjectTest, InterfaceRemove)
{
    TestingObjectWrapper *to = (TestingObjectWrapper *) calloc (1, sizeof (TestingObjectWrapper));

    ccsObjectInit (to, &ccsDefaultObjectAllocator);
    ccsObjectAddInterface (to, (const CCSInterface *) &SomeDummyInterface, 1);

    EXPECT_EQ (*((CCSObject *) to)->interfaces, (const CCSInterface *) (&SomeDummyInterface));
    EXPECT_EQ (((CCSObject *) to)->n_interfaces, 1);
    EXPECT_EQ (*((CCSObject *) to)->interface_types, 1);

    ccsObjectRemoveInterface (to, 1);

    EXPECT_EQ (NULL, ((CCSObject *) to)->interfaces);
    EXPECT_EQ (((CCSObject *) to)->n_interfaces, 0);
    EXPECT_EQ (NULL, ((CCSObject *) to)->interface_types);

    free (to);
}

TEST(CCSObjectTest, InterfaceFetchCall)
{
    TestingObjectWrapper *to = (TestingObjectWrapper *) calloc (1, sizeof (TestingObjectWrapper));

    ccsObjectInit (to, &ccsDefaultObjectAllocator);
    ccsObjectAddInterface (to, (const CCSInterface *) &SomeDummyInterface, 1);

    EXPECT_EQ (*((CCSObject *) to)->interfaces, (const CCSInterface *) (&SomeDummyInterface));
    EXPECT_EQ (((CCSObject *) to)->n_interfaces, 1);
    EXPECT_EQ (*((CCSObject *) to)->interface_types, 1);

    const DummyInterface *myDummyInterface = (const DummyInterface *) ccsObjectGetInterface (to, 1);
    GoogleMockDummy mockDummy;

    EXPECT_CALL (mockDummy, dummyFunc ());

    (*myDummyInterface->dummy) (&mockDummy);

    ccsObjectRemoveInterface (to, 1);

    EXPECT_EQ (NULL, ((CCSObject *) to)->interfaces);
    EXPECT_EQ (((CCSObject *) to)->n_interfaces, 0);
    EXPECT_EQ (NULL, ((CCSObject *) to)->interface_types);

    free (to);
}

TEST(CCSObjectTest, SetPrivateGetPrivate)
{
    TestingObjectWrapper *to = (TestingObjectWrapper *) calloc (1, sizeof (TestingObjectWrapper));

    int i = 1;

    ccsObjectInit (to, &ccsDefaultObjectAllocator);
    ccsObjectSetPrivate (to, (CCSPrivate *) &i);

    CCSPrivate *p = ccsObjectGetPrivate (to);

    EXPECT_EQ (&i, (int *) p);
    EXPECT_EQ (i, (*((int *) p)));

    free (to);
}

void ccsFreeTestingObjectWrapper (TestingObjectWrapper *wrapper)
{
    GoogleMockDummy *dummy =
        reinterpret_cast <GoogleMockDummy *> (reinterpret_cast <CCSObject *> (ccsObjectGetPrivate (wrapper)));
    dummy->freeTestingObjectWrapper (wrapper);
}

#define CCSREF_OBJ(type,dtype) \
    void ccs##type##Ref (dtype *d) \
    { \
	ccsObjectRef (d); \
    } \
    \
    void ccs##type##Unref (dtype *d) \
    { \
	ccsObjectUnref (d, ccsFree##type); \
    } \

CCSREF_HDR(TestingObjectWrapper, TestingObjectWrapper);
CCSREF_OBJ(TestingObjectWrapper, TestingObjectWrapper);

TEST(CCSObjectTest, TestRefUnrefFreesObject)
{
    TestingObjectWrapper *to = (TestingObjectWrapper *) calloc (1, sizeof (TestingObjectWrapper));

    GoogleMockDummy dummy;

    ccsObjectInit (to, &ccsDefaultObjectAllocator);
    ccsObjectSetPrivate (to, reinterpret_cast <CCSPrivate *> (&dummy));
    ccsTestingObjectWrapperRef (to);

    EXPECT_CALL (dummy, freeTestingObjectWrapper (to));

    ccsTestingObjectWrapperUnref (to);

    free (to);
}

TEST(CCSObjectTest, TestFinalizeFreesEverything)
{
    TestingObjectWrapper *to = (TestingObjectWrapper *) calloc (1, sizeof (TestingObjectWrapper));

    ccsObjectInit (to, &ccsDefaultObjectAllocator);
    ccsObjectAddInterface (to, (const CCSInterface *) &SomeDummyInterface, 1);

    EXPECT_EQ (*((CCSObject *) to)->interfaces, (const CCSInterface *) (&SomeDummyInterface));
    EXPECT_EQ (((CCSObject *) to)->n_interfaces, 1);
    EXPECT_EQ (*((CCSObject *) to)->interface_types, 1);

    int *i = (int *) malloc (sizeof (int));

    *i = 1;

    ccsObjectSetPrivate (to, (CCSPrivate *) i);

    CCSPrivate *p = ccsObjectGetPrivate (to);

    EXPECT_EQ (i, (int *) p);
    EXPECT_EQ (*i, (*((int *) p)));

    ccsObjectFinalize (to);

    EXPECT_EQ (NULL, ((CCSObject *) to)->priv);
    EXPECT_EQ (NULL, ((CCSObject *) to)->interfaces);
    EXPECT_EQ (((CCSObject *) to)->n_interfaces, 0);
    EXPECT_EQ (NULL, ((CCSObject *) to)->interface_types);

    free (to);
}

TEST(CCSObjectTest, TestReallocFailures)
{
    TestingObjectWrapper *to = (TestingObjectWrapper *) calloc (1, sizeof (TestingObjectWrapper));
    StrictMock <ObjectAllocationGMock> objectAllocationGMock;
    FailingObjectReallocation fakeFailingAllocator (1);

    CCSObjectAllocationInterface failingAllocatorGMock = failingAllocator;

    failingAllocatorGMock.allocator = static_cast <void *> (&objectAllocationGMock);

    ccsObjectInit (to, &failingAllocatorGMock);

    EXPECT_CALL (objectAllocationGMock, realloc_ (_, _))
	    .WillOnce (Invoke (&fakeFailingAllocator, &FailingObjectReallocation::realloc_))
	    .WillOnce (Invoke (&fakeFailingAllocator, &FailingObjectReallocation::realloc_));

    EXPECT_CALL (objectAllocationGMock, free_ (_))
	    .WillOnce (Invoke (&fakeFailingAllocator, &FailingObjectReallocation::free_));

    ccsObjectAddInterface (to, (const CCSInterface *) &SomeDummyInterface, 1);

    EXPECT_EQ (NULL, ((CCSObject *) to)->interfaces);
    EXPECT_EQ (((CCSObject *) to)->n_interfaces, 0);
    EXPECT_EQ (NULL, ((CCSObject *) to)->interface_types);

    free (to);
}

TEST(CCSObjectTest, TestAllocatedMemoryOnRemove)
{
    TestingObjectWrapper *to = (TestingObjectWrapper *) calloc (1, sizeof (TestingObjectWrapper));

    ccsObjectInit (to, &ccsDefaultObjectAllocator);
    ccsObjectAddInterface (to, (const CCSInterface *) &SomeDummyInterface, 1);

    EXPECT_EQ (*((CCSObject *) to)->interfaces, (const CCSInterface *) (&SomeDummyInterface));
    EXPECT_EQ (((CCSObject *) to)->n_interfaces, 1);
    EXPECT_EQ (*((CCSObject *) to)->interface_types, 1);

    ccsObjectAddInterface (to, (const CCSInterface *) &SomeDummyInterface, 2);

    EXPECT_EQ (*((CCSObject *) to)->interfaces, (const CCSInterface *) (&SomeDummyInterface));
    EXPECT_EQ (((CCSObject *) to)->n_interfaces, 2);
    EXPECT_EQ (*((CCSObject *) to)->interface_types, 1);

    ccsObjectRemoveInterface (to, 1);

    EXPECT_EQ (*((CCSObject *) to)->interfaces, (const CCSInterface *) (&SomeDummyInterface));
    EXPECT_EQ (((CCSObject *) to)->n_interfaces, 1);
    EXPECT_EQ (((CCSObject *) to)->n_allocated_interfaces, 2);
    EXPECT_EQ (*((CCSObject *) to)->interface_types, 2);

    ccsObjectRemoveInterface (to, 2);

    EXPECT_EQ (NULL, ((CCSObject *) to)->interfaces);
    EXPECT_EQ (((CCSObject *) to)->n_interfaces, 0);
    EXPECT_EQ (((CCSObject *) to)->n_allocated_interfaces, 0);
    EXPECT_EQ (NULL, ((CCSObject *) to)->interface_types);

    free (to);
}
