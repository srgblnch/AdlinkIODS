
#ifndef __ADLINK_IO_DEVICE___H___sdfsgrgry7678dsE78____
#define __ADLINK_IO_DEVICE___H___sdfsgrgry7678dsE78____

#include "AdlinkBase.h"
#include "Utils.h"
#include "AdlDeviceFactorySingleton.h"
#include "AdlinkCommon.h"

#include "tango.h"
#include <iostream>
#include <sys/time.h>

class CommonDeviceInterface;

class BaseBehaviour : public AdlinkBase
{
///////////////////////////////////////////////////////////////////////////////
//Public structures
///////////////////////////////////////////////////////////////////////////////
public:
	/// The state of the device
	enum OperationState
	{
			StateFault = -1,
			StateNone,				/// The device is uninitalized
			StateRegistered,		/// Device has been registered
			StateReady,				/// Device is registered and started, ready to
									/// perform an adquisition
			StateRunning,			/// Device is acquiring data
	};

	struct StatusObserver
	{
		virtual void on_change_status(const std::string & st) = 0;
	};
	
	typedef double volts_type;

///////////////////////////////////////////////////////////////////////////////
// Protected structures
///////////////////////////////////////////////////////////////////////////////
protected:
	/// Some abstract functions mimic the AnalogIODS interface.
	/// They all return a pointer to some data which has to be
	/// stored internally in BaseBehaviour. All that data
	/// is grouped here.
	struct AttributeBuffers{
		std::vector< std::vector <volts_type> > spectrumChannelValues;
		std::vector< volts_type > scalarChannelValues;

		std::vector< volts_type > spectrumLastValues;

		std::vector< volts_type > doubleDataValues;
		std::vector< long > rawDataValues;

		std::string triggerSourceString;
	};

///////////////////////////////////////////////////////////////////////////////
// Private structures
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Private data
///////////////////////////////////////////////////////////////////////////////
private:
	/// Current state of the device
	OperationState m_state;

	StatusObserver* m_statusObserver;

	/// That's the parameters the code will look at. It should
	/// be a copy of type params with possibly some values overwritten.
	const AdlDeviceFactorySingleton::AdlBoardParams* m_boardParams;

	/// Pointer to the object implementing the right interface for this m_mode.
	/// Part interfaces can handle Analog Input, Analog Output or DigitalIO
	/// parts of the device.
	DevicePartInterface * m_device;

	/// Different implementations of BaseBehaviour are to control different
	/// values of m_mode.
	/// @todo I don't really like it much...
	AdlDeviceFactorySingleton::ADFMode m_mode;

	TriggerMode m_triggerMode;
	RangeChecker m_rangeChecker;

	/// Total channels
	U16 	m_numChannels;

	/// Number of shots to be taken. -1 means infinite.
	int	m_totalShots;

	/// Size of the buffer per channel
	long	m_chSamplesPerTrigger;

	/// Sample rate of operation
	long m_sampleRate;

	unsigned m_delay;
	AdlinkDelaySource m_delaySource;
	
	int m_infiniteRetrigger;

///////////////////////////////////////////////////////////////////////////////
// Protected data
///////////////////////////////////////////////////////////////////////////////
protected:

	/// See struct AttributeBuffers. It is not considered part
	/// of the state of the object as the contents of this structure
	/// only have a meaning outside the object, but we have to set
	/// and keep the data, that's why it's marked as mutable.
	mutable AttributeBuffers m_attrBuffers;

///////////////////////////////////////////////////////////////////////////////
// Protected API
///////////////////////////////////////////////////////////////////////////////
protected:

	inline void set_state(OperationState st)
	{	m_state = st;	}

	/// Why it's protected? Because from the public API you should use
	/// adapt_sample_rate() instead.
	inline void set_sample_rate(long sr)
	{	m_sampleRate = sr;	}
	
	///Allows to do the reallocation of the different buffers.
	void update_user_buffer_size(long chSamples) throw (std::bad_alloc);

///////////////////////////////////////////////////////////////////////////////
// Public API
///////////////////////////////////////////////////////////////////////////////
public:

	BaseBehaviour(
		AdlDeviceFactorySingleton::AdlBoardParams* p,
		AdlDeviceFactorySingleton::ADFMode mode,
		const std::string & _boardId,
		StatusObserver* ob);

	virtual ~BaseBehaviour();

	/// @name Accessors
	/// @{
		inline const AdlDeviceFactorySingleton::AdlBoardParams* params() const
		{	return this->m_boardParams;	}
		inline const bool is_d2k() const
		{	return this->m_boardParams->isD2K;	}
		inline TriggerMode & trigger_mode()
		{	return this->m_triggerMode;	}
		inline const TriggerMode & trigger_mode() const
		{	return this->m_triggerMode;	}
		inline RangeChecker & range()
		{	return this->m_rangeChecker;	}
		inline const RangeChecker & range() const
		{	return this->m_rangeChecker;	}
		inline DevicePartInterface & device()
		{	return *this->m_device;	}
		inline const DevicePartInterface & device() const
		{	return *this->m_device;	}
		inline DevicePartInterface * device_ptr()
		{	return this->m_device;	}
		inline const DevicePartInterface * device_ptr() const
		{	return this->m_device;	}
		inline U16 nchannels() const
		{	return this->m_numChannels;	}
		inline long num_of_triggers() const
		{	return this->m_totalShots; }
		inline long channel_samples_per_trigger() const
		{	return this->m_chSamplesPerTrigger; }
		inline long sample_rate() const
		{	return m_sampleRate;	}
		inline OperationState get_state() const
		{	return m_state;	}
		std::string get_state_name() const;
		inline unsigned delay() const
		{	return this->m_delay; }
		inline AdlinkDelaySource delay_source() const
		{	return this->m_delaySource; }
		/// An Adlink board can be used by several devices. Some of
		/// it's functionallity can be shared. This common device
		/// centralizes shared functionalities so only one device
		/// can use them at any time.
		inline CommonDeviceInterface * common_device()
		{	return this->device().common_device(); }
		/// @see common_device()
		inline const CommonDeviceInterface * common_device() const
		{	return this->device().common_device(); }
		inline int infinite_retrigger() const
		{	return this->m_infiniteRetrigger; }
	/// @}

	//RH
	inline int get_total_shots() const
	{ return m_totalShots; }

	inline void set_delay(unsigned d)
	{	this->m_delay = d; }

	inline void set_delay_source(AdlinkDelaySource dm)
	{	this->m_delaySource = dm; }

	void set_status_observer(StatusObserver* ob)
	{	this->m_statusObserver = ob; }
	
	inline void set_infinite_retrigger(int ir)
	{
		if (ir != 0 && ir != 1 && ir != 2)
			throw_error("BAD_PARAM", "TriggerInfinite must be :\n\t0: No retrigger\n\t1: 1 event-> retrigger forever\n\t2: Each event a retrigger.", "BaseBehaviour::set_infinite_retrigger");
		this->m_infiniteRetrigger = ir;
	}

	void register_device();
	void unregister_device();

	void set_num_of_triggers(long totalShots);
	void set_channel_samples_per_trigger(long chBufSize);

	virtual void set_reference_ground(const std::string &referenceGround)
	{}

	void apply_range();

	/// That's from where update_status() gets the information. An
	/// implementation of BaseBehaviour can overwrite this method
	/// to change the information showed. It should call the parent
	/// get_status_info() so to add information instead of changing
	/// it.
	/// @see update_status()
	virtual void get_status_info(std::ostream & statusStream);

	/// This method rewrites the string in Status method. It prints the most
	/// important variable values and the last exception reported from
	/// Hardware.
	/// @see get_status_info()
	void update_status();

	virtual bool is_input() const = 0;
	
	virtual void on_channel_samples_per_trigger_about_to_change(long chspt) = 0;
	
	void perform_auto_calibration();
	void load_calibration_settings(unsigned long bank);
	void save_calibration_settings(unsigned long bank);

/// @name Tango AnalogIODS API
/// These methods try to mimic the most important parts of the AnalogIODS
/// tango abstract class API. They are mostly abstract methods to be
/// implemented by the concrete BaseBehaviour.
/// @{

	virtual void update_state_fast() = 0;

	virtual bool read_raw_data(long *& rawData, long &x, long &y, struct timeval& time) const = 0;
	virtual bool read_double_data(volts_type *& rawData, long &x, long &y, struct timeval& time) const = 0;
	virtual bool read_scalar_value(ssize_t port, volts_type* &value, struct timeval& time) const = 0;
	virtual bool read_last_values(volts_type* &data, long &x, struct timeval& time) const = 0;
	virtual bool read_channel_spectrum(ssize_t port, volts_type* &data, long &x, struct timeval& time) const = 0;

	/// Analog Input specific:
	/// Sometimes it is usefull if instead of last value we are able
	/// to read the mean of last buffer. This will make any call requesting
	/// for last value of a buffer return the last mean instead.
	virtual void set_last_is_mean(bool lim) {};
	virtual bool last_is_mean() const { return true; };

	virtual bool write_scalar_value(ssize_t port, volts_type value) = 0;
	virtual bool write_last_values(const volts_type* data, long x) = 0;
	virtual bool write_channel_spectrum(ssize_t port, const volts_type* data, long x) = 0;

	virtual void start() = 0;
	virtual void stop() = 0;

	virtual unsigned volts2bits(double volts) const = 0;
	virtual double bits2volts(unsigned bits) const = 0;

	virtual void adapt_sample_rate(unsigned long sr) = 0;

	void import_stream(std::istream & is);
	void export_stream(std::ostream & os) const;
/// @}
};

#endif //__ADLINK_IO_DEVICE___H___sdfsgrgry7678dsE78____
