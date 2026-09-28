
#include "stdafx.h"
#include "UI/Events/KanturuEvent.h"
#include "UI/Dialogs/CommonMessageBox.h"
#include "UI/Core/WindowSystem.h"
#include "UI/Core/WindowGeometry.h"
#include "Engine/AI/ZzzAI.h"
#include "Render/Effects/ZzzEffect.h"
#include "I18N/All.h"

#include "GameLogic/Items/ChangeRingManager.h"
#include "GameLogic/Events/Cinematic/CDirection.h"
#include "Audio/DSPlaySound.h"

#include "Core/Utilities/StringUtils.h"
#include "Render/RmlUi/RmlUiRuntime.h"
#include "Render/Text/CUIRenderTextSDLTtf.h"
#include "UI/RmlBridge/RmlDocumentVisibility.h"
#include "UI/RmlBridge/RmlRootTransform.h"
#include "UI/RmlBridge/RmlTheme.h"

#include <RmlUi/Core/ElementDocument.h>

namespace
{
// RenderText() shrinks a text wider than its box to fit it: the size it drew `text` at.
float KanturuTextPxInBox(UI::Scaling::FontRole role, const UI::Scaling::Transform& transform, const wchar_t* text,
                         float boxWidth)
{
    g_pRenderText->SetFont(role == UI::Scaling::FontRole::Bold ? g_hFontBold : g_hFont);
    const int width = g_pRenderText->MeasureText(text, static_cast<int>(wcslen(text))).cx;
    return UI::Scaling::NativeTextPixelSizeInBox(role, transform, static_cast<float>(width), boxWidth);
}
} // namespace

mu::ui::window::CKanturu2ndEnterNpc::CKanturu2ndEnterNpc()
{
    m_pNewUIMng = NULL;
    m_Pos.x = m_Pos.y = 0;
    m_pNpcObject = NULL;
    m_dwRefreshTime = 0;
    m_dwRefreshButtonGapTime = 0;

    Initialize();
}

mu::ui::window::CKanturu2ndEnterNpc::~CKanturu2ndEnterNpc()
{
    Release();
}

void mu::ui::window::CKanturu2ndEnterNpc::Initialize()
{
    m_bNpcAnimation = false;
    m_bEnterRequest = false;

    m_iStateTextNum = 0;
    ZeroMemory(m_strSubject, sizeof(m_strSubject));
    for (int i = 0; i < KANTURU2ND_STATETEXT_MAX; i++)
    {
        ZeroMemory(m_strStateText[i], sizeof(m_strStateText[i]));
    }
}

bool mu::ui::window::CKanturu2ndEnterNpc::Create(CManager* pNewUIMng, int x, int y)
{
    if (NULL == pNewUIMng)
        return false;

    m_pNewUIMng = pNewUIMng;
    m_pNewUIMng->AddUIObj(mu::ui::window::INTERFACE_KANTURU2ND_ENTERNPC, this);

    SetPos(x, y);

    BuildRmlUi();
    UI::RmlBridge::RegisterForThemeReload(this, [this] { ReloadRmlTheme(); });

    Show(false);

    return true;
}

void mu::ui::window::CKanturu2ndEnterNpc::Release()
{
    UI::RmlBridge::UnregisterForThemeReload(this);

    if (m_pNewUIMng)
    {
        m_pNewUIMng->RemoveUIObj(this);
        m_pNewUIMng = NULL;
    }
}

void mu::ui::window::CKanturu2ndEnterNpc::SetPos(int x, int y)
{
    m_Pos.x = x;
    m_Pos.y = y;
}

bool mu::ui::window::CKanturu2ndEnterNpc::UpdateMouseEvent()
{
    // The Refresh, Enter and Close buttons are RmlUi's (see Update()); the window keeps the pointer.
    if (mu::ui::window::WindowGeometry(m_Pos.x, m_Pos.y, KANTURU2ND_ENTER_WINDOW_WIDTH, KANTURU2ND_ENTER_WINDOW_HEIGHT).Contains(MouseX, MouseY))
    {
        return false;
    }

    return true;
}

bool mu::ui::window::CKanturu2ndEnterNpc::UpdateKeyEvent()
{
    if (g_pNewUISystem->IsVisible(mu::ui::window::INTERFACE_KANTURU2ND_ENTERNPC) == true)
    {
        if (mu::ui::window::IsPress(VK_ESCAPE) == true)
        {
            g_pNewUISystem->Hide(mu::ui::window::INTERFACE_KANTURU2ND_ENTERNPC);
            PlayBuffer(SOUND_CLICK01);
            return false;
        }
    }

    return true;
}

bool mu::ui::window::CKanturu2ndEnterNpc::Update()
{
    // The Refresh button unlocks a second after its click (the original's BtnProcess()).
    if (m_RefreshLocked && timeGetTime() - m_dwRefreshButtonGapTime > KANTURU2ND_REFRESHBUTTON_GAPTIME)
    {
        m_RefreshLocked = false;
    }

    SyncRmlModel();

    // Clicks RmlUi reported, handled like the original's BtnProcess().
    const bool refresh = m_PendingRefresh;
    const bool enter = m_PendingEnter;
    const bool close = m_PendingClose;
    m_PendingRefresh = m_PendingEnter = m_PendingClose = false;

    if (g_pNewUISystem->IsVisible(mu::ui::window::INTERFACE_KANTURU2ND_ENTERNPC) == true)
    {
        if (refresh && !m_RefreshLocked)
        {
            ProcessRefresh();
        }
        else if (enter && !m_EnterLocked)
        {
            ProcessEnter();
        }
        else if (close)
        {
            g_pNewUISystem->Hide(mu::ui::window::INTERFACE_KANTURU2ND_ENTERNPC);
        }
    }

    if (g_pNewUISystem->IsVisible(mu::ui::window::INTERFACE_KANTURU2ND_ENTERNPC) == true)
    {
        if (timeGetTime() - m_dwRefreshTime > KANTURU2ND_REFRESH_GAPTIME)
        {
            SendRequestKanturu3rdInfo();
        }
    }

    return true;
}

bool mu::ui::window::CKanturu2ndEnterNpc::Render()
{
    // Nothing native left: the frame, the texts and the buttons are RmlUi. Kept because CObject
    // requires the override.
    return true;
}

float mu::ui::window::CKanturu2ndEnterNpc::GetLayerDepth()
{
    return 10.1f;
}

void mu::ui::window::CKanturu2ndEnterNpc::SetNpcObject(OBJECT* pObj)
{
    m_pNpcObject = pObj;
}

bool mu::ui::window::CKanturu2ndEnterNpc::IsNpcAnimation()
{
    return m_bNpcAnimation;
}

void mu::ui::window::CKanturu2ndEnterNpc::SetNpcAnimation(bool bValue)
{
    m_bNpcAnimation = bValue;
}

bool mu::ui::window::CKanturu2ndEnterNpc::IsEnterRequest()
{
    return m_bEnterRequest;
}

void mu::ui::window::CKanturu2ndEnterNpc::SetEnterRequest(bool bValue)
{
    m_bEnterRequest = bValue;
}

void mu::ui::window::CKanturu2ndEnterNpc::CreateMessageBox(BYTE btResult)
{
    wchar_t strMessage[256];
    if (btResult == POPUP_FAILED || btResult == POPUP_FAILED2)
    {
        wcscpy(strMessage, I18N::Game::FailedToEnter);
    }
    else if (btResult == POPUP_UNIRIA)
    {
        wcscpy(strMessage, I18N::Game::YouCannotWarpWhileRidingOnAUnicorn);
    }
    else if (btResult == POPUP_CHANGERING)
    {
        wcscpy(strMessage, I18N::Game::YouCanTWarpWearingTheRingOfTransformation);
    }
    else if (btResult == POPUP_NOT_HELPER)
    {
        wcscpy(strMessage, I18N::Game::YouCanOnlyWarpRidingA);
    }
    else
    {
        wcscpy(strMessage, I18N::Game::Lookup(2170 + btResult));
    }

    mu::ui::window::CreateOkMessageBox(strMessage);
}

void mu::ui::window::CKanturu2ndEnterNpc::ReceiveKanturu3rdInfo(BYTE btState, BYTE btDetailState, BYTE btEnter, BYTE btUserCount, int iRemainTime)
{
    if (m_pNpcObject && m_pNpcObject->CurrentAction == KANTURU2ND_NPC_ANI_ROT)
    {
        return;
    }

    if (g_MessageBox->IsEmpty() == false)
    {
        return;
    }

    Initialize();

    m_byState = btState;

    m_EnterLocked = btEnter != 1;

    if (btState == KANTURU_STATE_TOWER)
    {
        if (btDetailState == KANTURU_TOWER_REVITALIXATION || btDetailState == KANTURU_TOWER_NOTIFY)
        {
            wcscpy(m_strSubject, I18N::Game::YouMayNowProceedToTheRefineryTower);
            wcscpy(m_strStateText[0], I18N::Game::PathToTheRefineryTowerIsNowOpened);
            mu_swprintf(m_strStateText[1], I18N::Game::PathToTheRefineryTowerWillBeClosedInDHours, iRemainTime / 3600);
            m_iStateTextNum = 2;
        }
        else
        {
            wcscpy(m_strSubject, I18N::Game::YouCanTWarpToTheRefineryTower);
            wcscpy(m_strStateText[0], I18N::Game::DefeatTheNightmareThatControllingThe);
            wcscpy(m_strStateText[1], I18N::Game::EntranceIsRestrictedToEnsureThe);
            m_iStateTextNum = 2;
        }
    }
    else if (btState == KANTURU_STATE_MAYA_BATTLE)
    {
        if (btDetailState != KANTURU_MAYA_DIRECTION_STANBY1
            && btDetailState != KANTURU_MAYA_DIRECTION_STANBY2
            && btDetailState != KANTURU_MAYA_DIRECTION_STANBY3)
        {
            wcscpy(m_strSubject, I18N::Game::BattleWithMayaIsOngoing);
            mu_swprintf(m_strStateText[0], I18N::Game::DPlayersAreTryingToOpen, btUserCount);
        }
        else
        {
            wcscpy(m_strSubject, I18N::Game::MorePlayersAreNeededToOpenThePathToTheTower);

            if (btDetailState == KANTURU_MAYA_DIRECTION_STANBY1)
            {
                if (btUserCount < 15)
                {
                    wcscpy(m_strStateText[0], I18N::Game::YouMayNowEnter);
                }
                else if (btUserCount == 15)
                {
                    wcscpy(m_strStateText[0], I18N::Game::MoonstonePendantAuthenticationHasFailed);
                }
                else
                {
                    if (!m_EnterLocked)
                    {
                        wcscpy(m_strStateText[0], I18N::Game::YouMayNowEnter);
                    }
                    else
                    {
                        wcscpy(m_strStateText[0], I18N::Game::MoonstonePendantAuthenticationHasFailed);
                    }
                }
                m_iStateTextNum = 1;
            }
            else if (btDetailState == KANTURU_MAYA_DIRECTION_STANBY2)
            {
                if (btUserCount < 15)
                {
                    mu_swprintf(m_strStateText[0], I18N::Game::NightmareHasLostTheControlOf, btUserCount);
                    mu_swprintf(m_strStateText[1], I18N::Game::MorePowerFromDPlayersAreNeeded, 15 - btUserCount);
                    m_iStateTextNum = 2;
                }
                else if (btUserCount == 15)
                {
                    wcscpy(m_strStateText[0], I18N::Game::NightmareHasLostTheControlOfMayaSLeftHand);
                    m_iStateTextNum = 1;
                }
            }
            else if (btDetailState == KANTURU_MAYA_DIRECTION_STANBY3)
            {
                if (btUserCount < 15)
                {
                    mu_swprintf(m_strStateText[0], I18N::Game::NightmareHasLostTheControlOf2166, btUserCount);
                    mu_swprintf(m_strStateText[1], I18N::Game::MorePowerFromDPlayersAreNeeded, 15 - btUserCount);
                    m_iStateTextNum = 2;
                }
                else if (btUserCount == 15)
                {
                    wcscpy(m_strStateText[0], I18N::Game::NightmareHasLostTheControlOfMayaSLeftHand);
                    m_iStateTextNum = 1;
                }
            }
            else
            {
                if (!m_EnterLocked)
                {
                    wcscpy(m_strStateText[0], I18N::Game::YouMayNowEnter);

                    m_iStateTextNum = 1;
                }
            }
        }

        if (btDetailState == KANTURU_MAYA_DIRECTION_NOTIFY || btDetailState == KANTURU_MAYA_DIRECTION_MONSTER1 || btDetailState == KANTURU_MAYA_DIRECTION_MAYA1
            || btDetailState == KANTURU_MAYA_DIRECTION_END_MAYA1 || btDetailState == KANTURU_MAYA_DIRECTION_ENDCYCLE_MAYA1)
        {
            mu_swprintf(m_strStateText[1], I18N::Game::CurrentlyDPlayersAreInBattleWithMayaSLefeHand, btUserCount);
            m_iStateTextNum = 2;
        }
        else if (btDetailState == KANTURU_MAYA_DIRECTION_MONSTER2 || btDetailState == KANTURU_MAYA_DIRECTION_MAYA2
            || btDetailState == KANTURU_MAYA_DIRECTION_END_MAYA2 || btDetailState == KANTURU_MAYA_DIRECTION_ENDCYCLE_MAYA2)
        {
            mu_swprintf(m_strStateText[1], I18N::Game::CurrentlyDPlayersAreInBattleWithMayaSRightHand, btUserCount);
            m_iStateTextNum = 2;
        }
        else if (btDetailState == KANTURU_MAYA_DIRECTION_MONSTER3 || btDetailState == KANTURU_MAYA_DIRECTION_MAYA3
            || btDetailState == KANTURU_MAYA_DIRECTION_END_MAYA3 || btDetailState == KANTURU_MAYA_DIRECTION_ENDCYCLE_MAYA3)
        {
            mu_swprintf(m_strStateText[1], I18N::Game::CurrentlyDPlayersAreInBattleWithMayaSBothHands, btUserCount);
            m_iStateTextNum = 2;
        }
        else if (btDetailState == KANTURU_MAYA_DIRECTION_NONE || btDetailState == KANTURU_MAYA_DIRECTION_END
            || btDetailState == KANTURU_MAYA_DIRECTION_ENDCYCLE)
        {
            m_iStateTextNum = 1;
        }
    }
    else if (btState == KANTURU_STATE_NIGHTMARE_BATTLE)
    {
        wcscpy(m_strSubject, I18N::Game::BattleWithMayaIsOngoing);
        mu_swprintf(m_strStateText[0], I18N::Game::DPlayersAreTryingToOpen, btUserCount);
        mu_swprintf(m_strStateText[1], I18N::Game::CurrentlyDPlayersAreInBattleWithNightmare, btUserCount);
        m_iStateTextNum = 2;
    }
    else if (btState == KANTURU_STATE_STANDBY)
    {
        wcscpy(m_strSubject, I18N::Game::BossBattleWillStartSoon);
        if (btDetailState == 1)	// STANBY_START
        {
            mu_swprintf(m_strStateText[0], I18N::Game::ForceOfTheNightmareHasInvaded, iRemainTime / 60);
        }
        else // STANBY_NONE || STANBY_NOTIFY || STANBY_END || STANBY_ENDCYCLE
        {
            mu_swprintf(m_strStateText[0], I18N::Game::YouWillBeAbleToApproachMayaShortly);
        }
        mu_swprintf(m_strStateText[1], I18N::Game::DefeatTheNightmareThatControllingThe);
        mu_swprintf(m_strStateText[2], I18N::Game::EntranceIsRestrictedToEnsureThe);
        m_iStateTextNum = 3;
    }
    else
    {
        wcscpy(m_strSubject, I18N::Game::FailedToEnter);
    }

    if (g_pNewUISystem->IsVisible(mu::ui::window::INTERFACE_KANTURU2ND_ENTERNPC) == false)
    {
        g_pNewUISystem->Show(mu::ui::window::INTERFACE_KANTURU2ND_ENTERNPC);
    }
}

void mu::ui::window::CKanturu2ndEnterNpc::ReceiveKanturu3rdEnter(BYTE btResult)
{
    m_bEnterRequest = false;
    CreateMessageBox(btResult);

    // The original dereferenced the gateway NPC unchecked: an entry answer before the client had
    // seen the NPC (the pointer is set when the NPC enters the viewport) crashed it.
    m_bNpcAnimation = false;
    if (m_pNpcObject)
    {
        m_pNpcObject->AnimationFrame = 0;
        SetAction(m_pNpcObject, KANTURU2ND_NPC_ANI_STOP);
    }

    DeleteJoint(BITMAP_JOINT_ENERGY, NULL);

    g_pNewUISystem->Hide(mu::ui::window::INTERFACE_KANTURU2ND_ENTERNPC);
}

void mu::ui::window::CKanturu2ndEnterNpc::SendRequestKanturu3rdInfo()
{
    SocketClient->ToGameServer()->SendKanturuInfoRequest();
    m_dwRefreshTime = timeGetTime();
}

void mu::ui::window::CKanturu2ndEnterNpc::SendRequestKanturu3rdEnter()
{
    SocketClient->ToGameServer()->SendKanturuEnterRequest();
    m_bEnterRequest = true;
}

void mu::ui::window::CKanturu2ndEnterNpc::ProcessRefresh()
{
    SendRequestKanturu3rdInfo();

    m_RefreshLocked = true;
    m_dwRefreshButtonGapTime = timeGetTime();
}

void mu::ui::window::CKanturu2ndEnterNpc::ProcessEnter()
{
    if (m_pNpcObject)
    {
        g_pNewUISystem->Hide(mu::ui::window::INTERFACE_KANTURU2ND_ENTERNPC);

        if (m_byState == KANTURU_STATE_TOWER)
        {
            SetAction(m_pNpcObject, KANTURU2ND_NPC_ANI_ROT);
            m_bNpcAnimation = true;
            return;
        }

        ITEM *pItemHelper, *pItemRingLeft, *pItemRingRight, *pItemWing;
        pItemHelper = &CharacterMachine->Equipment[EQUIPMENT_HELPER];
        pItemRingLeft = &CharacterMachine->Equipment[EQUIPMENT_RING_LEFT];
        pItemRingRight = &CharacterMachine->Equipment[EQUIPMENT_RING_RIGHT];
        pItemWing = &CharacterMachine->Equipment[EQUIPMENT_WING];

        if (pItemHelper->Type == ITEM_HORN_OF_UNIRIA)
        {
            CreateMessageBox(POPUP_UNIRIA);
            return;
        }

        if (g_ChangeRingMgr->CheckChangeRing(pItemRingLeft->Type) ||
            g_ChangeRingMgr->CheckChangeRing(pItemRingRight->Type))
        {
            CreateMessageBox(POPUP_CHANGERING);
            return;
        }

        if (!((pItemWing->Type >= ITEM_WINGS_OF_ELF && pItemWing->Type <= ITEM_WINGS_OF_DARKNESS) ||
              (pItemWing->Type >= ITEM_WING_OF_STORM && pItemWing->Type <= ITEM_WING_OF_DIMENSION) ||
              (ITEM_WING + 130 <= pItemWing->Type && pItemWing->Type <= ITEM_WING + 134) ||
              pItemHelper->Type == ITEM_HORN_OF_DINORANT || pItemHelper->Type == ITEM_DARK_HORSE_ITEM ||
              pItemWing->Type == ITEM_CAPE_OF_LORD || pItemHelper->Type == ITEM_HORN_OF_FENRIR ||
              (pItemWing->Type >= ITEM_CAPE_OF_FIGHTER && pItemWing->Type <= ITEM_CAPE_OF_OVERRULE) ||
              (pItemWing->Type == ITEM_WING + 135)))
        {
            CreateMessageBox(POPUP_NOT_HELPER);
            return;
        }

        if (pItemRingLeft->Type == ITEM_MOONSTONE_PENDANT || pItemRingRight->Type == ITEM_MOONSTONE_PENDANT)
        {
            SetAction(m_pNpcObject, KANTURU2ND_NPC_ANI_ROT);
            m_bNpcAnimation = true;
        }
        else
        {
            CreateMessageBox(POPUP_NOT_MUNSTONE);
            return;
        }
    }
}

void mu::ui::window::CKanturu2ndEnterNpc::BuildRmlUi()
{
    if (m_pRmlDoc || !RmlUiRuntime::Instance().IsCreated())
        return;

    const bool modelCreated = m_RmlBinder.Create(
        RmlUiRuntime::Instance().GetContext(), "kanturu_enter",
        [this](Rml::DataModelConstructor& c, KanturuEnterRmlModel& model)
        {
            c.Bind("root_x", &model.rootX);
            c.Bind("root_y", &model.rootY);
            c.Bind("root_scale", &model.rootScale);
            c.Bind("text_px", &model.textPx);
            auto line = c.RegisterStruct<KanturuEnterLineEntry>();
            line.RegisterMember("text", &KanturuEnterLineEntry::text);
            line.RegisterMember("top", &KanturuEnterLineEntry::top);
            line.RegisterMember("text_px", &KanturuEnterLineEntry::textPx);
            line.RegisterMember("bold", &KanturuEnterLineEntry::bold);
            line.RegisterMember("tone", &KanturuEnterLineEntry::tone);
            c.RegisterArray<std::vector<KanturuEnterLineEntry>>();
            c.Bind("lines", &model.lines);
            c.Bind("refresh_text", &model.refreshText);
            c.Bind("enter_text", &model.enterText);
            c.Bind("close_text", &model.closeText);
            c.Bind("refresh_locked", &model.refreshLocked);
            c.Bind("enter_locked", &model.enterLocked);
            c.Bind("label_top", &model.labelTop);
            c.Bind("label_line_px", &model.labelLinePx);
            c.BindEventCallback("kanturu_refresh", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
                                { m_PendingRefresh = true; });
            c.BindEventCallback("kanturu_enter", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
                                { m_PendingEnter = true; });
            c.BindEventCallback("kanturu_close", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
                                { m_PendingClose = true; });
        });

    if (!modelCreated)
        return;

    m_pRmlDoc = UI::RmlBridge::LoadThemedDocument(RmlUiRuntime::Instance().GetContext(),
                                                  "Data/Interface/RmlUi/kanturu_enter.rml");
}

void mu::ui::window::CKanturu2ndEnterNpc::ReloadRmlTheme()
{
    if (!m_pRmlDoc)
        return;
    Rml::Context* context = RmlUiRuntime::Instance().GetContext();
    m_RmlBinder.Destroy(context);
    context->UnloadDocument(m_pRmlDoc);
    m_pRmlDoc = nullptr;

    BuildRmlUi();
}

void mu::ui::window::CKanturu2ndEnterNpc::SyncRmlModel()
{
    BuildRmlUi();
    if (!m_pRmlDoc)
        return;

    // Layer depth 10.1: over the HUD and the panels.
    UI::RmlBridge::SyncDocumentVisibilityInFront(m_pRmlDoc, IsVisible());
    if (!IsVisible())
        return;

    UI::RmlBridge::SyncRootTransform(m_RmlBinder, m_Pos);
    UI::RmlBridge::SyncNativeTextSize(m_RmlBinder);
    SyncContent();
}

void mu::ui::window::CKanturu2ndEnterNpc::SyncContent()
{
    const UI::Scaling::Transform transform = UI::Scaling::GetActiveTransform();
    KanturuEnterRmlModel updated = m_RmlBinder.GetModel();
    const int lineHeight = CUIRenderTextSDLTtf::LineHeight(UI::Scaling::FontRole::Normal);
    updated.labelTop = static_cast<float>(23 / 2 - lineHeight / 2);
    updated.labelLinePx = static_cast<float>(lineHeight) * transform.scaleY;
    updated.refreshText = StringUtils::WideToNarrow(I18N::Game::Refresh);
    updated.enterText = StringUtils::WideToNarrow(I18N::Game::Enter);
    updated.closeText = StringUtils::WideToNarrow(I18N::Game::Close388);
    updated.refreshLocked = m_RefreshLocked;
    updated.enterLocked = m_EnterLocked;

    // The original's RenderTexts(): the subject bold, split into lines of 52 characters, 12 units
    // apart from y 30; 20 units below it the state texts, the first in green, the others bright
    // yellow, 15 units between two texts.
    updated.lines.clear();
    auto addLine = [&](const wchar_t* text, float top, bool bold, int tone)
    {
        const auto role = bold ? UI::Scaling::FontRole::Bold : UI::Scaling::FontRole::Normal;
        updated.lines.push_back({StringUtils::WideToNarrow(text), top,
                                 KanturuTextPxInBox(role, transform, text, KANTURU2ND_ENTER_WINDOW_WIDTH), bold, tone});
    };
    float textY = 30.f;
    wchar_t separated[3][52] = {};
    int lineCount = SeparateTextIntoLines(m_strSubject, separated[0], 3, 52);
    for (int i = 0; i < lineCount; i++)
    {
        addLine(separated[i], textY, true, 0);
        textY += 12.f;
    }
    textY += 20.f;
    for (int i = 0; i < m_iStateTextNum; i++)
    {
        ZeroMemory(separated, sizeof(separated));
        lineCount = SeparateTextIntoLines(m_strStateText[i], separated[0], 3, 52);
        for (int j = 0; j < lineCount; j++)
        {
            addLine(separated[j], textY, false, i == 0 ? 1 : 2);
            textY += 12.f;
        }
        textY += 15.f;
    }

    KanturuEnterRmlModel& model = m_RmlBinder.GetModel();
    auto sync = [&](auto field, const char* name)
    {
        if (!(model.*field == updated.*field))
        {
            model.*field = updated.*field;
            m_RmlBinder.MarkDirty(name);
        }
    };
    sync(&KanturuEnterRmlModel::labelTop, "label_top");
    sync(&KanturuEnterRmlModel::labelLinePx, "label_line_px");
    sync(&KanturuEnterRmlModel::refreshText, "refresh_text");
    sync(&KanturuEnterRmlModel::enterText, "enter_text");
    sync(&KanturuEnterRmlModel::closeText, "close_text");
    sync(&KanturuEnterRmlModel::refreshLocked, "refresh_locked");
    sync(&KanturuEnterRmlModel::enterLocked, "enter_locked");
    const bool sameLines = model.lines.size() == updated.lines.size() &&
                           std::equal(model.lines.begin(), model.lines.end(), updated.lines.begin(),
                                      [](const KanturuEnterLineEntry& a, const KanturuEnterLineEntry& b)
                                      {
                                          return a.text == b.text && a.top == b.top && a.textPx == b.textPx &&
                                                 a.bold == b.bold && a.tone == b.tone;
                                      });
    if (!sameLines)
    {
        model.lines = std::move(updated.lines);
        m_RmlBinder.MarkDirty("lines");
    }
}

mu::ui::window::CKanturuInfoWindow::CKanturuInfoWindow()
{
    m_pNewUIMng = NULL;
    m_Pos.x = m_Pos.y = 0;

    m_iMinute = 0;
    m_iSecond = 0;
    m_dwSyncTime = 0;
}

mu::ui::window::CKanturuInfoWindow::~CKanturuInfoWindow()
{
    Release();
}

bool mu::ui::window::CKanturuInfoWindow::Create(CManager* pNewUIMng, int x, int y)
{
    if (NULL == pNewUIMng)
        return false;

    m_pNewUIMng = pNewUIMng;
    m_pNewUIMng->AddUIObj(mu::ui::window::INTERFACE_KANTURU_INFO, this);

    SetPos(x, y);

    LoadImages();

    Show(false);

    return true;
}

void mu::ui::window::CKanturuInfoWindow::Release()
{
    UnloadImages();

    if (m_pNewUIMng)
    {
        m_pNewUIMng->RemoveUIObj(this);
        m_pNewUIMng = NULL;
    }
}

void mu::ui::window::CKanturuInfoWindow::SetPos(int x, int y)
{
    m_Pos.x = x;
    m_Pos.y = y;
}

bool mu::ui::window::CKanturuInfoWindow::UpdateMouseEvent()
{
    return true;
}

bool mu::ui::window::CKanturuInfoWindow::UpdateKeyEvent()
{
    return true;
}

bool mu::ui::window::CKanturuInfoWindow::Update()
{
    if (g_pNewUISystem->IsVisible(mu::ui::window::INTERFACE_KANTURU_INFO))
    {
        if (M39Kanturu3rd::IsInKanturu3rd() == false)
        {
            g_pNewUISystem->Hide(mu::ui::window::INTERFACE_KANTURU_INFO);
        }
    }

    return true;
}

bool mu::ui::window::CKanturuInfoWindow::Render()
{
    EnableAlphaTest();

    RenderFrame();

    RenderInfo();

    return true;
}

void mu::ui::window::CKanturuInfoWindow::RenderFrame()
{
    RenderImage(IMAGE_KANTURUINFO_WINDOW, m_Pos.x, m_Pos.y, 99.f, 78.f);
}

void mu::ui::window::CKanturuInfoWindow::RenderInfo()
{
    g_pRenderText->SetFont(g_hFontBold);

    wchar_t strText[256];
    mu_swprintf(strText, I18N::Game::CharacterD, UserCount);
    g_pRenderText->SetBgColor(0);
    g_pRenderText->SetTextColor(134, 134, 199, 255);
    g_pRenderText->RenderText(m_Pos.x + 10, m_Pos.y + 15, strText);

    if (g_Direction.m_CKanturu.m_iMayaState == KANTURU_MAYA_DIRECTION_MAYA1
        || g_Direction.m_CKanturu.m_iMayaState == KANTURU_MAYA_DIRECTION_MAYA2
        || g_Direction.m_CKanturu.m_iMayaState == KANTURU_MAYA_DIRECTION_MAYA3)
    {
        g_pRenderText->RenderText(m_Pos.x + 10, m_Pos.y + 35, I18N::Game::MonsterBoss2182);
    }
    else
    {
        mu_swprintf(strText, I18N::Game::MonsterD, MonsterCount);
        g_pRenderText->RenderText(m_Pos.x + 10, m_Pos.y + 35, strText);
    }

    int iCurrentTime = (GetTickCount() - m_dwSyncTime) / 1000;
    int iPastSecond = m_iSecond - iCurrentTime;

    m_iMinute = iPastSecond / 60;
    int iSecond;

    if (m_iMinute <= 0)
    {
        iSecond = 0;
    }
    else
    {
        iSecond = iPastSecond % (60 * m_iMinute);
    }

    static DWORD dwTime = timeGetTime();
    static bool bRender = true;
    if (timeGetTime() - dwTime > 500)
    {
        dwTime = timeGetTime();
        bRender = !bRender;
    }

    if (bRender)
    {
        g_pRenderText->RenderText(m_Pos.x + 48, m_Pos.y + 57, L":");
    }

    mu::ui::window::RenderNumber(m_Pos.x + 35, m_Pos.y + 55, m_iMinute, 1.f);
    mu::ui::window::RenderNumber(m_Pos.x + 65, m_Pos.y + 55, iSecond, 1.f);
}

float mu::ui::window::CKanturuInfoWindow::GetLayerDepth()
{
    return 1.92f;
}

float mu::ui::window::CKanturuInfoWindow::GetKeyEventOrder()
{
    return 9.1f;
}

void mu::ui::window::CKanturuInfoWindow::LoadImages()
{
    LoadBitmap(L"Interface\\newui_Figure_kantru.tga", IMAGE_KANTURUINFO_WINDOW, GL_LINEAR);
}

void mu::ui::window::CKanturuInfoWindow::UnloadImages()
{
    DeleteBitmap(IMAGE_KANTURUINFO_WINDOW);
}

void mu::ui::window::CKanturuInfoWindow::SetTime(int iTimeLimit)
{
    m_iMinute = 0;
    m_iSecond = iTimeLimit / 1000;
    m_dwSyncTime = GetTickCount();
}
