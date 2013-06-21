
#ifndef __ADLINK_COMMON___H___sdDFKDS3jefkdsEFW____
#define __ADLINK_COMMON___H___sdDFKDS3jefkdsEFW____

#include "AdlDeviceFactorySingleton.h"
#include "AdlinkBase.h"

#include <d2kdask.h>
#include <dask.h>
#include <omnithread.h>

class BaseBehaviour;

enum AdlinkDelaySource
{
	DelayTimebase = 0,
	DelaySamples,
};

/**
 *
 *  An Adlink D2K-DASK Device can be divided in various subdevices. We do
 *  consider InputDeviceInterface, OutputDeviceInterface (Analog) and
 *  DigitalDeviceInterface. Also there's CommonDeviceInterface.
 *
 */
class DeviceInterface : public AdlinkBase
{
public:
	typedef AdlDeviceFactorySingleton_::ADFMode ADFMode;
	typedef AdlDeviceFactorySingleton_::AdlBoardParams AdlBoardParams;

private:
	/// m_typeParams defines the default properties of
	/// all the boards of a given type. So it's also defining
	/// the type of the board.
	const AdlBoardParams* m_typeParams;

	/// Unique id for each board of the same type in the rack
	int	m_boardId;

	/// @brief Adlink API Handler: Current board.
	/// With the m_boardId and m_typeParamswe can identify a board.
	/// But to identify the same board in the Adlink API we need
	/// another identifyier.
	I16	m_boardHandler;

protected:
	DeviceInterface(const AdlBoardParams* params, int boardId, I16 adlPtr);

	virtual ~DeviceInterface();

public:
	/// @name Accessors
	/// @{
	const AdlBoardParams * type_params() const
	{	return this->m_typeParams;	}
	int board_id() const
	{	return this->m_boardId;	}
	I16 handler() const
	{	return this->m_boardHandler; }
	/// @}
};


class CommonDeviceInterface;
class DevicePartInterface;

/**
 *
 *  An Adlink D2K-DASK Device can be divided in various subdevices. We do
 *  consider InputDeviceInterface, OutputDeviceInterface (Analog) and
 *  DigitalDeviceInterface. But there's some common functionality, and if
 *  the Input device configures it somehow, we don't want the Output device
 *  to overwrite it!
 *
 */
class CommonDeviceInterface : public DeviceInterface
{
	friend class AdlDeviceFactorySingleton;

protected:
	/// Constructor and destructor are private, as objects of this class
	/// should be created using AdlDeviceFactorySingleton
	CommonDeviceInterface(const AdlBoardParams* params, int boardId, I16 adlPtr);
	~CommonDeviceInterface();

public:
	/// @name Shared functionalities
	/// Functionalities shared between different tango devices- but the
	/// same physical one- related methods.
	/// The CommonDeviceInterface implementation should check
	/// which common functionalities will need the base behaviour
	/// as it is configured now and then try to lock them for itself so
	/// that no other device is able to change them.
	/// @{
		virtual void request_shared_control(BaseBehaviour* dev) {}
		virtual void leave_shared_control(BaseBehaviour* dev) {}
		virtual I16 configure_shared_resources(BaseBehaviour* dev) { return 0;}
	/// @}
		
	virtual void perform_auto_calibration() {}
	virtual void save_calibration_settings(unsigned long bank)
	{ throw_error("UNSUPPORTED",
				  "It is not supported by this board",
				  "CommonDeviceInterface::save_calibration_settings"); }
	virtual void load_calibration_settings(unsigned long bank)
	{ throw_error("UNSUPPORTED",
				  "It is not supported by this board",
				  "CommonDeviceInterface::load_calibration_settings"); }
};


/**
 *
 *  An Adlink D2K-DASK Device can be divided in various subdevices. We do
 *  consider InputDeviceInterface, OutputDeviceInterface (Analog) and
 *  DigitalDeviceInterface. Each of these parts implements an API to
 *  configure the device part related to him. There's also a
 *  CommonDeviceInterface for configuration options that are
 *  transversal to various Parts.
 *  
 *  DevicePartInterface is the class from where InputDeviceInterface,
 *  OutputDeviceInterface and DigitalDeviceInterface inherit.
 *  
 */
class DevicePartInterface : public DeviceInterface
{
	CommonDeviceInterface* m_commonDevice;
public:
	DevicePartInterface(CommonDeviceInterface* commonDevice) :
		DeviceInterface(commonDevice->type_params(), commonDevice->board_id(), commonDevice->handler()),
		m_commonDevice(commonDevice)
	{
	}
	CommonDeviceInterface* common_device() { return this->m_commonDevice; }
	const CommonDeviceInterface* common_device() const { return this->m_commonDevice; }
};



///
/// We are not really expecting any special interface from
/// DigitalIODeviceInterface other than function handler(), because it uses
/// the Adlink APIs directly.
/// 
/// This class is defined just to make clear the purpose of DevicePartInterface,
/// and get a coherent picture with InputDeviceInterface and
/// OutputDeviceInterface.
///
class DigitalIODeviceInterface : public DevicePartInterface
{
	public:
	DigitalIODeviceInterface(CommonDeviceInterface* commonDevice) :
		DevicePartInterface(commonDevice)
	{}
};


/**
 *
 *  An Adlink D2K-DASK Device can be divided in various subdevices. We do
 *  consider InputDeviceInterface, OutputDeviceInterface (Analog) and
 *  DigitalDeviceInterface. But there's some common functionality, and if
 *  the Input device configures it somehow, we don't want the Output device
 *  to overwrite it!
 *
 *  Currently the common functionality is the Analog Trigger configuration.
 *
 *  A device must request_analog_trigger_control() to be able to
 *  configure_analog_trigger(). When the Analog trigger settings are not
 *  important anymore for the device, it must leave_analog_trigger_control()
 *  so other devices can use it.
 *
 */
class CommonDeviceD2K : public CommonDeviceInterface
{
	friend class AdlDeviceFactorySingleton;

	/// Only one device should be able to touch common settings.
	/// This device is stored here.
	/// @todo It makes MUCH more sense to use DevicePartInterface instead of BaseBehaviour here...
	BaseBehaviour* m_owner;

	/// Different devices may be running on different threads, so we use
	/// a mutex to access m_owner.
	omni_mutex m_mutex;

protected:
	CommonDeviceD2K(const AdlBoardParams* params, int boardId, I16 adlPtr);

public:

	virtual void request_shared_control(BaseBehaviour* dev);
	virtual void leave_shared_control(BaseBehaviour* dev);
	virtual I16 configure_shared_resources(BaseBehaviour* dev);
	
	/// @name Analog trigger methods
	/// @{
		bool request_analog_trigger_control(BaseBehaviour* dev);
		void leave_analog_trigger_control(BaseBehaviour* dev);
		bool has_analog_trigger_control(BaseBehaviour* dev);
		I16 configure_analog_trigger(BaseBehaviour* dev);
	/// @}
		
	virtual void perform_auto_calibration();
	virtual void load_calibration_settings(unsigned long bank);
	virtual void save_calibration_settings(unsigned long bank);
};



class CommonDevicePSDASK : public CommonDeviceInterface
{
	friend class AdlDeviceFactorySingleton;

protected:
	CommonDevicePSDASK(const AdlBoardParams* params, int boardId, I16 adlPtr);
};


#endif //__ADLINK_COMMON___H___sdDFKDS3jefkdsEFW____
