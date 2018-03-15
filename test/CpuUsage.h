#pragma once
#if _WIN32
#include <windows.h>

class CpuUsage {
  public:
    CpuUsage();

    short GetUsage();

  private:
    ULONGLONG SubtractTimes(const FILETIME &ftA, const FILETIME &ftB);
    bool EnoughTimePassed();
    inline bool IsFirstRun() const { return (m_dwLastRun == 0); }

    // system total times
    FILETIME m_ftPrevSysKernel;
    FILETIME m_ftPrevSysUser;

    // process times
    FILETIME m_ftPrevProcKernel;
    FILETIME m_ftPrevProcUser;

    short m_nCpuUsage;
    ULONGLONG m_dwLastRun;

    volatile LONG m_lRunCount;
};
#else

class CpuUsage {
  public:
    CpuUsage() {}

    short GetUsage() { return 0; }

#endif