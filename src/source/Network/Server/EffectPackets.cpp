#include "stdafx.h"
#include "EffectPackets.h"
#include "WSclient.h"
#include "Engine/Object/ZzzCharacter.h"
#include "GameLogic/Buffs/BuffEffects.h"
#include "GameLogic/Buffs/w_BuffTimeControl.h"
#include "UI/Chat/ChatMessages.h"
#include "I18N/All.h"

namespace Network::Server
{
void ReceiveEffectDuration(const BYTE* ReceiveBuffer)
{
    auto Data = (LPPMSG_ITEMEFFECTCANCEL)ReceiveBuffer;

    auto bufftype = static_cast<eBuffState>(Data->byBuffType);

    if (bufftype == eBuffNone || bufftype >= eBuff_Count)
        return;

    if (Data->byEffectOption == 0)
    {
        RegisterBuff(bufftype, (&Hero->Object), Data->wEffectTime);
        TheBuffTimeControl().RefreshEffectDuration(bufftype, Data->wEffectTime);

        if (bufftype == eBuff_HelpNpc)
        {
            UI::Chat::PostSystem(I18N::Game::DamageAndDefenseIncreasedWithABlessing,
                                 mu::ui::window::TYPE_SYSTEM_MESSAGE);
        }
    }
    else
    {
        UnRegisterBuff(bufftype, (&Hero->Object));
    }
}
} // namespace Network::Server
