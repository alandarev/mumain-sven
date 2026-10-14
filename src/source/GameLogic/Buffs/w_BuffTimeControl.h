#pragma once

#include "EffectDuration.h"

SmartPointer(BuffTimeControl);

class BuffTimeControl
{
public:
    static BuffTimeControlPtr Make();
    virtual ~BuffTimeControl();

public:
    void RegisterBuffTime(eBuffState bufftype, DWORD curbufftime);
    void RefreshEffectDuration(eBuffState bufftype, std::uint32_t seconds);
    void RemoveEffectDuration(eBuffState bufftype);
    bool UnRegisterBuffTime(eBuffState bufftype);
    bool IsBuffTime(eBuffTimeType bufftype);

public:
    void GetBuffStringTime(eBuffState bufftype, std::wstring& timeText);
    void GetBuffStringTime(DWORD type, std::wstring& timeText, bool issecond = true);
    const DWORD GetBuffTime(DWORD type);
    DWORD GetRemainingMilliseconds(eBuffState bufftype) const;
    void GetStringTime(DWORD time, std::wstring& timeText, bool isSecond = true);

private:
    eBuffTimeType CheckBuffTimeType(eBuffState bufftype);
    DWORD GetBuffEventTime(eBuffTimeType bufftimetype);
    DWORD GetBuffMaxTime(eBuffState bufftype, DWORD curbufftime = 0);
    bool CheckBuffTime(DWORD type);
    BuffTimeControl();

private:
    struct BuffTimeInfo
    {
        eBuffState s_BuffType;
        DWORD s_CurBuffTime;
        DWORD s_EventBuffTime;

        BuffTimeInfo() : s_BuffType(eBuffNone), s_CurBuffTime(0), s_EventBuffTime(0) {}
    };
    typedef std::map<DWORD, BuffTimeInfo> BuffTimeInfoMap;

private:
    BuffTimeInfoMap m_BuffTimeList;
    std::map<eBuffState, GameLogic::Buffs::EffectDuration> m_EffectDurations;
};
