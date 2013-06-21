

#ifndef _ADLHELP_H___fejisFJDJedr884K9391LLlk__
#define _ADLHELP_H___fejisFJDJedr884K9391LLlk__

#include <dask.h>		// PCI_DASK
#include <d2kdask.h>	// D2K-DASK
#include <string>
#include <sstream>

#define assert_adlink(errcode, adfunct) do { _assert_adlink(errcode, __PRETTY_FUNCTION__, adfunct); } while(0)
void _assert_adlink(I16 errcode, const std::string & tmethod,
		   const std::string & adfunct);
void throw_adlink_error(const std::string & tmethod,
				const std::string & adfunct,
				I16 errcode);
const char* pcisdask_error_text (int _err_code);

class AdlExcept : public std::exception
{
public:
	std::string shortDesc;
	std::string longDesc;
	std::string functionName;
	std::string _what;
	I16 errorCode;
	AdlExcept(
		const std::string &shortd,
		const std::string &longd,
		const std::string &func,
		I16 ec)
		: shortDesc(shortd), longDesc(longd), functionName(func), errorCode(ec)
	{
		std::string ss;
		_what = "Adlink library error in function ";
		_what += functionName;
		_what += "\n";
		_what += shortDesc;
		_what += "\n";
		_what += longDesc;
	}

	virtual const char* what() const throw()
	{
		return _what.c_str();
	}

	void tango_throw();

	~AdlExcept() throw() {};
};

#endif //_ADLHELP_H___fejisFJDJedr884K9391LLlk__
