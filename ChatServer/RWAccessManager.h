#ifndef _RW_ACCESS_MANAGER_H_
#define _RW_ACCESS_MANAGER_H_

#include <memory>
#include "Event.h"

class RWAccessManager
{
public:
    typedef Event::MutexLock MutexLock;

    RWAccessManager()
        : m_canReadEvent(new Event),
        m_canWriteEvent(new Event),
        m_nCurrUsers(0),
        m_nPendingWriters(0),
        m_nPendingReaders(0)
    {
    }

    void LockRead()
    {
        MutexLock lk(m_mtx);
        if (m_nCurrUsers == -1 || m_nPendingWriters)
        {
            ++m_nPendingReaders;
            lk.unlock();
            m_canReadEvent->Wait();
        }
        else
            ++m_nCurrUsers;
    }
    void LockWrite()
    {
        MutexLock lk(m_mtx);
        if (m_nCurrUsers != 0)
        {
            ++m_nPendingWriters;
            lk.unlock();
            m_canWriteEvent->Wait();
        }
        else
            m_nCurrUsers = -1;
    }
    void Unlock()
    {
        MutexLock lk(m_mtx);

        if (m_nCurrUsers == -1)
            m_nCurrUsers = 0;
        else if (m_nCurrUsers)
            --m_nCurrUsers;

        if (!m_nCurrUsers)
        {
            if (m_nPendingWriters)
            {
                m_nCurrUsers = -1;
                --m_nPendingWriters;
                m_canWriteEvent->Signal(false);
            }
            else if (m_nPendingReaders)
            {
                m_nCurrUsers = m_nPendingReaders;
                m_nPendingReaders = 0;
                m_canReadEvent->Signal(true);
            }
        }
    }

private:
    std::unique_ptr<Event> m_canReadEvent;
    std::unique_ptr<Event> m_canWriteEvent;
    std::mutex m_mtx;
    uint32_t m_nCurrUsers;  // (-1) - 1 writer; (> 0) - N readers; 0 - no users
    uint32_t m_nPendingWriters;
    uint32_t m_nPendingReaders;
};

class RWLocker
{
public:
    RWLocker(RWAccessManager& rwm, bool write = false) : m_rwm(rwm) 
    {
        Lock(write);
    }
    ~RWLocker() 
    {
        Unlock();
    }
    void Lock(bool write = false)
    {
        write ? m_rwm.LockWrite() : m_rwm.LockRead();
    }
    void Unlock() 
    {
        m_rwm.Unlock(); 
    }

private:
    RWAccessManager& m_rwm;
};

#endif // !_RW_ACCESS_MANAGER_H_

