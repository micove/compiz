#include <map>
#include <vector>
#include <string>
#include <istream>
#include <ostream>
#include <fstream>
#include <iterator>
#include <iostream>
#include <sstream>
#include <libgen.h>

using namespace std;

int usage ()
{
    cout << "Usage: PATH_TO_TEST_BINARY --gtest_list_tests | ./build_test_cases PATH_TO_TEST_BINARY --wrapper PATH_TO_WRAPPER" << endl;
    return 1;
}

int main (int argc, char **argv)
{
    string testBinary;
    string wrapperBinary;
    cin >> noskipws;

    if (argc == 2)
	testBinary = string (argv[1]);
    else if (argc == 4 &&
	     string (argv[2]) == "--wrapper")
    {
	testBinary = string (argv[1]);
	wrapperBinary = string (argv[3]);
    }
    else
	return usage ();

    map<string, vector<string> > testCases;
    string line;
    string currentTestCase;

    while (getline (cin, line))
    {
	/* Is test case */
	if (line.find ("  ") == 0)
	    testCases[currentTestCase].push_back (currentTestCase + line.substr (2));
	else
	    currentTestCase = line;

    }

    ofstream testfilecmake;
    char *base = basename (argv[1]);
    string   gtestName (base);

    testfilecmake.open (string (gtestName  + "_test.cmake").c_str (), ios::out | ios::trunc);

    if (testfilecmake.is_open ())
    {
	for (map <string, vector<string> >::iterator it = testCases.begin ();
	     it != testCases.end (); ++it)
	{
	    for (vector <string>::iterator jt = it->second.begin ();
		 jt != it->second.end (); ++jt)
	    {
		if (testfilecmake.good ())
		{
		    stringstream addTest, escapedWrapper, escapedTest, testExec, gTestFilter, endParen;

		    addTest << "ADD_TEST (";
		    if (wrapperBinary.size ())
			escapedWrapper << " \"" << wrapperBinary << "\"";

		    escapedTest << " \"" << testBinary << "\"";
		    testExec << escapedWrapper.str () << escapedTest.str ();
		    gTestFilter << " \"--gtest_filter=";
		    endParen << "\")";

		    std::string testName = jt->substr(0, jt->find("#"));

		    testfilecmake <<
			addTest.str () <<
			testName <<
			testExec.str () <<
			gTestFilter.str () <<
			testName <<
			endParen.str () <<
			endl;
		}
	    }
	}

	testfilecmake.close ();
    }

    return 0;
}
