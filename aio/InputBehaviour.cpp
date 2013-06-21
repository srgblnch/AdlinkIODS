
#include "InputBehaviour.h"
#include "AdlinkDeviceThread.h"
#include "InputDeviceInterface.h"
#include "tango_utils.h"


#define _OCLASSNAME "InputBehaviour"
#include "odebug.h"


///
/// This is just an implementation of CallbackEventReceiver interface
/// needed by InputDevice to inform me about signals produced by the
/// driver. It does nothing but send the information to the appropiate
/// InputBehaviour object.
///
class ICER : public InputDeviceInterface_::CallbackEventReceiver
{
	InputBehaviour * parent;
public:
	ICER(InputBehaviour *p) : parent(p) {}
	virtual void on_event_received(InputDeviceInterface_::CallbackEvent ev)
	{
		try {
			if ( ev == InputDeviceInterface_::EventHalfBufferReady ) {
				std::cout << "+" << std::flush;
				parent->on_half_ready_callback();
			} else if (ev == InputDeviceInterface_::EventTrigger ) {
				std::cout << "-" << std::flush;
				parent->on_trigger_callback();
			} else if ( ev == InputDeviceInterface_::EventCompleted ) {
				std::cout << "*" << std::flush;
				parent->on_ai_completed_callback();
			}
		} catch (const Tango::DevFailed & e) {
			std::cerr	<< _CFN_ 
						<< " TANGO EXCEPTION:\n"
						<< e << "\n--------------" << std::endl;
			throw;
		}
	}

	virtual void on_event_disconnected(InputDeviceInterface_::CallbackEvent ev)
	{
		// We are not interested in knowing when does the callback get
		// really disconnected from us now.
	}
};

InputBehaviour::InputBehaviour(
			AdlDeviceFactorySingleton::AdlBoardParams* p,
			const std::string & boardId,
			StatusObserver* ob,
			bool canIgnoreSharedBuffer,
			bool canUseCallbacks,
			const StatsSettings & statSettings) :
	m_mode(ModeNone),
	m_canIgnoreSharedBuffer(canIgnoreSharedBuffer),
	m_canUseCallbacks(canUseCallbacks),
	//m_csSemaphore(0),
	m_csMutex(),
	m_csCondition(&m_csMutex),
	m_thread(0),
	m_overruns(0),
	m_totalRuns(0),
	m_callback(0),
	m_usingCallbacks(false),
	m_changeEventListener(0),
	m_displayableTriggers(-1),
	BaseBehaviour(p, AdlDeviceFactorySingleton_::ModeAnalogInput, boardId, ob)
{
	m_bufferControl.set_device(&this->device());

	this->m_statsCalculator.reconfigure(this->nchannels(), statSettings);

	this->m_hsize = this->channel_samples_per_trigger() * this->nchannels();
	this->m_channelSamples = this->channel_samples_per_trigger()*this->num_of_triggers();

	this->m_callback = new ICER(this);

	this->device().set_nchannels(nchannels());

	if (!this->is_d2k()) {
		/// @todo this should be provided by InputDeviceInterface (or probably
		/// some bigger IODeviceInterface)
		assert(this->params()->type == PCI_9116);
		static unsigned ok9116[] = {	AD_B_5_V, AD_B_2_5_V,
										AD_B_1_25_V, AD_B_0_625_V,
							// Theoretically supported, but not in "real life":
										AD_U_10_V, AD_U_5_V,
										AD_U_2_5_V, AD_U_1_25_V 
									};
		std::set<unsigned> _ok9116;
		for(size_t n=0; n<sizeof(ok9116)/sizeof(ok9116[0]); ++n)
			_ok9116.insert(ok9116[n]);

		this->range().set_valid_ranges(_ok9116);
		this->range().set_range_id(ok9116[0]);
	}
}

InputBehaviour::~InputBehaviour()
{
	this->stop_ai();
	delete this->m_callback;
	this->m_callback = 0;
}

InputDeviceInterface & InputBehaviour::device()
{	return static_cast<InputDeviceInterface&>(BaseBehaviour::device()); }
const InputDeviceInterface& InputBehaviour::device() const
{	return dynamic_cast<const InputDeviceInterface&>(BaseBehaviour::device()); }


inline double InputBehaviour::bits_to_volts(U16 _bits) const
{
	return this->device().bits_to_volts(this->range().range_id(), _bits);
}

inline bool InputBehaviour::bits_to_volts(const U16 *bits, volts_type* volts, size_t count) const
{
	return this->device().bits_to_volts(this->range().range_id(), bits, volts, count);
}

inline void InputBehaviour::push_change_events()
{
	if (this->m_changeEventListener)
		this->m_changeEventListener->on_data_changed();
}

void InputBehaviour::subscribe_change_events(ChangeEventListenerInterface* cel)
{
	/// m_changeEventListener is not mutex pretected,
	/// so InputBehaviour cannot be using it
	assert ( this->get_state() != StateRunning );
	this->m_changeEventListener = cel;
}

void InputBehaviour::stop_ai()
{
	assert(this->m_mode <= MODE_INPUT_LAST);

	bool notify_invalid = false;
	if ( (this->get_state() == StateReady) || (this->get_state() == StateRunning) )
		notify_invalid = true;

	if(this->m_mode == ModeContinuousInput)
	{
		info() << "Uninstalling Mode: ModeContinuousInput" << std::endl;

		if (!this->m_usingCallbacks)
			AdlinkDeviceThread::stop_thread(this->m_thread);
		else
			this->stop_callbacks();
		
		this->device().release_buffers();
	}else if(this->m_mode == ModeContinuousShotInput )
	{
		info() << "Uninstalling Mode: ModeContinuousShotInput" << std::endl;
		
		if (!this->m_usingCallbacks) {
//          zreszela & rhoms 30.04.2013
//			this->m_csSemaphore.trywait();
//			this->m_csSemaphore.post();
			this->m_csCondition.broadcast();
			AdlinkDeviceThread::stop_thread(this->m_thread);
		} else
			this->stop_callbacks();

		this->device().release_buffers();
	}else if (this->m_mode == ModeSingleShotInput ) {
		info() << "Uninstalling Mode: ModeSingleShotInput" << std::endl;
	}

	this->m_usingCallbacks = false;

	this->common_device()->leave_shared_control(this);
	
	this->m_mode = ModeNone;
	this->set_state(StateNone);

	// Now attribute values are invalid, notify it
	if (notify_invalid)
		this->push_change_events();
}

void InputBehaviour::start_ai()
{
	bool registerCard = (this->get_state() == StateNone);
	
	if (registerCard)
		this->apply_range();
	
	if( ( 		(this->m_mode == ModeContinuousInput)
			||	(this->m_mode == ModeContinuousShotInput) 
		)
		&& (this->get_state() == StateReady)
		&& ( (this->m_thread) || (this->m_usingCallbacks) ) ) {
		/// For continuous Input Not as continuous (eg, can be stopped by
		/// a trigger signal, this is trigger mode pre or middle)
		/// We have to attend that special case to re start the acquisition.

		/// Continous shot input also has this problem. In this case, the
		/// problem looks like: If we try to re start the acquisition without
		/// unregistering and re-registering the buffers, the machine gets hung
		this->stop_ai();
		registerCard = true;
	}
	
	if(registerCard)
	{
		assert(this->m_mode == ModeNone);
	
		try {
			const long chSamples = 
					this->num_of_displayable_triggers()*this->channel_samples_per_trigger();
			this->update_user_buffer_size(chSamples);
		} catch (std::bad_alloc & e) {
			error() << _CFN_ << "Memory problems " << e.what();
			throw_error("NOT_ENOUGH_MEMORY", e.what(), _CFN_);
		}
		
		m_bufferControl.stop_circular_buffer();

		this->configure_ai();
	}

	this->m_statsCalculator.set_spectrums_max_length(this->num_of_triggers());

	/// Card is already registered, just make an acquisition
	switch(this->m_mode)
	{
		case ModeContinuousInput:
			/// Normally: Do nothing, adquisitions are already running
			/// in the background in continuous mode
												 
			/// Pre/Middle trigger can make our continuous acquisition
			/// to stop. So, restart if necessary.
			if(this->get_state() == StateReady) {
				/// Start continuous adquisition

				bool ok = true;
				if (!this->m_usingCallbacks) {
					this->m_thread = AdlinkDeviceThread::start_thread(this);
					ok = this->m_thread ? 1 : 0;
				} else {
					this->set_state(StateRunning);
					ok = (this->init_continuous_ai() == 0);
				}
				if (! ok ) {
					this->m_mode = ModeNone;
					this->set_state(StateRegistered);
					throw_error(	"Continuous Input Mode init error",
									"Board is successfully registered, but some"
									" problem arised preparing the required"
									" mode.",
									_CFN_);
				}
			}
			break;
		case ModeSingleShotInput:
		{
			/// Do an adquisition on each channel and store the result in 
			/// the shared buffer...

			m_bufferControl.stop_circular_buffer();
            m_bufferControl.init_circular_buffer();

            /// Lock the buffer and aquire data
			{
                InputBufferWriteLock lock(m_bufferControl);
				this->device().read_channels_shot(lock.get());
			}

			this->push_change_events();
			break;
		}
		case ModeContinuousShotInput:
			/// Start an adquisition. The results will
			/// eventually get into the shared buffer.

			/// @todo I am not sure at all about this...
			if (	(this->get_state() != StateReady)
				&&	(this->m_retrigMode != RetrigSoftware)
				&&	(this->m_retrigMode != RetrigNo) )
				throw_error("ModeContinuousShotInput error",
							"Error starting. Restart only allowed for software or no retrigger.",
							_CFN_
						);
			
			if (this->m_usingCallbacks) {
				if (!this->device().cont_scan_channels(this->m_sampIntrv))
					throw_error("ModeContinuousShotInput error",
								"Error in cont_scan_channels",
								_CFN_ );
				this->set_state(StateRunning);
			} else {
			//	if (	(this->m_retrigMode == RetrigSoftware)
			//		||	(this->m_retrigMode == RetrigNo) ) {
//                  zreszela & rhoms 30.04.2013			
//					this->m_csSemaphore.trywait();
//					this->m_csSemaphore.post();
				this->m_csCondition.broadcast();
			//	}
			}
			break;
		case ModeNone:
		default:
			throw_error(
					"Unexpected acquisition",
					"Unexpected adquisition, device not ready!",
					_CFN_);
	}
}





void InputBehaviour::configure_ai()
{
	assert(this->m_mode == ModeNone );
	assert(this->channel_samples_per_trigger() == 1 || this->nchannels()*this->channel_samples_per_trigger() % 2 == 0);// so samples_per_trigger() will also be ok
	assert(this->channel_samples_per_trigger() >=1 );
	assert(this->num_of_triggers() >= 1);

	this->m_usingCallbacks = false;

	this->m_overruns = 0;
	this->m_totalRuns = 0;

	if(this->channel_samples_per_trigger() < 1)
		throw_error(	"Bad Size",
						"Bad size for channel buffer, it should be > 0",
						_CFN_ );

	I16 e;

	unsigned short totalChannels = this->nchannels();
	
	this->m_channelSamples = this->channel_samples_per_trigger()*this->num_of_triggers();
	bool canIgnoreSharedBuffer = (this->num_of_displayable_triggers() == 1)?this->m_canIgnoreSharedBuffer:false;

	try {
		this->trigger_mode().apply();
		this->common_device()->request_shared_control(this);
		assert_adlink(this->common_device()->configure_shared_resources(this), "Error configuring AIO");

		this->device().preconfigure_input_channels(this->range().range_id());
		this->device().set_delay(this->delay(), this->delay_source());
		
		if (this->channel_samples_per_trigger() > 1) /// Continuous
		{
			m_bufferControl.rebuild(this->half_size(), this->num_of_displayable_triggers() * 2, not canIgnoreSharedBuffer);

			if (this->infinite_retrigger() == 1) /// Continuous Input
			{
				/// @todo no se si forsar num_of_triggers a 1 o de fet mira tu ja esta be tb, simplement
				/// que aqui representa el total de displayed buffers enlloc del total de buffers capturats
				/// (que seguira creixent fins l'infinit, esclar)
				info() << "Configuration Mode: ModeContinuousInput" << std::endl;

				this->device().configure_ai(0, this->trigger_mode().get_mode());

				this->device().configure_double_buffer(this->half_size());
				this->device().clear_overrun_flag();
	
				this->m_mode = ModeContinuousInput; /// @todo rename a ModeInfiniteInput

				this->m_usingCallbacks = false;
				if (this->m_canUseCallbacks) {
					this->m_usingCallbacks = true;

					std::set<InputDeviceInterface_::CallbackEvent> sc;
					sc.insert(InputDeviceInterface_::EventCompleted);
					sc.insert(InputDeviceInterface_::EventHalfBufferReady);
					if (this->device().supports_callbacks(sc)) {
						this->device().set_callback(
									InputDeviceInterface_::EventCompleted,
									this->m_callback);
						this->device().set_callback (
									InputDeviceInterface_::EventHalfBufferReady,
									this->m_callback );
					} else {
						this->m_usingCallbacks = false;
					}
				}
				
				// As the thread can be thrown several times without
				// changing the configuration (as it can be stopped
				// by an external trigger if configured that way)
				// It's creation is not here anymore
				this->set_state(StateReady);
			} else /// ModeContinuousShotInput
			{
				info()	<< "Configuration Mode: "
						<< "ModeContinuousShotInput" << std::endl;
	
				/// @todo test aixo que m'he carregat per aqui RetrigInfinite. I si realment no cal, pos s'elimina i punto...
				/// @test lala l'antic mode num_of_shots=0, calia per a res?
				/*
				if (this->num_of_triggers()==0) /// @todo que no te sentit!! com se distingeix aixo ara?? mes aviat caldra afegir algo al trigger_mode() que sigui retrigger, igual com te l'AO... O aviam, aixo de fet quin sentit te? cap, ja ve a ser l'infinite normal no? xq el retrigger es posa sol de fet amb num_of_triggers()>0... aix que complicaaaat...
					this->m_retrigMode = RetrigInfinite;
				*/
				
				if (this->num_of_triggers() == 1)
					this->m_retrigMode = RetrigNo;
				else
					this->m_retrigMode = RetrigFinite;
				
				if (this->trigger_mode().is_software()) {
					this->m_retrigMode = RetrigSoftware;
					if(this->num_of_triggers() != 1)
						/// It's unsuported because it does not work properly!
						/// Still don't know how we are supposed to send
						/// the software trigger to get a second buffer of data
						this->throw_error(
							"Invalid_Mode",
							"Currently, ContinuousShotInput mode retriggered by"
							" SW is not allowed. Use this->num_of_triggers()=1 for no"
							" retriggering and get one buffer each time.",
							_CFN_);
				}
				
				// Pre configure :
				long totalTriggers = this->num_of_triggers();
				U32 triggerMode = this->trigger_mode().get_mode();
				if ((totalTriggers != 1) || (this->infinite_retrigger()==2)) {
					if ((triggerMode & DAQ2K_AI_TRGMOD_POST != DAQ2K_AI_TRGMOD_POST)
					&& (triggerMode & DAQ2K_AI_TRGMOD_DELAY != DAQ2K_AI_TRGMOD_DELAY))
						throw_error("Invalid_Mode", "Retriggered acquisition (NumOfTriggers>1) is only avialble for TriggerMode POST and DELAY.", _CFN_);
					triggerMode |= DAQ2K_AI_ReTrigEn;
					if(totalTriggers == 1)
						// Otherwise it ignores DAQ2K_AI_ReTrigEn
						totalTriggers = 0;
				} else
					totalTriggers = 0; /// @todo WHY??
				this->device().configure_ai(
								totalTriggers, triggerMode);

	            DBGCOUT	<< DBGVAR(this->half_size()) << "\n\t"
			            << DBGVAR(totalTriggers) << "\n\t"
			            << std::endl;
				if (totalTriggers) 
                    this->device().configure_single_buffer(this->half_size() * totalTriggers); //DFC 20121203
                else         
                    this->device().configure_single_buffer(this->half_size() );
				//this->device().configure_single_buffer(this->half_size());

				this->m_mode = ModeContinuousShotInput;
				this->set_state(StateReady);

				// Check for callback support availability
				this->m_usingCallbacks = false;
				if (this->m_canUseCallbacks) {
					this->m_usingCallbacks = true;

					std::set<InputDeviceInterface_::CallbackEvent> sc;
					sc.insert(InputDeviceInterface_::EventCompleted);
					sc.insert(InputDeviceInterface_::EventTrigger);
					if (this->device().supports_callbacks(sc)) {
						this->device().set_callback(
									InputDeviceInterface_::EventCompleted,
									this->m_callback );
						this->device().set_callback (
									InputDeviceInterface_::EventTrigger,
									this->m_callback );
					} else {
						this->m_usingCallbacks = false;
					}
				}

				// Start callback or polling thread
				if ( this->m_usingCallbacks ) {
					// Take the first element to write into
					m_bufferControl.init_circular_buffer();
					this->m_halfId = 0;
					
				} else {
					this->m_thread = AdlinkDeviceThread::start_thread(this);
					if (! this->m_thread ) {
						this->m_mode = ModeNone;
						this->set_state(StateRegistered);
						throw_error("ModeContinuousShotInput error",
								"Board is registered, but something during"
								" initialization failed for"
								" ModeContinuousShotInput",
								_CFN_);
					}
				}
			}
		} else if ( /// Single Shot Input
						(this->channel_samples_per_trigger() == 1)
					&&	(this->num_of_triggers() == 1)
					&&	this->trigger_mode().is_software() ) {
			info() << "Configuration Mode: ModeSingleShotInput" << std::endl;

			m_bufferControl.rebuild(this->half_size(), this->num_of_displayable_triggers() * 2, true);

			this->device().configure_ai(0, 0);

			this->m_mode = ModeSingleShotInput;
			this->set_state(StateReady);
		} else
			throw_error("Problems configuring AI",
						"Selected ChannelBufferSize/NumShots/TriggerSource"
						" combination is not a defined operation mode for"
						" this device.",
						_CFN_);
	} catch (...) {
		this->stop_ai();
		throw;
	}
}


/**
 * In adquisition modes triggered by software, we have to wait until the
 * user wants to acquire and then set up the device to acquire.
*/
bool InputBehaviour::thread_continuous_shot_wait_trigger_ai(
		size_t & iteration,
		size_t totalRetrig,
		const volatile bool & forceStop)
{
	I16 e;
	switch(this->m_retrigMode){
		case RetrigInfinite:
			break;
		case RetrigFinite:
			++iteration;
			if(iteration > totalRetrig)
				iteration = 0;
			break;
		case RetrigNo:
		case RetrigSoftware:
			iteration = 0;
	}
	if(iteration == 0) {
		// Wait for a new software trigger
		this->set_state(StateReady);
//      zreszela & rhoms 30.04.2013		
//      this->m_csSemaphore.wait();
		this->m_csCondition.wait();
		if(forceStop)
			return false;
		// U32 a,b; D2K_AI_AsyncClear(this->handler(), &a, &b);

		if(!this->device().cont_scan_channels(this->m_sampIntrv))
			return false;
		iteration = 1;
		this->set_state(StateRunning);
	}

	return true;
}


/** After setting up the device, we have to wait until the adquisition
 * is complete (or buffer is full). That method waits for it to happen.
*/
bool InputBehaviour::thread_continuous_shot_wait_data_ai(
							const volatile bool & forceStop)
{
	bool finished = false, trgReady = false;
	U32 accesCnt;
	U16 rdyTrigCnt;
/// @test thread_continuous_shot_wait_data_ai has been updated...
	try {
		do
		{
			switch(this->m_retrigMode)
			{
				case RetrigInfinite:
				case RetrigFinite:
					trgReady = this->device().
								async_retrig_next_ready(finished, rdyTrigCnt);
					break;
				case RetrigNo:
				case RetrigSoftware:
					finished = this->device().async_check_finished(accesCnt);
			}
			if (finished || trgReady)
				return true;
			sched_yield();
		}while (!forceStop);
	} catch(...) {
		return false;
	}
	return false;
}

/**
 * @brief Continuous shot acquisition thread main loop.
 * @see thread_body
 */
void InputBehaviour::thread_continuous_shot_ai(const volatile bool & forceStop)
{
	size_t iteration = 0;
	size_t totalRetrig = this->num_of_triggers();
	
	// Tricky...
	if(this->m_retrigMode == RetrigFinite)
		iteration = totalRetrig + 1;

	m_bufferControl.init_circular_buffer();

	do {
		if (!this->thread_continuous_shot_wait_trigger_ai(
											iteration, totalRetrig, forceStop) )
			break;
		if (!this->thread_continuous_shot_wait_data_ai(forceStop))
			break;

        DBGCOUT << DBGVAR(iteration) << "\n\t"
             << DBGVAR(totalRetrig) << "\n\t"
             << DBGVAR(forceStop) << " no callbacks DFC \n\t"
             << std::endl;

        this->save_special_half_buffer(0, this->half_size()*(iteration-1), this->half_size());
	    //this->save_half_buffer(0);

	} while(!forceStop);
	
	this->device().async_clear(0, 0);

	if (!forceStop) {
		// Leaving after an error, not after wished stop...
		status_warn()
				<< "thread_continuous_shot_ai Finished because"
				<< " of an unexpected stop!" << std::endl;
		this->set_state(StateFault);
		return;
	}
	this->set_state(StateRegistered);
}



/**
 * @brief Continuous adquisition specific thread inicialization.
 * @see thread_init
*/
I16 InputBehaviour::thread_init_continuous_ai()
{
	return this->init_continuous_ai();
}

I16 InputBehaviour::init_continuous_ai()
{
	assert(this->m_mode == ModeContinuousInput);

	this->m_halfId = 0;

    m_bufferControl.init_circular_buffer();

	if(!this->device().cont_scan_channels(this->m_sampIntrv))
		return -1;
	return 0;
}

/** After setting up the device, we have to wait until the adquisition
 * is complete (or buffer is full). That method waits for it to happen.
 * Two versions are provided at compile time: Polling based and interrupt
 * based.
 * @param forceStop
 * @param finished Finished flag
 * @return false error || stopForced. true acquisition waiting
*/
bool InputBehaviour::thread_continuous_wait_data_ai(
							const volatile bool & forceStop, bool & finished)
{
	BOOLEAN halfReady, _finished = BOOLEAN(0);
	finished = false;
	do {
		this->device().dbl_buf_half_ready(&halfReady, &_finished);

		if (halfReady || _finished || forceStop)
			break;
		// By adding this line to release the CPU to be used by other threads
		// or processes the number of overruns when controlling various
		// devices decreases A LOT:
		sched_yield();
	} while (true);
	finished = _finished;

	if (finished || forceStop)
		info()	<< "halfReady=" << halfReady
				<< ", finished="<< finished
				<< ", forceStop=" << forceStop << std::endl;

	return !forceStop;
//	return halfReady && !finished && !forceStop;
}

/// It's called each time an acquisition is completed by some external
/// trigger. It checks how many data is still waiting to be read and
/// does it. It's a special case because it most probably won't be
/// a full buffer but only part of it.
void InputBehaviour::on_ai_completed()
{
	U32 first, count;
	this->device().async_clear(&first, &count);

	DBGCOUT	<< DBGVAR(first) << "\n\t"
			<< DBGVAR(count) << "\n\t"
			<< DBGVAR(this->half_size()) << "\n\t"
			<< std::endl;

	// If count!=0, we have added something to the final buffer, so
	// read and store it
	if (count) {
		if (this->m_mode == ModeContinuousShotInput) {
			// Don't know why, PRE trigger mode on ContinuousShot
			// gives sometimes count > this->half_size(), but we only
			// have one buffer!
			if (count > this->half_size()) {
				first = count - this->half_size();
				count = this->half_size();
			}
            DBGCOUT << DBGVAR(this->half_size()) <<" ModeContinuousShotInput DFC \n\t"<<std::endl;

			this->save_special_half_buffer(0, first, count);
			return;
		}
		// Some times count is greater than half_size(). In this case we
		// have to read both last half buffers.
		if (count <= this->half_size()) {
			this->save_special_half_buffer(this->m_halfId, first, count);
		} else {
			assert(first == 0);
			this->save_half_buffer(this->m_halfId);
			this->m_halfId = this->m_halfId ? 0 : 1;
			this->save_special_half_buffer(
					this->m_halfId, 0, count - this->half_size());
		}
		this->m_halfId = this->m_halfId ? 0 : 1;
	}
}

/**
 * @brief Continuous adquisition specific thread inicialization.
 * @see thread_body
 */
void InputBehaviour::thread_continuous_ai(const volatile bool & forceStop)
{
	assert(!this->m_usingCallbacks);

	// NO: this->m_halfId = 0; now it must be done before we get any callback
	// So, it's in thread_init_continuous_ai()

	info() << _CFN_ << " BEGIN " << std::endl;

	this->set_state(StateRunning);

	U16 hasOverrun;
	bool finished = false;

	while (!forceStop) {

		if(!thread_continuous_wait_data_ai(forceStop, finished))
			break;

		if (finished) {
			this->on_ai_completed();
			// That was the last data available, so break loop
			break;
		} else {
			this->save_half_buffer(this->m_halfId);
			this->m_halfId = this->m_halfId ? 0 : 1;
		}

		this->device().last_buffer_handled();

		hasOverrun = this->device().has_overrun();
		this->overrun_check(hasOverrun);
		if(hasOverrun)
			this->device().clear_overrun_flag();
	} // ~ while (!forceStop)

	this->device().async_clear(0, 0);

	this->set_state(StateReady);

	info() << _CFN_ << " END " << std::endl;
}


/**
 * @brief Time to initialize the thread. It runs on the new thread, but the
 * thread creating this will be waiting for the result of this method.
 * 
 * Gets called when the thread begins to make on thread initialization. Does
 * nothing but redirect to an InputOperationMode specific method...
 * 
 * It's just for initialization, the thread main loop should go in thread_body,
 * so the original thread can be run in paralel.
 * 
 * @returns 0 if everything is OK
*/
I16 InputBehaviour::thread_init()
{
// 	// http://www.ibm.com/developerworks/linux/library/l-osmig1.html
// 	// change the thread priority dynamically
// 	struct sched_param param;  // scheduling priority
// 	int policy = SCHED_FIFO;  // scheduling policy  //abans RR
// 	// Get the current thread id
// 	pthread_t thread_id = pthread_self();
// 	// To set the scheduling priority of the thread
// 	param.sched_priority = 99;
// 	pthread_setschedparam(thread_id, policy, &param);
	if (this->m_mode == ModeContinuousInput) {
		I16 r = this->thread_init_continuous_ai();
		if(r == 0)
			this->set_state(StateRunning);
		return r;
	}else if (this->m_mode == ModeContinuousShotInput) {
		this->set_state(StateReady);
		return 0;
	}
	assert(0); // Should NEVER happen
	return -1;
}

/**
 * @brief body of the auxiliar thread.
 * @param stopThread The main loop inside the thread will
 *                 be polling this value to stop when its
 *                 set to 1.
*/
void InputBehaviour::thread_body(const volatile bool & stopThread)
{
	if (this->m_mode == ModeContinuousInput)
		this->thread_continuous_ai(stopThread);
	else if (this->m_mode == ModeContinuousShotInput)
		this->thread_continuous_shot_ai(stopThread);
	else
		assert(0); // Should NEVER happen
}



/**
 * It gets called when the driver sends a signal informing us that there's
 * a half buffer ready to be read. Note that it's not a thread, it's running
 * in signal time.
*/
void InputBehaviour::on_half_ready_callback()
{
    DBGCOUT	<< " DFC callback \n\t"
			            << std::endl;
	// After the signal to read this half buffer, but before getting to here,
	// we have received a operation completed signal. This means we have
	// already read this last half buffer, so there's no need
	if (this->get_state() != StateRunning) {
		assert (this->get_state() == StateReady);
		return;
	}

	this->save_half_buffer(this->m_halfId);
	this->m_halfId = this->m_halfId ? 0 : 1;

	// Looks like callback version does not really like it...
	// - this->device().last_buffer_handled();
	
	// Check overruns...
	this->overrun_check(this->device().has_overrun());

	// Even if there's no overrun, looks like callback version needs it.
	// Else it has a predictable sequence of 1 overrun each 3 calls to
	// this function
	this->device().clear_overrun_flag();
}


void InputBehaviour::on_trigger_callback()
{
	BOOLEAN stopped;
//	static U32 myFirst_old  = 0 ;
	static U32 myOverrunCnt  = 0 ;
	static U32 myStart  = 0 ;
	U32 myFirst;

	// After the signal to read this half buffer, but before getting to here,
	// we have received a operation completed signal. This means we have
	// already read this last half buffer, so there's no need
	if (this->get_state() != StateRunning) {
		assert (this->get_state() == StateReady);
		return;
	}
    // DFC...it was always copying the first (0) part of the buffer: not good..
	//this->save_half_buffer(0);
    //
    //
	this->device().async_check_finished(myFirst);

    if (myStart > myFirst) {
	    myOverrunCnt  = 0 ;
        myStart = 0 ;
    }  
	this->save_special_half_buffer(0, myStart, this->half_size());
    myStart +=this->half_size();
    if (myStart != myFirst)
        myOverrunCnt++;

    DBGCOUT	<< DBGVAR(myFirst)<<" " << DBGVAR(myStart)<< " " << DBGVAR(myOverrunCnt)
            << " DFC \n\r" << std::endl;

    /* simplified DFC borrame
    assert(myFirst >= this->half_size());
    if (myFirst_old + this->half_size() == myFirst || myFirst == this->half_size()) {
        myFirst_old = myFirst;
	    this->save_special_half_buffer(0, myFirst-this->half_size(), this->half_size());
    } else  {
	    this->save_special_half_buffer(0, myFirst_old, this->half_size());
        myFirst_old += this->half_size();
        myOverrunCnt++;
    }
    DBGCOUT	<< DBGVAR(myFirst)<<" " << DBGVAR(myFirst_old)<< " " << DBGVAR(myOverrunCnt)
            << " copy special buffer now DFC \n\r" << std::endl;
    */
}

/**
 * It gets called when the driver sends a signal informing us that acquisition
 * has been externally stopped. Now it's our responsibility to read the last
 * data in the buffers. Note that it's not a thread, it's running
 * in signal time.
*/
void InputBehaviour::on_ai_completed_callback()
{
	this->on_ai_completed();
	this->set_state(StateReady);
}

/**
 * Requests the driver to stop sending callbacks and changes the device state
 * accordingly.
*/
void InputBehaviour::stop_callbacks()
{
	this->device().stop_callbacks(true);
	this->device().async_clear(0, 0);

	this->set_state(StateReady);
}


/** @brief Gets last voltage value buffer read on each port
 *  @param results Voltage values
 *  @param resSz Output parameter, how many data has been saved to results
 *  @param[out] time Timestamp, when the data was acquired.
 *  @returns Wheter an acquisition exists(true) or the given
 *           value is just garbage(false).
*/
bool InputBehaviour::get_last_values(std::vector<volts_type> & results, size_t &resSz, struct timeval& time) const
{
	assert(this->m_mode != ModeNone);

	InputBufferReadLock bca(m_bufferControl);
	if (!bca.ok())
		return false;
	
	const U16 * rawData = bca.read(bca.first_bucket(), time);
	this->bits_to_volts(
			&rawData[bca.last_bucket_size() - this->nchannels()],
			&results[0],
			this->nchannels() );
    
	resSz = this->nchannels();
	return true;
}


/** @brief Gets last voltage value for a given port
 * @param port Port number
 * @param result Voltage value
 * @param[out] time Timestamp, when the data was acquired.
 * @returns Wheter an acquisition exists(true) or the given
 *          value is just garbage(false).
*/
bool InputBehaviour::get_last(unsigned port, volts_type & result, struct timeval& time) const
{
	assert(this->m_mode != ModeNone);

	InputBufferReadLock bca(m_bufferControl);
	if (!bca.ok())
		return false;
	const U16 * rawData = bca.read(bca.first_bucket(), time);
	result = this->bits_to_volts(
			rawData[bca.last_bucket_size() - this->nchannels() + port]);
    
	return true;
}


/** @brief Gets last voltage value buffer read for a given port
 *  @param[in] port Port number
 *  @param[out] result Voltage values
 *  @param[out] realSize result.size() may be bigger than the real
 *              size of acquired data
 *  @param[out] time Timestamp of the last part of the buffer acquired.
 *  @returns Wheter an acquisition exists(true) or the given
 *           value is just garbage(false).
*/
bool InputBehaviour::get_last(
				unsigned port,
				std::vector<volts_type> & result,
				size_t &realSize,
				struct timeval& time) const
{
	assert(this->m_mode != ModeNone);

	const size_t channelSize = this->channel_samples_per_trigger();
	const size_t width = this->num_of_displayable_triggers();

	assert(result.size() == width*channelSize); // Maximum size needed
	realSize = 0;
	{
		InputBufferReadLock bca(m_bufferControl, width);
		if (!bca.ok())
			return false;

		size_t b = 0;
	
		int bucket = bca.first_bucket();
		do
		{
			const U16* rawData = bca.read(bucket, time);
		
			for(size_t n =0; n < channelSize; ++n) {
				result[b*channelSize + n] = 
				this->bits_to_volts(rawData[n*this->nchannels() + port]);
			}
			++b;
			realSize += channelSize;
		} while (bca.iterate(bucket));
	
		realSize -= (this->half_size() - bca.last_bucket_size())/this->nchannels();
	}

	// Maximum size previously allocated, is not
	// necessarily the real final size, if we are at
	// the very beggining of a continuous acquisition
	// for example
	// result.resize(realSize); 
	// oops, but valgrind says we spend too much time here

	return true;
}


/**
 * Helper for get_last(std::vector<long>) and get_last(std::vector<double>).
 * They do almost the same, so everything is done here. getLong==true get data
 * on resultl. getLong==false get data on resultd.
*/
bool InputBehaviour::get_last_big_aux(
								bool getLong, std::vector<long> & resultl,
								std::vector<volts_type> & resultd,
								size_t &realSize,
								size_t width,
								struct timeval& time) const
{
	const size_t sampleSize = this->half_size();

	if(getLong)
		resultl.resize(width*sampleSize);
	else
		resultd.resize(width*sampleSize);

	realSize = 0;
	{
		InputBufferReadLock bca(m_bufferControl, width);
		if (!bca.ok())
			return false;
		
		const size_t totalBuckets = bca.total();
		const size_t nchan = this->nchannels();
		const size_t channelSamplesPerBucket = this->channel_samples_per_trigger();
		const size_t channelSamples = channelSamplesPerBucket*totalBuckets;
		
		int bucket = bca.first_bucket();
		size_t bucketCount = 0;
		
		do {
			const U16* rawData = bca.read(bucket, time);
		
			//for(size_t x = 0; x < vsize; ++x) {
			for(size_t sample = 0; sample < channelSamplesPerBucket; ++sample) {
				for(size_t chan = 0; chan < nchan; ++chan) {
					const size_t origPos = chan + sample*nchan;
					const size_t resPos = \
						chan*channelSamples + bucketCount*channelSamplesPerBucket + sample;
					//const size_t resPosition = b*sampleSize + x + y*vsize;
					if(getLong)
						resultl[resPos] = rawData[origPos];
					else
						resultd[resPos] = this->bits_to_volts(rawData[origPos]);
				}
			}
			realSize += this->half_size();
			++bucketCount;
		} while (bca.iterate(bucket));
	
		realSize -= (this->half_size() - bca.last_bucket_size());
	}

	// Maximum size previously allocated, is not
	// necessarily the real final size, if we are at
	// the very beggining of a continuous acquisition
	// for example
	// But valgrind says we spend too many time here
	//if(getLong)
	//	resultl.resize(realSize);
	//else
	//	resultd.resize(realSize);

	return true;
}

/** @brief Gets last raw binary values buffer read for all the ports
 * @param[out] result raw binary values in ordered this way
 *               "ch0m0 ch0m1 .. ch0mN ch1m0 ch1m1"
 *               where ch=channel and m=sample
 * @param[out] resSz Size of the result. For optimization reasons, it does
 *                   necessarily match exactly result.size()
 * @param[out] time Timestamp of the last part of the buffer acquired.
 * @returns Wheter an acquisition exists(true) or the given
 *          value is just garbage(false).
 */
bool InputBehaviour::get_last(std::vector<long> & result, size_t &resSz, struct timeval& time) const
{
	static std::vector<volts_type> ___unused;
#if ADLD_GET_FULL_BUFFER_ == 0
	size_t width = 0;
	return false;
#elif ADLD_GET_FULL_BUFFER_ == 1
	size_t width = 1;
#else
	size_t width = this->num_of_displayable_triggers();
#endif
	return this->get_last_big_aux(true, result, ___unused, resSz, width, time);
}

/** @brief Gets last voltage values buffer read for all the ports
 * @param[out] result raw binary values in ordered this way
 *               "ch0m0 ch0m1 .. ch0mN ch1m0 ch1m1"
 *               where ch=channel and m=sample
 * @param[out] resSz Size of the result. For optimization reasons, it does
 *                   necessarily match exactly result.size()
 * @param[out] time Timestamp of the last part of the buffer acquired.
 * @returns Wheter an acquisition exists(true) or the given
 *          value is just garbage(false).
*/
bool InputBehaviour::get_last(std::vector<volts_type> & result, size_t &resSz, struct timeval& time) const
{
	static std::vector<long> ___unused;
#if ADLD_GET_FULL_BUFFER_ == 0
	size_t width = 0;
	return false;
#elif ADLD_GET_FULL_BUFFER_ == 1
	size_t width = 1;
#else
	size_t width = this->num_of_displayable_triggers();
#endif
	return this->get_last_big_aux(false, ___unused, result, resSz, width, time);
}


///
/// @todo It should depend on timebase. See comments
void InputBehaviour::adapt_sample_rate(unsigned long sampleRate)
{
	assert(this->params() >= 0);
	
	if (!sampleRate) {
		info() << "InputBehaviour: WARN Given SampleRate is 0" << std::endl;
		sampleRate = 1;//40000000/65535;
	}

	this->m_sampIntrv = this->params()->timeBase / sampleRate;
	
	// See documentation (D2K_AI_ContReadChannel)
	// It should not be the same for sampIntrv and scanIntrv
	// and for internal or external timebase
	if	(this->m_sampIntrv < this->params()->minInputScanIntrv)
		this->m_sampIntrv = this->params()->minInputScanIntrv;
	if	(this->m_sampIntrv > this->params()->maxInputScanIntrv)
		this->m_sampIntrv = this->params()->maxInputScanIntrv;

	sampleRate = this->params()->timeBase / this->m_sampIntrv;
	this->set_sample_rate(sampleRate);
	
	info()	<< "InputBehaviour: sampIntrv set to " << this->m_sampIntrv 
			<< " ~= " << this->params()->timeBase << "/"
			<< sampleRate << std::endl;	
}

/*virtual*/ bool InputBehaviour::read_raw_data(long *& rawData, long &x, long &y, struct timeval& time) const
{
	BaseBehaviour::AttributeBuffers &buf = this->m_attrBuffers;

	if(this->get_state() == InputBehaviour::StateNone) 
		return false;
	y = this->nchannels();
	size_t readSz;
	if ( this->get_last(buf.rawDataValues, readSz, time) ) {
		if (!readSz) {
			rawData = 0;
			x = 0;
			return true;
		}
		rawData = &buf.rawDataValues[0];
		x = readSz/this->nchannels();
		return true;
	} else
		return false;
}

/*virtual*/ bool InputBehaviour::read_double_data(volts_type *& rawData, long &x, long &y, struct timeval& time) const
{
	BaseBehaviour::AttributeBuffers &buf = this->m_attrBuffers;

	if(this->get_state() == StateNone)
		return false;

	size_t readSz;
	y = this->nchannels();
	x = 0;
	rawData = 0;
	if (this->get_last(buf.doubleDataValues, readSz, time)) {
		if (readSz) {
			x = readSz / this->nchannels();
			rawData = &buf.doubleDataValues[0];
		}
		return true;
	} else
		return false;
}

/*virtual*/ bool InputBehaviour::read_scalar_value(ssize_t port, volts_type* &value, struct timeval& time) const
{
	BaseBehaviour::AttributeBuffers &buf = this->m_attrBuffers;

	if(this->get_state() == StateNone) 
		return false;

	value = &buf.scalarChannelValues[port];

	// Really read last value
	return this->get_last(port, *value, time);
}

/*virtual*/ bool InputBehaviour::read_last_values(volts_type* &data, long &x, struct timeval& time) const
{
	BaseBehaviour::AttributeBuffers &buf = this->m_attrBuffers;
	std::vector<volts_type> &res = buf.spectrumLastValues;

	res.resize(this->nchannels());
	size_t readSz = 0;
	
	if(this->get_state() == StateNone)
		return false;

	if (!this->get_last_values(res, readSz, time)) 
		return false;

	if (res.size()) {
		data = &res[0];
		x = readSz;
	} else {
		data = (volts_type*)0;
		x = 0;
	}
	return true;
}

/*virtual*/ bool InputBehaviour::read_channel_spectrum(ssize_t port, volts_type* &data, long &x, struct timeval& time) const
{
	BaseBehaviour::AttributeBuffers &buf = this->m_attrBuffers;
	if(this->get_state() == StateNone)
		return false;

	std::vector<volts_type> & res = buf.spectrumChannelValues[port];

	size_t readSz;
	if( this->get_last(port, res, readSz, time) ) {
		x = readSz;
		data = &res[0];
		return true;
	}
	return false;
}

/*virtual*/ void InputBehaviour::start()
{
	this->start_ai();
}

/*virtual*/ void InputBehaviour::stop()
{
	this->stop_ai();
	this->device().status_warn_clear();
	this->status_warn_clear();
}

/*virtual*/ void InputBehaviour::set_reference_ground(const std::string &referenceGround)
{
	this->device().set_reference_ground(referenceGround);

}

void InputBehaviour::overrun_check(bool overrun)
{
	/// @todo What to do if there's overrun?
	++(this->m_totalRuns);
	if (overrun) {
		++(this->m_overruns);
		error()	<< "OVERRUN! "
				<< (100.0*double(this->m_overruns)/this->m_totalRuns) 
				<< "%   "
				<< this->m_overruns
				<< "   " 
				<< this->m_totalRuns <<std::endl;
	}
}

/*virtual*/ void InputBehaviour::get_status_info(std::ostream & statusStream)
{
	BaseBehaviour::get_status_info(statusStream);
	if (this->m_overruns ) {
		statusStream	<< "\nOverruns: "
						<< this->m_overruns << " ("
						<< (100.0*double(this->m_overruns)/this->m_totalRuns) 
						<< "%)\n";
	}
	statusStream	<< "\nWarnings: " << std::endl
				<< this->status_warn_string() << std::endl
				<< this->device().status_warn_string();
}

/*virtual*/ void InputBehaviour::on_channel_samples_per_trigger_about_to_change(long chSamplesPerTrigger)
{
	U32 hsize = chSamplesPerTrigger*this->nchannels();
	this->device().check_valid_buffer_size(hsize);

	this->m_hsize = hsize;
}
