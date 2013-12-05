#include <ccs_gsettings_wrapper_factory_mock.h>
#include <ccs-object.h>

const CCSGSettingsWrapperFactoryInterface mockInterface =
{
    CCSGSettingsWrapperFactoryGMock::ccsGSettingsWrapperFactoryNewGSettingsWrapper,
    CCSGSettingsWrapperFactoryGMock::ccsGSettingsWrapperFactoryNewGSettingsWrapperWithPath,
    CCSGSettingsWrapperFactoryGMock::ccsFreeGSettingsWrapperFactory
};

CCSGSettingsWrapperFactory *
ccsMockGSettingsWrapperFactoryNew ()
{
    CCSGSettingsWrapperFactory *wrapper = (CCSGSettingsWrapperFactory *) calloc (1, sizeof (CCSGSettingsWrapperFactory));

    if (!wrapper)
	return NULL;

    CCSGSettingsWrapperFactoryGMock *gmockWrapper = new CCSGSettingsWrapperFactoryGMock (wrapper);

    if (!gmockWrapper)
    {
	free (wrapper);
	return NULL;
    }

    ccsObjectInit (wrapper, &ccsDefaultObjectAllocator);
    ccsObjectAddInterface (wrapper, (const CCSInterface *) &mockInterface, GET_INTERFACE_TYPE (CCSGSettingsWrapperFactoryInterface));
    ccsObjectSetPrivate (wrapper, (CCSPrivate *) gmockWrapper);

    ccsGSettingsWrapperFactoryRef (wrapper);

    return wrapper;
}

void
ccsMockGSettingsWrapperFactoryFree (CCSGSettingsWrapperFactory *wrapper)
{
    CCSGSettingsWrapperFactoryGMock *gmockWrapper = reinterpret_cast <CCSGSettingsWrapperFactoryGMock *> (ccsObjectGetPrivate (wrapper));

    delete gmockWrapper;

    ccsObjectSetPrivate (wrapper, NULL);
    ccsObjectFinalize (wrapper);
    free (wrapper);
}
    
