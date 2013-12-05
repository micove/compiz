import compiz_config_test
import unittest

class CompizConfigTestSetting (compiz_config_test.CompizConfigTest):

    def runTest (self):

	plugin = self.context.Plugins["mock"]
	setting = self.ccs.Setting (plugin, "mock")

	self.assertEqual (setting.Plugin, plugin)
	self.assertEqual (setting.Name, "mock")
	self.assertEqual (setting.ShortDesc, "Mock Setting")
	self.assertEqual (setting.LongDesc, "A Mock Setting")
	self.assertEqual (setting.Group, "A Mock Group")
	self.assertEqual (setting.SubGroup, "A Mock Subgroup")
	self.assertEqual (setting.Type, "String")
	self.assertEqual (setting.Hints, [])
	self.assertEqual (setting.DefaultValue, "Mock Value")
	self.assertEqual (setting.Value, "Mock Value")
	self.assertEqual (setting.Integrated, False)
	self.assertEqual (setting.ReadOnly, False)

if __name__ == '__main__':
    unittest.main()
