#include "BaseBehaviour.h"
#include "AdlinkCommon.h"
#include <cassert>
#include <sstream>

#define _OCLASSNAME "BaseBehaviour"
#include "odebug.h"


BaseBehaviour::BaseBehaviour(
	AdlDeviceFactorySingleton::AdlBoardParams* p,
	AdlDeviceFactorySingleton::ADFMode mode,
	const std::string & _boardId,
	StatusObserver* ob) :
		m_boardParams(p),
		m_state(StateNone),
		m_statusObserver(0),
		m_device(0),
		m_sampleRate(0),
		m_totalShots(1),
		m_chSamplesPerTrigger(1),
		m_numChannels(p->totalChannels),
		m_delay(0),
		m_delaySource(DelayTimebase),
		m_mode(mode),
		m_infiniteRetrigger(false)
{
	assert(p);

	this->set_status_observer(ob);

	I16 boardId;
	std::stringstream ss(_boardId);
	ss >> boardId;

	assert_adlink(
			AdlDeviceFactorySingleton::get(
										&this->m_device, this->params()->typeName,
										boardId,
										this->m_mode ),
			"Error trying to open AdlinkDevice" );
};

/*virtual*/
BaseBehaviour::~BaseBehaviour()
{
	if (!this->m_device)
		return;

	this->common_device()->leave_shared_control(this);

	AdlDeviceFactorySingleton::release(&this->m_device, this->m_mode);
}

void BaseBehaviour::set_channel_samples_per_trigger(long chSamplesPerTrigger)
{
	assert(chSamplesPerTrigger > 0);
	
	if (chSamplesPerTrigger < 1)
		chSamplesPerTrigger = 1;
		
	// hsize = nchannels()*chSamplesPerTrigger
	// must be even
	if (chSamplesPerTrigger > 1)
		if ((this->nchannels()*chSamplesPerTrigger) % 2)
			chSamplesPerTrigger += 1;
	
	// AO needs to know it to resize some buffers.
	// AI buffers will be resized when you press start().
	// Both take the change to check if the size is allright with the
	// driver buffers...
	try {
		this->on_channel_samples_per_trigger_about_to_change(chSamplesPerTrigger);
	} catch (std::bad_alloc &e) {
		chSamplesPerTrigger = 1;
		this->on_channel_samples_per_trigger_about_to_change(chSamplesPerTrigger);
		this->m_chSamplesPerTrigger = chSamplesPerTrigger;
		throw_error("NOT_ENOUGH_MEMORY",
					"Can't set the specified ChannelSamplesPerTrigger. Setting it to 1",
					_CFN_);
	}
	
	// Ok, so we will use these settings...
	this->m_chSamplesPerTrigger = chSamplesPerTrigger;
}

void BaseBehaviour::set_num_of_triggers(long totalShots)
{
	if (totalShots < 1)
		throw_error("BAD_PARAM", "Num Of Triggers must be > 0", _CFN_);
	this->m_totalShots = totalShots;
}


void BaseBehaviour::apply_range()
{
	/// Do not call update_range() each time min or max is changed.
	/// A range is valid depending on both min and max, but if we did so
	/// we would be changing min and max. If allowed (0, 3) and (0,4) no problem,
	/// but if allowed are (0,3) and (14, 21) then we can never really change
	/// the range!!
	/// In fact that's why there's an apply_range() method on AdlinkDevice
	/// in the first place instead of applying them directly!
	if(!this->range().apply_range()) {
		double badMin, badMax, okMin, okMax;
		this->range().get_next_min_max_range(badMin, badMax);
		this->range().get_min_max_range(okMin, okMax);
		std::stringstream ss;
		ss	<< "The selected range (" << badMin << ", " << badMax
			<< ") is not supported by Adlink board. Reverting to old values ("
			<< okMin << ", " << okMax << ")";
		info() << ss.str() << std::endl;
		this->range().set_next_min_range(okMin);
		this->range().set_next_max_range(okMax);

		throw_error( "Invalid range", ss.str(), _CFN_);
	}
}


void BaseBehaviour::update_user_buffer_size(long chSamples) throw (std::bad_alloc)
{
	info() << _CFN_ << " entering... "<< std::endl;
	
	BaseBehaviour::AttributeBuffers &buf = this->m_attrBuffers;
	
	buf.scalarChannelValues.resize(this->nchannels());
	buf.spectrumChannelValues.resize(this->nchannels());
	for(size_t n = 0; n < this->nchannels(); ++n) {
		std::cout << "Resizing spectrum channel buffer " << n << ": " 
			<< chSamples << std::endl;
		buf.spectrumChannelValues[n].resize(chSamples);
	}
}

std::string BaseBehaviour::get_state_name() const
{
	switch (this->get_state()) {
			case StateFault:
				return "Fault";
			case StateNone:
				return "None";
			case StateRegistered:
				return "Registered";
			case StateReady:
				return "Ready";
			case StateRunning:
				return "Running";
	}
	assert(false);
	return "-- UNKNOWN --";
}

/*virtual*/ void BaseBehaviour::get_status_info(std::ostream & statusStream)
{
	statusStream
		<< "Internal device state: " << this->get_state_name()
		<< "\nConfigured as " << (this->is_input() ? "Input" : "Output")
		<< "\n  ChannelSamplesPerTrigger: " << this->channel_samples_per_trigger()
		<< "\n  SampleRate:               " << this->sample_rate()
		<< "\n  NumberOfChannels:         " << this->nchannels()
		<< "\n  TriggerMode:              " << this->trigger_mode().get_trigger_mode(this->is_input())
		<< "\n  NumOfTriggers:            " << this->num_of_triggers()
		<< "\n  InfiniteRetrigger:        " << this->infinite_retrigger()
	;
}

void BaseBehaviour::update_status()
{
	std::stringstream statusStream;
	this->get_status_info(statusStream);

	if (this->m_statusObserver)
		this->m_statusObserver->on_change_status(statusStream.str());
}


void BaseBehaviour::export_stream(std::ostream & os) const
{
	long int *theBuf;
	struct timeval time; // Ignored
	long _x, _y;
	if (!this->read_raw_data(theBuf, _x, _y, time))
		this->throw_error(
			"No raw data to write" ,
			"RawData attribute is NULL now, so there's nothing to save",
			_CFN_);
	
	for(long i=0; i<this->channel_samples_per_trigger(); ++i) {
		for (int j=0; j<this->nchannels()-1; ++j)
			os	<< this->bits2volts(theBuf[this->channel_samples_per_trigger()*j+i])
				<< "\t";
		const int j = this->nchannels();
		os
			<< this->bits2volts(theBuf[this->channel_samples_per_trigger()*j+i])
			<< std::endl;
	}
}

void BaseBehaviour::import_stream(std::istream & is)
{
	if (this->is_input())
		this->throw_error(
						"Unable to import file",
						"It makes no sense for a non Output device like this",
						_CFN_);

	double val;
	std:string tmp;
	bool ok = true;

	// chv temporary variable format:
	// ch0bits0, ch0,bits1..ch0bitsN, ch1bits0,..ch1bitsN, ch2bits0, chMbitsN
	// where:
	//   N = channel_samples_per_trigger()
	//   M = nchannels()
	std::vector<volts_type> chv(channel_samples_per_trigger() * nchannels());

	size_t readLen = 0;

	for (size_t el=0; el < this->channel_samples_per_trigger(); ++el) {
		// Get a line
		getline(is, tmp);
		std::istringstream ss(tmp);
		if (!ss.good()) {
			readLen = el;
			break;
		}

		// Put TAB separated values into chv
		for (size_t channel=0; channel < this->nchannels(); ++channel) {
			ss >> val;
			if (!ss.good()) {
				readLen = el;
				ok = false;
				break;
			}
			chv[channel*channel_samples_per_trigger() + el] = val;
		}
		if (!ok)
			break;
	}

	if ((!ok) || (readLen == 0)) {
		std::stringstream ss;
		ss	<< "Error reading line " << readLen
			<< ". Check format." << std::endl;
		this->throw_error(
						"Unable to import",
						ss.str(),
						_CFN_);
	}

	for (size_t channel=0; channel < this->nchannels(); ++channel) {
		const volts_type * data = & chv[channel*channel_samples_per_trigger()];
		this->write_channel_spectrum(channel, data, readLen);
	}
}

void BaseBehaviour::perform_auto_calibration()
{
	this->common_device()->perform_auto_calibration();
}

void BaseBehaviour::load_calibration_settings(unsigned long bank)
{
	this->common_device()->load_calibration_settings(bank);
}

void BaseBehaviour::save_calibration_settings(unsigned long bank)
{
	this->common_device()->save_calibration_settings(bank);
}
