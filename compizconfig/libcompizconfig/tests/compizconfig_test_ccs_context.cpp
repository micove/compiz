#include <boost/shared_ptr.hpp>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <ccs.h>

#include <gtest_shared_autodestroy.h>

#include "ccs_backend_loader_interface.h"
#include "ccs_backend_loader.h"

#include "ccs_config_file_interface.h"
#include "ccs_config_file.h"

#include "ccs-backend.h"
#include "ccs-private.h"

#include "compizconfig_ccs_config_file_mock.h"
#include "compizconfig_ccs_backend_loader_mock.h"
#include "compizconfig_ccs_backend_mock.h"
#include "compizconfig_ccs_context_mock.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::SetArgPointee;
using ::testing::Return;
using ::testing::DoAll;
using ::testing::Eq;

class CCSContextTest :
    public ::testing::Test
{
};

TEST_F(CCSContextTest, TestMock)
{
    CCSContext *context = ccsMockContextNew ();
    CCSContextGMock *mock = (CCSContextGMock *) ccsObjectGetPrivate (context);

    EXPECT_CALL (*mock, getPlugins ());
    EXPECT_CALL (*mock, getCategories ());
    EXPECT_CALL (*mock, getChangedSettings ());
    EXPECT_CALL (*mock, getScreenNum ());
    EXPECT_CALL (*mock, addChangedSetting (_));
    EXPECT_CALL (*mock, clearChangedSettings ());
    EXPECT_CALL (*mock, stealChangedSettings ());
    EXPECT_CALL (*mock, getPrivatePtr ());
    EXPECT_CALL (*mock, setPrivatePtr (_));
    EXPECT_CALL (*mock, loadPlugin (_));
    EXPECT_CALL (*mock, findPlugin (_));
    EXPECT_CALL (*mock, pluginIsActive (_));
    EXPECT_CALL (*mock, getActivePluginList ());
    EXPECT_CALL (*mock, getSortedPluginStringList ());
    EXPECT_CALL (*mock, setBackend (_));
    EXPECT_CALL (*mock, getBackend ());
    EXPECT_CALL (*mock, setIntegrationEnabled (_));
    EXPECT_CALL (*mock, setProfile (_));
    EXPECT_CALL (*mock, setPluginListAutoSort (_));
    EXPECT_CALL (*mock, getIntegrationEnabled ());
    EXPECT_CALL (*mock, getProfile ());
    EXPECT_CALL (*mock, getPluginListAutoSort ());
    EXPECT_CALL (*mock, processEvents (_));
    EXPECT_CALL (*mock, readSettings ());
    EXPECT_CALL (*mock, writeSettings ());
    EXPECT_CALL (*mock, writeChangedSettings ());
    EXPECT_CALL (*mock, exportToFile (_, _));
    EXPECT_CALL (*mock, importFromFile (_, _));
    EXPECT_CALL (*mock, canEnablePlugin (_));
    EXPECT_CALL (*mock, canDisablePlugin (_));
    EXPECT_CALL (*mock, getExistingProfiles ());
    EXPECT_CALL (*mock, deleteProfile (_));

    char *foo = strdup ("foo");
    char *bar = strdup ("bar");

    ccsContextGetPlugins (context);
    ccsContextGetCategories (context);
    ccsContextGetChangedSettings (context);
    ccsContextGetScreenNum (context);
    ccsContextAddChangedSetting (context, NULL);
    ccsContextClearChangedSettings (context);
    ccsContextStealChangedSettings (context);
    ccsContextGetPrivatePtr (context);
    ccsContextSetPrivatePtr (context, NULL);
    ccsLoadPlugin (context, foo);
    ccsFindPlugin (context, foo);
    ccsPluginIsActive (context, foo);
    ccsGetActivePluginList (context);
    ccsGetSortedPluginStringList (context);
    ccsSetBackend (context, bar);
    ccsGetBackend (context);
    ccsSetIntegrationEnabled (context, TRUE);
    ccsSetProfile (context, foo);
    ccsSetPluginListAutoSort (context, TRUE);
    ccsGetIntegrationEnabled (context);
    ccsGetProfile (context);
    ccsGetPluginListAutoSort (context);
    ccsProcessEvents (context, 0);
    ccsReadSettings (context);
    ccsWriteSettings (context);
    ccsWriteChangedSettings (context);
    ccsExportToFile (context, bar, TRUE);
    ccsImportFromFile (context, foo, FALSE);
    ccsCanEnablePlugin (context, NULL);
    ccsCanDisablePlugin (context, NULL);
    ccsGetExistingProfiles (context);
    ccsDeleteProfile (context, foo);

    free (foo);
    free (bar);

    ccsFreeContext (context);
}

const CCSInterfaceTable ccsMockInterfaceTable =
{
    &ccsDefaultContextInterface,
    NULL,
    NULL,
    NULL,
    NULL
};

class CCSContextTestWithMockedBackendProfile :
    public ::testing::Test
{
    public:

	CCSContextTestWithMockedBackendProfile () :
	    loader (ccsMockBackendLoaderNew (&ccsDefaultObjectAllocator)),
	    config (ccsMockConfigFileNew (&ccsDefaultObjectAllocator)),
	    backend (AutoDestroy (ccsMockBackendNew (),
				  ccsFreeMockBackend)),
	    mockLoader (reinterpret_cast <CCSBackendLoaderGMock *> (ccsObjectGetPrivate (loader))),
	    mockConfig (reinterpret_cast <CCSConfigFileGMock *> (ccsObjectGetPrivate (config))),
	    mockBackend (reinterpret_cast <CCSBackendGMock *> (ccsObjectGetPrivate (backend.get ()))),
	    mockBackendStr ("mock"),
	    availableProfileStr ("available"),
	    unavailableProfileStr ("unavailable")
	{
	    inst = this;

	    /* Insert a stub dynamic backend interface into the mock backend */
	    CCSDynamicBackendInterface backendIface =
	    {
		CCSContextTestWithMockedBackendProfile::GetMockBackendName,
		CCSContextTestWithMockedBackendProfile::GetMockBackendSupportRead,
		CCSContextTestWithMockedBackendProfile::GetMockBackendSupportWrite,
		CCSContextTestWithMockedBackendProfile::GetMockBackendSupportProfiles,
		CCSContextTestWithMockedBackendProfile::GetMockBackendSupportIntegration,
		CCSContextTestWithMockedBackendProfile::GetMockBackendSupportRaw
	    };
	    stubDynamicBackend = backendIface;

	    ccsObjectAddInterface (backend.get (),
				   (const CCSInterface *) &stubDynamicBackend,
				   GET_INTERFACE_TYPE (CCSDynamicBackendInterface));

	    char       *mockBackendCopy = strdup (mockBackendStr.c_str ());
	    EXPECT_CALL (*mockConfig, readConfigOption (_, _)).Times (AtLeast (0));
	    EXPECT_CALL (*mockConfig, writeConfigOption (_, _)).Times (AtLeast (0));
	    EXPECT_CALL (*mockConfig, setConfigWatchCallback (_, _)).Times (AtLeast (0));
	    EXPECT_CALL (*mockLoader, loadBackend (_, _, _)).Times (AtLeast (0));
	    EXPECT_CALL (*mockBackend, init (_)).Times (AtLeast (0));
	    EXPECT_CALL (*mockBackend, getExistingProfiles (_)).Times (AtLeast (0));

	    ON_CALL (*mockConfig, readConfigOption (OptionBackend, _))
		.WillByDefault (DoAll (SetArgPointee <1> (mockBackendCopy),
				       Return (1)));

	    ON_CALL (*mockLoader, loadBackend (_, _, Eq (mockBackendStr)))
		.WillByDefault (Return (backend.get ()));

	    /* We are using ccsFreeContext instead of ccsContextDestroy as that does not
	     * free the backend as well */
	    context = AutoDestroy (ccsEmptyContextNew (0,
						       CCSContextTestWithMockedBackendProfile::ImportProfile,
						       CCSContextTestWithMockedBackendProfile::AvailableProfiles,
						       loader,
						       config,
						       &ccsMockInterfaceTable),
				   ccsFreeContext);
	}

	~CCSContextTestWithMockedBackendProfile ()
	{
	    inst = NULL;
	}

    protected:

	void AddAvailableSysconfProfile (const std::string &profile)
	{
	    availableProfiles.push_back (profile);
	}

	void ExistingProfile (const std::string &profile)
	{
	    CCSString *string = reinterpret_cast <CCSString *> (calloc (1, sizeof (CCSString)));
	    string->value = strdup (profile.c_str ());
	    ccsStringRef (string);

	    CCSStringList existing (ccsStringListAppend (NULL, string));
	    ON_CALL (*mockBackend, getExistingProfiles (_)).WillByDefault (Return (existing));
	}

    private:

	static CCSStringList AvailableProfiles (const char *directory)
	{
	    CCSStringList list = NULL;


	    for (std::vector <std::string>::iterator it = inst->availableProfiles.begin ();
		 it != inst->availableProfiles.end ();
		 ++it)
	    {
		CCSString *string = reinterpret_cast <CCSString *> (calloc (1, sizeof (CCSString)));
		ccsStringRef (string);
		string->value = strdup (it->c_str ());
		list = ccsStringListAppend (list, string);
	    }

	    return list;
	}

	static Bool ImportProfile (CCSContext *context,
				   const char *fileName,
				   Bool       overwriteNonDefault)
	{
	    return inst->ImportProfileVerify (context, fileName, overwriteNonDefault);
	}

	static const char * GetMockBackendName (CCSDynamicBackend *backend)
	{
	    return inst->mockBackendStr.c_str ();
	}

	static Bool GetMockBackendSupportRead (CCSDynamicBackend *backend)
	{
	    return TRUE;
	}

	static Bool GetMockBackendSupportWrite (CCSDynamicBackend *backend)
	{
	    return TRUE;
	}

	static Bool GetMockBackendSupportIntegration (CCSDynamicBackend *backend)
	{
	    return FALSE;
	}

	static Bool GetMockBackendSupportProfiles (CCSDynamicBackend *backend)
	{
	    return TRUE;
	}

	static CCSBackend * GetMockBackendSupportRaw (CCSDynamicBackend *backend)
	{
	    return inst->backend.get ();
	}

    protected:

	MOCK_METHOD3 (ImportProfileVerify, Bool (CCSContext *, const char*, Bool));

	CCSBackendLoader *loader;
	CCSConfigFile    *config;

	/* This is not managed by the context */
	boost::shared_ptr <CCSBackend> backend;

	CCSBackendLoaderGMock *mockLoader;
	CCSConfigFileGMock    *mockConfig;
	CCSBackendGMock       *mockBackend;
	const std::string mockBackendStr;
	const std::string availableProfileStr;
	const std::string unavailableProfileStr;

	CCSDynamicBackendInterface stubDynamicBackend;

	boost::shared_ptr <CCSContext> context;

    private:

	std::vector <std::string> availableProfiles;

    protected:

	static CCSContextTestWithMockedBackendProfile *inst;
};

CCSContextTestWithMockedBackendProfile *
CCSContextTestWithMockedBackendProfile::inst = NULL;

TEST_F (CCSContextTestWithMockedBackendProfile, TestSetup)
{
}

/* We should always import any new profiles that we encounter which are not
 * available from the backend. The reason for this being that we don't want to
 * just load the profile and end up with an empty config */
TEST_F (CCSContextTestWithMockedBackendProfile, ImportProfileIfNotAvailableInBackend)
{
    const std::string sysconfProfile (std::string (SYSCONFDIR) + "/compizconfig/" + unavailableProfileStr);

    ExistingProfile (availableProfileStr);
    AddAvailableSysconfProfile (sysconfProfile);

    EXPECT_CALL (*this, ImportProfileVerify (_, Eq (sysconfProfile), _))
	.WillOnce (Return (TRUE));

    ccsSetProfile (context.get (), unavailableProfileStr.c_str ());
}

TEST_F (CCSContextTestWithMockedBackendProfile, ImportProfileDotIni)
{
    const std::string sysconfProfile (std::string (SYSCONFDIR) + "/compizconfig/" + unavailableProfileStr + ".ini");

    ExistingProfile (availableProfileStr);
    AddAvailableSysconfProfile (sysconfProfile);

    EXPECT_CALL (*this, ImportProfileVerify (_, Eq (sysconfProfile), _))
	.WillOnce (Return (TRUE));

    ccsSetProfile (context.get (), unavailableProfileStr.c_str ());
}

TEST_F (CCSContextTestWithMockedBackendProfile, ImportProfileDotProfile)
{
    const std::string sysconfProfile (std::string (SYSCONFDIR) + "/compizconfig/" + unavailableProfileStr + ".profile");

    ExistingProfile (availableProfileStr);
    AddAvailableSysconfProfile (sysconfProfile);

    EXPECT_CALL (*this, ImportProfileVerify (_, Eq (sysconfProfile), _))
	.WillOnce (Return (TRUE));

    ccsSetProfile (context.get (), unavailableProfileStr.c_str ());
}

/* Not when available in both */
TEST_F (CCSContextTestWithMockedBackendProfile, NoImportProfileIfAvailableInBackend)
{
    ExistingProfile (availableProfileStr);
    AddAvailableSysconfProfile (availableProfileStr);

    EXPECT_CALL (*this, ImportProfileVerify (_, _, _)).Times (0);

    ccsSetProfile (context.get (), availableProfileStr.c_str ());
}

/* Not when ther selected profile isn't in sysconfdir */
TEST_F (CCSContextTestWithMockedBackendProfile, NoImportProfileIfNotInDir)
{
    ExistingProfile (availableProfileStr);
    AddAvailableSysconfProfile (availableProfileStr);

    EXPECT_CALL (*this, ImportProfileVerify (_, _, _)).Times (0);

    ccsSetProfile (context.get (), unavailableProfileStr.c_str ());
}
