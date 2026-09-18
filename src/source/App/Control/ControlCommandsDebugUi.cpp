// Debug-UI family of the control socket: reproducible UI states without a
// server round trip, for screenshot comparison of UI ports and themes.
//
//   inject   run one decrypted server->client packet through the receive
//            path as if the server had sent it;
//   net      mute/unmute every real incoming packet (the session stays open);
//   ui       list/show/hide/toggle main-scene windows by name, switch the
//            RmlUi theme, change the UI scale;
//   window   resize the game window;
//   hover    move the pointer to a window pixel and leave it there;
//   render   skip the 3D world so only the UI is drawn.
//
// The window registry differs between the trees this file is built in: the
// RmlUi port (UI/Core/WindowSystem.h, namespace mu::ui::window) and the
// original UI (UI/NewUI/NewUISystem.h, namespace SEASON3B). Everything that
// depends on it is kept behind MU_DEBUG_UI_RMLUI so one file serves both.
#include "stdafx.h"
#include "App/Control/ControlCommands.h"

#include "App/Control/ControlTaps.h"
#include "Data/GameConfig/GameConfig.h"
#include "Network/Server/WSclient.h"
#include "Render/Textures/ZzzOpenglUtil.h"
#include "Scenes/MainScene.h"

#if __has_include("UI/Core/WindowSystem.h")
#define MU_DEBUG_UI_RMLUI 1
#include "UI/Core/WindowSystem.h"
#include "UI/RmlBridge/RmlTheme.h"
#include "UI/Core/SceneUICoordinator.h"
#include "UI/Windows/RememberPasswordPrompt.h"
#else
#define MU_DEBUG_UI_RMLUI 0
#include "UI/NewUI/NewUISystem.h"
#endif

#include "json.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Window plumbing in App/Platform/Windows/Winmain.cpp, declared there without
// a header (UI/Options/OptionWindow.cpp declares it the same way). It resizes
// through SDL and runs the client's own resize handling synchronously.
void MuApplyWindowResolution(unsigned int width, unsigned int height, bool windowed);

namespace
{
using App::Control::Act;
using App::Control::ErrorCode;
using App::Control::Request;
using nlohmann::json;

// Incoming packets dropped while muted, by head code, readable from the
// main thread while the network thread counts.
std::atomic<bool> g_muted{false};
std::atomic<std::uint64_t> g_droppedTotal{0};
std::array<std::atomic<std::uint32_t>, 256> g_droppedByCode{};

#if MU_DEBUG_UI_RMLUI
namespace Windows = mu::ui::window;
#else
namespace Windows = SEASON3B;
#endif

// A window the `ui` command can address, named after its INTERFACE_* key
// without the prefix, lowercased.
struct NamedWindow
{
    std::string_view name;
    DWORD key;
};

// clang-format off
#define MU_UI_WINDOW(id) {#id, Windows::INTERFACE_##id},
constexpr NamedWindow NamedWindows[] = {
    MU_UI_WINDOW(FRIEND)
    MU_UI_WINDOW(MOVEMAP)
    MU_UI_WINDOW(PARTY)
    MU_UI_WINDOW(MYQUEST)
    MU_UI_WINDOW(NPCQUEST)
    MU_UI_WINDOW(GUILDINFO)
    MU_UI_WINDOW(TRADE)
    MU_UI_WINDOW(STORAGE)
    MU_UI_WINDOW(STORAGE_EXT)
    MU_UI_WINDOW(MIXINVENTORY)
    MU_UI_WINDOW(COMMAND)
    MU_UI_WINDOW(PET)
    MU_UI_WINDOW(NPCSHOP)
    MU_UI_WINDOW(INVENTORY)
    MU_UI_WINDOW(INVENTORY_EXT)
    MU_UI_WINDOW(MYSHOP_INVENTORY)
    MU_UI_WINDOW(PURCHASESHOP_INVENTORY)
    MU_UI_WINDOW(CHARACTER)
    MU_UI_WINDOW(NPCBREEDER)
    MU_UI_WINDOW(SERVERDIVISION)
    MU_UI_WINDOW(DEVILSQUARE)
    MU_UI_WINDOW(BLOODCASTLE)
    MU_UI_WINDOW(NPCGUILDMASTER)
    MU_UI_WINDOW(GUARDSMAN)
    MU_UI_WINDOW(SENATUS)
    MU_UI_WINDOW(GATEKEEPER)
    MU_UI_WINDOW(GATESWITCH)
    MU_UI_WINDOW(CATAPULT)
    MU_UI_WINDOW(REFINERY)
    MU_UI_WINDOW(REFINERYINFO)
    MU_UI_WINDOW(KANTURU2ND_ENTERNPC)
    MU_UI_WINDOW(CURSEDTEMPLE_NPC)
    MU_UI_WINDOW(CURSEDTEMPLE_GAMESYSTEM)
    MU_UI_WINDOW(CURSEDTEMPLE_RESULT)
    MU_UI_WINDOW(CHATINPUTBOX)
    MU_UI_WINDOW(WINDOW_MENU)
    MU_UI_WINDOW(OPTION)
    MU_UI_WINDOW(HELP)
    MU_UI_WINDOW(ITEM_EXPLANATION)
    MU_UI_WINDOW(SETITEM_EXPLANATION)
    MU_UI_WINDOW(QUICK_COMMAND)
    MU_UI_WINDOW(KANTURU_INFO)
    MU_UI_WINDOW(CHATLOGWINDOW)
    MU_UI_WINDOW(PARTY_INFO_WINDOW)
    MU_UI_WINDOW(BLOODCASTLE_TIME)
    MU_UI_WINDOW(CHAOSCASTLE_TIME)
    MU_UI_WINDOW(BATTLE_SOCCER_SCORE)
    MU_UI_WINDOW(SLIDEWINDOW)
#if MU_DEBUG_UI_RMLUI
    MU_UI_WINDOW(MU_HELPER_BAR)
#else
    MU_UI_WINDOW(HERO_POSITION_INFO)
#endif
    MU_UI_WINDOW(MESSAGEBOX)
    MU_UI_WINDOW(DUEL_WINDOW)
    MU_UI_WINDOW(CRYWOLF)
    MU_UI_WINDOW(NAME_WINDOW)
    MU_UI_WINDOW(SIEGEWARFARE)
    MU_UI_WINDOW(MAINFRAME)
    MU_UI_WINDOW(SKILL_LIST)
    MU_UI_WINDOW(ITEM_ENDURANCE_INFO)
    MU_UI_WINDOW(BUFF_WINDOW)
    MU_UI_WINDOW(MASTER_LEVEL)
    MU_UI_WINDOW(GOLD_BOWMAN)
    MU_UI_WINDOW(GOLD_BOWMAN_LENA)
    MU_UI_WINDOW(LUCKYCOIN_REGISTRATION)
    MU_UI_WINDOW(EXCHANGE_LUCKYCOIN)
    MU_UI_WINDOW(DUELWATCH)
    MU_UI_WINDOW(DUELWATCH_MAINFRAME)
    MU_UI_WINDOW(DUELWATCH_USERLIST)
    MU_UI_WINDOW(INGAMESHOP)
    MU_UI_WINDOW(DOPPELGANGER_NPC)
    MU_UI_WINDOW(DOPPELGANGER_FRAME)
    MU_UI_WINDOW(QUEST_PROGRESS)
    MU_UI_WINDOW(QUEST_PROGRESS_ETC)
    MU_UI_WINDOW(EMPIREGUARDIAN_NPC)
    MU_UI_WINDOW(EMPIREGUARDIAN_TIMER)
    MU_UI_WINDOW(MINI_MAP)
    MU_UI_WINDOW(NPC_DIALOGUE)
    MU_UI_WINDOW(GENSRANKING)
    MU_UI_WINDOW(UNITEDMARKETPLACE_NPC_JULIA)
    MU_UI_WINDOW(LUCKYITEMWND)
    MU_UI_WINDOW(HOTKEY)
    MU_UI_WINDOW(ITEM_TOOLTIP)
    MU_UI_WINDOW(MUHELPER)
    MU_UI_WINDOW(MUHELPER_EXT)
    MU_UI_WINDOW(MUHELPER_SKILL_LIST)
    MU_UI_WINDOW(SYSTEMLOGWINDOW)
    MU_UI_WINDOW(COMMAND_LIST)
#if MU_DEBUG_UI_RMLUI
    // Windows the RmlUi port moved onto the same registry.
    MU_UI_WINDOW(CREDITS)
    MU_UI_WINDOW(SERVER_MESSAGE)
    MU_UI_WINDOW(SERVER_SELECT)
    MU_UI_WINDOW(MSG_WINDOW)
    MU_UI_WINDOW(SYS_MENU)
    MU_UI_WINDOW(CHAR_SEL_MAIN)
    MU_UI_WINDOW(CHAR_MAKE)
    MU_UI_WINDOW(LOGIN_MAIN)
    MU_UI_WINDOW(LOGIN)
    MU_UI_WINDOW(CHAR_INFO_BALLOON)
    MU_UI_WINDOW(GENERIC_CONFIRM_DIALOG)
    MU_UI_WINDOW(GENERIC_MENU_DIALOG)
#endif
};
#undef MU_UI_WINDOW
// clang-format on

// Two rendered frames: one for the event loop to consume a pushed SDL event
// or a resize, one for the UI to react to it.
constexpr int SettleFrames = 2;
constexpr std::chrono::milliseconds SettleDeadline{5000};

// Finishes after a fixed number of frames with a prepared result.
class SettleAct : public Act
{
public:
    SettleAct(std::string_view name, std::string encodedResult, int frames = SettleFrames)
        : m_name(name), m_encodedResult(std::move(encodedResult)), m_frames(frames)
    {
    }

    [[nodiscard]] std::string_view Name() const override
    {
        return m_name;
    }

    [[nodiscard]] std::optional<std::chrono::milliseconds> Deadline() const override
    {
        return SettleDeadline;
    }

    [[nodiscard]] Status Tick(std::string& response) override
    {
        if (--m_frames > 0)
        {
            return Status::Running;
        }
        response = App::Control::EncodeResult(EncodedId(), m_encodedResult);
        return Status::Finished;
    }

private:
    std::string_view m_name;
    std::string m_encodedResult;
    int m_frames;
};

std::string Lowercase(std::string_view text)
{
    std::string lowered(text);
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return lowered;
}

std::string WindowName(const NamedWindow& window)
{
    return Lowercase(window.name);
}

const NamedWindow* FindWindow(std::string_view name)
{
    const std::string wanted = Lowercase(name);
    for (const NamedWindow& window : NamedWindows)
    {
        if (WindowName(window) == wanted)
        {
            return &window;
        }
    }
    return nullptr;
}

std::string KnownWindowNames()
{
    std::string names;
    for (const NamedWindow& window : NamedWindows)
    {
        if (!names.empty())
        {
            names += ", ";
        }
        names += WindowName(window);
    }
    return names;
}

// Whether the main-scene window registry exists (it is created on entering
// the world and torn down on leaving it).
bool WindowSystemReady()
{
    return g_pNewUISystem != nullptr && g_pNewUIMng != nullptr;
}

json WindowObject(const NamedWindow& window)
{
    json object;
    object["window"] = WindowName(window);
    object["key"] = static_cast<unsigned>(window.key);
    object["registered"] = g_pNewUIMng->FindUIObj(window.key) != nullptr;
    object["visible"] = g_pNewUIMng->IsInterfaceVisible(window.key);
    return object;
}

// Hex text -> bytes; whitespace and an optional 0x prefix per byte are
// accepted. Empty on malformed input.
std::vector<BYTE> DecodeHex(std::string_view text)
{
    std::vector<BYTE> bytes;
    int nibble = -1;
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        const char c = text[i];
        if (std::isspace(static_cast<unsigned char>(c)) || c == ',' || c == ':')
        {
            continue;
        }
        if ((c == 'x' || c == 'X') && nibble == 0)
        {
            nibble = -1; // "0x" prefix
            continue;
        }
        int value = 0;
        if (c >= '0' && c <= '9')
        {
            value = c - '0';
        }
        else if (c >= 'a' && c <= 'f')
        {
            value = c - 'a' + 10;
        }
        else if (c >= 'A' && c <= 'F')
        {
            value = c - 'A' + 10;
        }
        else
        {
            return {};
        }
        if (nibble < 0)
        {
            nibble = value;
        }
        else
        {
            bytes.push_back(static_cast<BYTE>(nibble * 16 + value));
            nibble = -1;
        }
    }
    if (nibble >= 0)
    {
        return {};
    }
    return bytes;
}

std::string HexByte(unsigned value)
{
    static constexpr char Digits[] = "0123456789ABCDEF";
    return std::string{Digits[(value >> 4) & 0xF], Digits[value & 0xF]};
}

// Length the packet's own header claims, or 0 when the header is not a
// plain C1/C2 one (encrypted C3/C4 packets are not accepted: the receive
// path expects them decoded already).
std::size_t DeclaredLength(const std::vector<BYTE>& bytes)
{
    if (bytes.size() < 3)
    {
        return 0;
    }
    if (bytes[0] == 0xC1)
    {
        return bytes[1];
    }
    if (bytes[0] == 0xC2)
    {
        return (static_cast<std::size_t>(bytes[1]) << 8) | bytes[2];
    }
    return 0;
}

json NetStatus()
{
    json status;
    status["muted"] = g_muted.load();
    status["dropped"] = g_droppedTotal.load();
    json byCode = json::object();
    for (unsigned code = 0; code < g_droppedByCode.size(); ++code)
    {
        const std::uint32_t count = g_droppedByCode[code].load();
        if (count != 0)
        {
            byCode[HexByte(code)] = count;
        }
    }
    status["dropped_by_code"] = byCode;
    return status;
}

SDL_Window* GameWindow()
{
    int count = 0;
    SDL_Window** windows = SDL_GetWindows(&count);
    SDL_Window* window = count > 0 ? windows[0] : nullptr;
    SDL_free(static_cast<void*>(windows));
    return window;
}

json WindowGeometry()
{
    json geometry;
    geometry["width"] = WindowWidth;
    geometry["height"] = WindowHeight;
#if MU_DEBUG_UI_RMLUI
    geometry["ui_scale_percent"] = GameConfig::GetInstance().GetUIScalePercent();
#else
    geometry["ui_scale_percent"] = nullptr;
#endif
    return geometry;
}

std::string UiList(const Request& request)
{
    json result;
    json windows = json::array();
    for (const NamedWindow& window : NamedWindows)
    {
        windows.push_back(WindowObject(window));
    }
    result["windows"] = std::move(windows);
#if MU_DEBUG_UI_RMLUI
    result["theme"] = UI::RmlBridge::GetActiveThemeName();
#else
    result["theme"] = nullptr;
#endif
    result["ui_scale_percent"] = WindowGeometry()["ui_scale_percent"];
    return App::Control::EncodeResult(request.EncodedId(), result.dump());
}

std::string UiVisibility(const Request& request, std::string_view action, std::unique_ptr<Act>& act)
{
    std::string name;
    if (!request.GetString("window", name) || name.empty())
    {
        return App::Control::EncodeError(request.EncodedId(), ErrorCode::BadRequest,
                                         "`ui " + std::string(action) + "` needs a `window` name");
    }
    const NamedWindow* window = FindWindow(name);
    if (window == nullptr)
    {
        return App::Control::EncodeError(request.EncodedId(), ErrorCode::BadRequest,
                                         "unknown window `" + name + "`; known: " + KnownWindowNames());
    }

    bool raw = false;
    (void)request.GetBool("raw", raw);

    bool show = action == "show";
    if (action == "toggle")
    {
        show = !g_pNewUIMng->IsInterfaceVisible(window->key);
    }
    if (raw)
    {
        // The manager's flag flip only: no dock-neighbour repositioning, no
        // group hiding, no "cannot hide while trading" refusal.
        g_pNewUIMng->ShowInterface(window->key, show);
    }
    else if (show)
    {
        g_pNewUISystem->Show(window->key);
    }
    else
    {
        g_pNewUISystem->Hide(window->key);
    }

    json result = WindowObject(*window);
    result["raw"] = raw;
    act = std::make_unique<SettleAct>("ui", result.dump());
    return {};
}

std::string UiTheme(const Request& request, std::unique_ptr<Act>& act)
{
    std::string name;
    if (!request.GetString("name", name) || name.empty())
    {
        return App::Control::EncodeError(request.EncodedId(), ErrorCode::BadRequest, "`ui theme` needs a theme `name`");
    }
#if MU_DEBUG_UI_RMLUI
    if (!UI::RmlBridge::ThemeExists(name))
    {
        return App::Control::EncodeError(request.EncodedId(), ErrorCode::Failed, "no RmlUi theme named `" + name + "`");
    }
    // The same sweep the `$theme` console command performs
    // (Core/Utilities/Log/muConsoleDebug.cpp): the scene registry, the
    // main-scene registry when it exists, and the one prompt outside both.
    GameConfig::GetInstance().SetRmlTheme(std::wstring(name.begin(), name.end()));
    UI::RmlBridge::SetActiveThemeName(name);
    CSceneUICoordinator::Instance().GetNewStyleMng().ReloadAllRmlThemes();
    if (Windows::CManager* mainSceneRegistry = g_pNewUIMng)
    {
        mainSceneRegistry->ReloadAllRmlThemes();
    }
    UI::Login::ReloadRmlTheme();

    json result;
    result["theme"] = UI::RmlBridge::GetActiveThemeName();
    act = std::make_unique<SettleAct>("ui", result.dump());
    return {};
#else
    (void)act;
    return App::Control::EncodeError(request.EncodedId(), ErrorCode::Failed,
                                     "this build has no RmlUi themes (theme `" + name + "` cannot be applied)");
#endif
}

std::string UiScale(const Request& request, std::unique_ptr<Act>& act)
{
    int percent = 0;
    if (!request.GetInt("percent", percent) || percent < 50 || percent > 400)
    {
        return App::Control::EncodeError(request.EncodedId(), ErrorCode::BadRequest,
                                         "`ui scale` needs a `percent` between 50 and 400");
    }
#if MU_DEBUG_UI_RMLUI
    GameConfig::GetInstance().SetUIScalePercent(percent);
    // Everything that caches the scale recomputes on a resize to the
    // current size.
    MuApplyWindowResolution(WindowWidth, WindowHeight, true);
    act = std::make_unique<SettleAct>("ui", WindowGeometry().dump());
    return {};
#else
    (void)act;
    return App::Control::EncodeError(request.EncodedId(), ErrorCode::Failed,
                                     "this build has no UI scale setting (" + std::to_string(percent) +
                                         "% cannot be applied)");
#endif
}
} // namespace

namespace App::Control::Net
{
bool DropIncoming(const unsigned char* buffer, int size)
{
    if (!g_muted.load(std::memory_order_relaxed))
    {
        return false;
    }
    g_droppedTotal.fetch_add(1, std::memory_order_relaxed);
    if (size >= 4)
    {
        const unsigned code = buffer[0] % 2 == 1 ? buffer[2] : buffer[3];
        g_droppedByCode[code].fetch_add(1, std::memory_order_relaxed);
    }
    return true;
}
} // namespace App::Control::Net

namespace App::Control::Commands
{
std::string Inject(const Request& request, std::unique_ptr<Act>& act)
{
    (void)act;
    std::string hex;
    if (!request.GetString("hex", hex) || hex.empty())
    {
        return EncodeError(request.EncodedId(), ErrorCode::BadRequest, "`inject` needs the packet as `hex`");
    }
    const std::vector<BYTE> bytes = DecodeHex(hex);
    if (bytes.empty())
    {
        return EncodeError(request.EncodedId(), ErrorCode::BadRequest, "`hex` is not a whole number of hex bytes");
    }
    const std::size_t declared = DeclaredLength(bytes);
    if (declared == 0)
    {
        return EncodeError(request.EncodedId(), ErrorCode::BadRequest,
                           "the packet must start with a C1 or C2 header (decrypted, at least 3 bytes)");
    }
    if (declared != bytes.size())
    {
        return EncodeError(request.EncodedId(), ErrorCode::BadRequest,
                           "the header declares " + std::to_string(declared) + " bytes but " +
                               std::to_string(bytes.size()) + " were given");
    }
    if (SocketClient == nullptr)
    {
        return EncodeError(request.EncodedId(), ErrorCode::NotConnected, "not connected to a game server");
    }

    // The same object the network thread queues, processed here and now on
    // the main thread — a command runs after this frame's real packets.
    PacketInfo packet;
    packet.ReceiveBuffer = std::make_unique<BYTE[]>(bytes.size());
    std::memcpy(packet.ReceiveBuffer.get(), bytes.data(), bytes.size());
    packet.ConnectionHandle = SocketClient->GetHandle();
    packet.Size = static_cast<int32_t>(bytes.size());
    packet.EnqueuedAt = std::chrono::steady_clock::now();
    ProcessPacketCallback(&packet);

    json result;
    result["bytes"] = bytes.size();
    result["code"] = HexByte(bytes[0] == 0xC1 ? bytes[2] : bytes[3]);
    return EncodeResult(request.EncodedId(), result.dump());
}

std::string Net(const Request& request, std::unique_ptr<Act>& act)
{
    (void)act;
    std::string action;
    if (!request.GetString("action", action) || action.empty())
    {
        return EncodeError(request.EncodedId(), ErrorCode::BadRequest,
                           "`net` needs an `action`: mute, unmute or status");
    }
    if (action == "mute")
    {
        g_muted.store(true);
    }
    else if (action == "unmute")
    {
        g_muted.store(false);
    }
    else if (action == "reset")
    {
        g_droppedTotal.store(0);
        for (auto& count : g_droppedByCode)
        {
            count.store(0);
        }
    }
    else if (action != "status")
    {
        return EncodeError(request.EncodedId(), ErrorCode::BadRequest,
                           "unknown `net` action `" + action + "`; known: mute, unmute, status, reset");
    }
    return EncodeResult(request.EncodedId(), NetStatus().dump());
}

std::string Ui(const Request& request, std::unique_ptr<Act>& act)
{
    std::string action;
    if (!request.GetString("action", action) || action.empty())
    {
        return EncodeError(request.EncodedId(), ErrorCode::BadRequest,
                           "`ui` needs an `action`: list, show, hide, toggle, theme or scale");
    }
    if (action == "theme")
    {
        return UiTheme(request, act);
    }
    if (action == "scale")
    {
        return UiScale(request, act);
    }
    if (!WindowSystemReady())
    {
        return EncodeError(request.EncodedId(), ErrorCode::WrongScene,
                           "the main-scene windows exist only in the world");
    }
    if (action == "list")
    {
        return UiList(request);
    }
    if (action == "show" || action == "hide" || action == "toggle")
    {
        return UiVisibility(request, action, act);
    }
    return EncodeError(request.EncodedId(), ErrorCode::BadRequest,
                       "unknown `ui` action `" + action + "`; known: list, show, hide, toggle, theme, scale");
}

std::string Window(const Request& request, std::unique_ptr<Act>& act)
{
    std::string action;
    if (!request.GetString("action", action) || action.empty())
    {
        return EncodeError(request.EncodedId(), ErrorCode::BadRequest, "`window` needs an `action`: resize or status");
    }
    if (action == "status")
    {
        return EncodeResult(request.EncodedId(), WindowGeometry().dump());
    }
    if (action != "resize")
    {
        return EncodeError(request.EncodedId(), ErrorCode::BadRequest,
                           "unknown `window` action `" + action + "`; known: resize, status");
    }
    int width = 0;
    int height = 0;
    if (!request.GetInt("width", width) || !request.GetInt("height", height) || width < 640 || height < 480)
    {
        return EncodeError(request.EncodedId(), ErrorCode::BadRequest,
                           "`window resize` needs `width` and `height` of at least 640x480");
    }
    if (GameWindow() == nullptr)
    {
        return EncodeError(request.EncodedId(), ErrorCode::Failed, "there is no game window");
    }
    // Windowed only: a scripted client is never fullscreen. The answer
    // reports the size the window really got, which a tiling compositor may
    // decide differently.
    MuApplyWindowResolution(static_cast<unsigned int>(width), static_cast<unsigned int>(height), true);
    act = std::make_unique<SettleAct>("window", WindowGeometry().dump());
    return {};
}

std::string Render(const Request& request, std::unique_ptr<Act>& act)
{
    std::string world;
    if (!request.GetString("world", world) || (world != "on" && world != "off" && world != "status"))
    {
        return EncodeError(request.EncodedId(), ErrorCode::BadRequest, "`render` needs `world`: on, off or status");
    }
    if (world != "status")
    {
        SetSkipWorldRender(world == "off");
    }
    json result;
    result["world"] = IsWorldRenderSkipped() ? "off" : "on";
    if (world == "status")
    {
        return EncodeResult(request.EncodedId(), result.dump());
    }
    act = std::make_unique<SettleAct>("render", result.dump());
    return {};
}

std::string Hover(const Request& request, std::unique_ptr<Act>& act)
{
    double x = 0.0;
    double y = 0.0;
    if (!request.GetDouble("x", x) || !request.GetDouble("y", y))
    {
        return EncodeError(request.EncodedId(), ErrorCode::BadRequest, "`hover` needs `x` and `y` window pixels");
    }
    SDL_Window* window = GameWindow();
    if (window == nullptr)
    {
        return EncodeError(request.EncodedId(), ErrorCode::Failed, "there is no game window");
    }

    // A motion event through the real queue reaches every consumer the
    // event loop feeds (the legacy pointer globals and, where present, the
    // RmlUi context), and the pointer stays there until the next motion.
    SDL_Event event{};
    event.type = SDL_EVENT_MOUSE_MOTION;
    event.motion.timestamp = SDL_GetTicksNS();
    event.motion.windowID = SDL_GetWindowID(window);
    event.motion.which = 0;
    event.motion.state = 0;
    event.motion.x = static_cast<float>(x);
    event.motion.y = static_cast<float>(y);
    event.motion.xrel = 0.0f;
    event.motion.yrel = 0.0f;
    if (!SDL_PushEvent(&event))
    {
        return EncodeError(request.EncodedId(), ErrorCode::Failed,
                           std::string("SDL refused the event: ") + SDL_GetError());
    }

    json result;
    result["x"] = x;
    result["y"] = y;
    act = std::make_unique<SettleAct>("hover", result.dump());
    return {};
}
} // namespace App::Control::Commands
