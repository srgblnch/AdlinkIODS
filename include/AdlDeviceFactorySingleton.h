
#ifndef __ADLINK_DEVICE_MANAGER__H___DSFJ9feudjfejr32SDd___
#define __ADLINK_DEVICE_MANAGER__H___DSFJ9feudjfejr32SDd___


#include <d2kdask.h>
#include <dask.h>
#include <omnithread.h>
#include <map>
#include <set>
#include <vector>
#include <string>

///
/// Implements a singleton factory of adlink devices.
/// D2K_Register_Card fails if called more than once for the same board
/// without releasing previously. So what if we want to tango devices
/// to get acces to the same physical device? In this case, a tango device
/// accessing analog ports an another on digital ones? Both can't
/// tegister the card! That's why we have this factory. It's the one that
/// registers or deregisters cards, depending on how many tango devices
/// are using them.
///
/// Alse defined here funciton get_board_params() defines the properties
/// of a device according to the board class.
///

class CommonDeviceInterface;
class DevicePartInterface;

namespace AdlDeviceFactorySingleton_
{
	enum ADFMode {
		ModeAnalogInput = 0,
		ModeAnalogOutput,
		ModeDigital,
		ModeCounter,
		ADFMODE_TOTAL
	};
	
	struct AdlBoardParams {
		char				typeName[32];
		/// Device type, as defined in adlink API functions Register_Card or D2K_Register_Card
		/// depending on adl_params->isD2K
		I16 				type; //adl_params->type
		/// Does the device use D2K-DASK API or PCI-DASK
		bool				isD2K; // adl_params->isD2K
		/// For each operation mode how many available slots
		/// does this kind of board have? For example, it usually
		/// will have 0 or 1 analog input slots and D2K use to have
		/// 2 counters.
		unsigned modeSlots[ADFMODE_TOTAL];
		/// Number of analog channels the device has
		__attribute__((__deprecated__)) unsigned			totalChannels; ///@todo DEPRECATED EXTERMINAR!
		unsigned			aiChannels;
		unsigned			aoChannels;
		/// Internal timebase
		U32 				timeBase; //adl_params->timeBase
		U32				minInputScanIntrv;
		U32				maxInputScanIntrv;
	
		/// @todo Some boards have FIFO shared between AI or AO, some have
		/// different sizes for each... handle this!!
		long 				onBoardBufferSz; //adl_params->onBoardBufferSz
	
		/// @todo what about available voltage ranges?
		/// @todo what about total digital channels?
		
		short ai_nBits; // IGNORED, now AnalogInput uses PS/D2K-DASK own methods
						// to translate from bits to volts
		short ao_nBits;
	};
}

class AdlDeviceFactorySingleton
{
public:
	typedef AdlDeviceFactorySingleton_::AdlBoardParams AdlBoardParams;
	typedef AdlDeviceFactorySingleton_::ADFMode ADFMode;

private:
	AdlDeviceFactorySingleton();

	typedef std::pair<const AdlBoardParams *, U16> CardTypeNumPair;
	typedef std::multiset<ADFMode> OpenCardModesSet;
	typedef std::pair<I16, OpenCardModesSet> OpenCardMode;
	typedef std::map<CardTypeNumPair, OpenCardMode> CardMap;
	typedef std::map<CardTypeNumPair, CommonDeviceInterface*> CommonMap;

	static CardMap s_cardMap;
	static CommonMap s_commonMap;

	static omni_mutex s_mutex;

	static bool _check_single(const AdlBoardParams* params, U16 cardNum);
	static I16 __get_common(CommonDeviceInterface** dev, const AdlBoardParams* params, U16 cardNum, ADFMode mode);
	static bool __release_common(CommonDeviceInterface** _dev, ADFMode mode);

	static DevicePartInterface* create_input_device_psdask(CommonDeviceInterface* commonDevice);
	static DevicePartInterface* create_input_device_d2k(CommonDeviceInterface* commonDevice);

	static DevicePartInterface* create_output_device_psdask(CommonDeviceInterface* commonDevice);
	static DevicePartInterface* create_output_device_d2k(CommonDeviceInterface* commonDevice);
	static DevicePartInterface* create_digital_device(CommonDeviceInterface* commonDevice);
	static DevicePartInterface* create_counter_device(CommonDeviceInterface* commonDevice);
public:	
	static const AdlBoardParams* get_board_params(const std::string & boardType);
	static void get_supported_boards(std::vector<std::string> &boards);


	static I16 get(DevicePartInterface** dev, const std::string & boardType, U16 cardNum, ADFMode mode);
	static bool release(DevicePartInterface** dev, ADFMode mode);
};

#endif //__ADLINK_DEVICE_MANAGER__H___DSFJ9feudjfejr32SDd___
