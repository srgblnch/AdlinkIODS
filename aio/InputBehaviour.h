
#ifndef __ADLINK_AI_DEVICE___H___fgj043fru83j89___
#define __ADLINK_AI_DEVICE___H___fgj043fru83j89___


#include "InputBuffer.h"
#include "BaseBehaviour.h"
#include "StatsCalculator.h"

// These are to be defined in the Makefile,
// 0 no, 1 last, 2 full
//#define ADLD_GET_FULL_BUFFER_ 2


class AdlinkDeviceThread;

typedef SharedBuffer<U16> SharedBufferU16;

class InputDeviceInterface;
namespace InputDeviceInterface_
{
	class CallbackEventReceiver;
};

class ChangeEventListenerInterface
{
	public:
	virtual void on_data_changed() = 0;
};


/** Manages the configuration and execution of Single/Continuous Analog Input.
 */
class InputBehaviour : public BaseBehaviour
{
	friend class AdlinkDeviceThread;

	/// Identifies the retriggering mode for an adquisition.
	enum RetrigMode
	{
		RetrigInfinite = 0,
		RetrigFinite,
		RetrigNo,
		RetrigSoftware
	};
public:

	/// The device can be set-up to operate in several different ways.
	enum InputOperationMode
	{
		ModeNone = 0,
		ModeSingleShotInput,
		ModeContinuousShotInput,
		ModeContinuousInput
	};

	/// Identifies the last input operation mode in the InputOperationMode
	/// enum, so it can be divided in input modes and output modes just by
	/// using this threshold.
	static const InputOperationMode MODE_INPUT_LAST = ModeContinuousInput;


	Stats::StatsCalculator m_statsCalculator; /// @todo make private...

private:
	/// @name Device private status variables
	/// @{
		InputOperationMode    m_mode;
		RetrigMode            m_retrigMode;
		/// How many overruns have happened during operation
		long long unsigned    m_overruns;
		/// Used with m_overruns to compute the overruns ratio
		long long unsigned    m_totalRuns;
	/// @}

	/// @name Private memory management variables
	/// @{

		InputBuffer m_bufferControl;

		/// Size of each one of the half buffers
		U32         m_hsize;
		
		/// Stores the ID of the current half buffer ( 0 or 1 ) in double
		/// buffer acquisition mode, so we know which one to ask for to the
		/// library!
		int         m_halfId;
	/// @}

	/// @todo adl_timebase: Nevertheless, we should have a timeBase
	/// attribute for external timebase...
	U32     m_sampIntrv;
	
	/// See AdlinkAIO property "UseSharedBuffer"
	const bool m_canIgnoreSharedBuffer;
	/// See AdlinkAIO property "UseCallbackSignals"
	const bool m_canUseCallbacks;

	/// How many buffers CXX_ChannelValues will be able to display:
	/// So how many trigger buffers I need to keep saving. If it is -1,
	/// num_of_triggers() will be used.
	long m_displayableTriggers;

	AdlinkDeviceThread* m_thread;
	
	long m_channelSamples;
	
	inline U32 half_size() const
	{
		assert(m_channelSamples == channel_samples_per_trigger()*num_of_triggers());
		assert(this->m_hsize == channel_samples_per_trigger()*nchannels());
		return m_hsize;
	}
	
	inline long channel_samples() const
	{
		assert(m_channelSamples == channel_samples_per_trigger()*num_of_triggers());
		assert(this->m_hsize == channel_samples_per_trigger()*nchannels());
		return m_channelSamples;
	}
	
	/// For software re-triggered acquisition, a semaphore is need
	/// to put the acquisition thread to sleeep between triggers
	///
	/// I want it to be a MAX=1 semaphore, but omni threads on posix
	/// does not provide maximum. So, before post() I ALWAYS call
	/// trywait(), so, if its counter starts as 0 or 1, it will never
	/// go further than 1. To do it 100% right we should use a mutex
	/// around, so maybe it will end up being an omni_condition...
	//omni_semaphore		m_csSemaphore;
	
	/// zreszela & rhoms 30.04.2013
	/// solution with m_csSemaphore, does not work correctly when 
	/// acquire with multiple hardware triggers (e.g. ExtD)
	/// this was especially painful when stopping acquisition and 
	/// starting the new one
	/// change to solution with omni_condition
	omni_mutex          m_csMutex;
	omni_condition      m_csCondition;


	/// @name Signal callbacks
	/// 
	/// InputDevice allows you to subscribe to certain events (will get a 
	/// signal from the driver) using a callback object. So, we create our
	/// own callback object type (ICER, based on CallbackEventReceiver )
	/// which just sends it back the callback to us in the method
	/// on_half_ready(). The new little-type ICER must have acces
	/// to this private method!
	/// @{
		
		/// Are we in callback mode?
		bool m_usingCallbacks;

		friend class ICER;
		/// Callback receiver object
		InputDeviceInterface_::CallbackEventReceiver* m_callback;

		// Callbacks
		void on_half_ready_callback();
		void on_ai_completed_callback();
		void on_trigger_callback();

		/// Stops callback managed analog input.
		void stop_callbacks();
	/// @}


	/// @name Private configuration/release methods
	/// @{
		void configure_ai(/*long chBufSize, long totalShots, U32 trigCtrl*/);
		I16 init_continuous_ai();
	/// @}



	/// @name Private thread management methods
	/// @{
		/// Gets called when the thread begins to make on thread initialization.
		/// Does nothing but redirect to a InputOperationMode specific method...
		/// @returns 0 if everything is OK
		I16 thread_init();
		/// Loop doing adquisition until something fails or stopThread changes
		/// to 1. In fact just calls a method doing it depending on the
		/// InputOperationMode
		/// @param stopThread: When the thread has to stop
		/// this variable changes to true.
		void thread_body(const volatile bool & stopThread);

		/// This method gets called when it's requested that the thread
		/// stops, so if it is stoped waiting for a semaphore or something
		/// this is the place to wake it up. thread->stopThread is already
		/// set to true when this method is run.
		void thread_stopping() { /* Nothing to do now */ };

		I16 thread_init_continuous_ai();
		void thread_continuous_ai(const volatile bool & stopThread);
		void thread_continuous_shot_ai(const volatile bool & forceStop);
	
		bool thread_continuous_shot_wait_data_ai(
								const volatile bool & forceStop);
		bool thread_continuous_shot_wait_trigger_ai(
								size_t & iteration,
								size_t totalRetrig,
								const volatile bool & forceStop);
	
		inline bool thread_continuous_wait_data_ai(const volatile bool & forceStop, bool & finished);
	/// @}


	/// It's called each time an acquisition is completed by some external
	/// trigger. It checks how many data is still waiting to be read and
	/// does it. It's a special case because it most probably won't be
	/// a full buffer but only part of it.
	void on_ai_completed();

	/// Makes something if there's overrun
	/// Now it just prints overrun ratio in the console
	inline void overrun_check(bool hasOverrun);

	/// @name Change Events
	/// This is all related to how the device manages to inform
	/// clients that the data has changed.
	/// @{
		/// The event listener. It's not mutex protected, so push_change_events
		/// and subscribe_change_events cannot be run at the same time
		ChangeEventListenerInterface* m_changeEventListener;

		/// This method is called when new data is available (when reading
		/// an attribute would result in a value different than last time
		/// it was called). It is responsible to notify anyone interested
		/// in knowing about this situation.
		void push_change_events();
	/// @}

	void save_half_buffer( U16 halfId )
    { this->m_bufferControl.save_half_buffer(halfId); this->push_change_events(); }
    
	void save_special_half_buffer(U16 halfId, U32 first, U32 count)
    { this->m_bufferControl.save_special_half_buffer(halfId, first, count); this->push_change_events(); }
public:
	/// @name Input methods
	/// @brief Methods to retreive the last input value read on each port.
	///
	/// Different versions are to get the very last value on a port, or the
	/// last buffer aquired for a port, or the last buffer for all port 
	/// simultaneously, as a voltage value or as the binary representation.
	/// @{
		/// This one simplifies get_last for all raw and double, which
		/// share most of the code thanks to it.
		bool get_last_big_aux(
				bool getLong,
				std::vector<long> & resultl,
				std::vector<volts_type> & resultd,
				size_t &resSz,
				size_t width,
				struct timeval& time) const;

		bool get_last(unsigned port, std::vector<volts_type> & result, size_t &resSz, struct timeval& time) const;
		bool get_last(unsigned port, volts_type & result, struct timeval& time) const;
		bool get_last(std::vector<long> & result, size_t &resSz, struct timeval& time) const;
		bool get_last(std::vector<volts_type> & result, size_t &resSz, struct timeval& time) const;
		// Last value of all buffers
		bool get_last_values(std::vector<volts_type> & results, size_t &resSz, struct timeval& time) const;
	/// @}

	inline InputDeviceInterface & device();
	inline const InputDeviceInterface& device() const;

public:

	inline long num_of_displayable_triggers() const
	{ return (m_displayableTriggers < 0)? this->num_of_triggers() : m_displayableTriggers; }
	inline long get_num_of_displayable_triggers() const
	{ return m_displayableTriggers; }
	inline void set_num_of_displayable_triggers(long ntrig)
	{ m_displayableTriggers = ntrig; }

	InputBehaviour(
			AdlDeviceFactorySingleton::AdlBoardParams* p,
			const std::string & boardId,
			StatusObserver* ob,
			bool canIgnoreSharedBuffer,
			bool canUseCallbacks,
			const StatsSettings &statSettings);
	~InputBehaviour();

	/// @name Status Methods
	/// @{
		bool continuous_input() const;
		inline bool is_input() const { return true; }
	/// @}


	/// @name start, configuration & stop methods
	/// @{
		virtual void adapt_sample_rate(unsigned long sampleRate);
		void start_ai();
		void stop_ai();
	/// @}


	/// @name Change Events
	/// @{
		/// This method cannot be executed on Running state
		/// @param cel An object implementing ChangeEventListenerInterface
		/// use NULL to unsubscribe. 
		void subscribe_change_events(ChangeEventListenerInterface* cel);
	/// @}

	double bits_to_volts(U16 bits) const;
	bool bits_to_volts(const U16 *bits, volts_type* volts, size_t count) const;

	/// A/D input channels reference Ground Selection (only available for 
	/// DAQ-2204/2205/2206/2208/2213/2214):
	///  - DAQ-2010, DAQ-2005, DAQ-2006, DAQ-2501, DAQ-2502: 0
	///  - DAQ-2204, DAQ-2205, DAQ-2206, DAQ-2208, DAQ-2213, DAQ-2214:
	///   AI_RSE : Referenced single ended mode ( 64chs common to ground
	///           system on board) (default value)
	///  AI_DIFF : Differential mode
	///  AI_NRSE : Non-referenced single ended mode ( 64chs common to
	///            AISENSE pin )
	virtual void set_reference_ground(const std::string &referenceGround);


///////////////////////////////////////////////////////////////////////////////
// 
///////////////////////////////////////////////////////////////////////////////

	virtual void update_state_fast() {};

	virtual bool read_raw_data(long *& rawData, long &x, long &y, struct timeval& time) const;
	virtual bool read_double_data(volts_type *& rawData, long &x, long &y, struct timeval& time) const;
	virtual bool read_scalar_value(ssize_t port, volts_type* &value, struct timeval& time) const;
	virtual bool read_last_values(volts_type* &data, long &x, struct timeval& time) const;
	virtual bool read_channel_spectrum(ssize_t port, volts_type* &data, long &x, struct timeval& time) const;

	virtual bool write_scalar_value(ssize_t port, volts_type value)
	{	return false;	}
	virtual bool write_last_values(const volts_type* data, long x)
	{	return false;	}
	virtual bool write_channel_spectrum(ssize_t port, const volts_type* data, long x)
	{	return false;	}

	// start, start_ai, stop, stop_ai, rejoin something...
	virtual void start();
	virtual void stop();

	/// So, internally we use volt_scale_ai, which can be inlined,
	/// and we also provide bits2volts, to get the proper transformation
	/// by virtuals from any BaseBehaviour.
	/// @todo volts2bits is not used, so now it's not defined but it should..
	virtual unsigned volts2bits(double volts) const
	{	assert(false);	}
	virtual double bits2volts(unsigned bits) const
	{	return bits_to_volts(bits);	}

	void get_status_info(std::ostream & statusStream);

	virtual void on_channel_samples_per_trigger_about_to_change(long chspt);
};

#endif //__ADLINK_AI_DEVICE___H___fgj043fru83j89___
