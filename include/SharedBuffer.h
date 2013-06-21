#ifndef __SHARED_BUFFER_H____dsjd93wuefDFIdFDs0ef___
#define __SHARED_BUFFER_H____dsjd93wuefDFIdFDs0ef___

///
/// @class SharedBuffer
/// HOW IT WORKS:
///
/// Supports one writer thread and several concurrent readers. To do so,
/// the mutex does not lock the whole data transfer but only before and
/// after, where some conditions are checked.
/// 
/// When we want to write n:
/// <pre>
///     lock:
///       "I'm gonna write bucket n"
///       If somebody is reading 'n', wait until done
///     write
///     lock:
///       "Ok, I'm not writing anymore" -> signal() to anyone interested
/// </pre>
/// When we want to read n:
/// <pre>
///     lock:
///       "Is n being writen, or sb wants to write it?" if yes, wait until done
///       "I'm gonna read bucket n"
///     read
///     lock:
///       "Im not reading anymore n"
///       if sb wants to write n, ->signal()
/// </pre>
///
/// wait() are safe, as they internally release the lock and get back when done.
/// Reader does not really know if sb is already writing or wants to, cos writer
/// has preferency so reader has to wait alwais.
/// Reading buckets are stored in a multiset, so multiple threads can be reading
/// the same buffer. It also allows one thread to write/lock various buckets at
/// the same time.
/// There are 2 omni conditions, one for waiting writers and for writing readers.
/// You can use write_bucket() and read_buckets() if you just want to copy data
/// from/to different locations. If you want to do something more elaborate, you
/// can also use SharedBuffer::WriteLock and  SharedBuffer::ReadLock
/// objects which follow RIIA to lock/unlock some buckets you
/// can then acces using get().
///
/// Global defines that modify the behaviour of this class:
///
/// _RSUNE_ENABLE_SHAREDBUFFER_FORCE_ONE_WRITTER
///   If it is defined a mutex will protect write access. If it's not defined,
///   we assume that only one thread will try to get write access to the device
///   so it's not needed.
///   If you don't set this macro and several threads try to write concurrently,
///   some unexpected behavior may happen.
///   
/// _RSUNE_ENABLE_SHAREDBUFFER_DEBUG
///   If it is defined debug information will be printed into console.
///

#include <set>
#include <omnithread.h>
#include <cassert>
#ifdef _RSUNE_ENABLE_SHAREDBUFFER_RECORD_TIME
#include <sys/time.h>
#endif

typedef int SharedBufferBucket;

template<typename Data> class SharedBufferWriteLock;
template<typename Data> class SharedBufferReadLock;
template<typename Data> class SharedBufferIterateReadLock;

template<typename Data>
class SharedBuffer
{
	friend class SharedBufferWriteLock<Data>;
	friend class SharedBufferReadLock<Data>;
	friend class SharedBufferIterateReadLock<Data>;
public:
	typedef SharedBufferBucket Bucket;
	typedef std::multiset<Bucket> BucketMultiSet;

	typedef SharedBufferWriteLock<Data> WriteLock;
	typedef SharedBufferReadLock<Data> ReadLock;
	typedef SharedBufferReadLock<Data> IterateReadLock;
private:
/// @name Synchronization elements
/// @{
	mutable omni_mutex			m_writingMutex;
	mutable omni_mutex			m_readingMutex;
	
	mutable omni_condition		m_writingDone;
	mutable omni_condition		m_readingDone;

// We can have several threads reading at the same
// time, but only one is alowed to write.
	mutable BucketMultiSet		m_readingBuckets;
	mutable Bucket				m_writingBucket;

#ifdef _RSUNE_ENABLE_SHAREDBUFFER_FORCE_ONE_WRITTER
// This one ensures that only one thread has write access, so no
// unexpected behaviour happens when multiple threads try to
// write concurrently.
	mutable omni_mutex			m_singleWritingMutex;
#endif
/// @}

/// @name Buffer
/// @{
	Data *				m_buffer;
	const size_t		m_bucketSize;
	const size_t		m_totalBuckets;
/// @}

#ifdef _RSUNE_ENABLE_SHAREDBUFFER_RECORD_TIME
	struct timeval * m_bucketTimes;
#endif

/// Private auxiliar methods
// The following internal methods are just to simulate
// the Java 'finally' construct on the
// write_bucket(), read_bucket() and read_buckets() methods
	inline void end_write_bucket(Bucket bucket) const;
	inline void end_read_buckets(Bucket first, Bucket last) const;

	inline void begin_write_bucket(Bucket bucket) const;
	inline void begin_read_buckets(Bucket first, Bucket last) const;

#ifdef _RSUNE_ENABLE_SHAREDBUFFER_RECORD_TIME
	inline const struct timeval* get_bucket_time(Bucket bucket) const
	{ return & this->m_bucketTimes[bucket]; }
#endif

public:
	SharedBuffer(size_t bucketSize, size_t totalBuckets) :
		m_writingDone(&m_writingMutex),
		m_readingDone(&m_readingMutex),
		m_bucketSize(bucketSize),
		m_totalBuckets(totalBuckets),
		m_writingBucket(-1)
	{
		this->m_buffer = new Data[bucketSize * totalBuckets];
#ifdef _RSUNE_ENABLE_SHAREDBUFFER_RECORD_TIME
		this->m_bucketTimes = new struct timeval[totalBuckets];
#endif
	}

	~SharedBuffer();

	void write_bucket(Bucket bucket, const Data * data);
	void read_buckets(Bucket first, Bucket last, Data * data) const;

	bool bucket_in_range(Bucket bucket, Bucket first, Bucket last) const;
	Bucket next_bucket(Bucket bucket) const;
	Bucket prev_bucket(Bucket bucket) const;

	inline size_t total_buckets() const
	{ return this->m_totalBuckets; }
	
	inline size_t bucket_size() const
	{ return this->m_bucketSize; }
};


template<typename Data>
class SharedBufferReadLock
{
public:
	typedef SharedBufferBucket Bucket;
protected:
	SharedBuffer<Data> & buffer;
	Bucket first;
	Bucket last;
public:
	SharedBufferReadLock(SharedBuffer<Data> &buf, Bucket fir, Bucket las) : buffer(buf), first(fir), last(las)
	{
		assert(first >= 0);
		assert(first < buffer.m_totalBuckets);
		assert(last >= 0);
		assert(last < buffer.m_totalBuckets);

		buffer.begin_read_buckets(first, last);
	}

	const Data * get(Bucket bucket) const
	{
		assert(buffer.bucket_in_range(bucket, first, last));
		return buffer.m_buffer + bucket * buffer.m_bucketSize;
	}

	bool iterate(Bucket & bucket)
	{	
		if(bucket == last)
			return false;
		bucket = buffer.next_bucket(bucket);
		return true;
	}

    size_t total() const
    {
        if (first <= last)
            return last - first + 1;
        return buffer.m_totalBuckets - first + last + 1;
    }

#ifdef _RSUNE_ENABLE_SHAREDBUFFER_RECORD_TIME
	inline const struct timeval* get_time(Bucket bucket) const
	{
		assert(buffer.bucket_in_range(bucket, first, last));
		return buffer.get_bucket_time(bucket);
	}
#endif

	~SharedBufferReadLock()
	{
		buffer.end_read_buckets(first, last);
	}
};

template<typename Data>
class SharedBufferIterateReadLock : public SharedBufferReadLock<Data>
{
public:
	typedef SharedBufferBucket Bucket;
	
public:
	SharedBufferIterateReadLock(SharedBuffer<Data> &buf, Bucket fir, Bucket las) :
	SharedBufferReadLock<Data>(buf, fir, las)
	{}
	
	bool iterate(Bucket & bucket)
	{
		typedef SharedBufferReadLock<Data> Base;
		Bucket prev = bucket;
		if (Base::iterate(bucket)) {
			if (Base::buffer.m_writingBucket >= 0) {
				Base::buffer.end_read_buckets(Base::first, prev);
				Base::first = bucket;
			}
			return true;
		} else {
			return false;
		}
	}
};

template<typename Data>
class SharedBufferWriteLock
{
public:
	typedef SharedBufferBucket Bucket;
private:
	SharedBuffer<Data> & buffer;
	Bucket bucket;
public:
	SharedBufferWriteLock(SharedBuffer<Data> &buf, Bucket buc) : buffer(buf), bucket(buc)
	{
		assert(bucket >= 0);
		assert(bucket < buffer.m_totalBuckets);

		buffer.begin_write_bucket(bucket);
	}

	~SharedBufferWriteLock()
	{
		buffer.end_write_bucket(bucket);
	}

	Data * get()
	{
		return buffer.m_buffer + bucket * buffer.m_bucketSize;
	}
};

#endif //__SHARED_BUFFER_H____dsjd93wuefDFIdFDs0ef___
