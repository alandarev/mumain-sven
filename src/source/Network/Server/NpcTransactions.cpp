#include "stdafx.h"
#include "Network/Server/NpcTransactions.h"
#include "Network/Server/WSclient.h"
#include "UI/Inventory/InventoryContents.h"
#include "UI/Inventory/ShopUpdates.h"
#include "UI/Core/WindowAccess.h"
#include "UI/Chat/ChatMessages.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInventory.h"
#include "GameLogic/Items/InventoryUtils.h"
#include "Audio/DSPlaySound.h"
#include "I18N/All.h"

extern int BuyCost;
extern int CalcItemLength(std::span<const BYTE> itemData);

void ReceiveBuyExtended(const std::span<const BYTE> ReceiveBuffer)
{
    auto Data = safe_cast<PHEADER_DEFAULT_ITEM_EXTENDED_HEAD>(ReceiveBuffer);
    if (Data == nullptr)
    {
        assert(false);
        return;
    }

    constexpr BYTE BUY_FAILED = 0xFE;
    constexpr BYTE BUY_FAILED_SILENT = 0xFF;

    auto Offset = sizeof(PHEADER_DEFAULT_ITEM_EXTENDED_HEAD);
    auto itemData = ReceiveBuffer.subspan(Offset);
    if (!itemData.empty())
    {
        int length = CalcItemLength(itemData);
        itemData = itemData.subspan(0, length);
    }

    if (Data->Index == BUY_FAILED)
    {
        UI::Windows::HideAll();
        UI::Chat::PostChat(Hero->ID, I18N::Game::CannotBeTraded, mu::ui::window::TYPE_ERROR_MESSAGE);
    }
    else if (Data->Index == BUY_FAILED_SILENT)
    {
        // do nothing, error message is sent separately.
    }
    else
    {
        if (IsPlayerInventorySlot(Data->Index))
            UI::Inventory::InsertItem(Data->Index, itemData);

        PlayBuffer(SOUND_GET_ITEM01);
    }

    BuyCost = 0;
    UI::Shop::NpcPurchaseFinished(IsPlayerInventorySlot(Data->Index) && !itemData.empty(), Data->Index);

    g_ConsoleDebug->Write(MCD_RECEIVE, L"0x32 [ReceiveBuy(%d)]", Data->Index);
}

void ReceiveSell(const BYTE* ReceiveBuffer)
{
    auto Data = (LPPRECEIVE_GOLD)ReceiveBuffer;
    if (Data->Flag != 0)
    {
        if (Data->Flag == 0xff)
        {
            UI::Inventory::RestorePickedItem();

            UI::Chat::PostChat(Hero->ID, I18N::Game::CannotBeSold, mu::ui::window::TYPE_ERROR_MESSAGE);
        }
        else if (Data->Flag == 0xfe)
        {
            UI::Inventory::RestorePickedItem();
            UI::Windows::HideAll();

            UI::Chat::PostChat(Hero->ID, I18N::Game::CannotBeSold, mu::ui::window::TYPE_ERROR_MESSAGE);
        }
        else
        {
            UI::Inventory::DiscardPickedItem();

            CharacterMachine->Gold = Data->Gold;

            PlayBuffer(SOUND_GET_ITEM01);
        }
    }
    else
    {
        UI::Inventory::RestorePickedItem();
    }

    UI::Shop::NpcSaleFinished(Data->Flag != 0 && Data->Flag != 0xff && Data->Flag != 0xfe);
}

void ReceiveRepair(const BYTE* ReceiveBuffer)
{
    auto Data = (LPPRECEIVE_REPAIR_GOLD)ReceiveBuffer;

    if (Data->Gold != 0)
    {
        CharacterMachine->Gold = Data->Gold;
        CharacterMachine->CalculateAll();
        PlayBuffer(SOUND_REPAIR);
    }

    UI::Shop::NpcRepairFinished(Data->Gold != 0);

    g_ConsoleDebug->Write(MCD_RECEIVE, L"0x34 [ReceiveRepair(%d)]", Data->Gold);
}
