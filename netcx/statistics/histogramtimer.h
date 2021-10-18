// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include <KNew.h>
#include <KStopwatch.h>

namespace Statistics
{
    class Histogram;

    struct PreciseStopwatch
    {
        using Type = KStopwatch;
    };

    struct FastStopwatch
    {
        using Type = KTickCounterStopwatch;
    };

    template<typename Stopwatch = FastStopwatch>
    class Timer
    {
    public:
        NONPAGED
        Timer(
            _In_ Histogram *histogram,
            bool measureTime = true
        )
            : m_timeHistogram(histogram)
        {
            if (measureTime)
            {
                Start();
            }
        }

        NONPAGED
        ~Timer()
        {
            Stop();
        }

        NONPAGED
        void
        Start(
            void
        )
        {
            m_started = true;
            m_stopwatch.Start();
        }

        NONPAGED
        ULONG64
        Stop(
            void
        )
        {
            if (!m_started)
            {
                return 0;
            }

            auto value = m_stopwatch.Stop();
            if (m_timeHistogram)
            {
                m_timeHistogram->AddValue(value);
            }
            return value;
        }

    private:
        typename Stopwatch::Type
            m_stopwatch {};

        bool
            m_started = false;

        Histogram *
            m_timeHistogram {};
    };
}
