
#include "AdlinkCommon.h"
#include "BaseBehaviour.h"
#include "AdlHelp.h"
#include "AdlDeviceFactorySingleton.h"
#include <sched.h>
#include <omnithread.h>
#include <sstream>
#include <cassert>
#include <cmath>

#include "odebug.h"

DeviceInterface::DeviceInterface(
		const AdlDeviceFactorySingleton::AdlBoardParams* params,
		int boardId,
		I16 adlPtr) :
	m_typeParams(params),
	m_boardId(boardId),
	m_boardHandler(adlPtr)
{
	assert(this->m_typeParams);
	assert(this->m_boardHandler >= 0);
	assert(this->m_boardId >= 0);
}

/*virtual*/ DeviceInterface::~DeviceInterface() 
{
}


CommonDeviceInterface::CommonDeviceInterface(const AdlDeviceFactorySingleton::AdlBoardParams* params, int boardId, I16 adlPtr) :
	DeviceInterface(params, boardId, adlPtr)
{
}

CommonDeviceInterface::~CommonDeviceInterface()
{
}

CommonDeviceD2K::CommonDeviceD2K(const AdlBoardParams* params, int boardId, I16 adlPtr) :
	m_owner(0),
	CommonDeviceInterface(params, boardId, adlPtr)
{
}


/*virtual*/ I16 CommonDeviceD2K::configure_shared_resources(BaseBehaviour* bb)
{
	if (bb->trigger_mode().is_analog())
			return this->configure_analog_trigger(bb);
	return 0; // If we don't use it, don't even try to set it! 0 means OK
}


/*virtual*/ void CommonDeviceD2K::leave_shared_control(BaseBehaviour* bb)
{
	// Actually just analog trigger control, but who knows if
	// some future version will have to grant acces to different
	// kind of functionalities
	if (bb->trigger_mode().is_analog())
		this->leave_analog_trigger_control(bb);
}


void CommonDeviceD2K::request_shared_control(BaseBehaviour* bb)
{
	// see alse CommonDeviceD2K::leave_shared_control
	// Request only if we need to use Analog trigger source
	if (bb->trigger_mode().is_analog())
		if (!this->request_analog_trigger_control(bb))
				this->throw_error( 	"Can't lock shared control",
									"An Adlink board can be used by various"
									" tango devices, offering different"
									" functionallity (like AI and AO). But some"
									" settings are shared (like Analog Trigger"
									" configuration) and so if a device is using"
									" this settings, another device cannot set"
									" them until the first one has finished.",
									_SLOW_FUNCTION_NAME_ );
}

bool CommonDeviceD2K::request_analog_trigger_control(BaseBehaviour* dev)
{
	omni_mutex_lock lock(this->m_mutex);
	if (dev && dev == this->m_owner)
		return true;
	if (this->m_owner)
		return false;
	this->m_owner = dev;
	return true;
}

void CommonDeviceD2K::leave_analog_trigger_control(BaseBehaviour* dev)
{
	omni_mutex_lock lock(this->m_mutex);
	if (!this->m_owner || (dev != this->m_owner) ) {
		error()	<< __FUNCTION__ << ": "
			<< "The device does not own AnalogTrigger rights"
			<< std::endl;
		return;
	}
	this->m_owner = 0;
}

bool CommonDeviceD2K::has_analog_trigger_control(BaseBehaviour* dev)
{
	omni_mutex_lock lock(this->m_mutex);
	return (dev && dev == this->m_owner);
}

I16 CommonDeviceD2K::configure_analog_trigger(BaseBehaviour* dev)
{
	/// @todo support external time bases...
	/// It is only necessary if we use an external timebase or 
	/// Analog TriggerSource
	
	omni_mutex_lock lock(this->m_mutex);
	if (!this->m_owner || (dev != this->m_owner) ) {
		error()	<< __FUNCTION__ << ": "
			<< "The device does not own AnalogTrigger rights"
			<< std::endl;
		return -1;
	}

	assert(dev->trigger_mode().is_analog());

	U16 anaTrigAIOConf = dev->trigger_mode().analog_conf();

	double min, max;
	dev->range().get_min_max_range(min, max);

	/// @todo After the old analog trigger source strings have been deprecated
	/// long enough, deleting it's support will mean also do the volts_to_255()
	/// transformation here, not in trigger mode!
	U16 anaTrigHigh = dev->trigger_mode().analog_high(min, max);
	U16 anaTrigLow = dev->trigger_mode().analog_low(min, max);

	//U16 anaTrigHigh = volts_to_255(dev->trigger_mode().analog_high(), min, max);
	//U16 anaTrigLow = volts_to_255(dev->trigger_mode().analog_low(), min, max);
	
	return D2K_AIO_Config(
							this->handler(),
							DAQ2K_IntTimeBase,
							anaTrigAIOConf,
							anaTrigHigh,
							anaTrigLow );
}

void CommonDeviceD2K::perform_auto_calibration()
{
	I16 e = D2K_DB_Auto_Calibration_ALL(this->handler());
	if (e != NoError) {
		this->throw_error(
				"Adlink ERROR",
				pcisdask_error_text(e),
				_SLOW_FUNCTION_NAME_);
	}
}

void CommonDeviceD2K::load_calibration_settings(unsigned long bank)
{
	if (bank < 0 || bank > 3)
		this->throw_error(
				"BAD_RANGE",
				"The bank should be in range 0 to 3.",
				_SLOW_FUNCTION_NAME_ );
	I16 e = D2K_Load_CAL_Data(this->handler(), bank);
	if (e != NoError) {
		this->throw_error(
				"Adlink ERROR",
				pcisdask_error_text(e),
				_SLOW_FUNCTION_NAME_);
	}
}

void CommonDeviceD2K::save_calibration_settings(unsigned long bank)
{
	if (bank < 0 || bank > 3)
		this->throw_error(
				"BAD_RANGE",
				"The bank should be in range 0 to 3.",
				_SLOW_FUNCTION_NAME_ );
	I16 e = D2K_EEPROM_CAL_Constant_Update(this->handler(), bank);
	if (e != NoError) {
		this->throw_error(
				"Adlink ERROR",
				pcisdask_error_text(e),
				_SLOW_FUNCTION_NAME_);
	}
}

CommonDevicePSDASK::CommonDevicePSDASK(const AdlBoardParams* params, int boardId, I16 adlPtr) :
	CommonDeviceInterface(params, boardId, adlPtr)
{
}
