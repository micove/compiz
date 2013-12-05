#include <test-pluginclasshandler.h>

class BuildPlugin:
    public Plugin,
    public PluginClassHandler <BuildPlugin, Base>
{
    public:
	BuildPlugin (Base *base) :
	    Plugin(base),
	    PluginClassHandler <BuildPlugin, Base> (base)
	{
	}
};

class PluginClassHandlerConstruction :
    public CompizPCHTest
{
    public:

	PluginClassHandlerConstruction ();
	~PluginClassHandlerConstruction ();
};

PluginClassHandlerConstruction::PluginClassHandlerConstruction ()
{
    namespace cpi = compiz::plugin::internal;
    cpi::LoadedPluginClassBridge <BuildPlugin, Base>::allowInstantiations (key);
}

PluginClassHandlerConstruction::~PluginClassHandlerConstruction ()
{
    namespace cpi = compiz::plugin::internal;
    cpi::LoadedPluginClassBridge <BuildPlugin, Base>::disallowInstantiations (key);
}

TEST_F (PluginClassHandlerConstruction, TestConstruction)
{
    Plugin *p;

    bases.push_back(new Base());
    plugins.push_back(static_cast<Plugin *>(new BuildPlugin(bases.back())));
    bases.push_back(new Base());
    plugins.push_back(static_cast<Plugin *>(new BuildPlugin(bases.back())));

    if (bases.front()->pluginClasses.size() != globalPluginClassIndices.size())
    {
	FAIL() << "allocated number of plugin classes is not the same as the"
		" global number of allocated plugin classes";
    }

    if (!ValueHolder::Default()->hasValue(
	    compPrintf("%s_index_%lu", typeid(BuildPlugin).name(), 0)))
    {
	FAIL() << "ValueHolder does not have value "
		<< compPrintf("%s_index_%lu", typeid(BuildPlugin).name(), 0);
    }

    p = BuildPlugin::get(bases.front());

    if (p != plugins.front())
    {
	FAIL() << "Returned Plugin * is not plugins.front ()";
    }

    p = BuildPlugin::get(bases.back());

    if (p != plugins.back())
    {
	FAIL() << "Returned Plugin * is not plugins.back ()";
    }
}

