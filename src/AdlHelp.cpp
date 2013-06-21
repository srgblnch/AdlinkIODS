
#include "AdlHelp.h"
#ifndef _ADLINK_WITHOUT_TANGO_
#include <tango.h>
#endif // _ADLINK_WITH_TANGO_
#include <iostream>
#include <sstream>

void AdlExcept::tango_throw()
{
#ifndef _ADLINK_WITHOUT_TANGO_
	Tango::Except::throw_exception(
			this->shortDesc,
			this->longDesc,
			this->functionName);
#else // _ADLINK_WITHOUT_TANGO_
	std::cerr << "ADLINK EXCEPTION" << std::endl;
	std::cerr << this->functionName << std::endl;
	std::cerr << this->shortDesc << std::endl;
	std::cerr << this->longDesc << std::endl;
	throw 0;
#endif // _ADLINK_WITHOUT_TANGO_
}

void throw_adlink_error(const std::string & tmethod,
				 const std::string & adfunct,
				 I16 errcode)
{

	std::stringstream shortdesc, longdesc;
	shortdesc << "ADLINK error " << errcode;
	longdesc << "Error in call to " << adfunct << ", " <<
			pcisdask_error_text(errcode);

	//throw AdlExcept(shortdesc.str(), longdesc.str(), tmethod, errcode);
	AdlExcept(shortdesc.str(), longdesc.str(), tmethod, errcode).tango_throw();
}

void _assert_adlink(I16 errcode, const std::string & tmethod,
		   const std::string & adfunct)
{
	if(errcode != NoError)
		throw_adlink_error(tmethod, adfunct, errcode);
}

const char* pcisdask_error_text (int _err_code)
{
  switch (_err_code) 
  {
    case NoError:
      return "No error occured";
      break;
    case ErrorUnknownCardType:
      return "The card type argument is not valid";
      break;
    case ErrorInvalidCardNumber:
      return "The cardNumber argument is out of range (larger than 31)";
      break;
    case ErrorTooManyCardRegistered:
      return "More than 32 cards were registered";
      break; 
    case ErrorCardNotRegistered:
      return "No card register as id CardNumber";
      break;
    case ErrorFuncNotSupport:
      return "The function called is not supported by this type of card";
      break;
    case ErrorInvalidIoChannel:
      return "The specified channel or port argument is out of range";
      break;
    case ErrorInvalidAdRange:
      return "The specified analog input range is invalid";
      break;
    case ErrorContIoNotAllowed:
      return "The specified continuous IO operation is not supported by this"
			" type of card";
      break;
    case ErrorDiffRangeNotSupport:
      return "All the analog input ranges must be the same for multi-channel"
			" analog input";
      break;
    case ErrorLastChannelNotZero:
      return "The channels for multi-channel analog input must be ended with or"
			"started with zero";
      break;
    case ErrorChannelNotDescending:
      return "The channel fo multi-channel analog input must be contiguous and"
			" 'in descending order";
      break;
    case ErrorChannelNotAscending:
      return "The channel fo multi-channel analog input must be contiguous and"
			" in ascending order";
      break;
    case ErrorOpenDriverFailed:
      return "Failed to open the device driver";
      break;
    case ErrorOpenEventFailed:
      return "Open event failed in device driver";
      break;
    case ErrorTransferCountTooLarge:
      return "The size of transfer is larger than the size of initially"
			" allocated memory in driver";
      break;
    case ErrorNotDoubleBufferMode:
      return "Double buffer mode is disabled";
      break;
    case ErrorInvalidSampleRate:
      return "The specified sampling rate is out of range";
      break;
    case ErrorInvalidCounterMode:
      return "The value of the Mode argument is invalid";
      break;
    case ErrorInvalidCounter:
      return "The value of the Ctr argument is out of range";
      break;
    case ErrorInvalidCounterState:
      return "The value of th state argument is out of range";
      break;
    case ErrorInvalidBinBcdParam:
      return "The value of BinBcd argument is invalid";
      break;
    case ErrorBadCardType:
      return "The value of Card Type argument is invalid";
      break;
    case ErrorInvalidDaRefVoltage:
      return "The value of DA reference voltage argument is invalid ";
      break;
    case ErrorAdTimeOut:
      return "Time out for AD operation";
      break;
    case ErrorNoAsyncAI:
      return "Continuous analog input is not set as Asynchronous mode";
      break;
    case ErrorNoAsyncAO:
      return "Continuous analog output is not set as Asynchronous mode";
      break;
    case ErrorNoAsyncDI:
      return "Continuous digital input is not set as Asynchronous mode";
      break;
    case ErrorNoAsyncDO:
      return "Continuous didgtal output is not set as Asynchronous mode";
      break;
    case ErrorNotInputPort:
      return "The value of AI/DI port argument is invalid";
      break;
    case ErrorNotOutputPort:
      return "The value of AO/DO argument is invalid";
      break;
    case ErrorInvalidDioPort:
      return "The value of DI/O port argument is invalid";
      break;
    case ErrorInvalidDioLine:
      return "The value of DI/O line argument is invalid";
      break;
    case ErrorContIoActive:
      return "Continuous IO operation is not active";
      break;
    case ErrorDblBufModeNotAllowed:
      return "Double buffer mode is not allowed";
      break;
    case ErrorConfigFailed:
      return "The specified function configuration has failed";
      break;
    case ErrorInvalidPortDirection:
      return "The value of DIO port direction argument is invalid";
      break;
    case ErrorBeginThreadError:
      return "Failed to create thread";
      break;
    case ErrorInvalidPortWidth:
      return "The port width setting is invalid";
      break;
    case ErrorInvalidCtrSource:
      return "The clock source setting is invalid";
      break;
    case ErrorOpenFile:
      return "Failed to open file";
      break;
    case ErrorAllocateMemory:
      return "The memory allocation has failed";
      break;
    case ErrorDaVoltageOutOfRange:
      return "The value od DA voltage is out of range";
      break;

#ifdef ErrorInvalidSyncMode
	case ErrorInvalidSyncMode:
		return "ErrorInvalidSyncMode  or  ErrorLockMemory";
	case ErrorInvalidBufferID:
		return "ErrorInvalidBufferID";
	case ErrorInvalidCNTInterval:
		return "ErrorInvalidCNTInterval";
	case ErrorReTrigModeNotAllowed:
		return "ErrorReTrigModeNotAllowed";
	case ErrorResetBufferNotAllowed:
		return "ErrorResetBufferNotAllowed";
	case ErrorAnaTrggerLevel:
		return "ErrorAnaTrggerLevel";
	// jeffrey add for specific usage
	case ErrorSignalSetup:
		return "ErrorSignalSetup";
//	case ErrorLockMemory:  // see ErrorInvalidSyncMode
//		return "ErrorLockMemory";
	case ErrorInvalidMemBank:
		return "ErrorInvalidMemBank";
	case ErrorInvalidCALSource:
		return "ErrorInvalidCALSource";
	case ErrorInvalidDAPolar:
		return "ErrorInvalidDAPolar";
#endif //ErrorInvalidSyncMode

#ifdef ErrorConfigIoctl
   case ErrorConfigIoctl:
     return "The configuration API has failed";
     break;
   case ErrorAsyncSetIoctl:
     return "The async mode API has failed";
     break;
    case ErrorDBSetIoctl:
      return "The double buffer setting has failed";
      break;
    case ErrorDBHalfReadyIoctl:
      return "The half-ready API has failed";
      break;
    case ErrorContOPIoctl:
      return "The continuous data acquisition API has failed";
      break;
    case ErrorContStatusIoctl:
      return "The continuous data API data acquisition status API has failed";
      break;
    case ErrorPIOIoctl:
      return "The polling data API has failed";
      break;
    case ErrorDIntSetIoctl:
      return "The dual  interrupt setting API has failed";
      break;
    case ErrorWaitEvtIoctl:
      return "The wait event API is failed";
      break;
    case ErrorOpenEvtIoctl:
      return "The open event API has failed";
      break;
    case ErrorCOSIntSetIoctl:
      return "The cos interrupt setting API has failed";
      break;
    case ErrorMemMapIoctl:
      return "The memory mapping API has failed";
      break;
    case ErrorMemUMapSetIoctl:
      return "The memory unmapping API has failed";
      break;
    case ErrorCTRIoctl:
      return "The counter API has failed";
      break;
#endif // ErrorConfigIoctl
    case -125:
      // This is not defined in adlink userspace headers, but it is very
      // common... It is referred in driver code as DAS_ERR_BUILD_DMA_DSCR
      // in both d2k and pci drivers.
      return "Problems with DMA. Check A*_PAGE variables when loading the"
             " kernel module, they can be the root of the problem if too low.";
      break;
    default:
      return "Unknown error";
      break;

  }
  return "ErrorUnknown";
}
