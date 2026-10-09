#include "stdafx.h"
#include <doctest.h>
#include "AuctionFixture.h"
#include "Engine/Object/ZzzInfomation.h"
#include "GameLogic/AuctionHouse/AuctionHouse.h"
#include "GameLogic/AuctionHouse/AuctionHouseTransport.h"
#include "UI/AuctionHouse/Receipts.h"
#include "UI/AuctionHouse/Window.h"
#include "UI/Core/WindowCommon.h"
#include "UI/RmlBridge/RmlNativeText.h"
#include <RmlUi/Core.h>

#include <cwchar>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// The production auction window (UI::AuctionHouse::Window) in a real RmlUi context: its document,
// bindings, request pump and market controller, with the transport captured and a test clock.

namespace Market = GameLogic::AuctionHouse;
using AuctionFixture::Load;

namespace
{
std::vector<Market::PacketBytes> sent;
int closedDialogs = 0;

constexpr std::uint8_t CodeGoodsList = 0x52, CodeQuote = 0x53, CodeBuyGoods = 0x55, CodeCollection = 0x59,
                       CodeClaim = 0x5A;
constexpr std::uint16_t Bless = 7181, Soul = 7182;
constexpr std::uint8_t ResultSuccess = 0, ResultBusy = 3;

class Renderer : public Rml::RenderInterface
{
public:
    Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex>, Rml::Span<const int>) override
    {
        return 1;
    }
    void RenderGeometry(Rml::CompiledGeometryHandle, Rml::Vector2f, Rml::TextureHandle) override {}
    void ReleaseGeometry(Rml::CompiledGeometryHandle) override {}
    Rml::TextureHandle LoadTexture(Rml::Vector2i& size, const Rml::String&) override
    {
        size = {256, 256};
        return 1;
    }
    Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte>, Rml::Vector2i) override
    {
        return 1;
    }
    void ReleaseTexture(Rml::TextureHandle) override {}
    void EnableScissorRegion(bool) override {}
    void SetScissorRegion(Rml::Rectanglei) override {}
};

void Put16(Market::PacketBytes& packet, std::uint16_t value)
{
    packet.push_back(static_cast<std::uint8_t>(value));
    packet.push_back(static_cast<std::uint8_t>(value >> 8));
}

void Put32(Market::PacketBytes& packet, std::uint32_t value)
{
    Put16(packet, static_cast<std::uint16_t>(value));
    Put16(packet, static_cast<std::uint16_t>(value >> 16));
}

Market::PacketBytes C1(std::uint8_t code, const Market::PacketBytes& body)
{
    Market::PacketBytes packet{0xC1, 0, 0x7B, code};
    packet.insert(packet.end(), body.begin(), body.end());
    packet[1] = static_cast<std::uint8_t>(packet.size());
    return packet;
}

std::uint16_t RequestId(const Market::PacketBytes& request)
{
    return static_cast<std::uint16_t>(request[4] | (std::uint16_t{request[5]} << 8));
}

std::uint16_t Word(const Market::PacketBytes& request, std::size_t offset)
{
    return static_cast<std::uint16_t>(request[offset] | (std::uint16_t{request[offset + 1]} << 8));
}

// Each kind at its own price, so its row can be told apart (the test has no item models, so every
// goods row is named "Unknown item").
std::uint32_t UnitPrice(std::uint16_t type)
{
    return type == Soul ? 1200000 : 590000;
}
constexpr std::string_view BlessRow = "590k Zen each", SoulRow = "1.2kk Zen each";

struct GoodsListRow
{
    std::uint16_t type;
    std::uint32_t available;
};

// 52: one directory page of level-0 goods.
Market::PacketBytes GoodsListAnswer(std::uint16_t id, std::uint16_t page, std::uint32_t total,
                                    const std::vector<GoodsListRow>& rows)
{
    Market::PacketBytes packet{0xC2, 0, 0, 0x7B, CodeGoodsList};
    Put16(packet, id);
    packet.push_back(ResultSuccess);
    Put16(packet, page);
    Put32(packet, total);
    packet.push_back(static_cast<std::uint8_t>(rows.size()));
    for (const auto& row : rows)
    {
        Put16(packet, row.type);
        packet.push_back(0);
        Put32(packet, UnitPrice(row.type));
        Put32(packet, row.available);
        Put16(packet, 1);
    }
    packet[1] = static_cast<std::uint8_t>(packet.size() >> 8);
    packet[2] = static_cast<std::uint8_t>(packet.size());
    return packet;
}

// 53: a quote of `quantity` level-0 units at one price tier, or a refusal.
Market::PacketBytes QuoteAnswer(std::uint16_t id, std::uint8_t result, std::uint16_t type, std::uint32_t quantity)
{
    const bool success = result == ResultSuccess;
    Market::PacketBytes body;
    Put16(body, id);
    body.push_back(result);
    Put16(body, type);
    body.push_back(0);
    Put32(body, quantity);
    Put32(body, success ? quantity : 0);
    Put32(body, success ? quantity * UnitPrice(type) : 0);
    body.push_back(success ? 1 : 0);
    if (success)
    {
        Put32(body, UnitPrice(type));
        Put32(body, 6);
    }
    return C1(CodeQuote, body);
}

// 55: a purchase delivered in full to the bag.
Market::PacketBytes BuyGoodsAnswer(std::uint16_t id, std::uint16_t type, std::uint32_t quantity)
{
    Market::PacketBytes body;
    Put16(body, id);
    body.push_back(ResultSuccess);
    Put16(body, type);
    body.push_back(0);
    Put32(body, quantity);
    Put32(body, quantity * UnitPrice(type));
    Put32(body, quantity);
    Put32(body, 0);
    return C1(CodeBuyGoods, body);
}

class ItemCatalogue
{
public:
    ItemCatalogue() : saved(ItemAttribute), attributes(std::make_unique<ITEM_ATTRIBUTE[]>(MAX_ITEM))
    {
        ItemAttribute = attributes.get();
        std::wcscpy(attributes[Bless].Name, L"Jewel of Bless");
        std::wcscpy(attributes[Soul].Name, L"Jewel of Soul");
    }
    ~ItemCatalogue()
    {
        ItemAttribute = saved;
    }
    ItemCatalogue(const ItemCatalogue&) = delete;
    ItemCatalogue& operator=(const ItemCatalogue&) = delete;

private:
    ITEM_ATTRIBUTE* saved;
    std::unique_ptr<ITEM_ATTRIBUTE[]> attributes;
};

class WindowFixture
{
    using RequestClockTime = UI::AuctionHouse::RequestClock::Time;

public:
    WindowFixture()
        : cwd(std::filesystem::current_path()), savedMachine(CharacterMachine),
          machine(std::make_unique<CHARACTER_MACHINE>())
    {
        std::filesystem::current_path(MU_AUCTION_BIN);
        Rml::SetRenderInterface(&renderer);
        REQUIRE(Rml::Initialise());
        REQUIRE(Rml::LoadFontFace("fonts/DejaVuSans.ttf"));
        REQUIRE(Rml::LoadFontFace("fonts/LiberationSans-Regular.ttf"));
        context = Rml::CreateContext("auction-window", {1280, 960});
        REQUIRE(context);
        context->SetDensityIndependentPixelRatio(1.f);
        machine->Gold = 100000000;
        CharacterMachine = machine.get();
        sent.clear();
        closedDialogs = 0;
        Market::SetListener(nullptr);
        Market::Reset();
        window.emplace(UI::AuctionHouse::Window::Environment{[this] { return context; }, [this] { return now; }});
        Market::SetListener(&*window);
        Open();
    }
    ~WindowFixture()
    {
        Market::SetListener(nullptr);
        Market::Reset();
        window.reset();
        CharacterMachine = savedMachine;
        mu::ui::window::CNewKeyInput::GetInstance()->SetKeyState(VK_ESCAPE, mu::ui::window::CNewKeyInput::KEY_NONE);
        Rml::Shutdown();
        Rml::SetRenderInterface(nullptr);
        std::filesystem::current_path(cwd);
    }
    WindowFixture(const WindowFixture&) = delete;
    WindowFixture& operator=(const WindowFixture&) = delete;

    void Open()
    {
        Market::ReceivePacket(Load("auction-house-state-open"));
        REQUIRE(window->IsVisible());
        UI::RmlBridge::ApplyNativeTextSize(context);
        Step(0);
    }
    // One frame `milliseconds` after the last.
    void Step(int milliseconds)
    {
        now += std::chrono::milliseconds(milliseconds);
        window->Update();
        context->Update();
        context->Update();
    }
    Rml::ElementDocument* Document()
    {
        for (int i = 0; i < context->GetNumDocuments(); ++i)
            if (auto* document = context->GetDocument(i); document && document->IsVisible())
                return document;
        return nullptr;
    }
    // The first visible button whose text starts with `text`.
    Rml::Element* Button(std::string_view text)
    {
        auto* document = Document();
        REQUIRE(document);
        Rml::ElementList buttons;
        document->GetElementsByTagName(buttons, "button");
        for (auto* button : buttons)
            if (button->IsVisible(true) && std::string_view(button->GetInnerRML()).starts_with(text))
                return button;
        return nullptr;
    }
    // A press on the element, then the frame that handles it.
    void Click(Rml::Element* element)
    {
        REQUIRE(element);
        const auto position = element->GetAbsoluteOffset(Rml::BoxArea::Border);
        const auto size = element->GetBox().GetSize(Rml::BoxArea::Border);
        context->ProcessMouseMove(static_cast<int>(position.x + size.x / 2), static_cast<int>(position.y + size.y / 2),
                                  0);
        context->ProcessMouseButtonDown(0, 0);
        context->ProcessMouseButtonUp(0, 0);
        Step(300);
    }
    void Escape()
    {
        auto* keys = mu::ui::window::CNewKeyInput::GetInstance();
        keys->SetKeyState(VK_ESCAPE, mu::ui::window::CNewKeyInput::KEY_PRESS);
        window->UpdateKeyEvent();
        keys->SetKeyState(VK_ESCAPE, mu::ui::window::CNewKeyInput::KEY_NONE);
        Step(0);
    }
    bool Shows(std::string_view text)
    {
        auto* document = Document();
        return document && document->GetInnerRML().find(text) != Rml::String::npos;
    }
    // The last request with this code, if it was sent after `from` requests.
    const Market::PacketBytes* Request(std::uint8_t code, std::size_t from = 0)
    {
        for (auto i = sent.size(); i > from; --i)
            if (sent[i - 1].size() > 5 && sent[i - 1][3] == code)
                return &sent[i - 1];
        return nullptr;
    }
    void Answer(Market::PacketBytes packet)
    {
        Market::ReceivePacket(packet);
        Step(300);
    }
    // Jewels, after the search debounce. Rebuilding the type choices re-runs the search once, so
    // only the last request is answered.
    void SelectJewels()
    {
        Click(Button("Jewels"));
        for (int frame = 0; frame < 4; ++frame)
            Step(400);
    }
    // Answers every goods-directory request from `directory` (the rows of each page) until the window
    // asks no more: a first answer can rebuild the type choices and search once more, and a list
    // shorter than the panel loads its next page by itself.
    void AnswerGoodsDirectory(std::size_t from, const std::vector<std::vector<GoodsListRow>>& directory)
    {
        std::uint32_t total = 0;
        for (const auto& page : directory)
            total += static_cast<std::uint32_t>(page.size());
        for (int round = 0; round < 8; ++round)
        {
            const auto* request = Request(CodeGoodsList, from);
            if (!request)
                return;
            from = sent.size();
            const auto page = Word(*request, 6);
            Answer(GoodsListAnswer(RequestId(*request), page, total,
                                   page < directory.size() ? directory[page] : std::vector<GoodsListRow>{}));
            for (int frame = 0; frame < 3; ++frame)
                Step(400);
        }
    }
    Rml::Element* Row(std::string_view name)
    {
        Rml::ElementList rows;
        Document()->GetElementsByClassName(rows, "listing");
        for (auto* row : rows)
            if (row->IsVisible(true) && row->GetInnerRML().find(name) != Rml::String::npos)
                return row;
        return nullptr;
    }

    Rml::Context* context = nullptr;
    RequestClockTime now = RequestClockTime{} + std::chrono::hours(1);
    std::optional<UI::AuctionHouse::Window> window;

private:
    Renderer renderer;
    std::filesystem::path cwd;
    ItemCatalogue catalogue;
    CHARACTER_MACHINE* savedMachine;
    std::unique_ptr<CHARACTER_MACHINE> machine;
};
} // namespace

namespace GameLogic::AuctionHouse::Transport
{
bool Send(std::span<const std::uint8_t> packet)
{
    sent.emplace_back(packet.begin(), packet.end());
    return true;
}

void CloseDialog()
{
    ++closedDialogs;
}
} // namespace GameLogic::AuctionHouse::Transport

TEST_CASE("Auction window stays open while a sent action awaits its answer, then closes [auction][window]")
{
    // D27, UX3-01: Collect all is sent; Esc and × keep the window, the request and its receipt.
    WindowFixture fixture;
    fixture.Click(fixture.Button("Collection"));
    const auto* page = fixture.Request(CodeCollection);
    REQUIRE(page);
    auto collection = Load("auction-collection");
    collection[5] = (*page)[4];
    collection[6] = (*page)[5];
    fixture.Answer(collection);
    REQUIRE(fixture.Button("Collect all"));

    auto collectAll = [&fixture]
    {
        const auto before = sent.size();
        fixture.Click(fixture.Button("Collect all"));
        fixture.Step(300);
        const auto* claim = fixture.Request(CodeClaim, before);
        REQUIRE(claim);
        REQUIRE(Market::GetState().IsPending(Market::Operation::Claim));
        return RequestId(*claim);
    };
    const auto claim = collectAll();

    fixture.Escape();
    CHECK(fixture.window->IsVisible());
    CHECK(Market::GetState().IsOpen());
    CHECK(Market::GetState().IsPending(Market::Operation::Claim));
    CHECK(fixture.Shows(UI::AuctionHouse::StillAnsweringText));
    // Not an error: the result line keeps its neutral tone.
    auto* status = fixture.Document()->QuerySelector("div.status");
    REQUIRE(status);
    CHECK_FALSE(status->IsClassSet("danger"));
    CHECK(closedDialogs == 0);

    auto* close = fixture.Document()->QuerySelector("button.close");
    fixture.Click(close);
    CHECK(fixture.window->IsVisible());
    CHECK(Market::GetState().IsPending(Market::Operation::Claim));
    CHECK(closedDialogs == 0);

    // The answer arrives: its receipt shows, and closing works again.
    auto result = Load("auction-claim-result");
    result[4] = static_cast<std::uint8_t>(claim);
    result[5] = static_cast<std::uint8_t>(claim >> 8);
    fixture.Answer(result);
    CHECK_FALSE(Market::GetState().IsPending(Market::Operation::Claim));
    CHECK(fixture.Shows("Collected"));
    fixture.Escape();
    CHECK_FALSE(fixture.window->IsVisible());
    CHECK(closedDialogs == 1);

    // Unanswered past the timeout: the recovery text, and × closes.
    fixture.Open();
    fixture.Click(fixture.Button("Collection"));
    page = fixture.Request(CodeCollection);
    REQUIRE(page);
    collection[5] = (*page)[4];
    collection[6] = (*page)[5];
    fixture.Answer(collection);
    collectAll();
    fixture.Escape();
    CHECK(fixture.window->IsVisible());
    fixture.Step(16000);
    CHECK(fixture.Shows("Outcome unknown"));
    CHECK(fixture.Document()->QuerySelector("div.status")->IsClassSet("danger"));
    fixture.Click(fixture.Document()->QuerySelector("button.close"));
    CHECK_FALSE(fixture.window->IsVisible());
    CHECK(closedDialogs == 2);
}

TEST_CASE("Auction window recovers from a failed first goods quote without reselecting [auction][window]")
{
    // FR-01: the first quote of the selected jewel answers Busy, or never; the window asks again.
    for (const bool timeout : {false, true})
    {
        INFO(std::string(timeout ? "timeout" : "busy"));
        WindowFixture fixture;
        fixture.SelectJewels();
        fixture.AnswerGoodsDirectory(0, {{{Bless, 18}}});
        REQUIRE(fixture.Row(BlessRow));
        fixture.Click(fixture.Row(BlessRow));
        const auto* first = fixture.Request(CodeQuote);
        REQUIRE(first);
        CHECK(Word(*first, 6) == Bless);
        // Before any answer: "No prices yet" and Refresh.
        CHECK(fixture.Shows(UI::AuctionHouse::NoQuoteYetText));
        CHECK(fixture.Button("Refresh"));

        std::size_t before = sent.size();
        if (timeout)
            fixture.Step(16000);
        else
        {
            fixture.Answer(QuoteAnswer(RequestId(*first), ResultBusy, Bless, 1));
            CHECK(fixture.Shows(UI::AuctionHouse::NoQuoteYetText));
            CHECK(fixture.Button("Refresh"));
            // Review purchase on the failed quote asks again instead of repeating the refusal.
            fixture.Click(fixture.Button("Review purchase"));
            // Within the one-second Busy pause, well before the next five-second poll.
            for (int frame = 0; frame < 4 && !fixture.Request(CodeQuote, before); ++frame)
                fixture.Step(300);
            const auto* retry = fixture.Request(CodeQuote, before);
            REQUIRE(retry);
            fixture.Answer(QuoteAnswer(RequestId(*retry), ResultBusy, Bless, 1));
            before = sent.size();
        }
        // The next poll, at most five seconds later, without touching the row.
        for (int frame = 0; frame < 20 && !fixture.Request(CodeQuote, before); ++frame)
            fixture.Step(300);
        const auto* again = fixture.Request(CodeQuote, before);
        REQUIRE(again);
        CHECK(Word(*again, 6) == Bless);
        fixture.Answer(QuoteAnswer(RequestId(*again), ResultSuccess, Bless, 1));
        CHECK(fixture.Shows("Updated just now"));
        CHECK(fixture.Shows("590,000 Zen"));
        CHECK_FALSE(fixture.Shows(UI::AuctionHouse::NoQuoteYetText));
    }
}

TEST_CASE("Auction window keeps the bought goods selected when they are on a later directory page [auction][window]")
{
    // FR-03 with page size 1: Bless on page 0, Soul on page 1; buy one Soul.
    WindowFixture fixture;
    fixture.SelectJewels();
    const std::vector<std::vector<GoodsListRow>> directory{{{Bless, 18}}, {{Soul, 12}}};
    fixture.AnswerGoodsDirectory(0, directory);
    auto soulRow = [&fixture] { return fixture.Row(SoulRow); };
    REQUIRE(fixture.Row(BlessRow));
    REQUIRE(soulRow());
    fixture.Click(soulRow());
    const auto* quote = fixture.Request(CodeQuote);
    REQUIRE(quote);
    REQUIRE(Word(*quote, 6) == Soul);
    fixture.Answer(QuoteAnswer(RequestId(*quote), ResultSuccess, Soul, 1));
    fixture.Click(fixture.Button("Review purchase"));
    fixture.Click(fixture.Document()->GetElementById("confirm-action"));
    fixture.Step(300);
    const auto* buy = fixture.Request(CodeBuyGoods);
    REQUIRE(buy);
    const auto before = sent.size();
    fixture.Answer(BuyGoodsAnswer(RequestId(*buy), Soul, 1));

    // The directory reloads from page 0, which does not hold Soul; the window reads on to page 1.
    fixture.AnswerGoodsDirectory(before, directory);
    std::vector<std::uint16_t> pages;
    for (auto i = before; i < sent.size(); ++i)
        if (sent[i][3] == CodeGoodsList)
            pages.push_back(Word(sent[i], 6));
    REQUIRE(pages.size() >= 2);
    CHECK(pages[0] == 0);
    CHECK(pages[1] == 1);
    REQUIRE(soulRow());
    CHECK(soulRow()->IsClassSet("selected"));
    // The kept selection's quote refreshes.
    const auto* refreshed = fixture.Request(CodeQuote, before);
    REQUIRE(refreshed);
    CHECK(Word(*refreshed, 6) == Soul);
}
