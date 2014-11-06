
#include "StatsCalculator.h"

#include <gsl/gsl_statistics.h>
#include <gsl/gsl_math.h>

#include "Utils.h"
#include "InputBehaviour.h"
#include "StatsSettings.h"

/// @todo move somewhere more generic....
std::ostream & operator << (std::ostream &os, const struct timeval & t)
{
	os << "time(" << t.tv_sec << ", " << t.tv_usec << ")";
	return os;
}

namespace Stats
{

	BufferOperationResults::BufferOperationResults(size_t nchannels, SelectedOperation operation, const double* const data, size_t nsamples, struct timeval &time)
	{
		/// @todo mmm, nop, nomes resize els apropiats si faig mean no cal el quadratic...
		resultsOpChannel[OperationMean].resize(nchannels);
		resultsOpChannel[OperationStdDev].resize(nchannels);
		resultsOpChannel[OperationQuadraticMean].resize(nchannels);

		
		size_t chSize = nsamples / nchannels;
		for (size_t n =0; n< nchannels; ++n) {
			this->operate(operation, &data[n*chSize], chSize, n);
		}

// 		bufferTime = time;
	}

	void BufferOperationResults::operate(SelectedOperation operation, const double* const data, size_t chSamples, size_t chanId)
	{
		if (operation == OperationNone)
			return;
		
		const double mean = gsl_stats_mean(data, 1, chSamples);
		resultsOpChannel[OperationMean][chanId] = mean;
		if (operation == OperationMean)
			return;
		
		const double stdDev = gsl_stats_sd_m(data, 1, chSamples, mean);
		resultsOpChannel[OperationStdDev][chanId] = stdDev;
		if (operation == OperationStdDev)
			return;
		
		const double quadMean = gsl_hypot(mean, stdDev);
		resultsOpChannel[OperationQuadraticMean][chanId] = quadMean;
		return;
	}


	void StatsCalculator::clear_buffer()
	{
			m_bufferOperationResultsList.clear();
			// m_bufferOperationResultsList.resize(0);
    }     

	bool StatsCalculator::calc_stats(InputBehaviour* input)
	{
		if (m_hardestOperation == OperationNone)
			return false;
		
		static std::vector<long> __useless;
		size_t width = 1;

		std::vector<double> values;
		size_t size;

		struct timeval time;

		time = m_lastBufferTime;

		if (!input->get_last_big_aux(false, __useless, values, size, width, time))
			return false;

		// get_last_big_aux has done nothing, we were already up-to-date
		if (time == m_lastBufferTime)
			return true;

		m_bufferOperationResultsList.push_back(BufferOperationResults(input->nchannels(), m_hardestOperation, &values[0], size, time));

		m_lastBufferTime = time;

		while (m_bufferOperationResultsList.size() > m_spectrumsMaxLenght) {
        
            // DFC 
			//m_bufferOperationResultsList.resize( m_spectrumsMaxLenght);
			m_bufferOperationResultsList.pop_front();
		}
		return true;
	}


	bool StatsCalculator::get_last(SelectedOperation operation, unsigned channel, double &mean, struct timeval& time) const
	{
		BufferOperationResultsList::const_reverse_iterator op = m_bufferOperationResultsList.rbegin();
		if (op == m_bufferOperationResultsList.rend())
			return false;
		
		mean = op->resultsOpChannel[operation][channel];
		time = m_lastBufferTime;
		
		return true;
	}

	bool StatsCalculator::update_spectrums_cache(unsigned channel) const
	{
		BufferOperationResultsList::const_iterator i = m_bufferOperationResultsList.begin();
		BufferOperationResultsList::const_iterator e = m_bufferOperationResultsList.end();

	
		if (i == e)
			return false;

		if (m_hardestOperation == OperationNone)
			return true;

		ChannelSpectrumAttributeCache& result = m_spectrumsCache[channel];

		if ( m_lastBufferTime == result.lastBufferTime )
			return true;
		
		result.quadMeanSpectrum.resize(0);
		result.stdDevSpectrum.resize(0);
		result.meanSpectrum.resize(0);
		result.lastBufferTime = m_lastBufferTime;

		for(; i != e; ++i) {
			switch (m_hardestOperation) {
				case OperationQuadraticMean: {
					double quadMean = i->resultsOpChannel[OperationQuadraticMean][channel];
					result.quadMeanSpectrum.push_back(quadMean);
				}
				case OperationStdDev: {
					double stdDev = i->resultsOpChannel[OperationStdDev][channel];
					result.stdDevSpectrum.push_back(stdDev);
				}
				case OperationMean: {
					double mean = i->resultsOpChannel[OperationMean][channel];
					result.meanSpectrum.push_back(mean);
				}
				default:
					break;
			}
		}

		return true;
	}

	bool StatsCalculator::get_last_buffer(SelectedOperation operation, unsigned channel, const double* &data, size_t &len, struct timeval& time) const
	{
		if (!this->update_spectrums_cache(channel))
			return false;

		ChannelSpectrumAttributeCache& result = m_spectrumsCache[channel];
		len = 0;
		data = 0;

		switch (operation) {
			case OperationQuadraticMean: {
				len = result.quadMeanSpectrum.size();
				if (len) {
					data = &result.quadMeanSpectrum[0];
					time = result.lastBufferTime;
				}
				break;
			}
			case OperationStdDev: {
				len = result.stdDevSpectrum.size();
				if (len) {
					data = &result.stdDevSpectrum[0];
					time = result.lastBufferTime;
				}
				break;
			}
			case OperationMean: {
				len = result.meanSpectrum.size();
				if (len) {
					data = &result.meanSpectrum[0];
					time = result.lastBufferTime;
				}
				break;
			}
			default:
				len = 0;
				data = 0;
				break;
		}
		return true;
	}



void StatsCalculator::reconfigure(size_t nChannels, const StatsSettings &settings)
{
	m_spectrumsCache.resize(nChannels);
	
	if ( settings.last_quadratic_mean_enabled || settings.buf_quadratic_mean_enabled ) {
		m_hardestOperation = OperationQuadraticMean;
		return;
	}

	if ( settings.last_std_dev_enabled || settings.buf_std_dev_enabled ) {
		m_hardestOperation = OperationStdDev;
		return;
	}

	if ( settings.last_mean_enabled || settings.buf_mean_enabled ) {
		m_hardestOperation = OperationMean;
		return;
	}
	
	m_hardestOperation = OperationNone;
}

} // ~namespace
