#ifndef __UTILS_H__
#define __UTILS_H__

#include <iostream>
#include <vector>

namespace Utils
{
	std::vector<char> readFile(const std::string& filename);
	double GetTimeEclapsed();

	void GetMSStart();
	double GetMSEnd();

	template <typename T> __forceinline T DivideByMultiple(T value, size_t alignment)
	{
		return (T)((value + alignment - 1) / alignment);
	}
};

#endif // !__UTILS_H__
