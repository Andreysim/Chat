#ifndef _EVENT_H_
#define _EVENT_H_

#include <mutex>
#include <condition_variable>

class Event
{
public:
    typedef std::unique_lock<std::mutex> MutexLock;

    struct SignalData
    {
        uint32_t nSignaled = 0;
        bool state = false;
        bool signalType = false;
    };

    Event(bool manualReset = false, bool initialState = false) 
        : manualReset(manualReset),
        signalData(new SignalData()),
        nWaits(0)
    {
        signalData->state = initialState;
    }
    ~Event() { delete signalData; }

    void Signal(bool bAll = false)
    {
        MutexLock lk(mtx);
        signalData->signalType = bAll || manualReset;
        if (nWaits != 0)
        {
            if (signalData->signalType)
            {
                signalData->nSignaled = nWaits;
                nWaits = 0;
                condVar.notify_all();
                signalData = new SignalData();
            }
            else
            {
                signalData->nSignaled += 1;
                nWaits -= 1;
                condVar.notify_one();
            }
            signalData->state = manualReset;
        }
        else  // leave notified
            signalData->state = true;
    }
    void Reset()
    {
        MutexLock lk(mtx);
        signalData->state = false;
    }

    void Wait()
    {
        Wait((std::chrono::milliseconds::max)());
    }
    template<class _Rep, class _Period>
    bool Wait(std::chrono::duration<_Rep, _Period> timeout)
    {
        MutexLock lk(mtx);
        if (!signalData->state)
        {
            ++nWaits;

            SignalData* sd = signalData;

            if (timeout == (timeout.max)())
                condVar.wait(lk, [sd] { return sd->nSignaled != 0; }); // infinite wait
            else
                if (!condVar.wait_for(lk, timeout, [sd] { return sd->nSignaled != 0; }))
                {
                    --nWaits;
                    return false;
                }
            --sd->nSignaled;

            if (sd->signalType && sd->nSignaled == 0)
                delete sd;
        }
        else if (!manualReset)
            signalData->state = false;
        return true;
    }

private:
    std::mutex mtx;
    std::condition_variable condVar;
    SignalData* signalData;
    uint32_t nWaits;
    bool manualReset;
};


#endif // !_EVENT_H_

