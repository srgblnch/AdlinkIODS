

#ifndef _EXCEPTION__H___dsfJKOEWKFSDfw564rtR____
#define _EXCEPTION__H___dsfJKOEWKFSDfw564rtR____

#include <exception>
#include <string>

namespace Exceptions
{

class Exception : public std::exception
{
	std::string	m_message;
	public:
		Exception(const std::string &msg) : m_message(msg) {};
		virtual const char * what () const throw()
		{ return m_message.c_str(); }
		~Exception() throw() {};
};

struct TimeError : public Exception
{
	TimeError() : Exception("Error reading current time") {}
};

struct BadRange : public Exception
{
	BadRange() : Exception("Parameter out of range") {}
};

struct UndefinedState : public Exception
{
	UndefinedState() : Exception("Undefined state") {}
};

} //namespace

#endif //_EXCEPTION__H___dsfJKOEWKFSDfw564rtR____
