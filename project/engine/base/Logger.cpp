#include "Logger.h"
#include <format>

namespace Logger
{
	void Log(const std::string& message)
	{
		OutputDebugStringA("[Log] --- Start ---\n");

		std::string finalMessage = message + "\n";
		OutputDebugStringA(finalMessage.c_str());

		OutputDebugStringA("[Log] --- End ---\n");
	}
}