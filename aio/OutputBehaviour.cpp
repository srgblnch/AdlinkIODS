
#include "OutputBehaviour.h"
#include "AdlHelp.h"
#include "d2kdask.h"
#include "dask.h"
#include <sstream>

#include "AdlinkCommon.h"

#define _OCLASSNAME "OutputBehaviour"
#include "odebug.h"

#include "OutputDeviceInterface.h"

using namespace std;

OutputDeviceInterface & OutputBehaviour::device()
{	return static_cast<OutputDeviceInterface&>(BaseBehaviour::device()); }
const OutputDeviceInterface& OutputBehaviour::device() const
{	return dynamic_cast<const OutputDeviceInterface&>(BaseBehaviour::device()); }

OutputBehaviour::OutputBehaviour(
			AdlDeviceFactorySingleton::AdlBoardParams* p,
			const std::string & boardId,
			StatusObserver* ob
			) :
	m_rawData(0),
	m_aoBuf(0),
	m_size(0),
	BaseBehaviour(p, AdlDeviceFactorySingleton_::ModeAnalogOutput, boardId, ob)
{
	// Creating Array with the identifiers of the buffered channels
	for (unsigned  h=0; h<nchannels(); h++)
		m_chanBufIDs[h] = h;

	/// @todo this should be provided by OutputDeviceInterface (or probably
	/// some bigger IODeviceInterface)
	// If using internal reference, these are the only valid values
	static unsigned okRanges[] = {	AD_B_10_V, AD_U_10_V };

	std::set<unsigned> _okRanges;
	for(size_t n=0; n<sizeof(okRanges)/sizeof(okRanges[0]); ++n)
		_okRanges.insert(okRanges[n]);

	this->range().set_valid_ranges(_okRanges);
	this->range().set_range_id(okRanges[0]);
}

OutputBehaviour::~OutputBehaviour()
{
	this->stop_ao(true);
	delete [] m_rawData;
	delete [] m_aoBuf;
}

/* ---------------------------------------------------------------------------
	ADLINK C++ OBJECT METHODS
/* ---------------------------------------------------------------------------*/

/*virtual*/ void OutputBehaviour::update_state_fast()
{
	if (this->get_state() != StateRunning)
		return;

	// Continuous acquisition. Else StateRunning shold not live for more than
	// just one method call, and so should never get here
	assert(this->get_state() == StateRunning);
	assert(!this->is_input());
	assert(m_size / nchannels() == this->channel_samples_per_trigger());
	assert(this->channel_samples_per_trigger() > 1);

	bool isStopped = false;
	U32 aoCount;

	this->device().async_check(isStopped, aoCount);
	m_aoCount = aoCount;

	if (isStopped) {
		info() << "OutputBehaviour:check_ao(): task has finished"  << std::endl;
		info() << "m_aoCount = " << m_aoCount << std::endl;
		this->set_state(StateReady);
		this->update_status();
	}
}

void OutputBehaviour::release_ao()
{
	m_aoCount = this->device().async_clear();
	this->device().reset_buffers();

	this->common_device()->leave_shared_control(this);
}

/**-----------------------------------------------------------------------------
 ******************************************************************************
 * CONTINUOUS OUTPUT
 ******************************************************************************
------------------------------------------------------------------------------*/
void OutputBehaviour::start_continuous_output(bool firstStart)
{
	// Compatability table
	//--------------------
	// D2K_AO_ContBufferCompose:	2010, 2005, 2006, 2204, 2205, 2206, 2214, 2501, 2502
	// D2K_AO_ContBufferComposeAll:	2010, 2005, 2006, 2204, 2205, 2206, 2214, 2501, 2502
	// ContWriteMultiChannel:		2010, 2005, 2006, 2204, 2205, 2206, 2214
	// ContWriteChannel: 			2010, 2005, 2006, 2204, 2205, 2206, 2214
	// D2K_AO_Group_WFM_Start:		2501, 2502

	if (!this->device().supports_continuous_output()) {
		throw_error(
				"Unsupported device",
				"Sorry, this board does not support continuous output.\n"
				"Set ChannelSamplesPerTrigger=1 or use a different card.",
				_CFN_);
	}

	if (firstStart) {
		assert(m_aoBuf == 0);

		/// @bug This is part of a little patch for an undocumented unexpected
		/// behaviour:It may - unlikely - be a bug in our device or in
		/// Adlink library.
		/// If it's an Adlink problem, if they solve it we may have problems
		/// here. Even worse, I originally implemented this patch differently,
		/// and now I have had to change it again...
		/// Now it consists in having m_size = 2*m_size if we are using
		/// DA_Group_A or DA_Group_B, but not if using DA_Group_AB
///@todo si va lo altre FORA		U32 buffer_size = this->m_size;

		// It's documented... NOWHERE, but D2K_AO_ContBufferCompose needs that the buffer
		// used by cont_write_channels to be TOTAL_BOARD_CHANNELS*channel_samples
		// instead of USED_CHANNELS*channel_samples...
		/// @todo si aixo es veritat, la comprovacio del tamany alocat al driver tb la he de variar i tal per donar missatges amb mes sentit... comrpovacio a on_channel_samples_per_trigger_about_to_change
		U32 buffer_size = this->channel_samples_per_trigger()*this->device().type_params()->aoChannels;
		U32 declared_buffer_size = buffer_size;

		for (int chan=0; chan<this->nchannels(); ++chan) {
			info() << "Setting up channel: " <<  chan << std::endl;
			this->device().configure_channel_range(chan, this->range().range_id());
		}

		if (this->device().uses_wfm_generation()) {
			this->device().set_wfm_group(m_chanBufIDs, this->nchannels());

			/// @todo Solve, some day. Why on earth I have to do it? don't
			/// know, but only this way it works...
			/// @bug This is part of a little patch for an undocumented unexpected
			/// behaviour:It may - unlikely - be a bug in our device or in
			/// Adlink library.
			/// If it's an Adlink problem, if they solve it we may have problems
			/// here. Even worse, I originally implemented this patch differently,
			/// and now I have had to change it again...
			/// Now it consists in having m_size = 2*m_size if we are using
			/// DA_Group_A or DA_Group_B, but not if using DA_Group_AB
			/// Where m_size is the size of the buffer passed to
			/// D2K_AO_ContBufferSetup
			switch(nchannels()) {
				case 1:
				case 2:
				case 3:
				case 4:
					declared_buffer_size = 2 * nchannels() * this->channel_samples_per_trigger();
					break;
				case 5:
				case 6:
				case 7:
					throw_error("Adlink ERROR", "We don't know how to make Adlink 2502 to work with NumOfChannels other than (1,2,3,4,8)", _CFN_);
					break;
				case 8:
					declared_buffer_size = nchannels() * this->channel_samples_per_trigger();
					break;
			}

		}

		m_aoBuf = new U16[buffer_size];
		m_bufId = this->device().cont_buffer_setup(m_aoBuf, declared_buffer_size);

		this->ao_config();
	}

/// @todo pero que cony, algu enten aixo?!?!? on ho diu, que chanBuf hagi de ser un array de punters al principi de cada canal? i on li dic quants canals tinc i tal??

	for (int chan=0; chan<nchannels(); ++chan)
		this->device().cont_buffer_compose(chan, this->channel_samples_per_trigger(), m_aoBuf, & this->bits_at(chan, 0) );

	info () << "OutputBehaviour: Using Continuous Analog Output" << endl;

	this->device().cont_write_multi_channels(
					this->nchannels(),                   // U16 wNumChans
					m_chanBufIDs,                        // U16 *pwChans
					m_bufId,                             // U16 bufId
					this->channel_samples_per_trigger(), // U32 dwUpdateCount
					this->num_of_triggers(),             // U32 wIterations
					m_sampIntrv,                         // U32 dwCHUI
					this->infinite_retrigger()>0?1:0,
					ASYNCH_OP );                         // U16 wSyncMode

	this->set_state(StateRunning);
}


void OutputBehaviour::ao_config()
{
	long totalTriggers = this->num_of_triggers();
	U32 triggerMode = this->trigger_mode().get_mode();

	if (totalTriggers != 1) {
		if  ((triggerMode & DAQ2K_DA_TRGMOD_POST != DAQ2K_DA_TRGMOD_POST)
		  && (triggerMode & DAQ2K_DA_TRGMOD_DELAY != DAQ2K_DA_TRGMOD_DELAY))
			throw_error("Invalid_Mode", "Retriggered acquisition (NumOfTriggers>1) is only avialble for TriggerMode POST and DELAY.", _CFN_);
		triggerMode |= DAQ2K_DA_ReTrigEn;
	} else
		totalTriggers = 0;

	if (this->is_triggered()) {
		assert_adlink(this->common_device()->configure_shared_resources(this), "Error configuring AIO");
	}

	if (triggerMode & DAQ2K_DA_TRGMOD_DELAY == DAQ2K_DA_TRGMOD_DELAY) {
		if (this->delay_source() == DelayTimebase)
			triggerMode |= DAQ2K_DA_Dly1InTimebase;
		if (this->delay_source() == DelaySamples)
			triggerMode |= DAQ2K_DA_Dly1InUI;
	}

	/// Trigger Configured as PostTrigger retriggerable indefinitely
	this->device().config(
				DAQ2K_DA_WRSRC_Int,
				triggerMode,
				totalTriggers,
				this->delay(),
				0,
				false);

	/**-----------------------------------------------------------------------------
	* @todo D2K_AO_Config to set 'start acquisition now' triggers (POST) but
	* we also have D2K_AO_Group_WFM_StopConfig for PRE triggers. Still more,
	* there are multiple things to delay, and we are supporting just one.
	* @bug for D2K_AO_Config (o D2K_AO_DelayTrig_Config?) / delay / PRE-triggers...
	* I16 D2K_AO_Group_WFM_StopConfig (U16 wCardNumber, U16 group, U16 stopSrc, U16 stopMode)
	------------------------------------------------------------------------------*/
}

/**-----------------------------------------------------------------------------
 ******************************************************************************
 * SINGLE OUTPUT
 ******************************************************************************
------------------------------------------------------------------------------*/
void OutputBehaviour::start_single_output(bool firstStart)
{
	assert(this->channel_samples_per_trigger() == 1);

	this->ao_config();

	for (int chan=0; chan<nchannels(); ++chan)
		this->device().configure_channel_range(chan, this->range().range_id());

	if (!this->device().uses_wfm_generation()) {
		info () << "OutputBehaviour: Using as SingleOutput" << endl;
		for (int chan=0; chan<nchannels(); ++chan) {
			this->device().write_channel(chan, this->bits_at(chan, 0));
		}
	} else {
		  // 2502, 2501 waveform boards do not support D2K_AO_WriteChannel!!
		  info () << "OutputBehaviour WAVEFORM: Using as SingleOutput" << endl;
		  this->device().set_wfm_group(m_chanBufIDs, this->nchannels());
		  this->device().wfm_group_update(this->m_rawData);
	  }

	// End of Single output code
	this->set_state(StateReady);
}

void OutputBehaviour::adapt_sample_rate(unsigned long sampleRate)
{
	assert(this->params() >= 0);
	
	if (!sampleRate) {
		info() << "AdlinkDevice: Given SampleRate is NULL!!!!!!! ----> forced to 1" << std::endl;
		sampleRate = 1;//40000000/65535;
	}

	this->m_sampIntrv = this->params()->timeBase / sampleRate;

	// CHECK THIS:
	// According to documentation (D2K_AO_ContWriteChannel)
	// for internal timebase: 40 - 16777215
	// for external timebase: 8 - 16777215
	if	(this->m_sampIntrv < 40)
		this->m_sampIntrv = 40;
	if	(this->m_sampIntrv > 16777215)
		this->m_sampIntrv = 16777215;

	sampleRate = this->params()->timeBase / this->m_sampIntrv;
	this->set_sample_rate(sampleRate);

	info() << "AdlinkDevice: sampIntrv set to " << this->m_sampIntrv << " ~= " << this->params()->timeBase << "/" << sampleRate << std::endl;	
}

/*virtual*/ void OutputBehaviour::on_channel_samples_per_trigger_about_to_change(long chSamplesPerTrigger)
{
	info() << _CFN_ << " entering... "<< endl;
	size_t numOfChannels = this->nchannels();
	
	size_t newSize = chSamplesPerTrigger*this->nchannels();
	size_t newSize_drv = chSamplesPerTrigger*this->device().type_params()->aoChannels;

	if (this->is_d2k()) {
		U32 totalDriverKB = this->device().initial_memory_allocated_kb();
		
		const size_t maxDriverSamples = totalDriverKB*1024/2; // 2: sizeof<ushort>
		if (newSize_drv > maxDriverSamples) {
			std::stringstream ss;
			ss << "Total requested size(" << (2*newSize_drv/1024) << " KB)"
				  " exceeds the limit configured  for the"
				  " board(" << (totalDriverKB)  << " KB)."
				  " Please, check that the variable AO_PAGE in driver"
				  " configuration file is at"
				  " least " << (2*newSize_drv/(4*1024)) << ".";
			throw_error("Adlink ERROR", ss.str(), _CFN_);
		}
	}
	
	/// The m_rawData allocation and adl.adl_init() initialization must be
	/// always together and following this order:\n
	/// \li allocation, copy old values, updateHW, resize spectrums
	info()	<< "Allocating m_rawData buffer with "
			<< newSize << " values" << endl;
	U16* newData = new U16[newSize];

	try {
		/// @todo : params().minInputScanIntrv la uso per algo?
		/// @todo more work on timeBase. params()->timeBase is the device internal
		/// timebase, but you may be using an external one, so we still need
		/// this->timeBase
	
		info()	<< _CFN_ << std::endl
				<< "nchannels() = " << nchannels() << std::endl
				<< "chSamplesPerTrigger = " << chSamplesPerTrigger << std::endl
				<< "num_of_triggers() = " << num_of_triggers() << std::endl
				<< std::endl;
		
		// Initial value: 0 Volts
		for (int i=0; i<newSize; i++)
			newData[i] = this->adl_volts2bits(0.0);
	
		if (this->m_rawData) {
			info()	<< "Copying old m_rawData buffer with " << this->m_size
					<< " values" << std::endl;
			for (int i=0; i<this->m_size && i<newSize; ++i)
				newData[i] = m_rawData[i];
		}

		this->update_user_buffer_size(chSamplesPerTrigger);
	} catch(...) {
		delete [] newData;
		throw;
	}
	
	delete [] this->m_rawData;
	this->m_size = newSize;
	this->m_rawData = newData;
}

#define debug_stream info
/*virtual*/ bool OutputBehaviour::read_raw_data(long *& res, long &x, long &y, struct timeval& time) const
{
	BaseBehaviour::AttributeBuffers &buf = this->m_attrBuffers;
	size_t size = this->nchannels() * this->channel_samples_per_trigger();
	buf.rawDataValues.resize(size);
	debug_stream()	<< "Reading m_rawData[0..." << (size - 1) << "]="
					<< "{" << m_rawData[0] << "..." << m_rawData[size - 1]
					<< "}" << endl;

	for (int k = 0; k < size; ++k)
		buf.rawDataValues[k] = m_rawData[k];

	res = &buf.rawDataValues[0];
	x = this->channel_samples_per_trigger();
	y = this->nchannels();

	gettimeofday(&time, 0);

	return true;
}

/*virtual*/ bool OutputBehaviour::read_double_data(volts_type *& res, long &x, long &y, struct timeval& time) const
{
	BaseBehaviour::AttributeBuffers &buf = this->m_attrBuffers;

	y = this->nchannels();
	x = this->channel_samples_per_trigger();
	buf.doubleDataValues.resize(y * x);
	for (int k=0; k<y*x; k++)
		buf.doubleDataValues[k] = this->adl_bits2volts(this->m_rawData[k]);
	res = &buf.doubleDataValues[0];
	gettimeofday(&time, 0);
	return true;
}

/*virtual*/ bool OutputBehaviour::read_scalar_value(ssize_t port, volts_type* &value, struct timeval& time) const
{
	BaseBehaviour::AttributeBuffers &buf = this->m_attrBuffers;

	value = &buf.scalarChannelValues[port];
	*value = this->adl_bits2volts(this->bits_at(port, this->channel_samples_per_trigger() - 1));
	gettimeofday(&time, 0);
	return true;
}


/*virtual*/ bool OutputBehaviour::read_last_values(volts_type* &data, long &x, struct timeval& time) const
{
	BaseBehaviour::AttributeBuffers &buf = this->m_attrBuffers;
	std::vector<volts_type> &res = buf.spectrumLastValues;
	res.resize(this->nchannels());
	size_t readSz = 0;
	
	for(size_t port=0; port<res.size(); ++port)
		res[port] =  this->adl_bits2volts(this->bits_at(port, this->channel_samples_per_trigger() - 1));
	
	data = &res[0];
	x = res.size();
	gettimeofday(&time, 0);
	return true;
}

/*virtual*/ bool OutputBehaviour::read_channel_spectrum(ssize_t port, volts_type* &data, long &x, struct timeval& time) const
{
	BaseBehaviour::AttributeBuffers &buf = this->m_attrBuffers;
	std::vector<volts_type> & res = buf.spectrumChannelValues[port];

	ssize_t maxSize = this->channel_samples_per_trigger();

// -	res.resize(maxSize); ( update_user_buffer_size already does it )
	for (int k=0; k<maxSize; k++) {
		res[k] = this->adl_bits2volts(this->bits_at(port, k));
	}

	if (res.size() == 0) {
		data = (volts_type*)0;
		x = 0;
	} else {
		data = &res[0];
		x = res.size();
	}
	gettimeofday(&time, 0);
	return true;
}

/*virtual*/ bool OutputBehaviour::write_scalar_value(ssize_t port, volts_type value)
{
	/// @todo write_scalar_value, write_last_values and write_channel_spectrum
	/// restart the output generation when they are on state ON, which is
	/// set only when an output has been finished.. is this what we should
	/// be really doing?
	BaseBehaviour::AttributeBuffers &buf = this->m_attrBuffers;
	volts_type & res = buf.scalarChannelValues[port];
	
	res = value;
		
	for (int k=0; k<this->channel_samples_per_trigger(); k++)
		this->bits_at(port, k) = this->adl_volts2bits(value);

	bool f1 = false;
	try {
		switch(this->get_state()) {
			case StateRunning:
			case StateReady:
				stop_ao(false);
			case StateRegistered:
				f1= true;
				start_ao(false);
		}
	} catch (...) {
		error()	<< "AdlinkAIO::write_scalar_value(): Error"
				<< " trying to " << (f1?"start":"stop") << " AdlinkDevice\n";
		return false;
	}

	this->update_status();
	return true;
}

/*virtual*/ bool OutputBehaviour::write_last_values(const volts_type* data, long szRead)
{
	for (unsigned port=0; port != szRead; ++port) {
		for (int k=0; k<this->channel_samples_per_trigger(); k++)
			this->bits_at(port, k) = this->adl_volts2bits(data[port]);
	}

	bool f1 = false;
	try {
		switch(this->get_state()) {
			case StateRunning:
			case StateReady:
				stop_ao(false);
			case StateRegistered:
				f1 = true;
				start_ao(false);
		}
	} catch (...) {
		error()	<< "AdlinkAIO::write_last_values(): Error"
				<< " trying to " << (f1?"start":"stop") << " AdlinkDevice\n";
		return false;
	}

	this->update_status();
	return true;
}


/*virtual*/ bool OutputBehaviour::write_channel_spectrum(ssize_t channel, const volts_type* data, long len)
{
	/// It converts each value from volts to binary and then copies
	/// it to m_rawData buffer
	for (int k=0; k<this->channel_samples_per_trigger(); k++) {
		if (k<len) {
			this->bits_at(channel, k) = adl_volts2bits(data[k]);
		} else {
			// If the spectrum given is smaller than the buffet the last value
			// in the spectrum will be repeated until the end
			this->bits_at(channel, k) = adl_volts2bits(data[len-1]);
		}
	}

	bool f1 = false;
	try {
		switch(this->get_state()) {
			case StateRunning:
			case StateReady:
				stop_ao(false);
			case StateRegistered:
				f1 = true;
				start_ao(false);
		}
	} catch (...) {
		error()	<< "AdlinkAIO::write_ChannelSpectrum(): Error "
				<< " trying to " << (f1?"start":"stop") << " AdlinkDevice\n";
		return false;
	}
	this->update_status();
	return true;
}

void OutputBehaviour::start_ao(bool doRegister)
{
	try {
		info()	<< _CFN_ << "("
				<< (doRegister? "true" : "false" ) << "): entering..." << std::endl;

		if (doRegister) {
			assert(this->get_state() == StateNone);

			this->trigger_mode().apply();
			this->common_device()->request_shared_control(this);
			this->set_state(StateRegistered);
		}

		assert(m_size/nchannels() == this->channel_samples_per_trigger());
		if (this->channel_samples_per_trigger() > 1)
			this->start_continuous_output(doRegister);
		else
			this->start_single_output(doRegister);
		
		this->update_status();
	} catch (...) {
		this->common_device()->leave_shared_control(this);
		this->update_status();
// 		this->set_state(StateFault);
		throw;
	}
}

/*virtual*/ void OutputBehaviour::start()
{
	if (this->get_state() == StateNone) {
		this->apply_range();
		this->start_ao(true);
	} else if (this->get_state() == StateReady || this->get_state() == StateRegistered) {
		this->stop_ao(false);
		this->apply_range();
		this->start_ao(false);
	} else 
		this->start_ao(false);
}

void OutputBehaviour::stop_ao(bool doRegister)
{
	assert(!this->is_input());
	
	info() << _CFN_ << "(" << std::boolalpha << doRegister << " ): entering ..." << std::endl;

	if (!params()->isD2K) {
		U16 val0 = adl_volts2bits(0);
		/// @todo this is discutible, com a poc... potser si parlessim de single shot i tal tindria mes sentit ?
		for (int chan=0; chan<nchannels(); chan++) {
			this->device().write_channel(chan, val0);
		}
	}

	if (doRegister) {
		// First release, then we are sure it is not using the
		// buffer so we can delete it.
		this->release_ao();
		delete[] m_aoBuf;
		m_aoBuf = 0;

		this->set_state(StateNone);
	} else {
		m_aoCount = this->device().async_clear();
		//this->common_device()->leave_shared_control(this);
	}

	this->update_status();
}

/*virtual*/ void OutputBehaviour::get_status_info(std::ostream & statusStream)
{
	BaseBehaviour::get_status_info(statusStream);
	statusStream	<< "\nWarnings: " << std::endl
				<< this->status_warn_string() << std::endl
				<< this->device().status_warn_string();
}

