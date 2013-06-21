
#include "AdlHelp.h"
#include "InputDeviceInterface.h"
#include <signal.h>
#include <vector>
#include <cassert>
#include <map>


#define _OCLASSNAME "InputDeviceD2K"
#include "odebug.h"

///
/// Implementation of InputDeviceInterface for D2K devices.
///
class InputDeviceD2K : public InputDeviceInterface
{
	std::vector<U16>	m_chanList; // To be used with D2K_AI_ScanReadChannels
	bool				m_usingDblBuf;
	std::vector<U16>	m_doubleBuf[2];
	U16					m_bufId;

	U32					m_hsize;
	U16					m_referenceGround;

	U32					m_delay;
	AdlinkDelaySource m_delaySource;

	/// @brief Singleton to the only InputD2KDevice that will get the signals.
	///
	/// Signals are sent to a function without more information. We cannot
	/// device which device produced the signal, so we must force only
	/// one device per process. This singleton represents that device.
	static InputDeviceD2K * s_device;

	/// Only one callback can be running at the same time, this is to
	/// ensure this and to ensure that none is running when the object
	/// is destroyed.
	static bool __callbackRunning;

	typedef std::map<CallbackEvent, CallbackEventReceiver*> CallbackMap;
	/// Maps a callback event to a callback receiver object. See set_callback()
	/// and callbackDispatcher
	CallbackMap m_callbackMap;

	/// We are not as interested in the signal number that produces the
	/// callback but the eventType we registered, why is Adlink sending us
	/// a signal? That's why we define different callbacks for each possible
	/// event type (3rd D2K_AI_EventCallBack parameter), but using a single
	/// function to do so.
	/// @verbinclude NOTES_ON_SIGNALS.TXT
	template<int eventType>
	static void __callbackDispatcher(int signal)
	{
		CallbackEvent ev;

		// eventType is a constant, the compiler should be able to
		// optimize this easily...
		if ( eventType == DAQEnd ) {
			assert(signal == 59);
			ev = InputDeviceInterface_::EventCompleted;
		} else if ( eventType == DBEvent ) {
			assert(signal == 58);
			ev = InputDeviceInterface_::EventHalfBufferReady;
		} else if ( eventType == TrigEvent) {
			ev = InputDeviceInterface_::EventTrigger;
		} else {
			std::cerr 
				<< _CFN_ << ": Unknown handler "
				<< eventType << std::endl;
			return;
		}

		/// We don't want to get an EventCompleted event while checking
		/// another interrupt. (It may cause deadlocks) so we forbid it...
		sigset_t sigset, oldset;
		sigemptyset(&sigset);

		sigaddset(&sigset, 59); // EventCompleted
		// It won't happen, so it's unneeded:
		// - sigaddset(&sigset, 58); // EventHalfBufferReady
		sigprocmask(SIG_BLOCK, &sigset, &oldset);
		assert(!__callbackRunning);
		__callbackRunning = true;

		try {
			s_device->callbackDispatcher(ev);
		} catch (const std::exception & e) {
			error()	<< _CFN_
					<< " unhandled std exception in signal handler: "
					<< e.what() << std::endl;
			return;
		} catch ( ... ) {
			error()	<< _CFN_
					<< " unhandled unknown exception in signal handler"
					<< std::endl;
		}
		__callbackRunning = false;
		sigprocmask(SIG_SETMASK, &oldset, 0);
	}

	void callbackDispatcher(CallbackEvent ev)
	{
		/// @todo uff, al this stuff is not really pretty...
		/// I don't like s_device as cutre-singleton and m_callbackMap without
		/// mutex... Don't really know, real todo is "Think about it all"
		CallbackMap::iterator i = this->m_callbackMap.find(ev);
		if ( i != this->m_callbackMap.end() ) {
			i->second->on_event_received(ev);
		}
	}

public:

	InputDeviceD2K(CommonDeviceInterface* commonDevice) :
		InputDeviceInterface(commonDevice),
		m_usingDblBuf(false),
		m_bufId(0),
		m_hsize(0),
		m_referenceGround(0),
		m_delay(0),
		m_delaySource(DelayTimebase)
	{
	}

	~InputDeviceD2K()
	{
		this->release_buffers();
		this->stop_callbacks();
	}

	virtual void release_buffers()
	{
		// Not needed depending on last parameter of D2K_AI_Config
		D2K_AI_ContBufferReset(this->handler());

		D2K_AI_AsyncDblBufferMode (this->handler(), (BOOLEAN)FALSE);
		this->m_usingDblBuf = false;
	}

	bool supports_callback(CallbackEvent event) const
	{
		/// @todo only 2010, 2005, 2006, 2204, 2205, 2206, 2208, 2213, 2214, 2501, 2502
		return true;
	}

	/// Install an object that will get callback signals for an event. It
	/// can be used only once, this is one event can only be mapped to
	/// one receiver. To change the receiver, you will have to explicitly
	/// uninstall the previous one first.
	/// @param ev Callback event type
	/// @param rec Callback object. Use 0 to uninstall a callback object
	virtual void set_callback(CallbackEvent ev, CallbackEventReceiver* rec)
	{
		assert(this->supports_callback(ev));

		I16 mode;
		I16 eventType;

		mode = 1; // add
		if (rec == 0)
			mode = 0; // remove
		
		if ( ev == InputDeviceInterface_::EventCompleted )
			eventType = DAQEnd;
		else if ( ev == InputDeviceInterface_::EventHalfBufferReady )
			eventType = DBEvent;
		else if ( ev == InputDeviceInterface_::EventTrigger )
			eventType = TrigEvent;
		else
			this->throw_error(
							"Cannot install callback signal handler",
							"Unknown signal type!",
							_CFN_);

		s_device = this;

		//I16  D2K_AI_EventCallBack (
		//    U16 wCardNumber,
		//    I16 mode,
		//    I16 EventType,
		//    void (*callbackAddr)(int) );
		if ( mode == 0 ) {
			// Don't throw problems when deleting
			/// @todo maybe it should and then catch them...
			D2K_AI_EventCallBack ( this->handler(), 0, eventType, 0 );
			if (this->m_callbackMap.count(ev)) {
				CallbackEventReceiver*r = this->m_callbackMap[ev];
				this->m_callbackMap.erase(ev);
				r->on_event_disconnected(ev);
			}
		} else {
			if (this->m_callbackMap.count(ev))
				this->throw_error(
								"Cannot install callback signal handler",
								"There's already a handler registered for"
								" this signal!",
								_CFN_);
			this->m_callbackMap[ev] = rec;
			try {
				switch (eventType){
#define CASE_CALLBACK(EVENT) case EVENT: \
	{ \
		adlink_assert( D2K_AI_EventCallBack (  \
			this->handler(), mode, eventType, __callbackDispatcher<EVENT> ) ); \
		break; \
	}
					CASE_CALLBACK(DAQEnd)
					CASE_CALLBACK(DBEvent)
					CASE_CALLBACK(TrigEvent)
					default:
						assert(false);
				}
			} catch (...) {
				this->m_callbackMap.erase(ev);
				throw;
			}
		}
	}

	/// Disconnect all callbacks.
	/// @param waitEnd If it is true, after all the callbacks have been
	/// disconencted it will wait until the last one has finished running.
	/// @warning Don't call this function with waitEnd=true from a signal
	/// callback or it will wait for itself to finish forever!!
	virtual void stop_callbacks(bool waitEnd=false)
	{
		this->set_callback(InputDeviceInterface_::EventCompleted, 0);
		this->set_callback(InputDeviceInterface_::EventHalfBufferReady, 0);
		this->set_callback(InputDeviceInterface_::EventTrigger, 0);

		if (!waitEnd)
			return;

		while(__callbackRunning) {
			/* just wait */
			sched_yield();
			std::cout << "  waiting callback to end" << std::endl;
		}
	}

	virtual void read_channels_shot(U16* val)
	{
		//std::cout << " DFC read_channels_shot" << std::endl;
		adlink_assert(
			D2K_AI_ScanReadChannels(
					this->handler(),
					this->nchannels(),	// Total channels
					&this->m_chanList[0],
					val ));
	}

	virtual void set_reference_ground(const std::string & rg)
	{
		//   AI_RSE : Referenced single ended mode ( 64chs common to ground
		//           system on board) (default value)
		//  AI_DIFF : Differential mode
		//  AI_NRSE : Non-referenced single ended mode ( 64chs common to
		//            AISENSE pin )
		assert(AI_RSE == 0);
		this->m_referenceGround = 0;
		if ((rg == "AI_RSE") || (rg==""))
			this->m_referenceGround = AI_RSE;
		else if (rg == "AI_DIFF")
			this->m_referenceGround = AI_DIFF;
		else if (rg == "AI_NRSE")
			this->m_referenceGround = AI_NRSE;
		else {
			info()	<< "ReferenceGround set to default." << std::endl;
			info() << "It was supposed to be set to " << rg << std::endl;
			status_warn()	<< " BAD ReferenceGround (\"" << rg << "\"):"
							<< " Valid values for this board are"
							<< " AI_RSE, AI_NRSE and AI_DIFF"
							<< std::endl;
			return;
		}
		info() << "Reference ground set to " << rg << std::endl;
	}

	virtual void preconfigure_input_channels(U16 range)
	{
		U16 gnd = this->m_referenceGround;
		for(I16 channel = 0; channel < this->nchannels(); ++channel) {
			info() << "SETTING INPUT CHANNEL " << channel << std::endl;
			adlink_assert(
					D2K_AI_CH_Config (this->handler(), channel, range | gnd));
		}
	}

	virtual void set_delay(unsigned delay, AdlinkDelaySource dm)
	{
		this->m_delay = delay;
		this->m_delaySource = dm;
	}

	virtual void configure_ai(long totalShots, U32 trigMode)
	{
		if (this->m_chanList.size() != this->nchannels()) {
			this->m_chanList.resize(this->nchannels());
			for(unsigned n = 0; n < this->nchannels(); ++n)
				this->m_chanList[n] = n;
		}

		U32 midOrDlyScans = 0;

		const U32 maskModes =   DAQ2K_AI_TRGMOD_POST | DAQ2K_AI_TRGMOD_DELAY
		                      | DAQ2K_AI_TRGMOD_PRE  | DAQ2K_AI_TRGMOD_MIDL;
		U32 tm = trigMode & maskModes;
		if ((tm & DAQ2K_AI_TRGMOD_DELAY) == tm) {
			midOrDlyScans = this->m_delay;
			if (this->m_delaySource == DelayTimebase)
				trigMode |= DAQ2K_AI_Dly1InTimebase;
			if (this->m_delaySource == DelaySamples)
				trigMode |= DAQ2K_AI_Dly1InSamples;
		} else if ((tm & DAQ2K_AI_TRGMOD_MIDL) == tm) {
			midOrDlyScans = 2; /// @todo Get num of data after the trigger for middle trigger mode from somewhere
		}
        /*
		printf("D2K_AI_Config (%d, %d, %d, %d, %d, %d, %d);\n",
					this->handler(), 
					0,
					trigMode,
					midOrDlyScans,
					0,
					totalShots,
					(BOOLEAN)0);
        */

		adlink_assert(
			D2K_AI_Config (
					this->handler(), 
					0,
					trigMode,
					midOrDlyScans,
					0,
					totalShots,
					(BOOLEAN)0) );

		printf("Used Trigger Mode       = 0x%x (%d) with:\n",trigMode,trigMode);
		printf("  D2K_AI_TRGMOD_PRE     = 0x%x\n", DAQ2K_AI_TRGMOD_PRE);
		printf("  D2K_AI_TRGMOD_DELAY   = 0x%x\n", DAQ2K_AI_TRGMOD_DELAY);
		printf("  D2K_AI_TRGMOD_MIDL    = 0x%x\n", DAQ2K_AI_TRGMOD_MIDL);
		printf("  D2K_AI_TRGSRC_SOFT    = 0x%x\n", DAQ2K_AI_TRGSRC_SOFT);
		printf("  D2K_AI_TRGSRC_ExtD    = 0x%x\n", DAQ2K_AI_TRGSRC_ExtD);
		printf("  D2K_AI_TrgPositive    = 0x%x\n", DAQ2K_AI_TrgPositive);
		printf("  D2K_AI_ReTrigEn       = 0x%x\n", DAQ2K_AI_ReTrigEn);
		printf("  D2K_AI_Dly1InSamples  = 0x%x\n", DAQ2K_AI_Dly1InSamples);
		printf("  D2K_AI_Dly1InTimebase = 0x%x\n", DAQ2K_AI_Dly1InTimebase);
		fflush(stdout);
	}

	virtual void clear_overrun_flag()
	{
		// Tested: status is NEVER modified when clearing_overrun_flag
		U16 status;
		D2K_AI_AsyncDblBufferOverrun(this->handler(), 1, &status);
	}

	virtual void configure_single_buffer(U32 hsize)
	{
		assert(hsize != 0);
		this->m_hsize = hsize;
		this->m_doubleBuf[0].resize(this->m_hsize);

		adlink_assert(
			D2K_AI_ContBufferSetup(
						this->handler(),
						&this->m_doubleBuf[0][0],
						this->m_hsize,
						&this->m_bufId));
        //DFC20121122
        //printf("D2K_AI_ContBufferSetup (SINGLE BUFFER) DFC (%d, m_hsize = %d, bufferId=%d m_dubleBuff = %x)\n", this->handler(), this->m_hsize, this->m_bufId, &this->m_doubleBuf[0][0]);
		this->m_usingDblBuf = false;
	}

	virtual void configure_double_buffer(U32 hsize)
	{
		assert(hsize != 0);
		this->m_hsize = hsize;
		adlink_assert(
			D2K_AI_AsyncDblBufferMode (this->handler(), (BOOLEAN)TRUE));
	
		this->m_doubleBuf[0].resize(this->m_hsize);
		this->m_doubleBuf[1].resize(this->m_hsize);
	
        //printf("D2K_AI_ContBufferSetup (DOUBLE BUFFER !!!!!) DFC (%d, m_hsize = %d, bufferId=%d m_dubleBuff = %x)\n", this->handler(), this->m_hsize, this->m_bufId, &this->m_doubleBuf[0][0]);
		// There's no need to call D2K_AI_ContBufferReset later because
		// last parameter of D2K_AI_Config is TRUE.
		adlink_assert(
						D2K_AI_ContBufferSetup (
										this->handler(),
										&this->m_doubleBuf[0][0],
										this->m_hsize,
										&this->m_bufId));

		adlink_assert(
						D2K_AI_ContBufferSetup (
										this->handler(),
										&this->m_doubleBuf[1][0],
										this->m_hsize,
										&this->m_bufId));
		this->m_usingDblBuf = true;
	}

	virtual bool cont_scan_channels(U32 scanIntrv)
	{
		U16 bufId = 0;
		if (!this->m_usingDblBuf)
			bufId = this->m_bufId;


		info()	<< "D2K_AI_ContScanChannels ("
				<< handler() << ", "
				<< nchannels() - 1 << ", "
				<< bufId << ", "
				<< this->m_hsize/this->nchannels() << ", "
				<< scanIntrv << ", "
				<< scanIntrv / this->nchannels() << ", "
				<< ASYNCH_OP << ")" << std::endl;

		return adlink_check(D2K_AI_ContScanChannels(
			this->handler(),
			this->nchannels() - 1,			// 2nd" Largest channel
											// number. The first is 0.
			bufId,							// BufferID, 0 if doing doubleBuffer
			this->m_hsize/this->nchannels(),// in DbBuf, size(in samples)
											// allocated for each device in the
											// circular buf. Multiple of 2
			scanIntrv,						// Scan interval, between 
											// to scan sequences
			scanIntrv / this->nchannels(),	// Sample interval
			ASYNCH_OP						// Sync mode
		));
	}

	virtual void async_clear(U32* startPos, U32* count)
	{
		U32 _startPos, _count;
		if (!count)
			count = &_count;
		if (!startPos)
			startPos = &_startPos;
		D2K_AI_AsyncClear(this->handler(), startPos, count);
	}

	virtual void dbl_buf_half_ready(BOOLEAN *halfReady, BOOLEAN *finished)
	{
		D2K_AI_AsyncDblBufferHalfReady(this->handler(), halfReady, finished);
	}

	virtual void transfer_from_buf
						(U16* rawData, U32 first, U32 count, U16 halfId) const
	{
        //DFC looks like this one gets the values from the card buffer into the attribute buffer ???  
		// first should be startPos param from async_clear
        //printf("DFC transfer_from_buf(SINGLE BUFFER)  (first=%d, rowData = %x, count=%d halfID = %d)\n", first, rawData, count, halfId);

		assert(halfId <= 1);
		assert(this->m_usingDblBuf?true:(halfId==0));

		for(size_t n=0; n<count; ++n)
			rawData[n] = this->m_doubleBuf[halfId][n + first];
	}

	virtual const U16* get_buf(U16 halfId) const
	{
		assert(halfId <= 1);
		assert(this->m_usingDblBuf?true:(halfId==0));

		return &this->m_doubleBuf[halfId][0];
	}

	virtual void last_buffer_handled()
	{
		assert(this->m_usingDblBuf);
		D2K_AI_AsyncDblBufferHandled(this->handler());
	}

	virtual bool has_overrun()
	{
		assert(this->m_usingDblBuf);
		U16 status;
		D2K_AI_AsyncDblBufferOverrun(this->handler(), 0, &status); //checking overrun status
		return status;
	}

	virtual double bits_to_volts(U16 range, U16 bits) const
	{
		double volts;
		D2K_AI_VoltScale(this->handler(), range, bits, &volts);
		return volts;
	}

	virtual bool bits_to_volts(U16 range, const U16* bits, double * volts, size_t count) const
	{
		return adlink_check(
					D2K_AI_ContVScale(
						this->handler(),
						range,
						const_cast<U16*>(bits),
						volts,
						count));
	}
	
	virtual bool bits_to_volts(U16 range, const U16* bits, float * volts, size_t count) const
	{
		double tmp;
		for(size_t n=0; n < count; ++n) {
			D2K_AI_VoltScale(this->handler(), range, bits[n], &tmp);
			volts[n] = tmp;
		}
		return true;
	}

	// Yes it may seem strange, but D2K_AI_AsyncReTrigNextReady rdyTrigCnt is
	// 16 bits and D2K_AI_AsyncCheck is 32
	virtual bool async_retrig_next_ready(bool & _finished, U16 &rdyTrigCnt)
	{
		BOOLEAN finished, trgReady;
		adlink_assert(
			D2K_AI_AsyncReTrigNextReady(
				this->handler(), &trgReady, &finished, &rdyTrigCnt) );
		_finished = finished;
        // DFC no callbacks 
		//std::cout << "async_retrig_next_ready " << finished << " " << trgReady << " " << rdyTrigCnt << std::endl;
		return trgReady;
	}

	virtual bool async_check_finished(U32 & accesCnt)
	{
		BOOLEAN finished;
		adlink_assert(
			D2K_AI_AsyncCheck(
				this->handler(), &finished, &accesCnt) );
        //printf ("async_check_finished %d\n\r", accesCnt);//DFC borrame
		return finished;
	}
	
	virtual void check_valid_buffer_size(U32 hsize) const
	{
		U32 totalDriverKB;
		adlink_assert(D2K_AI_InitialMemoryAllocated(handler(), &totalDriverKB));
		
		const size_t maxDriverSamples = totalDriverKB*1024/2; // 2: sizeof<ushort>
        //printf("DFC maxDriverSamples = %d  hsize=%d /n/r",maxDriverSamples, hsize);

		if (hsize > maxDriverSamples) {
			std::stringstream ss;
			ss  << "Total requested size(" << (2*hsize/1024) << " KB)"
				<< " exceeds the limit configured for the board"
				<< "(" << totalDriverKB << " KB). Please, check that the"
				<< " variable  AI_PAGE in driver configuration file is"
				<< " at least " << (2*hsize/(4*1024)) << ".";
			throw_error("Adlink ERROR", ss.str(), _CFN_);
		}
	}
};

/*static*/ InputDeviceD2K::InputDeviceD2K * InputDeviceD2K::s_device = 0;
/*static*/ bool InputDeviceD2K::__callbackRunning = false;

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Factory:
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

/*static*/ DevicePartInterface* AdlDeviceFactorySingleton::create_input_device_d2k(CommonDeviceInterface* commonDevice)
{
	return new InputDeviceD2K( commonDevice );
}
