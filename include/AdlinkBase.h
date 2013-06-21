#ifndef AdlinkBase_h___Ddsfks030
#define AdlinkBase_h___Ddsfks030

#include <iostream>
#include <sstream>
/**
 * @brief Just the base class for the classes representing AdlinkDevices.
 * 
 * Simply provides an standarized way to send info() messages, error()
 * messages and throwing exceptions. The idea is that just using different
 * streams and reimplementing throw_error, we can have a version of all
 * the codebase that launches Tango exceptions and writes to Tango logs
 * or a completely tango independent one that writes to standard
 * output and standard error and throws regular exceptions.
 * 
 * See also AdlHelp.h, as some of this is not 100% properly done, to change
 * all the exceptions you will have to edit both AdlHelp.cpp and AdlinkBase.cpp
 * 
 * @see AdlHelp.h
 */
class AdlinkBase
{
private:
	static std::ostream*	infoStream;
	static std::ostream*	errorStream;
	mutable std::stringstream m_warnStream;

protected:
	static std::ostream & info();
	static std::ostream & error();
	std::ostream & status_warn() const
	{		return m_warnStream;	}
	// {	return error();	}

	void throw_error(
			const std::string & reason,
			const std::string & desc,
			const std::string & method ) const;

public:
	static void set_info_stream(std::ostream* os)
	{	infoStream = os; }

	static void set_error_stream(std::ostream* os)
	{	errorStream = os; }

	void status_warn_clear()
	{		m_warnStream.clear();	}
	std::string status_warn_string() const
	{	return m_warnStream.str();	}
};

#endif // AdlinkBase_h___Ddsfks030
