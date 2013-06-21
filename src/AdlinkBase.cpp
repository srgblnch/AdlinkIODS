

#include "AdlinkBase.h"
#include <tango.h>

/*static*/ std::ostream* AdlinkBase::infoStream = 0;
/*static*/ std::ostream* AdlinkBase::errorStream = 0;

/*static*/ void AdlinkBase::throw_error(
		const std::string & reason,
		const std::string & desc,
		const std::string & method) const
{
	status_warn()	<< "---------\n" << reason << "\n"
					<< desc << std::endl;
	Tango::Except::throw_exception(reason, desc, method);
}



/*inline static*/ std::ostream & AdlinkBase::info()
{
	if(infoStream)
		return *infoStream;
	std::cerr << "STREAM NOT DEFINED" << std::endl;
	assert(false);
	return *infoStream;
}

/*inline static*/ std::ostream & AdlinkBase::error()
{
	if(errorStream)
		return *errorStream;
	return info();
}
