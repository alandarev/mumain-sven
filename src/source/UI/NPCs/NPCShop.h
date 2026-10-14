
#if !defined(AFX_NEWUINPCSHOP_H__EEE639A8_C89E_47B3_8DBA_22560F102D98__INCLUDED_)
#define AFX_NEWUINPCSHOP_H__EEE639A8_C89E_47B3_8DBA_22560F102D98__INCLUDED_

#pragma once

#include "UI/Inventory/ItemCameraTarget.h"
#include "UI/Inventory/ItemGridModel.h"
#include "Render/RmlUi/RmlUiRuntime.h"
#include "UI/Core/WindowObject.h"
#include "UI/Inventory/InventoryCtrl.h"
#include "UI/Inventory/MyInventory.h"
#include "UI/RmlBridge/RmlThemedView.h"
#include "UI/NPCs/PurchaseBatch.h"
#include <span>

namespace Rml { class ElementDocument; }

namespace mu::ui::window
{
    class CNPCShop : public CObject
    {
    public:
        enum
        {
            NPCSHOP_POS_X = 260,
            NPCSHOP_POS_Y = 0,
            SHOP_STATE_BUYNSELL = 1,
            SHOP_STATE_REPAIR = 2,
        };

    private:

        CManager* m_pNewUIMng;
        CInventoryCtrl* m_pNewInventoryCtrl;


        DWORD m_dwShopState;
        int m_iTaxRate;
        bool m_bRepairShop;
        bool m_bIsNPCShopOpen;

        DWORD m_dwStandbyItemKey;

        bool m_bSellingItem;
        UI::NPCs::PurchaseBatch m_Purchases;
        DWORD m_QuantityKey = 0;
        int m_OfferUnits = 1;
        std::int64_t m_OfferEstimate = 0;
        int m_BatchTax = 0;
        unsigned long long m_NextPurchaseAt = 0;
        unsigned long long m_FeedbackUntil = 0;
        Rml::String m_Feedback;
        Rml::String m_MerchantName;
        bool m_RepairPending = false;
        unsigned long long m_RepairStartedAt = 0;
        unsigned long long m_RepairGoldBefore = 0;

        // Window frame/title/tax-rate line/repair buttons/repair-money strip are RmlUi; the
        // inventory grid stays native since its icons are live 3D model renders (same reasoning
        // as CMyInventory/CStorageInventoryExt).
        struct NPCShopRmlModel
        {
            float textPx = 0.f; // native text size in physical px (RmlNativeTextSize.h)

            Rml::String title;
            Rml::String taxRateText;
            std::vector<int> rows;
            Rml::String contextName;
            Rml::String contextPrice;
            Rml::String contextHint;
            bool highlightVisible = false;
            int highlightColumn = 0, highlightRow = 0, highlightWidth = 1, highlightHeight = 1;
            bool buyPending = false;
            bool repairActive = false;
            bool quantityOpen = false;
            int quantity = 1;
            Rml::String quantityText = "1";
            bool quantityValid = true;
            Rml::String quantityName;
            Rml::String quantityTotal;

            bool repairVisible = false;
            Rml::String repairTooltip;
            Rml::String repairAllTooltip;

            // Repair-money strip: wallet-style like CMyInventory's gold strip, but showing
            // AllRepairGold (the cost to repair everything) instead of the character's own gold.
            Rml::String repairAllLabel;
            Rml::String repairGoldText;
            Rml::String repairGoldTier; // UI::RmlBridge::GoldTierKey() of the amount
            // The grids as their documents draw them (CInventoryCtrl::Cells()).
            UI::Items::ItemGridCells gridCells;
        };
        void BindRmlModel(Rml::DataModelConstructor& c, NPCShopRmlModel& model);
        UI::RmlBridge::ThemedView<NPCShopRmlModel> m_RmlView{"npc_shop",
            [this](Rml::DataModelConstructor& c, NPCShopRmlModel& model) { BindRmlModel(c, model); },
            {{"Data/Interface/RmlUi/npc_shop.rml"}}};

        // The grids' items, into the document's #item_view.
        void RenderItems();
        UI::Items::ItemCameraTarget m_ItemTarget{[this](const Rml::Vector2f&, const Rml::Vector2f&) { RenderItems(); }};

        void BuildRmlUi();
        void SyncRmlModel();
        void BindCounterModel(Rml::DataModelConstructor& c, NPCShopRmlModel& model);
        void SyncCounterModel();
        void SyncCounterGrid();
        void SyncCounterContext();
        void SyncQuantityModel();
        bool ProcessCounterClick(ITEM* item);
        void OpenQuantity(ITEM* item);
        void ConfirmQuantity();
        void StartPurchase(ITEM* item, int offers);
        void UpdatePurchase();
        void RepairAll();
        void UpdateRepairFeedback();
        void SetFeedback(const Rml::String& text);
        void ResetCounter();
        bool SubmitSale();

    public:
        CNPCShop();
        virtual ~CNPCShop();

        bool Create(CManager* pNewUIMng);
        void Release();


        bool UpdateMouseEvent();
        bool UpdateKeyEvent();
        bool Update();
        bool Render();
        Rml::ElementDocument* GetPlacedDocument() const override { return m_RmlView.Document(); }

        float GetLayerDepth();	//. 2.5f

        void SetTaxRate(int iTaxRate);
        int GetTaxRate();

        bool InsertItem(int iIndex, std::span<const BYTE> pbyItemPacket);

        void OpenningProcess();
        void DeleteAllItems();

        void ClosingProcess();
        void SetRepairShop(bool bRepair);
        void SetMerchant(int characterIndex);
        bool IsRepairShop();
        // The shop's grid, for the control socket.
        CInventoryCtrl* GetInventoryCtrl() const { return m_pNewInventoryCtrl; }
        void ToggleState();
        DWORD GetShopState();

        int GetPointedItemIndex();

        //. Exporting Functions
        void SetStandbyItemKey(DWORD dwItemKey);
        DWORD GetStandbyItemKey() const;
        int GetStandbyItemIndex();
        ITEM* GetStandbyItem();

        void SetSellingItem(bool bFlag);
        bool IsSellingItem();
        bool IsTransactionPending() const;
        void PurchaseFinished(bool success);
        void SaleFinished(bool success);
        void RepairFinished(bool success);
        bool SellPickedItem();

    private:
        void Init();

        bool InventoryProcess();
        bool WindowProcess();
    };
}

#endif // !defined(AFX_NEWUINPCSHOP_H__EEE639A8_C89E_47B3_8DBA_22560F102D98__INCLUDED_)
