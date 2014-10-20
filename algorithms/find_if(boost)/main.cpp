#include <iostream>
#include <vector>
#include <functional>
#include <boost/range/algorithm/find_if.hpp>
#include <boost/range/adaptor/reversed.hpp>
#include <boost/range/adaptor/filtered.hpp>
#include <boost/phoenix.hpp>


#include <random>
#include <boost/range/algorithm/generate.hpp>
#include <boost/range/algorithm/count_if.hpp>
#include <boost/range/algorithm/count.hpp>
#include <boost/timer/timer.hpp>

using namespace std;
using namespace boost::range;
using namespace boost::adaptors;
using namespace boost::phoenix::placeholders;

void FindFirstEvenValueInArray()
{
	int numbers[] = { 1, 3, 9, 10, 17, 12, 21 };

	auto it = find_if(numbers, arg1 % 2 == 0);

	if (it != end(numbers))
	{
		cout << "First even number in array is " << *it << endl;
	}
}

void FindLastNegativeNumberInVector()
{
	vector<int> numbers = { 1, 2, 3, 0, -4, -1, 20 };

	auto reversedNumbers = numbers | reversed;
	auto it = find_if(reversedNumbers, arg1 < 0);

	if (it != reversedNumbers.end())
	{
		cout << "Last negative number in vector is " << *it << endl;
	}
}

void Benchmark(function<size_t()> const& fn)
{
	size_t num = 0;
	{
		boost::timer::auto_cpu_timer t;
		for (int i = 0; i < 500; ++i)
		{
			num += fn();
		}
	}
	cout << num << endl;
}

void main()
{
	FindFirstEvenValueInArray();
	FindLastNegativeNumberInVector();	

	mt19937 gen;
	vector<int> randomNumbers;
	randomNumbers.reserve(1000000);
	generate_n(back_inserter(randomNumbers), 1000000, gen);

	size_t const size = randomNumbers.size();

	Benchmark([&randomNumbers, size](){
		size_t numNegativeNumbers = 0;
		for (size_t i = 0; i < size; ++i)
		{
			if (randomNumbers[i] < 0)
			{
				++numNegativeNumbers;
			}
		}
		return numNegativeNumbers;
	});

	Benchmark([&randomNumbers](){
		size_t numNegativeNumbers = 0;
		for (auto value : randomNumbers)
		{
			if (value < 0)
			{
				++numNegativeNumbers;
			}
		}
		return numNegativeNumbers;
	});

	Benchmark([&randomNumbers](){
		return count_if(randomNumbers.begin(), randomNumbers.end(), [](int value){return value < 0; });
	});

	Benchmark([&randomNumbers](){
		return count_if(randomNumbers, [](int value){return value < 0; });
	});

	Benchmark([&randomNumbers](){
		return count_if(randomNumbers, arg1 < 0);
	});
}
