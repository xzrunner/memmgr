#include "memmgr/Utility.h"

namespace mm
{

const char* Utility::ToSize(size_t value, float& result)
{
	if (value < 2000) {
		result = static_cast<float>(value);
		return "B";
	}
	if (value < 2000000) {
		result = value / 1024.0f;
		return "KB";
	}
	result = value / 1048576.0f;
	return "MB";
}

}