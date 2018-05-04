#pragma once

#include "KArray.h"

namespace Rtl
{
    class KRTL_CLASS KBitmap
    {
    public:

        KBitmap() = default;
        ~KBitmap() = default;

        bool Initialize(size_t numberOfBits)
        {
#ifdef _WIN64
            size_t bitsPerMachineWord = 64;
#else
            size_t bitsPerMachineWord = 32;
#endif

            if (!m_storage.resize((numberOfBits + bitsPerMachineWord - 1) / bitsPerMachineWord))
                return false;

            RtlInitializeBitMapEx(&m_bitmap, &m_storage[0], numberOfBits);
            RtlClearAllBitsEx(&m_bitmap);
            return true;
        }

        bool TestBit(size_t bitNumber)
        {
            return RtlCheckBitEx(&m_bitmap, bitNumber);
        }

        void SetBit(size_t bitNumber)
        {
            RtlSetBitEx(&m_bitmap, bitNumber);
        }

        void ClearBit(size_t bitNumber)
        {
            RtlClearBitEx(&m_bitmap, bitNumber);
        }

        size_t FindSetBits(size_t numberToFind, size_t hintIndex)
        {
            return RtlFindSetBitsEx(&m_bitmap, numberToFind, hintIndex);
        }

        size_t FindSetBitsAndClear(size_t numberToFind, size_t hintIndex)
        {
            return RtlFindSetBitsAndClearEx(&m_bitmap, numberToFind, hintIndex);
        }

    private:
        RTL_BITMAP_EX m_bitmap;
        Rtl::KArray<SIZE_T> m_storage;
    };

}
