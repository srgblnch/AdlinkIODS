
#ifndef __ADLINK_AO_DEVICE___H___dfjdRFDojaksdj209230___
#define __ADLINK_AO_DEVICE___H___dfjdRFDojaksdj209230___

#include "BaseBehaviour.h"
#include <string>
#include <cassert>
#include <cmath>

#define MAX_BUFFER_SIZE 12000 //15996
#define MAX_REF_SOURCE 10.0
#define MIN_REF_SOURCE -10.0
#define INTERNAL_TIMEBASE 40000000
#define POST_TRIGGER_SIGNAL 62

class OutputDeviceInterface;

void adl_triggerEventCallback(int sig_no);


/** The Class Adlink Device will manage the configuration and execution of Single/Continuous Analog Input/Output Acquisitions.
 */
class OutputBehaviour : public BaseBehaviour
{
private:
	/// @name Private device settings
	// @{
		/// @todo adl_timebase: Nevertheless, we should have a timeBase
		/// attribute for external timebase...
		U32 m_sampIntrv;
	// @}

	
	/** \name Private Memory management variables*/
	// @{
		U16	m_bufId;

		/// Main Raw data buffer. Format:ch0i0 ch0i1 ch0iN ch1i0 ch1iN...
		U16* m_rawData;

		/// Size of the raw data buffer allocated in memory
		U32 m_size;

		/// Buffer effectively used by the adlink driver to compose all the
		/// channels. It's similar to m_rawData, but in a board dependent format
		U16* m_aoBuf;

		/// Some Adlink methods need this array that just contain 0, 1, 2, 3...
		U16 m_chanBufIDs[255];
	// @}
	
	/** \name Values fixed by AdLink Driver */
	// @{
		U32 m_aoCount;
	// @}

	// start_ao(bool) and stop_ao(bool)
	void start_ao(bool doRegister);
	void stop_ao(bool doRegister);

	bool is_triggered() const
	{	return (bool)this->trigger_mode().get_mode();	}

	inline U16 adl_volts2bits(double volts) const
	{
		using std::abs;
		const short & adl_nbits = this->params()->ao_nBits;//this->params()->dynRange;
		
		if ((!this->params()->isD2K) && (this->params()->type == PCI_6208V)) {
			// Not D2K specific but PCI_6208V specific path!!
			// Its volts*pow instead of (volts-min)*pow
			I16 val = (volts/10.0)*32767.0;
			return reinterpret_cast<U16&>(val);
		} else
			return round((volts - range().next_min())*(pow(2.0,(double)adl_nbits)-1)/(range().next_max()-range().next_min()));
	}

	inline double adl_bits2volts(U16 bits) const
	{

		using std::abs;
		using std::pow;
		if ((!this->params()->isD2K) && (this->params()->type == PCI_6208V)) {
			// Not D2K specific but PCI_6208V specific path!!
			const I16 &ibits = reinterpret_cast<I16&>(bits);
			return 10.0*ibits/32767.0;
		}
		const short & adl_nbits = this->params()->ao_nBits; // this->params()->dynRange;
		return bits*(range().next_max()-range().next_min())/(pow((double)2,(double)adl_nbits)-1)-abs(range().next_min());
	}

	/// Clears all AO related settings applyied to the device. It should
	/// look almost like a D2K_Release_Card(), everything is stopped.
	void release_ao();

	void start_single_output(bool firstStart);
	void start_continuous_output(bool firstStart);

	inline const U16 & bits_at(long channel, long sample) const
	{
		assert(channel >=0 && channel < nchannels());
		assert(sample >=0 && sample < channel_samples_per_trigger());
		const size_t pos = this->channel_samples_per_trigger()*channel + sample;
		assert(pos < m_size);
		return this->m_rawData[pos];
	}

	inline U16 & bits_at(long channel, long sample)
	{
		return this->m_rawData[this->channel_samples_per_trigger()*channel + sample];
	}

	inline double volts_at(long channel, long sample) const
	{
		return this->adl_bits2volts(this->bits_at(channel, sample));
	}

	void ao_config();

	inline OutputDeviceInterface & device();
	inline const OutputDeviceInterface& device() const;

public:

	OutputBehaviour(
			AdlDeviceFactorySingleton::AdlBoardParams* p,
			const std::string & boardId,
			StatusObserver* ob);
	~OutputBehaviour();

	inline bool is_input() const
	{	return false;	}


	virtual void get_status_info(std::ostream & statusStream);
	
	///Allows to do the reallocation of the different buffers.
	virtual void on_channel_samples_per_trigger_about_to_change(long chspt);

	virtual void update_state_fast();

	virtual bool read_raw_data(long *& resRawData, long &x, long &y, struct timeval& time) const;
	virtual bool read_double_data(volts_type *& resRawData, long &x, long &y, struct timeval& time) const;
	virtual bool read_scalar_value(ssize_t port, volts_type* &value, struct timeval& time) const;
	virtual bool read_last_values(volts_type* &data, long &x, struct timeval& time) const;
	virtual bool read_channel_spectrum(ssize_t port, volts_type* &data, long &x, struct timeval& time) const;

	virtual bool write_scalar_value(ssize_t port, volts_type value);
	virtual bool write_last_values(const volts_type* data, long x);
	virtual bool write_channel_spectrum(ssize_t port, const volts_type* data, long x);

	// Why so many start and stop? Historical reasons.
	// start() and stop() satisfy the abstract interface
	virtual void start();
	virtual void stop()
	{ this->stop_ao(true); }

	/// So, internally we use adl_volts2bits, which can be inlined,
	/// and we also provide volts2bits, to get the proper transformation
	/// by virtuals from any BaseBehaviour
	virtual unsigned volts2bits(double volts) const
	{	return adl_volts2bits(volts);	}
	virtual double bits2volts(unsigned bits) const
	{	return adl_bits2volts(bits);	}

	virtual void adapt_sample_rate(unsigned long sampleRate);
};

#endif //__ADLINK_AO_DEVICE___H___dfjdRFDojaksdj209230___
