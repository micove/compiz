import compiz_config_test
import unittest

class CompizConfigTestPlugin (compiz_config_test.CompizConfigTest):

    def runTest (self):
	plugin = self.ccs.Plugin (self.context, "mock")
	plugin.Update ()

	self.assertEqual (plugin.Context, self.context)
	self.assertTrue ("A Mock Group" in plugin.Groups)
	self.assertTrue (plugin.Screen is not None)
	self.assertEqual (plugin.Name, "mock")
	self.assertEqual (plugin.ShortDesc, "Mock")
	self.assertEqual (plugin.LongDesc, "Mock plugin")
	self.assertEqual (plugin.Category, "Mocks")
	self.assertEqual (plugin.Features, ["mocking"])

if __name__ == '__main__':
    unittest.main()
