
#pragma once

#include "SharedBuffer.cpp"
#include "CircularPosition.h"
#include "AdlinkBase.h"
#include "AdlHelp.h"
#include <vector>

typedef SharedBuffer<U16> SharedBufferU16;
class InputDeviceInterface;

class InputBuffer : public AdlinkBase
{
    friend class InputBufferReadLock;
    friend class InputBufferWriteLock;

    InputDeviceInterface* m_device;

    InputDeviceInterface & device() { return *this->m_device; }
    const InputDeviceInterface & device() const { return *this->m_device; }
    
    /// @name Private memory management variables
    /// @{
        U32     m_hsize;            //! Size of each one of the half buffers
    
        /// A buffer, divided in parts(buckets) of the size of a half buffer,
        /// which can be locked by one single writer or multiple readers
        SharedBufferU16*    m_sharedBuffer;
        
        /// Maintains which bucket (half buffer sized) of m_sharedBuffer has
        /// been the last written. The readers then know what to read.
        CircularPosition    m_bufferPosition;
        
        /// In a Pre-triggered acquisition, the last half buffer may not get
        /// full, so we have to reflect this in the last bucket.
        /// -1 means full size.
        ssize_t                 m_lastBucketSize;
        
        /// Even more, if not using double buffer, the first data in last
        /// buffer bay be in a position different than 0!
        /// @see save_special_half_buffer()
        U32                 m_lastBucketBegin;

        /// If we don't use SharedBuffer we still ned a way to know the
        /// timestamp of the last acquisition. That's were we store it.
        /// Protected by m_lastHalfMutex
        struct timeval      m_lastHalfTimestamp;
    /// @}
        
    /// @name Without SharedBuffer Memory Management
    /// @see save_half_buffer
    /// @see read_half_buffer
    /// @{
        /// If true, we are using shared buffer to store the half buffers
        /// as soon as they can be read. If false, they will only be read
        /// when requested. It's not always possible to disable it.
        /// @see m_canIgnoreSharedBuffer
        bool m_useSharedBuffer;
    
        /// What's the difference with m_halfId? m_halfId is used only by one
        /// thread. m_lastHalfId is mutex protected, to be used only if not
        /// using shared buffer ( m_useSharedBuffer == false ). Because
        /// in this case, we have a thread writing and some threads reading it.
        U16 m_lastHalfId;
    
        /// @see m_lastHalfId
        mutable omni_mutex m_lastHalfMutex;
    
        /// It stores the last half buffer when read without SharedBuffer
        /// support nor "get_buf".
        /// @see read_half_buffer, m_lastHalfVectorTimestamp
        mutable std::vector<U16> m_lastHalfVector;

        /// Timestamp at which we copied m_lastHalfVector, so we don't
        /// need to re-copy it if it has not changed!
        /// @see read_half_buffer, m_lastHalfVector
        mutable struct timeval m_lastHalfVectorTimestamp;
    /// @}

    void copy_half_buffer(
                U16 halfId, U32 first, U32 count, U16* resBuf, U32 resSz) const;
    
    const U16* read_half_buffer(
                                        SharedBufferU16::ReadLock& lock,
                                        int bucket,
                                        struct timeval &time) const;

    const U16* read_half_buffer__shared_buffer(SharedBufferU16::ReadLock& lock,
                                        int bucket, struct timeval &time) const;
    const U16* read_half_buffer__no_shared_buffer(struct timeval &time) const;

public:

    InputBuffer() :
        m_device(0),
        m_hsize(0),
        m_useSharedBuffer(true),
        m_sharedBuffer(0),
        m_lastBucketSize(-1),
        m_lastBucketBegin(0)
    {
    }

    ~InputBuffer()
    {
        delete this->m_sharedBuffer;
    }

    void set_device(InputDeviceInterface* device);

    void rebuild(U32 hsize, long total_buckets, bool useSharedBuffer);

    void save_half_buffer( U16 halfId );
    void save_special_half_buffer(U16 halfId, U32 first, U32 count);



    void init_circular_buffer()
    {
        this->m_bufferPosition.starts_with(0);
        // Take the first element, to write into
        this->m_bufferPosition.go_next();
    }

    void stop_circular_buffer()
    {
        this->m_bufferPosition.stop();
    }
};

class InputBufferWriteLock : public AdlinkBase
{
    friend class InputBuffer;
    SharedBufferU16::WriteLock* m_lock;
    InputBuffer* m_parent;
public:
    InputBufferWriteLock(InputBuffer& parent) : m_parent(&parent)
    {
        assert(m_parent->m_useSharedBuffer);
        m_lock = new SharedBufferU16::WriteLock(*parent.m_sharedBuffer, parent.m_bufferPosition.current());
    }

    const U16* get() const { return m_lock->get(); }
    U16* get() { return m_lock->get(); }

    ~InputBufferWriteLock()
    {
        delete m_lock;
        this->m_parent->m_bufferPosition.go_next();
    }
};

class InputBufferReadLock : public AdlinkBase
{
    friend class InputBuffer;

    const InputBuffer* m_parent;
	SharedBufferU16::IterateReadLock* m_lock;

    bool m_ok;

public:

    bool ok() const { return this->m_ok; }

    int m_firstBucket;
    int m_lastBucket;
    size_t m_total;
    
    InputBufferReadLock(const InputBuffer &parent, int max=1, int backOffset=0);

    ~InputBufferReadLock()
    {
        delete m_lock;
    }

    size_t last_bucket_size() const;

    int first_bucket() const { return this->m_firstBucket; }
    int last_bucket() const { return this->m_lastBucket; }
    size_t total() const { return this->m_total; }

    bool iterate(int & bucket);
    const U16* read(int bucket, struct timeval &time) const;

};
