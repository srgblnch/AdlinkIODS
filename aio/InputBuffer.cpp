
#include <tango.h>
#include "InputBuffer.h"
#include "Utils.h"
#include "InputDeviceInterface.h"

#define _OCLASSNAME "InputBuffer"
#include "odebug.h"


void InputBuffer::set_device(InputDeviceInterface* device)
{
    if(m_device)
        throw_error(
                "Device already set.",
                "This function should be called ONCE and only ONCE." ,
                _CFN_);
    m_device = device;
}

void InputBuffer::rebuild(U32 hsize, long total_buckets, bool useSharedBuffer)
{
    assert(total_buckets > 0);

    try {
        m_lastBucketSize = -1;
        m_lastBucketBegin = 0;
        m_useSharedBuffer = useSharedBuffer;
        m_bufferPosition.stop();
        m_hsize = hsize;

        if (total_buckets != 1)
            m_useSharedBuffer = true;

        if (m_useSharedBuffer) {
            if (m_sharedBuffer) {
                if (    (m_sharedBuffer->total_buckets() != total_buckets)
                    ||  (m_sharedBuffer->bucket_size() != hsize ) ) {
                        delete m_sharedBuffer;
						m_sharedBuffer = 0;
                        m_sharedBuffer = new SharedBufferU16(this->m_hsize, total_buckets);
                }
                // We can re-use the current sharedBuffer :)
            } else {
                m_sharedBuffer = new SharedBufferU16(this->m_hsize, total_buckets);
			}
			assert(total_buckets == m_sharedBuffer->total_buckets());
        }
        m_bufferPosition.set_limits(0, total_buckets - 1 );
    }catch(std::bad_alloc &e) {
        std::stringstream desc;
        desc << "Out of memory!\n"
             << "\ttotal_buckets= " << total_buckets << "\n"
             << "\thsize=         " << hsize << "\n"
             << "\tuseSharedBuffer= " << m_useSharedBuffer << "\n\n"
             << "\n\t" << e.what() << std::endl;
        throw_error("Out_Of_Memory", desc.str(), _CFN_);
    }
}


/// Saves selected half buffer data into shared buffer, or just the
/// half id if we don't want to use shared buffer.
/// To be used when we want to store the full half buffer. Sometimes
/// the buffer is not complete or starts in an unusual place (not 0), for
/// this cases use save_special_half_buffer()
/// @param halfId Half id to read data from
void InputBuffer::save_half_buffer( U16 halfId )
{
    if (this->m_useSharedBuffer) {
        SharedBufferU16::WriteLock lock(
                                        *this->m_sharedBuffer,
                                        this->m_bufferPosition.current());
        U16 * rawData = lock.get();

        this->copy_half_buffer(	halfId,
                                    0,			// First valid position
                                    this->m_hsize, // src buf size
                                    rawData,	// Destination buffer
                                    this->m_hsize); // dst buf size
        this->m_lastBucketSize = -1;
    } else {
        omni_mutex_lock lock(this->m_lastHalfMutex);
        this->m_lastBucketSize = -1;
        this->m_lastBucketBegin = 0;
        this->m_lastHalfId = halfId;
        gettimeofday(&this->m_lastHalfTimestamp, 0);
    }
    this->m_bufferPosition.go_next();
}


/// Saves selected half buffer data into shared buffer, or just the
/// half id if we don't want to use shared buffer. For normal cases where
/// we want to save the full buffer use save_half_buffer().
///
/// @param halfId Half id to read data from
/// @param first Which is the first value in the buffer. Normally 0. If not 0
///              we take the buffer as circular, meaning:
///               (from first to m_hsize) + (from 0 to X), where X is
///               so that the resulting buffer has 'count' length.
/// @param count Size of the buffer. Must be minor than m_hsize
void InputBuffer::save_special_half_buffer(U16 halfId, U32 first, U32 count)
{
    DBGCOUT	<< "\n\t" << DBGVAR(count)
            << "\n\t" << DBGVAR(this->m_hsize)
            << "\n\t" << DBGVAR(first)
            << std::endl;
    assert(count <= this->m_hsize);

    if (this->m_useSharedBuffer) {
        SharedBufferU16::WriteLock lock(
                                        *this->m_sharedBuffer,
                                        this->m_bufferPosition.current());
        U16 * rawData = lock.get();

        this->copy_half_buffer (	halfId,
                                    first,
                                    count, // source buf size
                                    rawData, // destination buffer
                                    this->m_hsize ); // dest buf size

        this->m_lastBucketSize = count;
    } else {
        omni_mutex_lock lock(this->m_lastHalfMutex);
        this->m_lastBucketSize = count;
        this->m_lastBucketBegin = first;
        this->m_lastHalfId = halfId;
        gettimeofday(&this->m_lastHalfTimestamp, 0);
    }
    this->m_bufferPosition.go_next();
}

/// Do the actual copy from InputDeviceInterface memory to a buffer.
/// See save_special_half_buffer() to get a better idea of what it does.
/// @see save_special_half_buffer(), read_half_buffer(), save_half_buffer()
void InputBuffer::copy_half_buffer(
                U16 halfId, U32 first, U32 count, U16* resBuf, U32 resSz) const
{
    U32 M = resSz - first;
    if (count <= M || M == 0) {
        this->device().transfer_from_buf(resBuf, first, count, halfId);
    } else {
        this->device().transfer_from_buf(resBuf, first, M, halfId);
        this->device().transfer_from_buf(resBuf + M, 0, count - M, halfId);
    }
}



const U16* InputBuffer::read_half_buffer__shared_buffer(SharedBufferU16::ReadLock& sbLock,
                                        int bucket, struct timeval &time) const
{
    time = *sbLock.get_time(bucket);
    return sbLock.get(bucket);
}

const U16* InputBuffer::read_half_buffer__no_shared_buffer(struct timeval &time) const
{
    omni_mutex_lock lock(this->m_lastHalfMutex);

    size_t memsz = this->m_hsize;
    if ( this->m_lastBucketSize != -1 )
        memsz = this->m_lastBucketSize;

    const U16* v = this->device().get_buf(this->m_lastHalfId);

    // PS-DASK does not support get_buf() and return 0, meaning
    // we should use transfer_from_buf [ copy_half_buffer() ] instead.
    if (!v) {
        // Once in this case, If we try to read two channels, it would
        // call copy_half_buffer() twice, once per channel, each time
        // read_half_buffer() is executed! To avoid this we remember the
        // last timestamp.
        if (this->m_lastHalfVectorTimestamp != this->m_lastHalfTimestamp) {
            this->m_lastHalfVector.resize(memsz);
            U16* v2 = &this->m_lastHalfVector[0];
            this->copy_half_buffer( this->m_lastHalfId,
                                    this->m_lastBucketBegin,
                                    memsz, // Source size
                                    &this->m_lastHalfVector[0], // Dest buffer
                                    this->m_hsize); // Destination buf size
            this->m_lastHalfVectorTimestamp = this->m_lastHalfTimestamp;
        }

        v = &this->m_lastHalfVector[0];
    }

    time = this->m_lastHalfTimestamp;

    return v;
}

InputBufferReadLock::InputBufferReadLock(
        const InputBuffer &parent, int max, int backOffset) : m_parent(&parent), m_lock(0), m_ok(false)
{
    if (m_parent->m_useSharedBuffer) {
        assert(this->m_parent->m_hsize == this->m_parent->m_sharedBuffer->bucket_size());
        
        const size_t totalBuckets = this->m_parent->m_sharedBuffer->total_buckets();
        if (max > totalBuckets)
            throw_error(
                    "Too many data",
                    "Trying to retrieve more data than internally stored" ,
                    _CFN_);

        if(!this->m_parent->m_bufferPosition.last(max, backOffset, m_firstBucket, m_lastBucket)) {
            m_ok = false;
            return;
        }
		m_lock = new SharedBufferU16::IterateReadLock(*this->m_parent->m_sharedBuffer, m_firstBucket, m_lastBucket);
        m_total = m_lock->total();
        
        m_ok = true;
    } else {
        if (max != 1)
            throw_error(
                    "Too many data",
                    "Trying to retrieve more data than internally stored" ,
                    _CFN_);
        m_total = 1;
        m_ok = true;
    }
}

size_t InputBufferReadLock::last_bucket_size() const
{
    const int lastBucketSize = this->m_parent->m_lastBucketSize;
    if(lastBucketSize < 0)
        return this->m_parent->m_hsize;
    else
        return lastBucketSize;
}

bool InputBufferReadLock::iterate(int & bucket)
{
    if (m_lock)
        return this->m_lock->iterate(bucket);
    else
        return false;
}

const U16* InputBufferReadLock::read(int bucket, struct timeval &time) const
{
    if (m_lock)
        return this->m_parent->read_half_buffer__shared_buffer(*this->m_lock, bucket, time);
    else
        return this->m_parent->read_half_buffer__no_shared_buffer(time);
}
