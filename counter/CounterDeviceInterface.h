
#ifndef __d_COUNTER_DEVICE_INTERFACE__H__dkEK29__
#define __d_COUNTER_DEVICE_INTERFACE__H__dkEK29__

#include "AdlinkCommon.h"
#include "AdlHelp.h"

/**
 *
 * This is the DevicePartInterface implementation for a counter.
 * Each CounterDeviceInterface represents a counter.
 * we only support D2K API right now. This is mainly a wrapper
 * around the D2K API.
 *
 */
class CounterDeviceInterface : public DevicePartInterface
{
	unsigned m_counterID;
public:

	CounterDeviceInterface(CommonDeviceInterface* commonDevice) :
		DevicePartInterface(commonDevice),
		m_mode(SimpleGatedEventCNT),
		m_sourceTimeBaseExternal(0),
		m_sourceGateExternal(0),
		m_sourceUpDownExternal(0),
		m_polarityLowGate(0),
		m_polarityLowUpDown(0),
		m_polarityLowClockEn(0),
		m_polarityLowOutput(0),
		m_initialCount(0),
		m_pulseDelay(0),
		m_pulseWidth(0)
	{}

	/// @name Counter interface
	/// This is the basic interface added by CounterDeviceInterface
	/// @{

		/// This is very important, should be called soon! Sets the
		/// counterID!
		void set_counter_id(unsigned id)
		{ this->m_counterID = id; }

		/// Read the current counter value
		void read(unsigned long &value);

		/// Configure the counter as specified by the Counter parameters
		void setup();

		/// Stop the counting process
		void reset();

		/// Read status register.
		unsigned short status_register();

		void software_control_gate(unsigned short value);
		void software_control_updown(unsigned short value);
		void software_control_enable(unsigned short value);
	/// @}

	/// @name Counter parameters
	/// These are the parameters to configure the counting process.
	/// See setup(), as there's were they are used. For simplicity
	/// now they are public and the DS directly accesses them.
	/// @{
		unsigned short m_mode;
		/// If true: External signals, else internal
		/// timebase and software gate/updown.
		/// @{
			bool m_sourceTimeBaseExternal;
			bool m_sourceGateExternal;
			bool m_sourceUpDownExternal;
		/// @}

		/// If true, low active polarity
		/// @{
			bool m_polarityLowGate;
			bool m_polarityLowUpDown;
			bool m_polarityLowClockEn;
			bool m_polarityLowOutput;
		/// @}

		unsigned short m_initialCount;
		unsigned short m_pulseDelay;
		unsigned short m_pulseWidth;
	/// @}


	inline void __adlink_assert(I16 r, const char* fnCall, const char* fn) const
	{
		if (r != NoError) {
			std::stringstream ss;
			ss  << "+ " << fnCall << " in " << fn
				<< " FAILED with code " << r << ": "
				<< pcisdask_error_text(r) << std::endl;
			// status_warn() << ss.str() << std::endl; NO need to do so, it's
			// done by throw_error itself
			throw_error("Adlink ERROR", ss.str(), fn);
		}
	}

	#define adlink_assert(fn) __adlink_assert(fn, #fn, __PRETTY_FUNCTION__)

};

#endif // __d_COUNTER_DEVICE_INTERFACE__H__dkEK29__
