
#ifndef _INPUT_DEVICE_INTERFACE_H__dj3JEF__
#define _INPUT_DEVICE_INTERFACE_H__dj3JEF__

#include <set>
#include "AdlinkBase.h"
#include "AdlinkCommon.h"

namespace InputDeviceInterface_ {
	/// These are the callback types supported. Using 
	/// InputDeviceInterface::set_callback() you can set
	/// a different callback receiver object for each of 
	/// these types if they are supported by the Input device.
	enum CallbackEvent {
		EventCompleted = 0,
		EventHalfBufferReady,
		EventTrigger
	};
	
	/// Callback receiver object to be used with
	/// InputDeviceInterface::set_callback(). It's an object, not
	/// a single method that must be derived. In fact it has two
	/// callbacks: on_event_received and on_event_disconnected
	class CallbackEventReceiver {
	public:
		virtual void on_event_received(CallbackEvent ev) = 0;
		virtual void on_event_disconnected(CallbackEvent ev) {};
	};
}

///
/// Defines a virtual API to configure the Analog Input part of an
/// acquisition device.
/// 
/// Now it has two known implementations: InputDeviceD2K and
/// InputDevicePSDASK for the two different Adlink APIs available.
///
class InputDeviceInterface : public DevicePartInterface
{
	U16 m_numChannels;
protected:

	InputDeviceInterface(CommonDeviceInterface* commonDevice) :
		DevicePartInterface(commonDevice),
		m_numChannels(0)
	{
	}

	inline bool __adlink_check(I16 r, const char* fnCall, const char* fn) const
	{
		if (r != NoError) {
			status_warn()	<< "+ " << fnCall << " in " << fn
							<< " FAILED with code " << r << ": "
							<< pcisdask_error_text(r) << std::endl
							<< "----------" << std::endl;
			return false;
		}
		return true;
	}

	inline void __adlink_assert(I16 r, const char* fnCall, const char* fn) const
	{
		if (r != NoError) {
			std::stringstream ss;
			ss	<< "+ " << fnCall << " in " << fn
				<< " FAILED with code " << r << ": "
				<< pcisdask_error_text(r) << std::endl;
			// status_warn() << ss.str() << std::endl; NO need to do so, it's
			// done by throw_error itself
			throw_error("Adlink ERROR", ss.str(), fn);
		}
	}

	#define adlink_check(fn) __adlink_check(fn, #fn, __PRETTY_FUNCTION__)
	#define adlink_assert(fn) __adlink_assert(fn, #fn, __PRETTY_FUNCTION__)
public:

	virtual ~InputDeviceInterface() { }

	inline U16 nchannels() const
	{	return this->m_numChannels;	}

	inline void set_nchannels(U16 nch)
	{	this->m_numChannels = nch; }
	
	virtual void release_buffers() = 0;
	virtual void read_channels_shot(U16 *values) = 0;
	// AI_DIFF, AI_RSE, AI_NRSE
	virtual void set_reference_ground(const std::string &rg) = 0;
	virtual void preconfigure_input_channels(U16 range) = 0; /// @todo merge with something else...
	// totalShots!=0 continusousShot...
	virtual void set_delay(unsigned delay, AdlinkDelaySource dm) = 0;
	virtual void configure_ai(long totalShots, U32 mode) = 0;
	virtual void clear_overrun_flag() = 0;
	virtual void configure_single_buffer(U32 hsize) = 0;
	virtual bool cont_scan_channels(U32 scanIntrv) = 0;
	virtual void async_clear(U32* startPos, U32* count) = 0;
	virtual void dbl_buf_half_ready(BOOLEAN *halfReady, BOOLEAN *finished) = 0;
	virtual void transfer_from_buf(U16* rawData, U32 first, U32 count, U16 halfId) const = 0;
	virtual const U16* get_buf(U16 halfId) const = 0;
	virtual void last_buffer_handled() = 0;
	virtual bool has_overrun() = 0;
	virtual void configure_double_buffer(U32 hsize) = 0;
	virtual double bits_to_volts(U16 range, U16 bits) const = 0;
	virtual bool bits_to_volts(U16 range, const U16* bits, double * volts, size_t count) const = 0;
	virtual bool bits_to_volts(U16 range, const U16* bits, float * volts, size_t count) const = 0;

	virtual bool async_check_finished(U32 & accesCnt) = 0;
	virtual bool async_retrig_next_ready(bool & _finished, U16 &rdyTrigCnt) = 0;

	typedef InputDeviceInterface_::CallbackEvent CallbackEvent;
	typedef InputDeviceInterface_::CallbackEventReceiver CallbackEventReceiver;

	/// Install an object that will get callback signals for an event. It
	/// can be used only once, this is one event can only be mapped to
	/// one receiver. To change the receiver, you will have to explicitly
	/// uninstall the previous one first.
	/// @param ev Callback event type
	/// @param rec Callback object. Use 0 to uninstall a callback object
	virtual void set_callback(CallbackEvent ev, CallbackEventReceiver* rec) = 0;

	/// Check if the concrete device supports a callback event type
	virtual bool supports_callback(CallbackEvent event) const = 0;
	
	virtual void check_valid_buffer_size(U32 candidateBufferSize) const = 0;

	/// @see supports_callback(), It just works on a set of eventTypes.
	bool supports_callbacks(const std::set<CallbackEvent> & events) const
	{
		bool r = true;
		std::set<CallbackEvent>::const_iterator i, e;
		for (i = events.begin(), e = events.end(); i != e; ++i) {
			if (! this->supports_callback(*i))
				return false;
		}
		return true;
	}

	/// Disconnect all callbacks.
	/// @param waitEnd If it is true, after all the callbacks have been
	/// disconencted it will wait until the last one has finished running.
	/// @warning Don't call this function with waitEnd=true from a signal
	/// callback or it will wait for itself to finish forever!!
	virtual void stop_callbacks(bool waitEnd=false) {};
};

#endif //_INPUT_DEVICE_INTERFACE_H__dj3JEF__
