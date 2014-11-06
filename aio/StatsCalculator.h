
#pragma once

#include <vector>
#include <list>

#include <sys/time.h>

class StatsSettings; /// @todo tb al namespace Stats
class InputBehaviour;

namespace Stats
{
	
	enum SelectedOperation {
		OperationNone = -1,
		OperationMean = 0,
		OperationStdDev = 1,
		OperationQuadraticMean = 2,
		OperationTOTAL
	};

	struct BufferOperationResults
	/// For each AI buffer, store the stats.
	/// The number of data depends on the number of channels and the most complicated
	/// operation selected. If you are only interested in the OperationMean, we just
	/// need one data per channel. If you are interested in the QuadraticMean, we will
	/// need to calculate the Mean and StdDev anyways, so we store them.
	{
		/// Results of the operations. For each operation we have a vector
		/// the vector contains the results for each channel
		/// ex:
		///    results[OperationMean][ch0]
		std::vector < double >  resultsOpChannel [OperationTOTAL];

		BufferOperationResults(
			size_t nchannels,
			SelectedOperation op,
			const double* const data,
			size_t nsamples,
			struct timeval &time );

		void operate(SelectedOperation operation, const double* const data, size_t chSamples, size_t chanId);
	};

	struct ChannelSpectrumAttributeCache
	/// For each channel, we will need to reorganize the data contained
	/// in a list of BufferOperationResults, so we want an array of lenght(totalTriggers)
	/// with the mean of the buffers on each trigger.
	{
		struct timeval lastBufferTime;

		// Spectrums size: NumOfTriggers
		std::vector<double> meanSpectrum;
		std::vector<double> stdDevSpectrum;
		std::vector<double> quadMeanSpectrum;
	};


	class StatsCalculator {

		typedef std::list<BufferOperationResults> BufferOperationResultsList;

		/// The 'hardest' operation: If I want to calculate 'mean' I just need the mean.
		/// But if I want to calculate the StdDev, I need both Mean and StdDev, so the
		/// hardest is StdDev
		SelectedOperation m_hardestOperation;

		/// For each buffer: The Stats of each channel
		BufferOperationResultsList m_bufferOperationResultsList;
		
		size_t m_spectrumsMaxLenght;

		/// This should always be like m_bufferOperationResultsList.rbegin().lastBufferTime
		/// It is useful to know if we need to update m_spectrumsCache or not
		struct timeval m_lastBufferTime;

		/// For each channel, it's Stats spectrums
		mutable std::vector<ChannelSpectrumAttributeCache> m_spectrumsCache;

		bool update_spectrums_cache(unsigned channel) const;

	public:

		void reconfigure(size_t nChannels, const StatsSettings &settings);

		void set_spectrums_max_length(size_t nTriggers)
		{ m_spectrumsMaxLenght = nTriggers; }

		StatsCalculator() :  m_spectrumsMaxLenght(0), m_hardestOperation(OperationMean)
		{
		}

		bool calc_stats(InputBehaviour*);
		void clear_buffer();

		bool get_last(SelectedOperation operation, unsigned channel, double &mean, struct timeval& time) const;

		bool get_last_buffer(SelectedOperation operation, unsigned channel, const double* &data, size_t &len, struct timeval& time) const;

	};
}

