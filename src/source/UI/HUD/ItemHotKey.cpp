#include "stdafx.h"
#include "UI/HUD/ItemHotKey.h"
#include <algorithm>
#include "I18N/All.h"

#include "UI/Options/OptionWindow.h"
#include "UI/Core/WindowSystem.h"
#include "UI/Widgets/UIBaseDef.h"
#include "Audio/DSPlaySound.h"
#include "Engine/Object/ZzzInfomation.h"
#include "Render/Models/ZzzBMD.h"
#include "Engine/Object/ZzzObject.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInterface.h"
#include "Engine/Object/ZzzInventory.h"

#include "GameLogic/Items/CSItemOption.h"
#include "GameLogic/Events/CSChaosCastle.h"
#include "World/MapInfra/MapManager.h"
#include "Character/CharacterManager.h"
#include "GameLogic/Skills/SkillManager.h"
#include "UI/HUD/Skills/SkillIconAtlas.h"
#include "UI/HUD/Skills/SkillTooltip.h"
#include "UI/Scaling/UITransform.h"
#include "Core/Time/CTimCheck.h"
#include "GameLogic/Social/MonkSystem.h"
#include "GameLogic/Items/ItemCategories.h"

#ifdef PBG_ADD_INGAMESHOP_UI_MAINFRAME
#include "GameShop/InGameShopSystem.h"
#endif //PBG_ADD_INGAMESHOP_UI_MAINFRAME

#include "Render/RmlUi/RmlUiRuntime.h"
#include "Render/Textures/ZzzOpenglUtil.h"
#include "Render/Renderer/MuRenderer.h"
#include "Camera/CameraProjection.h"
#include "UI/RmlBridge/RmlDocumentVisibility.h"
#include "UI/RmlBridge/RmlTheme.h"
#include "UI/RmlBridge/RmlWorkspaceParticipant.h"
#include "UI/Placement/WindowPlacement.h"
#include "UI/RmlBridge/RmlTooltip.h"
#include "Core/Utilities/StringUtils.h"
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>
#include "GameLogic/Quests/QuestMng.h"
#include "UI/Social/FriendWindow.h"
#include "UI/Social/FriendWindow.h"

namespace
{
// The inventory index of the first item from `iStartItemType` down to `iEndItemType` the hotkey
// uses (healing potions of any level, other items of `itemLevel`), or with `bItemCount` their
// total count.
int ResolveInventoryRange(int iStartItemType, int iEndItemType, int itemLevel, bool bItemCount)
{
    int iItemCount = 0;
    ITEM* pItem = NULL;

    int iNumberofItems = g_pMyInventory->GetInventoryCtrl()->GetNumberOfItems();
    for (int i = iStartItemType; i >= iEndItemType; --i)
    {
        if (bItemCount)
        {
            for (int j = 0; j < iNumberofItems; ++j)
            {
                pItem = g_pMyInventory->GetInventoryCtrl()->GetItem(j);
                if (pItem == NULL)
                {
                    continue;
                }

                if ((pItem->Type == i && pItem->Level == itemLevel) ||
                    (pItem->Type == i && GameLogic::Items::IsHealingPotion(pItem)))
                {
                    if (pItem->Type == ITEM_ALE || pItem->Type == ITEM_TOWN_PORTAL_SCROLL ||
                        pItem->Type == ITEM_REMEDY_OF_LOVE)
                    {
                        iItemCount++;
                    }
                    else
                    {
                        iItemCount += pItem->Durability;
                    }
                }
            }
        }
        else
        {
            int iIndex = -1;
            if (GameLogic::Items::IsHealingPotionType(i))
            {
                iIndex = g_pMyInventory->FindItemReverseIndex(i);
            }
            else
            {
                iIndex = g_pMyInventory->FindItemReverseIndex(i, itemLevel);
            }

            if (-1 != iIndex)
            {
                pItem = g_pMyInventory->FindItem(iIndex);
                if ((pItem->Type != ITEM_SIEGE_POTION && pItem->Type != ITEM_TOWN_PORTAL_SCROLL &&
                     pItem->Type != ITEM_REMEDY_OF_LOVE) ||
                    pItem->Level == itemLevel)
                {
                    return iIndex;
                }
            }
        }
    }

    if (bItemCount == true)
    {
        return iItemCount;
    }

    return -1;
}
} // namespace

mu::ui::window::CItemHotKey::CItemHotKey()
{
    for (int i = 0; i < HOTKEY_COUNT; ++i)
    {
        m_iHotKeyItemType[i] = -1;
        m_iHotKeyItemLevel[i] = 0;
        m_SlotTargets[i] = std::make_unique<UI::Items::ItemCameraTarget>(
            [this, i](const Rml::Vector2f& offset, const Rml::Vector2f& size) { RenderSlot(i, offset, size); });
    }
}

mu::ui::window::CItemHotKey::~CItemHotKey()
{
}

bool mu::ui::window::CItemHotKey::UpdateKeyEvent()
{
    int iIndex = -1;

    if (mu::ui::window::IsPress('Q') == true)
    {
        iIndex = GetHotKeyItemIndex(HOTKEY_Q);
    }
    else if (mu::ui::window::IsPress('W') == true)
    {
        iIndex = GetHotKeyItemIndex(HOTKEY_W);
    }
    else if (mu::ui::window::IsPress('E') == true)
    {
        iIndex = GetHotKeyItemIndex(HOTKEY_E);
    }
    else if (mu::ui::window::IsPress('R') == true)
    {
        iIndex = GetHotKeyItemIndex(HOTKEY_R);
    }

    if (iIndex == -1)
        return true;
    UseInventoryItem(iIndex);
    return false;
}

bool mu::ui::window::CItemHotKey::UseInventoryItem(int index)
{
    if (Hero == nullptr || Hero->Dead != 0 || g_pMyInventory == nullptr || index < 0)
        return false;
    ITEM* item = g_pMyInventory->FindItem(index);
    if (item == nullptr || !CMyInventory::CanRegisterItemHotKey(item->Type))
        return false;
    if (GameLogic::Items::IsElixir(item))
    {
        const std::list<eBuffState> effects{eBuff_SecretPotion1, eBuff_SecretPotion2, eBuff_SecretPotion3,
                                            eBuff_SecretPotion4, eBuff_SecretPotion5};
        if (g_isCharacterBufflist((&Hero->Object), effects) != eBuffNone)
        {
            CreateOkMessageBox(I18N::Game::YouCannotUseThisItemWhileThePotionEffectsRemainActive,
                               RGBA(255, 30, 0, 255));
            return false;
        }
    }
    SendRequestUse(index, 0);
    return true;
}

int mu::ui::window::CItemHotKey::ResolveConsumable(int type, int level, bool count)
{
    if (g_pMyInventory == nullptr || g_pMyInventory->GetInventoryCtrl() == nullptr ||
        !CMyInventory::CanRegisterItemHotKey(type))
        return count ? 0 : -1;
    int first = type;
    int last = type;
    if (GameLogic::Items::IsHealingPotionType(type))
    {
        first = ITEM_LARGE_HEALING_POTION;
        last = ITEM_APPLE;
    }
    else if (GameLogic::Items::IsManaPotionType(type))
    {
        first = ITEM_LARGE_MANA_POTION;
        last = ITEM_SMALL_MANA_POTION;
    }
    return ResolveInventoryRange(first, last, level, count);
}

int mu::ui::window::CItemHotKey::GetHotKeyItemIndex(int iType, bool bItemCount)
{
    int iStartItemType = 0, iEndItemType = 0;

    switch (iType)
    {
    case HOTKEY_Q:
        if (GetHotKeyCommonItem(iType, iStartItemType, iEndItemType) == false)
        {
            if (GameLogic::Items::IsManaPotionType(m_iHotKeyItemType[iType]))
            {
                iStartItemType = ITEM_LARGE_MANA_POTION; iEndItemType = ITEM_SMALL_MANA_POTION;
            }
            else
            {
                iStartItemType = ITEM_LARGE_HEALING_POTION; iEndItemType = ITEM_APPLE;
            }
        }
        break;
    case HOTKEY_W:
        if (GetHotKeyCommonItem(iType, iStartItemType, iEndItemType) == false)
        {
            if (GameLogic::Items::IsHealingPotionType(m_iHotKeyItemType[iType]))
            {
                iStartItemType = ITEM_LARGE_HEALING_POTION; iEndItemType = ITEM_APPLE;
            }
            else
            {
                iStartItemType = ITEM_LARGE_MANA_POTION; iEndItemType = ITEM_SMALL_MANA_POTION;
            }
        }
        break;
    case HOTKEY_E:
        if (GetHotKeyCommonItem(iType, iStartItemType, iEndItemType) == false)
        {
            if (GameLogic::Items::IsHealingPotionType(m_iHotKeyItemType[iType]))
            {
                iStartItemType = ITEM_LARGE_HEALING_POTION; iEndItemType = ITEM_APPLE;
            }
            else if (GameLogic::Items::IsManaPotionType(m_iHotKeyItemType[iType]))
            {
                iStartItemType = ITEM_LARGE_MANA_POTION; iEndItemType = ITEM_SMALL_MANA_POTION;
            }
            else
            {
                iStartItemType = ITEM_ANTIDOTE; iEndItemType = ITEM_ANTIDOTE;
            }
        }
        break;
    case HOTKEY_R:
        if (GetHotKeyCommonItem(iType, iStartItemType, iEndItemType) == false)
        {
            if (GameLogic::Items::IsHealingPotionType(m_iHotKeyItemType[iType]))
            {
                iStartItemType = ITEM_LARGE_HEALING_POTION; iEndItemType = ITEM_APPLE;
            }
            else if (GameLogic::Items::IsManaPotionType(m_iHotKeyItemType[iType]))
            {
                iStartItemType = ITEM_LARGE_MANA_POTION; iEndItemType = ITEM_SMALL_MANA_POTION;
            }
            else
            {
                iStartItemType = ITEM_LARGE_SHIELD_POTION; iEndItemType = ITEM_SMALL_SHIELD_POTION;
            }
        }
        break;
    }

    return ResolveInventoryRange(iStartItemType, iEndItemType, m_iHotKeyItemLevel[iType], bItemCount);
}

bool mu::ui::window::CItemHotKey::GetHotKeyCommonItem(IN int iHotKey, OUT int& iStart, OUT int& iEnd)
{
    switch (m_iHotKeyItemType[iHotKey])
    {
    case ITEM_SIEGE_POTION:
    case ITEM_ANTIDOTE:
    case ITEM_ALE:
    case ITEM_TOWN_PORTAL_SCROLL:
    case ITEM_REMEDY_OF_LOVE:
    case ITEM_JACK_OLANTERN_BLESSINGS:
    case ITEM_JACK_OLANTERN_WRATH:
    case ITEM_JACK_OLANTERN_CRY:
    case ITEM_JACK_OLANTERN_FOOD:
    case ITEM_JACK_OLANTERN_DRINK:
    case ITEM_ELITE_HEALING_POTION:
    case ITEM_ELITE_MANA_POTION:
    case ITEM_ELIXIR_OF_STRENGTH:
    case ITEM_ELIXIR_OF_AGILITY:
    case ITEM_ELIXIR_OF_HEALTH:
    case ITEM_ELIXIR_OF_ENERGY:
    case ITEM_ELIXIR_OF_CONTROL:
    case ITEM_MEDIUM_ELITE_HEALING_POTION:
    case ITEM_CHERRY_BLOSSOM_WINE:
    case ITEM_CHERRY_BLOSSOM_RICE_CAKE:
    case ITEM_CHERRY_BLOSSOM_FLOWER_PETAL:
    case ITEM_ELITE_SD_POTION:
        if (m_iHotKeyItemType[iHotKey] != ITEM_REMEDY_OF_LOVE || m_iHotKeyItemLevel[iHotKey] == 0)
        {
            iStart = iEnd = m_iHotKeyItemType[iHotKey];
            return true;
        }
        break;
    default:
        if (m_iHotKeyItemType[iHotKey] >= ITEM_SMALL_SHIELD_POTION && m_iHotKeyItemType[iHotKey] <= ITEM_LARGE_SHIELD_POTION)
        {
            iStart = ITEM_LARGE_SHIELD_POTION; iEnd = ITEM_SMALL_SHIELD_POTION;
            return true;
        }
        else if (GameLogic::Items::IsComplexPotionType(m_iHotKeyItemType[iHotKey]))
        {
            iStart = ITEM_LARGE_COMPLEX_POTION; iEnd = ITEM_SMALL_COMPLEX_POTION;
            return true;
        }
        break;
    }
    return false;
}

int mu::ui::window::CItemHotKey::GetHotKeyItemCount(int iType)
{
    return 0;
}

void mu::ui::window::CItemHotKey::SetHotKey(int iHotKey, int iItemType, int iItemLevel)
{
    if (iHotKey != -1 && CMyInventory::CanRegisterItemHotKey(iItemType) == true
        )
    {
        m_iHotKeyItemType[iHotKey] = iItemType;
        m_iHotKeyItemLevel[iHotKey] = iItemLevel;
    }
    else
    {
        m_iHotKeyItemType[iHotKey] = -1;
        m_iHotKeyItemLevel[iHotKey] = 0;
    }
}

int mu::ui::window::CItemHotKey::GetHotKey(int iHotKey)
{
    if (iHotKey != -1)
    {
        return m_iHotKeyItemType[iHotKey];
    }

    return -1;
}

int mu::ui::window::CItemHotKey::GetHotKeyLevel(int iHotKey)
{
    if (iHotKey != -1)
    {
        return m_iHotKeyItemLevel[iHotKey];
    }

    return 0;
}

ITEM* mu::ui::window::CItemHotKey::GetSlotItem(int iSlotIndex)
{
    const int iIndex = GetHotKeyItemIndex(iSlotIndex);
    return iIndex != -1 ? g_pMyInventory->FindItem(iIndex) : nullptr;
}

void mu::ui::window::CItemHotKey::SyncSlotIcons(Rml::ElementDocument* document)
{
    if (document == nullptr)
        return;
    for (int i = 0; i < HOTKEY_COUNT; ++i)
    {
        Rml::Element* icon = document->GetElementById("item_icon_" + std::to_string(i));
        if (icon == nullptr)
            continue;
        const bool filled = GetSlotItem(i) != nullptr;
        icon->SetClass("hidden", !filled);
        m_SlotTargets[i]->Sync(icon, m_bSlotIconsShown && filled);
    }
}

void mu::ui::window::CItemHotKey::SetSlotIconsShown(bool shown)
{
    m_bSlotIconsShown = shown;
    if (shown)
        return;
    for (auto& target : m_SlotTargets)
        target->Disable();
}

void mu::ui::window::CItemHotKey::RenderSlot(int iSlotIndex, const Rml::Vector2f& offset, const Rml::Vector2f& size)
{
    if (ITEM* pItem = GetSlotItem(iSlotIndex))
        RenderItem3DWithHover(offset.x, offset.y, size.x, size.y, pItem->Type, pItem->Level, 0, 0, m_iHoveredSlot == iSlotIndex);
}

namespace
{
constexpr float ReferenceWindowHeight = 480.f;

struct ItemPreviewFrame
{
    float width;
    float height;
    float fieldOfView;
};

ItemPreviewFrame EquipmentPreviewFrame(const ITEM& item, std::uint32_t width, std::uint32_t height)
{
    // Fit the whole inventory footprint, including long weapons, into the target.
    constexpr float InventoryCellSize = 20.f;
    constexpr float EquipmentModelScale = 0.85f;
    const auto& attribute = ItemAttribute[item.Type];
    const float referenceWidth = std::max(1, static_cast<int>(attribute.Width)) * InventoryCellSize;
    const float referenceHeight = std::max(1, static_cast<int>(attribute.Height)) * InventoryCellSize;
    const float scale = std::min(width / referenceWidth, height / referenceHeight) * EquipmentModelScale;
    return {referenceWidth * scale, referenceHeight * scale, WindowHeight / (ReferenceWindowHeight * scale)};
}

void RenderItemPreview(ITEM* item, std::uint32_t width, std::uint32_t height, bool hovered,
                       const ItemPreviewFrame& frame, bool picked = false)
{
    const float windowWidth = static_cast<float>(WindowWidth);
    const float windowHeight = static_cast<float>(WindowHeight);
    const float w = static_cast<float>(width);
    const float h = static_cast<float>(height);
    const float x = (windowWidth - frame.width) * 0.5f;
    const float y = (windowHeight - frame.height) * 0.5f;

    // Item picking shares the camera cache. Restore it after the offscreen preview.
    SaveCameraPerspective();
    const UI::Scaling::ScopedWindowPixels pixels(WindowWidth, WindowHeight);
    auto& renderer = mu::GetRenderer();
    renderer.SetMatrixMode(GL_PROJECTION);
    renderer.PushMatrix();
    renderer.LoadIdentity();
    renderer.Scale(windowWidth / w, windowHeight / h, 1.f);
    SetRenderViewport(0, 0, WindowWidth, WindowHeight);
    gluPerspective2(frame.fieldOfView, windowWidth / windowHeight, RENDER_ITEMVIEW_NEAR, RENDER_ITEMVIEW_FAR);
    renderer.SetMatrixMode(GL_MODELVIEW);
    renderer.PushMatrix();
    renderer.LoadIdentity();
    CameraProjection::GetOpenGLMatrix(g_Camera.Matrix);
    EnableDepthTest();
    EnableDepthMask();
    if (picked)
        RenderItem3D(x, y, frame.width, frame.height, item->Type, item->Level, item->ExcellentFlags,
                     item->AncientDiscriminator, true);
    else
        RenderItem3DWithHover(x, y, frame.width, frame.height, item->Type, item->Level, 0, 0, hovered);
    renderer.SetMatrixMode(GL_MODELVIEW);
    renderer.PopMatrix();
    renderer.SetMatrixMode(GL_PROJECTION);
    renderer.PopMatrix();
    RestoreCameraPerspective();
}
} // namespace

void mu::ui::window::CItemHotKey::RenderItemModel(ITEM* item, std::uint32_t width, std::uint32_t height, bool hovered,
                                                  bool fitToBox)
{
    if (item == nullptr || width == 0 || height == 0)
        return;
    constexpr float ReferenceItemWidth = 20.f;
    constexpr float ReferenceItemHeight = 28.f;
    constexpr float CompactModelScale = 1.35f;
    const float w = static_cast<float>(width);
    const float h = static_cast<float>(height);
    const float boxScale = std::min(w / ReferenceItemWidth, h / ReferenceItemHeight) * CompactModelScale;
    const float fieldOfView = fitToBox ? WindowHeight / (ReferenceWindowHeight * boxScale) : 1.f;
    RenderItemPreview(item, width, height, hovered, {w, h, fieldOfView});
}

void mu::ui::window::CItemHotKey::RenderEquipmentModel(ITEM* item, std::uint32_t width, std::uint32_t height,
                                                       bool hovered)
{
    if (item == nullptr || item->Type < 0 || item->Type >= MAX_ITEM || width == 0 || height == 0)
        return;
    RenderItemPreview(item, width, height, hovered, EquipmentPreviewFrame(*item, width, height));
}

void mu::ui::window::CItemHotKey::RenderPickedItemModel(ITEM* item, std::uint32_t width, std::uint32_t height)
{
    if (item == nullptr || item->Type < 0 || item->Type >= MAX_ITEM || width == 0 || height == 0)
        return;
    RenderItemPreview(item, width, height, false, EquipmentPreviewFrame(*item, width, height), true);
}

void mu::ui::window::CItemHotKey::OnHotkeySlotRightClick(int iSlotIndex)
{
    // RmlUi's Context now does hit-testing for these 4 slots (data-event-mouseup). `Hero->Dead !=
    // 0` reproduces the old call site's own alive guard.
    if (Hero->Dead != 0)
    {
        return;
    }
    int iIndex = GetHotKeyItemIndex(iSlotIndex);
    if (iIndex != -1)
    {
        SendRequestUse(iIndex, 0);
    }
}

int mu::ui::window::CItemHotKey::GetSlotItemCount(int iSlotIndex)
{
    return GetHotKeyItemIndex(iSlotIndex, true);
}

void mu::ui::window::CItemHotKey::SyncNativeLayout(Rml::ElementDocument* document, bool visible)
{
    for (int i = 0; i < HOTKEY_COUNT; ++i)
    {
        auto* icon = document ? document->GetElementById("item_icon_" + std::to_string(i)) : nullptr;
        if (m_SlotTargets[i])
            m_SlotTargets[i]->Sync(icon, visible && m_bSlotIconsShown && icon && !icon->IsClassSet("hidden"));
    }
}
