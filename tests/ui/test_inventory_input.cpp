#include "stdafx.h"
#include <doctest.h>
#include <RmlUi/Core.h>
#include "UI/Core/WindowSystem.h"
#include "UI/Core/WindowCommon.h"
#include "UI/Inventory/InventoryActionController.h"
#include "UI/Inventory/InventoryCtrl.h"
#include "UI/Inventory/ItemMng.h"
#include "UI/Inventory/MixInventory.h"
#include "UI/Inventory/LuckyItemWnd.h"
#include "UI/Inventory/MyInventory.h"
#include "UI/Inventory/MyShopInventory.h"
#include "UI/Inventory/InventoryExtension.h"
#include "UI/RmlBridge/RmlTheme.h"
#include <array>
#include <filesystem>

extern bool g_bWndActive;

namespace
{
constexpr int TestItemType = 0;
constexpr int ExtensionSlot = 76;
constexpr int DestinationSlot = 12;
struct Request
{
    int count = 0;
    int from = -1, to = -1;
    STORAGE_TYPE source{}, destination{};
} request;

struct NullRenderer : Rml::RenderInterface
{
    Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex>, Rml::Span<const int>) override
    {
        return 0;
    }
    void RenderGeometry(Rml::CompiledGeometryHandle, Rml::Vector2f, Rml::TextureHandle) override {}
    void ReleaseGeometry(Rml::CompiledGeometryHandle) override {}
    Rml::TextureHandle LoadTexture(Rml::Vector2i&, const Rml::String&) override
    {
        return 0;
    }
    Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte>, Rml::Vector2i) override
    {
        return 0;
    }
    void ReleaseTexture(Rml::TextureHandle) override {}
    void EnableScissorRegion(bool) override {}
    void SetScissorRegion(Rml::Rectanglei) override {}
};
} // namespace

// Stub only asset loading and the outbound request boundary. Native control state, real owners,
// item allocation/removal, key readers and the action controller remain production code.
extern "C" bool __wrap__Z10LoadBitmapPKwjjjbb(const wchar_t*, GLuint, GLuint, GLuint, bool, bool)
{
    return true;
}
extern "C" bool __wrap__Z24SendRequestEquipmentItem12STORAGE_TYPEiP7tagITEMS_i(STORAGE_TYPE source, int from,
                                                                               ITEM* item, STORAGE_TYPE destination,
                                                                               int to)
{
    CHECK(item != nullptr);
    ++request.count;
    request.source = source;
    request.destination = destination;
    request.from = from;
    request.to = to;
    EquipmentItem = true;
    return true;
}

namespace mu::ui::window
{
class InventoryInputFixture
{
public:
    InventoryInputFixture()
        : oldRegistry(system->m_pNewUIMng), oldItems(system->m_pNewItemMng), oldShop(system->m_pNewMyShopInventory),
          oldInventory(system->m_pNewMyInventory), oldExtensions(system->m_pNewMyInventoryExt),
          oldDirectory(std::filesystem::current_path()), oldAttributes(ItemAttribute),
          oldTheme(UI::RmlBridge::GetActiveThemeName()), oldActive(g_bWndActive), oldEquipment(EquipmentItem),
          oldX(g_fWindowMouseX), oldY(g_fWindowMouseY)
    {
        for (size_t i = 0; i < keys.size(); ++i)
            states[i] = KeyState(keys[i]);
        g_bWndActive = true;
        EquipmentItem = false;
        g_fWindowMouseX = g_fWindowMouseY = 10.f;
        Frame(CNewKeyInput::KEY_NONE, CNewKeyInput::KEY_NONE);
        g_pNewKeyInput->SetKeyState(VK_RBUTTON, CNewKeyInput::KEY_NONE);
        system->m_pNewUIMng = &registry;
        system->m_pNewItemMng = &items;
        system->m_pNewMyShopInventory = &shop;
        system->m_pNewMyInventory = &inventory;
        system->m_pNewMyInventoryExt = &extensions;
        inventory.CObject::Show(true);
        ItemAttribute = &attribute;
        attribute.Width = attribute.Height = 1;
        Rml::SetRenderInterface(&renderer);
        REQUIRE(Rml::Initialise());
        std::filesystem::current_path(std::filesystem::path(MU_RMLUI_DIR).parent_path().parent_path().parent_path());
        UI::RmlBridge::SetActiveThemeName("munolith");
        REQUIRE(UI::RmlBridge::ThemeUsesEmbeddedInventory());
        REQUIRE(extensions.Create(&registry));
        request = {};
    }
    ~InventoryInputFixture()
    {
        CInventoryCtrl::DeletePickedItem();
        UI::RmlBridge::SetActiveThemeName(oldTheme);
        Rml::Shutdown();
        Rml::SetRenderInterface(nullptr);
        std::filesystem::current_path(oldDirectory);
        ItemAttribute = oldAttributes;
        system->m_pNewUIMng = oldRegistry;
        system->m_pNewItemMng = oldItems;
        system->m_pNewMyShopInventory = oldShop;
        system->m_pNewMyInventory = oldInventory;
        system->m_pNewMyInventoryExt = oldExtensions;
        for (size_t i = 0; i < keys.size(); ++i)
            g_pNewKeyInput->SetKeyState(keys[i], states[i]);
        g_bWndActive = oldActive;
        EquipmentItem = oldEquipment;
        g_fWindowMouseX = oldX;
        g_fWindowMouseY = oldY;
    }
    void AddItem(CInventoryCtrl& control)
    {
        ITEM prototype{};
        prototype.Type = TestItemType;
        prototype.Durability = 7;
        ITEM* item = items.DuplicateItem(&prototype);
        REQUIRE(item != nullptr);
        REQUIRE(control.AddItem(0, 0, item));
        items.DeleteItem(item); // Control owns the remaining reference.
        control.SetPresentation(nullptr, true);
        REQUIRE(control.Update());
        CHECK(control.GetPointedSquareIndex() == control.GetIndex(0, 0));
    }
    void Frame(CNewKeyInput::KEY_STATE left, CNewKeyInput::KEY_STATE shift)
    {
        g_pNewKeyInput->SetKeyState(VK_LBUTTON, left);
        g_pNewKeyInput->SetKeyState(VK_SHIFT, shift);
    }
    CManager registry;
    CItemMng items;
    CMyShopInventory shop;
    CMyInventory inventory;
    CInventoryExtension extensions;

private:
    static CNewKeyInput::KEY_STATE KeyState(int key)
    {
        if (IsPress(key))
            return CNewKeyInput::KEY_PRESS;
        if (IsRepeat(key))
            return CNewKeyInput::KEY_REPEAT;
        if (IsRelease(key))
            return CNewKeyInput::KEY_RELEASE;
        return CNewKeyInput::KEY_NONE;
    }
    CSystem* system = CSystem::GetInstance();
    CManager* oldRegistry;
    CItemMng* oldItems;
    CMyShopInventory* oldShop;
    CMyInventory* oldInventory;
    CInventoryExtension* oldExtensions;
    std::filesystem::path oldDirectory;
    ITEM_ATTRIBUTE* oldAttributes;
    ITEM_ATTRIBUTE attribute{};
    std::string oldTheme;
    bool oldActive, oldEquipment;
    float oldX, oldY;
    const std::array<int, 3> keys{VK_LBUTTON, VK_RBUTTON, VK_SHIFT};
    std::array<CNewKeyInput::KEY_STATE, 3> states{};
    NullRenderer renderer;
};
} // namespace mu::ui::window

using namespace mu::ui::window;

TEST_CASE_TEMPLATE("Themed material owners retain modified native pickup and locked guards [ui][inventory][input]",
                   Owner, CMixInventory, CLuckyItemWnd)
{
    InventoryInputFixture f;
    Owner owner;
    REQUIRE(owner.Create(&f.registry));
    owner.CObject::Show(true); // No opening request, renderer startup or scene mutation.
    auto* control = owner.GetInventoryCtrl();
    REQUIRE(control != nullptr);
    f.AddItem(*control);
    bool shift = true;
    bool locked = false;
    bool repair = false;
    SUBCASE("Shift held through release picks up once") {}
    SUBCASE("unmodified pickup remains available")
    {
        shift = false;
    }
    SUBCASE("locked slot remains untouched")
    {
        locked = true;
        control->LockInventory();
    }
    SUBCASE("repair mode does not pick up")
    {
        repair = true;
        control->SetRepairMode(true);
    }
    f.Frame(CNewKeyInput::KEY_NONE, shift ? CNewKeyInput::KEY_PRESS : CNewKeyInput::KEY_NONE);
    owner.UpdateMouseEvent();
    CHECK(control->GetEventState() == CInventoryCtrl::EVENT_HOVER);
    for (auto left : {CNewKeyInput::KEY_PRESS, CNewKeyInput::KEY_REPEAT})
    {
        f.Frame(left, shift ? CNewKeyInput::KEY_REPEAT : CNewKeyInput::KEY_NONE);
        owner.UpdateMouseEvent();
        CHECK(CInventoryCtrl::GetPickedItem() == nullptr);
    }
    CHECK(control->IsLocked() == locked);
    CHECK(CInventoryCtrl::GetPickedItem() == nullptr);
    f.Frame(CNewKeyInput::KEY_RELEASE, shift ? CNewKeyInput::KEY_REPEAT : CNewKeyInput::KEY_NONE);
    REQUIRE(IsRelease(VK_LBUTTON));
    if (shift)
        REQUIRE(IsRepeat(VK_SHIFT));
    CHECK(owner.UpdateMouseEvent() == (locked || repair));
    if (locked || repair)
    {
        CHECK(CInventoryCtrl::GetPickedItem() == nullptr);
        CHECK(control->GetNumberOfItems() == 1);
        CHECK(control->IsLocked() == locked);
        CHECK(control->IsRepairMode() == repair);
    }
    else
    {
        auto* held = CInventoryCtrl::GetPickedItem();
        REQUIRE(held != nullptr);
        CHECK(held->GetOwnerInventory() == control);
        CHECK(held->GetItem()->Type == TestItemType);
        CHECK(held->GetItem()->Durability == 7);
        CHECK(held->IsVisible());
        CHECK(control->GetNumberOfItems() == 0);
    }
    CHECK(request.count == 0);
    // Before the real owner dies; the held item's source pointer is intentionally native.
    CInventoryCtrl::DeletePickedItem();
}

namespace
{
struct TransferContext : SEASON3B::IInventoryActionContext
{
    explicit TransferContext(CMyInventory& owner) : owner(owner) {}
    SEASON3B::REPAIR_MODE GetRepairMode() const override
    {
        return owner.GetRepairMode();
    }
    bool IsEquipable(int, ITEM*) const override
    {
        return false;
    }
    void ResetMouseRButton() override
    {
        owner.ResetMouseRButton();
    }
    void ResetMouseLButton() override
    {
        ++presses;
        owner.ResetMouseLButton();
    }
    int FindEmptySlot(ITEM*) const override
    {
        ++destinations;
        return DestinationSlot;
    }
    bool IsRepairEnableLevel() const override
    {
        return false;
    }
    CMyInventory& owner;
    int presses = 0;
    mutable int destinations = 0;
};
} // namespace

TEST_CASE("Owner-handled modified transfer sends once without a release pickup [ui][inventory][input]")
{
    InventoryInputFixture f;
    CInventoryCtrl control;
    REQUIRE(control.Create(STORAGE_TYPE::INVENTORY, &f.items, &f.inventory, 8, 4, ExtensionSlot));
    f.AddItem(control);
    TransferContext context(f.inventory);
    CInventoryActionController actions;
    actions.SetContext(&context);
    f.Frame(CNewKeyInput::KEY_NONE, CNewKeyInput::KEY_PRESS);
    REQUIRE(control.UpdateMouseEvent(true));
    f.Frame(CNewKeyInput::KEY_PRESS, CNewKeyInput::KEY_REPEAT);
    REQUIRE(control.UpdateMouseEvent(true));
    REQUIRE(actions.HandleInventoryActions(&control));
    REQUIRE(CInventoryCtrl::GetPickedItem() != nullptr);
    CHECK_FALSE(CInventoryCtrl::GetPickedItem()->IsVisible());
    CHECK(control.GetNumberOfItems() == 0);
    CHECK(request.count == 1);
    CHECK(request.source == STORAGE_TYPE::INVENTORY);
    CHECK(request.destination == STORAGE_TYPE::INVENTORY);
    CHECK(request.from == ExtensionSlot);
    CHECK(request.to == DestinationSlot);
    for (auto left : {CNewKeyInput::KEY_REPEAT, CNewKeyInput::KEY_RELEASE, CNewKeyInput::KEY_NONE})
    {
        f.Frame(left, CNewKeyInput::KEY_REPEAT);
        REQUIRE(control.Update());
        CHECK(control.UpdateMouseEvent(true));
        actions.HandleInventoryActions(&control);
        CHECK(request.count == 1);
        CHECK(control.GetNumberOfItems() == 0);
        REQUIRE(CInventoryCtrl::GetPickedItem() != nullptr);
        CHECK_FALSE(CInventoryCtrl::GetPickedItem()->IsVisible());
    }
    CHECK(context.presses == 1);
    CHECK(context.destinations == 1);
    CInventoryCtrl::DeletePickedItem();
}
