
#pragma once

//
// Kernel stopwatch using QPC for millisecond granularity
//
class KStopwatch
{
public:
    KStopwatch() = default;
    ~KStopwatch() = default;

    void Start()
    {
        m_startTime = KeQueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&m_frequency)).QuadPart;
    }

    // Return the elapsed time immediately if they caller knows they want it
    ULONG64 Stop()
    {
        m_stopTime = KeQueryPerformanceCounter(nullptr).QuadPart;
        return GetElapsedTimeInMilliseconds();
    }

    // Expose the elapsed time independently if the caller needs it later than when calling Stop()
    ULONG64 GetElapsedTimeInMilliseconds() const
    {
        // guard against calling methods out of order
        if ((m_frequency == 0) || (m_stopTime < m_startTime))
        {
            return 0;
        }

        // time in milliseconds (qpc / qpf == seconds)
        auto delta = m_stopTime - m_startTime;
        delta *= 1000;
        delta /= m_frequency;

        return static_cast<ULONG64>(delta);
    }

private:

    LONG64 m_startTime = 0;
    LONG64 m_frequency = 0;
    LONG64 m_stopTime = 0;
};

