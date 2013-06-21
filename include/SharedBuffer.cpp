
#include "SharedBuffer.h"
#include <cassert>
#include <algorithm>

// For a description of how to use and configure
// these classes see SharedBuffer.h

#include <iostream> /// @todo It should'nt need it...
#ifdef _RSUNE_ENABLE_SHAREDBUFFER_DEBUG
#   define _RSUNE_SHAREDBUFFER_DEBUG std::cout << "   SharedBuffer::" << __FUNCTION__ << " "
#else
#   define _RSUNE_SHAREDBUFFER_DEBUG if (false) std::cout
#endif // ~ _RSUNE_ENABLE_SHAREDBUFFER_DEBUG

template<typename Data>
SharedBuffer<Data>::~SharedBuffer()
{
	omni_mutex_lock lockr(this->m_readingMutex);	
	omni_mutex_lock lockw(this->m_writingMutex);

	assert(this->m_readingBuckets.size() == 0);
	assert(this->m_writingBucket < 0);

	delete [] this->m_buffer;
#ifdef _RSUNE_ENABLE_SHAREDBUFFER_RECORD_TIME
	delete [] this->m_bucketTimes;
#endif
}

template<typename Data>
void SharedBuffer<Data>::begin_write_bucket(Bucket bucket) const
{
	_RSUNE_SHAREDBUFFER_DEBUG	<< "A " << bucket << ", "
								<< this->m_writingBucket << std::endl;

#ifdef _RSUNE_ENABLE_SHAREDBUFFER_FORCE_ONE_WRITTER
	this->m_singleWritingMutex.lock();
#endif

	omni_mutex_lock lock(this->m_readingMutex);	
	{
		omni_mutex_lock lock(this->m_writingMutex);
		
		assert(this->m_writingBucket < 0);
		this->m_writingBucket = bucket;
	}

	while (m_readingBuckets.count(bucket) != 0) {
		this->m_readingDone.wait();
	}

#ifdef _RSUNE_ENABLE_SHAREDBUFFER_RECORD_TIME
	// Now we have the write lock, we can store current time
	gettimeofday(&this->m_bucketTimes[bucket], 0);
#endif

	_RSUNE_SHAREDBUFFER_DEBUG	<< "B " << bucket << ", "
								<< this->m_writingBucket << std::endl;
}

template<typename Data>
void SharedBuffer<Data>::write_bucket(Bucket bucket, const Data * data)
{
	assert(bucket >= 0);
	assert(bucket < this->m_totalBuckets);

	this->begin_write_bucket(bucket);

	try {
		std::copy(
				data,						// first
				data + this->m_bucketSize,	// last, non-copied
				this->m_buffer + bucket * this->m_bucketSize);
	} catch (...) {
		this->end_write_bucket(bucket);
		throw;
	}
	this->end_write_bucket(bucket);
}

template<typename Data>
void SharedBuffer<Data>::end_write_bucket(Bucket bucket) const
{
	_RSUNE_SHAREDBUFFER_DEBUG	<< "A " << bucket << ", " 
								<< this->m_writingBucket << std::endl;

	omni_mutex_lock lock(this->m_writingMutex);

	this->m_writingBucket = -1;
	this->m_writingDone.signal();

#ifdef _RSUNE_ENABLE_SHAREDBUFFER_FORCE_ONE_WRITTER
	this->m_singleWritingMutex.unlock();
#endif

	_RSUNE_SHAREDBUFFER_DEBUG	<< "B " << bucket << ", " 
								<< this->m_writingBucket << std::endl;
}



template<typename Data>
void SharedBuffer<Data>::begin_read_buckets(Bucket first, Bucket last) const
{
	_RSUNE_SHAREDBUFFER_DEBUG 	<< "A " << first << ", " << last << ", "
								<< this->m_writingBucket << std::endl;

	omni_mutex_lock lock(this->m_readingMutex);
	{
		omni_mutex_lock lock(this->m_writingMutex);
		while (this->bucket_in_range(this->m_writingBucket, first, last)) {
			this->m_writingDone.wait();
		}
	}

	Bucket stop = this->next_bucket(last);
	if (first != stop)
		for(Bucket b = first; b != stop; b = this->next_bucket(b) )
			this->m_readingBuckets.insert(b);
	else {
		this->m_readingBuckets.insert(first);
		for(Bucket b = this->next_bucket(first); b != stop; b = this->next_bucket(b) )
			this->m_readingBuckets.insert(b);
	}

	_RSUNE_SHAREDBUFFER_DEBUG	<< "B " << first << ", " << last << ", "
								<< this->m_writingBucket << std::endl;
}

template<typename Data>
void SharedBuffer<Data>::read_buckets(Bucket first, Bucket last, Data * data) const
{
	assert(first >= 0 );
	assert(last >= 0 );
	assert(first < this->m_totalBuckets);
	assert(last < this->m_totalBuckets);

	this->begin_read_buckets(first, last);
	try {
		if (first < last)
			std::copy(
					this->m_buffer + first * this->m_bucketSize,
					this->m_buffer + ( last + 1 ) * this->m_bucketSize,
					data );
		else {
			std::copy(
					this->m_buffer + last * this->m_bucketSize,
					this->m_buffer + this->m_totalBuckets * this->m_bucketSize,
					data );
			std::copy(
					this->m_buffer,
					this->m_buffer + ( first + 1 ) * this->m_bucketSize,
					data );
		}
	} catch (...) {
		this->end_read_buckets(first, last);
		throw;
	}
	this->end_read_buckets(first, last);

}

template<typename Data>
void SharedBuffer<Data>::end_read_buckets(Bucket first, Bucket last) const
{
	_RSUNE_SHAREDBUFFER_DEBUG	<< "A " << first << ", " << last << ", "
								<< this->m_writingBucket << std::endl;
	
	omni_mutex_lock lock(this->m_readingMutex);

	Bucket stop = this->next_bucket(last);
	if (first != stop)
		for(Bucket b = first; b != stop; b = this->next_bucket(b) )
			this->m_readingBuckets.erase(this->m_readingBuckets.find(b));
	else {
		this->m_readingBuckets.erase(this->m_readingBuckets.find(first));
		for(Bucket b = this->next_bucket(first); b != stop; b = this->next_bucket(b) )
			this->m_readingBuckets.erase(this->m_readingBuckets.find(b));
	}
	{
		omni_mutex_lock lock(this->m_writingMutex);
		if (	(this->bucket_in_range(this->m_writingBucket, first, last)) &&
				(this->m_readingBuckets.count(this->m_writingBucket) == 0) )
			this->m_readingDone.signal();
	}

	_RSUNE_SHAREDBUFFER_DEBUG	<< "B " << first << ", " << last << ", " 
								<< this->m_writingBucket << std::endl;
}


template<typename Data>
bool SharedBuffer<Data>::bucket_in_range(Bucket bucket, Bucket first, Bucket last) const
{
	assert(first >= 0);
	assert(last >= 0);
	assert(first < this->m_totalBuckets);
	assert(last < this->m_totalBuckets);

	if (bucket < 0)
		return false;
	if (first < last)
		return (bucket >= first) && (bucket <= last) ;
	else
		return (bucket >= first) || (bucket <= last) ;
}

template<typename Data>
SharedBufferBucket SharedBuffer<Data>::next_bucket(Bucket bucket) const
{
	assert(bucket >= 0);
	assert(bucket < this->m_totalBuckets);

	return (bucket + 1) % this->m_totalBuckets;
}

template<typename Data>
SharedBufferBucket SharedBuffer<Data>::prev_bucket(Bucket bucket) const
{
	assert(bucket >= 0);
	assert(bucket < this->m_totalBuckets);

	if (bucket == 0)
		return this->m_totalBuckets - 1;
	return bucket - 1;
}
