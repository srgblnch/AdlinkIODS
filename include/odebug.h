
///
/// @file Helpful debug macros. Most of them are disabled if DEBUG_ADLINK_DEVICE
/// is not defined.
///
/// You must define _OCLASSNAME in your cpp file before including odebug.h
///

#pragma once

#define _CFN_ __odebug_addstrings(_OCLASSNAME, __FUNCTION__)

#define _SLOW_FUNCTION_NAME_ __odebug_clear_pretty(__PRETTY_FUNCTION__)

namespace {
	inline std::string __odebug_addstrings(const char* classname, const char* fname)
	{
		std::stringstream ss;
		ss << classname << "::" << fname;
		return ss.str();
	}
	
	inline std::string __odebug_clear_pretty(const char* pretty_function)
	{
		const char* current = pretty_function;
		
		size_t endPos = 0;
		size_t lastSpacePos = 0;
		while ( (*current != '\0') && (*current != '(') ) {
			if (*current == ' ')
				lastSpacePos = endPos;
			
			++current;
			++endPos;
		}
		
		return std::string(pretty_function + lastSpacePos, endPos - lastSpacePos);
	}
}

#include <sstream>

#ifdef DEBUG_ADLINK_DEVICE
  #include <cstdio>
  #define DBGCOUT std::cerr << _CFN_ << ": "
  #define DBGSTEP(n) DBGCOUT << n << std::endl
  #define DBGPRINT(fmt, ...) fprintf(ferror, "%s: " fmt "\n", _CFN_, __VA_ARGS__ )
  #define DBGVAL(var) DBGCOUT << #var << " = " << var << std::endl;
#else
  #define DBGCOUT if (0) std::cerr
  #define DBGSTEP(n)
  #define DBGPRINT(fmt, ...)
  #define DBGVAL(var)
#endif

#define DBGVAR(var) #var << "=" << (var)
