
#include "AdlHelp.h"
#include "InputDeviceInterface.h"
#include "AdlDeviceFactorySingleton.h"
#include <vector>
#include <cassert>
#include <cmath>

#define _OCLASSNAME "InputDevicePSDASK"
#include "odebug.h"

class InputDevicePSDASK : public InputDeviceInterface
{
protected:
	std::vector<U16>	m_buffer;
	bool				m_usingDblBuf;
	U16					m_range;
	U32					m_hsize;
public:

InputDevicePSDASK(CommonDeviceInterface* commonDevice) :
		InputDeviceInterface(commonDevice),
		m_range(0),
		m_usingDblBuf(false),
		m_hsize(0)
	{
	}

	~InputDevicePSDASK()
	{
		this->release_buffers();
	}

	virtual void release_buffers()
	{
		AI_AsyncDblBufferMode(this->handler(), (BOOLEAN)FALSE);
	}

	virtual bool supports_callback(CallbackEvent event) const
	{	
		return false;
	}

	virtual void set_callback(CallbackEvent ev, CallbackEventReceiver* rec)
	{
		// Callbacks are not supported in PSDASK now. Caller should have
		// checked supports_callback() before trying to run it here.
		assert(false);
	}

	virtual void read_channels_shot(U16* val)
	{
		for (size_t n=0; n< this->nchannels(); ++n) {
			adlink_assert(
				AI_ReadChannel(
						this->handler(),
						n,
						this->m_range,
						&val[n] ));
		}
	}

	virtual void set_reference_ground(const std::string & rg)
	{
		if (rg != "") {
			status_warn()	<< "Reference ground is not supported for this"
							<< " board and will be ignored" << std::endl;
		}
	}

	virtual void preconfigure_input_channels(U16 range)
	{
		this->m_range = range;

	}

	virtual void set_delay(unsigned delay, AdlinkDelaySource)
	{
		if (delay)
			this->throw_error(
				"Cant configure this board this way",
				"DELAY mode is unsupported, and so delay must be 0",
				_CFN_);
	}

// 	virtual void configure_ai(long totalShots, U32 trigMode)
// 	{	}

	virtual void clear_overrun_flag()
	{
		U16 status;
		AI_AsyncDblBufferOverrun(this->handler(), 1, &status);
	}

	virtual void configure_single_buffer(U32 hsize)
	{
		this->m_buffer.resize(hsize);
		this->m_hsize = hsize;
		this->m_usingDblBuf = false;
	}

	virtual void configure_double_buffer(U32 hsize)
	{
		this->m_hsize = hsize;
		adlink_assert(
					AI_AsyncDblBufferMode (this->handler(), (BOOLEAN)TRUE));
		this->m_usingDblBuf = true;
	}

	virtual bool cont_scan_channels(U32 scanIntrv)
	{
		/// @warning readCount has different meaning for 9116 and other devices,
		/// so its redefined in its own sublcass. But only 9116 has been tested.
		/// The wierd hsize=2*size stuff if using doubleBuffer is here bcos
		/// of 9116 experiences, and I on't really know if it also applies to
		/// others.
		U16 *buf = 0;
		U32 hsize = this->m_hsize;
		if (!this->m_usingDblBuf)
			buf = &this->m_buffer[0];
		else
			hsize = 2*hsize;

		info()	<< "AI_ContScanChannels ("
				<< handler() << ", "
				<< nchannels() - 1 << ", "
				<< this->m_range << ", "
				<< buf << ", "
				<< hsize << ", "
				<< double(scanIntrv) / this->nchannels() << ", "
				<< ASYNCH_OP << ")" << std::endl;

		return adlink_check(AI_ContScanChannels(
							this->handler(),
							this->nchannels() - 1,	// Last channel
							this->m_range,
							buf,				// Buffer
							hsize,				// readCount
							double(scanIntrv) / this->nchannels(),
							ASYNCH_OP));
	}

	virtual void async_clear(U32* startPos, U32* count)
	{
		U32 _count;
		if (!count)
			count = &_count;
		if (startPos)
			*startPos = 0;
		AI_AsyncClear(this->handler(), count);
	}

	virtual void dbl_buf_half_ready(BOOLEAN *halfReady, BOOLEAN *finished)
	{
		AI_AsyncDblBufferHalfReady(this->handler(), halfReady, finished);
	}

	virtual void transfer_from_buf
						(U16* rawData, U32 first, U32 count, U16 halfId) const
	{
		assert(halfId <= 1);
		assert(this->m_usingDblBuf?true:(halfId==0));
		assert(first == 0); // PSDASK does not support anything different...
							// first should be startPos param from async_clear

		if (!this->m_usingDblBuf) {
			for(size_t n=0; n<count; ++n)
				rawData[n] = this->m_buffer[n];
		} else {
			AI_AsyncDblBufferTransfer(this->handler(), rawData);
		}
	}

	virtual const U16* get_buf(U16 halfId) const
	{
		return 0;
	}

	virtual void last_buffer_handled()
	{
		// It crashes when running. Now it seems to work without it
		// (in fact the function exists but it's not even documented) 
		// so we'll stay this way.
		// AI_AsyncDblBufferHandled(this->handler());
	}

	virtual bool has_overrun()
	{
		U16 status;
		AI_AsyncDblBufferOverrun(this->handler(), 0, &status);
		return status;
	}

	virtual double bits_to_volts(U16 range, U16 bits) const
	{
		double volts;
		adlink_assert(AI_VoltScale(this->handler(), range, bits, &volts));
		return volts;
	}

	virtual bool bits_to_volts(U16 range, const U16* bits, double * volts, size_t count) const
	{
		return adlink_check(
				AI_ContVScale(
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
			AI_VoltScale(this->handler(), range, (I16)bits[n], &tmp);
			volts[n] = tmp;
		}
		return true;
	}

	virtual bool async_retrig_next_ready(bool & _finished, U16 &rdyTrigCnt)
	{
		// We should NOT be here...
		assert(false);
		// But if we get here in non debug mode, get noticed!
		error() <<	"async_retrig_next_ready is not supported by PSDASK"
					" devices, so it could not be performed." << std::endl;
		adlink_assert(-1);
		return true;
	}

	virtual bool async_check_finished(U32 & accesCnt)
	{
		BOOLEAN finished;
		adlink_assert(
			AI_AsyncCheck(
				this->handler(), &finished, &accesCnt) );
		return finished;
	}

};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Concrete: 9116
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

class InputDevicePSDASK9116 : public InputDevicePSDASK
{
	U16 m_referenceGround;
	U32 m_delay;
	AdlinkDelaySource m_delaySource;
public:

	InputDevicePSDASK9116(CommonDeviceInterface* commonDevice) :
		InputDevicePSDASK(commonDevice),
		m_referenceGround(0),
		m_delay(0),
		m_delaySource(DelayTimebase)
	{
	}

	virtual void set_reference_ground(const std::string & rg)
	{
		// 9116 CfgCtrl
		// (1) A/D Polarity Control
		//     P9116_AI_BiPolar
		//     P9116_AI_UniPolar
		// (2) A/D Channel Input Mode
		//     P9116_AI_SingEnded
		//     P9116_AI_Differential
		// (3) Common Mode Selection
		//     P9116_AI_LocalGND: Local Ground of cPCI-9116
		//     P9116_AI_UserCMMD: User defined Common Mode
		this->m_referenceGround = 0;
		if (rg == "AI_RSE")
			this->m_referenceGround = P9116_AI_SingEnded;
		else if (rg == "AI_DIFF")
			this->m_referenceGround = P9116_AI_Differential;
		//else if (rg == "AI_NRSE")
		//	this->m_referenceGround = AI_NRSE;
		else {
			info()	<< "ReferenceGround set to default." << std::endl;
			status_warn()	<< " BAD ReferenceGround: Valid values"
							<< " for this board are AI_RSE and AI_DIFF"
							<< std::endl;
			return;
		}

		/// This depends on m_range. To be sure it is set, we do it
		/// in configure_ai():
		/// - this->m_referenceGround |= P9116_AI_UniPolar;

		info() << "Reference ground set to " << rg << std::endl;
	}

	virtual bool cont_scan_channels(U32 scanIntrv)
	{
		info()	<< "AI_9116_CounterInterval ("
				<< handler() << ", "
				<< scanIntrv << ", "
				<< scanIntrv / this->nchannels() << ")" << std::endl;

		if (!adlink_check(AI_9116_CounterInterval(
				this->handler(),
				scanIntrv,							// ScanIntrv
				scanIntrv / this->nchannels()	)))	// SampIntrv
			return false;

		// NOT JUST:
		// return InputDevicePSDASK::cont_scan_channels(scanIntrv);
		// Because readCount Parameter does not mean the same for 9116
		// and other Adlink boards
		U16 *buf = 0;
		U32 hsize = this->m_hsize;

		if (!this->m_usingDblBuf)
			buf = &this->m_buffer[0];
		else 
			hsize = 2 * hsize;
		info()	<< "AI_ContScanChannels ("
				<< handler() << ", "
				<< nchannels() - 1 << ", "
				<< this->m_range << ", "
				<< buf << ", "
				<< hsize / this->nchannels() << ", "
				<< 1.0 << ", "
				<< ASYNCH_OP << ")" << std::endl;
		return adlink_check(AI_ContScanChannels(
							this->handler(),
							this->nchannels() - 1,	// Last channel
							this->m_range,
							buf,					// Buffer
							hsize/this->nchannels(),// readCount
							1.0,					// Ignored
							ASYNCH_OP ));
	}

	virtual void set_delay(unsigned delay, AdlinkDelaySource dm)
	{
		this->m_delay = delay;
		this->m_delaySource = dm;
	}

	virtual void configure_ai(long totalShots, U32 trigMode)
	{
		U32 postCnt = 0;

		// Translate trigMode (D2K bits) to trgCtrl (PSDASK):
		U16 trgCtrl = 0;
		{
			const U32 maskSrc =   DAQ2K_AI_TRGSRC_SOFT | DAQ2K_AI_TRGSRC_ANA
			                    | DAQ2K_AI_TRGSRC_ExtD | DAQ2K_AI_TRSRC_SSI;
			const U32 maskModes =   DAQ2K_AI_TRGMOD_POST | DAQ2K_AI_TRGMOD_DELAY
			                      | DAQ2K_AI_TRGMOD_PRE | DAQ2K_AI_TRGMOD_MIDL;
	
			U32 ts = trigMode & maskSrc;
			if((ts & DAQ2K_AI_TRGSRC_SOFT) == ts) {
				trgCtrl = P9116_TRGMOD_SOFT;
			} else if ((ts & DAQ2K_AI_TRGSRC_ExtD) == ts) {
				U32 tm = trigMode & maskModes;
				if ((tm & DAQ2K_AI_TRGMOD_POST) == tm)
					trgCtrl |= P9116_TRGMOD_POST;
				if ((tm & DAQ2K_AI_TRGMOD_DELAY) == tm) {
					trgCtrl |= P9116_TRGMOD_DELAY;
					if (this->m_delaySource == DelayTimebase)
						trgCtrl |= P9116_AI_DlyInTimebase;
					if (this->m_delaySource == DelaySamples)
						trgCtrl |= P9116_AI_DlyInSamples;
					postCnt = this->m_delay;
				} if ((tm & DAQ2K_AI_TRGMOD_PRE) == tm)
					trgCtrl |= P9116_TRGMOD_PRE;
				if ((tm & DAQ2K_AI_TRGMOD_MIDL) == tm)
					trgCtrl |= P9116_TRGMOD_MIDL;
			} else {
				throw_error(
						"Cant configure 9116 board this way",
						"It only supports Software and ExtD triggers",
						_CFN_);	
			}
			
			if (trigMode & DAQ2K_AI_ReTrigEn == DAQ2K_AI_ReTrigEn)
				trgCtrl |= P9116_AI_ReTrigEn;
		}

		// For 9116, an unipolar range is not enough. We must ALSO tell it
		// it's univpolar AGAIN at AI_9116_Config
		U16 refGround = this->m_referenceGround;
		switch (this->m_range) {
			case AD_U_10_V:
			case AD_U_5_V:
			case AD_U_2_5_V:
			case AD_U_1_25_V:
				refGround |= P9116_AI_UniPolar;
				break;
			case AD_B_5_V:
			case AD_B_2_5_V:
			case AD_B_1_25_V:
			case AD_B_0_625_V:
				refGround |= P9116_AI_BiPolar;
				break;
			default:
				assert(false);
		}

		info()	<< "AI_9116_Config( "
				<< this->handler() << ", "
				<< refGround << ","
				<< trgCtrl << ","
				<< postCnt << ","
				<< 0 << ","
				<< totalShots << ")" << std::endl;

		trgCtrl |= P9116_AI_DMA;
		adlink_assert(
			AI_9116_Config(
				this->handler(),
				refGround,	// ConfigCtrl
				trgCtrl,	// TrigCtrl
				postCnt,	// PostCnt
				0,			// MCnt
				totalShots	// ReTrgCnt
			));

		info() << "AI_9116_Config OK" << std::endl;
	}
	
	virtual void check_valid_buffer_size(U32 hsize) const
	{
		U32 totalDriverKB;
		adlink_assert(AI_InitialMemoryAllocated(handler(), &totalDriverKB));
		
		const size_t maxDriverSamples = totalDriverKB*1024/2; // 2: sizeof<ushort>
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

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Factory:
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

/*static*/ DevicePartInterface* AdlDeviceFactorySingleton::create_input_device_psdask(CommonDeviceInterface* commonDevice)
{
	if (commonDevice->type_params()->type != PCI_9116) {
		throw AdlExcept(
			"Unsupported device",
			"Sorry, the only Adlink PSDASK Device supported for AnalogInput"
			" by this server is PCI-9116",
			_CFN_,
			0);
	}
	return new InputDevicePSDASK9116( commonDevice );
}
