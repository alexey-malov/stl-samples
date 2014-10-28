#include <list>
#include <string>
#include <iostream>
#include <algorithm>

using namespace std;

typedef list<string> StringList;

StringList PopulateNamesList()
{
	StringList maleNames;
	maleNames.emplace_back("Ivan");
	maleNames.emplace_front("Sergey");
	
	StringList femaleNames;
	femaleNames.emplace_back("Irina");
	femaleNames.emplace_front("Anna");


	StringList allNames(move(maleNames));
	allNames.insert(allNames.end(), femaleNames.cbegin(), femaleNames.cend());
	return allNames;
}

void PrintNamesList(StringList const& names)
{
	for (auto const& name : names)
	{
		cout << name << ", ";
	}
	cout << endl;
}

void PrintNamesListByCopyingItemsToCout(StringList const& names)
{
	copy(names.begin(), names.end(), ostream_iterator<string>(cout, ", "));
	cout << endl;
}

void main()
{
	list<string> names = PopulateNamesList();
	PrintNamesList(names);
	PrintNamesListByCopyingItemsToCout(names);
}