#include "stdafx.h"
#include "App/Control/ControlCommands.h"

#include "App/Control/ControlEvents.h"
#include "App/Platform/Windows/Winmain.h"
#include "Core/Text/Utf8.h"
#include "Network/Server/ServerListManager.h"
#include "Network/Server/WSclient.h"
#include "Scenes/CharacterScene.h"
#include "Scenes/SceneCommon.h"
#include "Scenes/SceneCore.h"
#include "UI/Legacy/UIMng.h"
#include "Core/Utilities/Log/ErrorReport.h"
#include "Core/Utilities/Log/muConsoleDebug.h"

#include "json.hpp"

#include <chrono>
#include <memory>
#include <string>
#include <utility>

extern int LoadingWorld;
extern bool LogOut;

namespace
{
using App::Control::Act;
using App::Control::ErrorCode;
using App::Control::Request;
using nlohmann::json;

// The server list, the game-server handover and the account check are three
// round trips; a minute covers a slow stack without hanging a scenario.
constexpr std::chrono::milliseconds LoginDeadline{60000};
// Entering the world loads a map from disk.
constexpr std::chrono::milliseconds SelectCharacterDeadline{60000};
constexpr std::chrono::milliseconds LogoutDeadline{30000};
// Back to the server list: a logout and the connect server's list again.
constexpr std::chrono::milliseconds ServerListDeadline{30000};

// Seeded test accounts use the account name as the password; the spec makes
// that the default so `login test1` is enough.
std::string PasswordOr(const Request& request, const std::string& account)
{
    std::string password;
    if (request.GetString("password", password) && !password.empty())
    {
        return password;
    }
    return account;
}

// The character list as `login` and `select-char` report it.
json CharacterList()
{
    json characters = json::array();
    for (int slot = 0; slot < MAX_CHARACTERS_PER_ACCOUNT; ++slot)
    {
        const wchar_t* name = Scenes::CharacterNameInSlot(slot);
        if (name[0] == L'\0')
        {
            continue;
        }

        json character;
        // Slots are reported as `select-char --slot` counts them, from 1.
        character["slot"] = slot + 1;
        character["name"] = Core::Text::ToUtf8(name);
        character["level"] = Scenes::CharacterLevelInSlot(slot);
        characters.push_back(std::move(character));
    }
    return characters;
}

// Login failures reach the client only as a message box (design.md, D5):
// read its code, dismiss it so the login screen is usable again, and turn
// it into an error response.
bool TakeLoginFailure(std::string& reason)
{
    CMsgWin& messageWindow = CUIMng::Instance().m_MsgWin;
    const int code = messageWindow.PendingMessageCode();
    if (!App::Control::IsLoginFailureCode(code))
    {
        return false;
    }

    reason = App::Control::LoginFailureReason(code);
    messageWindow.DismissMessage();
    return true;
}

// login: server list -> server -> credentials -> character list.
// select-server runs the same act up to the login form and stops there: no
// credentials, and it never leaves a session.
class LoginAct : public Act
{
public:
    LoginAct(std::string account, std::string password, std::wstring serverGroup)
        : m_account(std::move(account)), m_password(std::move(password)), m_serverGroup(std::move(serverGroup))
    {
    }

    static std::unique_ptr<LoginAct> UpToLoginForm(std::wstring serverGroup)
    {
        auto act = std::make_unique<LoginAct>(std::string{}, std::string{}, std::move(serverGroup));
        act->m_stopAtLoginForm = true;
        return act;
    }

    [[nodiscard]] std::string_view Name() const override
    {
        return m_stopAtLoginForm ? "select-server" : "login";
    }
    [[nodiscard]] std::chrono::milliseconds Deadline() const override
    {
        return LoginDeadline;
    }

    [[nodiscard]] std::string ProgressObject() const override
    {
        json progress;
        progress["stage"] = StageName();
        progress["scene"] = App::Control::Commands::CurrentSceneName();
        return progress.dump();
    }

    [[nodiscard]] Status Tick(std::string& response) override
    {
        switch (m_stage)
        {
        case Stage::LeavingSession:
            return TickLeavingSession(response);
        case Stage::SelectingServer:
            return TickSelectingServer(response);
        case Stage::JoiningServer:
            return TickJoiningServer(response);
        case Stage::SubmittingCredentials:
            return TickSubmittingCredentials(response);
        }
        return Status::Running;
    }

private:
    enum class Stage : std::uint8_t
    {
        LeavingSession,
        SelectingServer,
        JoiningServer,
        SubmittingCredentials,
    };

    [[nodiscard]] std::string_view StageName() const
    {
        switch (m_stage)
        {
        case Stage::LeavingSession:
            return "leaving_session";
        case Stage::SelectingServer:
            return "selecting_server";
        case Stage::JoiningServer:
            return "joining_server";
        case Stage::SubmittingCredentials:
            return "submitting_credentials";
        }
        return "unknown";
    }

    Status Fail(std::string& response, ErrorCode code, const std::string& message)
    {
        App::Control::Events::RecordError(Name(), App::Control::ErrorCodeName(code), message);
        response = App::Control::EncodeError(EncodedId(), code, message, ProgressObject());
        return Status::Finished;
    }

    // Another account is already logged in on this client: leave that
    // session the way the in-game menu does before logging the new one in.
    Status TickLeavingSession(std::string& response)
    {
        if (!m_leaving)
        {
            if (Core::Text::ToUtf8(LogInID) == m_account)
            {
                // The account asked for is the one already logged in.
                return Answer(response);
            }

            LogOut = true;
            SocketClient->ToGameServer()->SendLogOut(LogOutType::BackToServerSelection);
            m_leaving = true;
            return Status::Running;
        }

        if (CurrentProtocolState >= RECEIVE_CHARACTERS_LIST)
        {
            return Status::Running;
        }

        m_stage = Stage::SelectingServer;
        return Status::Running;
    }

    Status TickSelectingServer(std::string& response)
    {
        // A session of another account is still open on this client.
        if (CurrentProtocolState >= RECEIVE_CHARACTERS_LIST)
        {
            if (m_stopAtLoginForm)
            {
                return Fail(response, ErrorCode::WrongScene, "a session is open: select-server never leaves one");
            }
            m_stage = Stage::LeavingSession;
            return Status::Running;
        }

        // Already past server selection (a previous login on this process).
        if (CurrentProtocolState >= RECEIVE_JOIN_SERVER_SUCCESS)
        {
            m_stage = Stage::JoiningServer;
            return Status::Running;
        }

        // A client that has just started is still asking the connect
        // server for its list; wait for it rather than refusing.
        if (g_ServerListManager->GetServerGroupSize() < 1)
        {
            return Status::Running;
        }

        CUIMng& uiManager = CUIMng::Instance();
        if (!uiManager.m_ServerSelWin.SelectServer(m_serverGroup.c_str(), m_serverIndex))
        {
            return Fail(response, ErrorCode::NotConnected,
                        m_serverGroup.empty() ? "the server list holds no server this client may join"
                                              : "no server group named as asked for");
        }

        m_stage = Stage::JoiningServer;
        return Status::Running;
    }

    Status TickJoiningServer(std::string& response)
    {
        std::string reason;
        if (TakeLoginFailure(reason))
        {
            return Fail(response, ErrorCode::LoginFailed, reason);
        }

        // ReceiveJoinServer sets this once the game server accepted the
        // connection and the login window is up.
        if (CurrentProtocolState != RECEIVE_JOIN_SERVER_SUCCESS)
        {
            return Status::Running;
        }

        if (m_stopAtLoginForm)
        {
            return AnswerLoginForm(response);
        }

        CUIMng::Instance().m_LoginWin.SubmitCredentials(Core::Text::FromUtf8(m_account).c_str(),
                                                        Core::Text::FromUtf8(m_password).c_str());
        m_stage = Stage::SubmittingCredentials;
        return Status::Running;
    }

    Status TickSubmittingCredentials(std::string& response)
    {
        std::string reason;
        if (TakeLoginFailure(reason))
        {
            return Fail(response, ErrorCode::LoginFailed, reason);
        }

        if (CurrentProtocolState != RECEIVE_CHARACTERS_LIST)
        {
            return Status::Running;
        }

        return Answer(response);
    }

    Status AnswerLoginForm(std::string& response)
    {
        json result;
        result["scene"] = App::Control::Commands::CurrentSceneName();
        result["login_form"] = CUIMng::Instance().m_LoginWin.IsShow();
        response = App::Control::EncodeResult(EncodedId(), result.dump());
        return Status::Finished;
    }

    Status Answer(std::string& response)
    {
        json result;
        result["account"] = Core::Text::ToUtf8(LogInID);
        result["scene"] = App::Control::Commands::CurrentSceneName();
        result["characters"] = CharacterList();
        response = App::Control::EncodeResult(EncodedId(), result.dump());
        return Status::Finished;
    }

    std::string m_account;
    std::string m_password;
    std::wstring m_serverGroup;
    // The first server of the group; MU's own list is one server per group
    // in this deployment.
    int m_serverIndex = 0;
    Stage m_stage = Stage::SelectingServer;
    bool m_leaving = false;
    bool m_stopAtLoginForm = false;
};

// select-char: enter the world with one character and wait for its map.
class SelectCharacterAct : public Act
{
public:
    explicit SelectCharacterAct(int slot) : m_slot(slot) {}

    [[nodiscard]] std::string_view Name() const override
    {
        return "select-char";
    }
    [[nodiscard]] std::chrono::milliseconds Deadline() const override
    {
        return SelectCharacterDeadline;
    }

    [[nodiscard]] std::string ProgressObject() const override
    {
        json progress;
        progress["slot"] = m_slot + 1;
        progress["scene"] = App::Control::Commands::CurrentSceneName();
        return progress.dump();
    }

    [[nodiscard]] Status Tick(std::string& response) override
    {
        if (!m_started)
        {
            if (!Scenes::StartGameWithSlot(m_slot))
            {
                response = App::Control::EncodeError(EncodedId(), ErrorCode::NoSuchCharacter,
                                                     "slot " + std::to_string(m_slot + 1) + " holds no character",
                                                     ProgressObject());
                return Status::Finished;
            }
            m_started = true;
            return Status::Running;
        }

        // The world is entered once the main scene is up and the loading
        // handshake has finished.
        if (SceneFlag != MAIN_SCENE || LoadingWorld != 0 || Hero == nullptr)
        {
            return Status::Running;
        }

        json result;
        result["scene"] = App::Control::Commands::CurrentSceneName();
        result["slot"] = m_slot + 1;
        result["character"] = Core::Text::ToUtf8(Hero->ID);
        result["map"] = gMapManager.WorldActive;
        result["position"] = json::array({Hero->PositionX, Hero->PositionY});
        response = App::Control::EncodeResult(EncodedId(), result.dump());
        return Status::Finished;
    }

private:
    int m_slot;
    bool m_started = false;
};

// logout: the game's own "back to character selection".
class LogoutAct : public Act
{
public:
    [[nodiscard]] std::string_view Name() const override
    {
        return "logout";
    }
    [[nodiscard]] std::chrono::milliseconds Deadline() const override
    {
        return LogoutDeadline;
    }

    [[nodiscard]] std::string ProgressObject() const override
    {
        json progress;
        progress["scene"] = App::Control::Commands::CurrentSceneName();
        return progress.dump();
    }

    [[nodiscard]] Status Tick(std::string& response) override
    {
        if (!m_sent)
        {
            LogOut = true;
            SocketClient->ToGameServer()->SendLogOut(LogOutType::BackToCharacterSelection);
            m_sent = true;
            return Status::Running;
        }

        // The character list is only trustworthy once the server has sent
        // it; before that the client's object table still holds the world.
        if (SceneFlag != CHARACTER_SCENE || CurrentProtocolState != RECEIVE_CHARACTERS_LIST)
        {
            return Status::Running;
        }

        json result;
        result["scene"] = App::Control::Commands::CurrentSceneName();
        result["characters"] = CharacterList();
        response = App::Control::EncodeResult(EncodedId(), result.dump());
        return Status::Finished;
    }

private:
    bool m_sent = false;
};

// server-list: the character list's system menu "Server Select", with the
// statements of that button's click branch (SysMenuWin.cpp), then wait
// until the connect server's list is shown on the login scene.
class ServerListAct : public Act
{
public:
    [[nodiscard]] std::string_view Name() const override
    {
        return "server-list";
    }
    [[nodiscard]] std::chrono::milliseconds Deadline() const override
    {
        return ServerListDeadline;
    }

    [[nodiscard]] std::string ProgressObject() const override
    {
        json progress;
        progress["scene"] = App::Control::Commands::CurrentSceneName();
        return progress.dump();
    }

    [[nodiscard]] Status Tick(std::string& response) override
    {
        CUIMng& rUIMng = CUIMng::Instance();
        if (!m_sent)
        {
            g_ErrorReport.Write(L"> Menu - Join another server.");
            g_ErrorReport.WriteCurrentTime();
            LogOut = true;
            SocketClient->ToGameServer()->SendLogOut(LogOutType::BackToServerSelection);
            g_ConsoleDebug->Write(MCD_SEND, L"0xF1 [SendRequestLogOut] 2");

            rUIMng.HideWin(&rUIMng.m_SysMenuWin);
            rUIMng.HideWin(&rUIMng.m_CharSelMainWin);
            m_sent = true;
            return Status::Running;
        }

        if (SceneFlag != LOG_IN_SCENE || CurrentProtocolState >= RECEIVE_JOIN_SERVER_SUCCESS ||
            SocketClient == nullptr || g_ServerListManager->GetServerGroupSize() < 1 || !rUIMng.m_ServerSelWin.IsShow())
        {
            return Status::Running;
        }

        json result;
        result["scene"] = App::Control::Commands::CurrentSceneName();
        result["server_groups"] = g_ServerListManager->GetServerGroupSize();
        response = App::Control::EncodeResult(EncodedId(), result.dump());
        return Status::Finished;
    }

private:
    bool m_sent = false;
};

// Whether a scene window would keep the character list's own click from
// reaching the scene (the click path returns early over any of them).
bool CharacterSceneCovered()
{
    CUIMng& manager = CUIMng::Instance();
    return manager.m_MsgWin.IsShow() || manager.m_CharMakeWin.IsShow() || manager.m_SysMenuWin.IsShow() ||
           manager.m_OptionWin.IsShow();
}
} // namespace

namespace App::Control::Commands
{
std::string Login(const Request& request, std::unique_ptr<Act>& act)
{
    std::string account;
    if (!request.GetString("account", account) || account.empty())
    {
        return EncodeError(request.EncodedId(), ErrorCode::BadRequest, "`login` needs an account");
    }

    std::string serverGroup;
    (void)request.GetString("server", serverGroup);

    act = std::make_unique<LoginAct>(account, PasswordOr(request, account), Core::Text::FromUtf8(serverGroup));
    return {};
}

std::string SelectCharacter(const Request& request, std::unique_ptr<Act>& act)
{
    if (CurrentProtocolState < RECEIVE_CHARACTERS_LIST)
    {
        return EncodeError(request.EncodedId(), ErrorCode::WrongScene, "no character list yet: log in first");
    }

    int slot = -1;
    std::string name;
    if (request.GetInt("slot", slot))
    {
        // The spec counts slots as the list shows them, from 1.
        slot -= 1;
    }
    else if (request.GetString("name", name) && !name.empty())
    {
        slot = Scenes::FindCharacterSlot(Core::Text::FromUtf8(name).c_str());
    }
    else
    {
        return EncodeError(request.EncodedId(), ErrorCode::BadRequest,
                           "`select-char` needs a character name or a slot");
    }

    if (slot < 0 || Scenes::CharacterNameInSlot(slot)[0] == L'\0')
    {
        json details;
        details["characters"] = CharacterList();
        return EncodeError(request.EncodedId(), ErrorCode::NoSuchCharacter,
                           name.empty() ? "no character in that slot" : "no character named `" + name + "`",
                           details.dump());
    }

    act = std::make_unique<SelectCharacterAct>(slot);
    return {};
}

std::string Logout(const Request& request, std::unique_ptr<Act>& act)
{
    (void)request;
    act = std::make_unique<LogoutAct>();
    return {};
}

std::string SelectSlot(const Request& request, std::unique_ptr<Act>&)
{
    if (SceneFlag != CHARACTER_SCENE || CurrentProtocolState != RECEIVE_CHARACTERS_LIST)
    {
        return EncodeError(request.EncodedId(), ErrorCode::WrongScene, "`select-slot` works on the character list");
    }

    int slot = 0;
    if (!request.GetInt("slot", slot) || slot < 1 || slot > MAX_CHARACTERS_PER_ACCOUNT ||
        Scenes::CharacterNameInSlot(slot - 1)[0] == L'\0')
    {
        json details;
        details["characters"] = CharacterList();
        return EncodeError(request.EncodedId(), ErrorCode::BadRequest, "`slot` is a slot holding a character, from 1",
                           details.dump());
    }

    if (CharacterSceneCovered())
    {
        return EncodeError(request.EncodedId(), ErrorCode::NotAllowed, "a dialog or menu covers the character list");
    }

    // Exactly what a single click on a character does (CharacterScene.cpp):
    // it becomes the selected hero and the main window enables Connect and
    // Delete. Nothing is sent and the world is not entered.
    SelectedHero = slot - 1;
    CUIMng::Instance().m_CharSelMainWin.UpdateDisplay();

    json result;
    result["slot"] = slot;
    result["name"] = Core::Text::ToUtf8(Scenes::CharacterNameInSlot(slot - 1));
    return EncodeResult(request.EncodedId(), result.dump());
}

std::string ServerList(const Request& request, std::unique_ptr<Act>& act)
{
    if (SceneFlag != CHARACTER_SCENE || CurrentProtocolState != RECEIVE_CHARACTERS_LIST)
    {
        return EncodeError(request.EncodedId(), ErrorCode::WrongScene, "`server-list` works on the character list");
    }
    if (CharacterSceneCovered())
    {
        return EncodeError(request.EncodedId(), ErrorCode::NotAllowed, "a dialog or menu covers the character list");
    }
    act = std::make_unique<ServerListAct>();
    return {};
}

std::string SelectServer(const Request& request, std::unique_ptr<Act>& act)
{
    if (SceneFlag != LOG_IN_SCENE || CurrentProtocolState >= RECEIVE_JOIN_SERVER_SUCCESS)
    {
        return EncodeError(request.EncodedId(), ErrorCode::WrongScene, "`select-server` works on the server list");
    }
    std::string serverGroup;
    if (request.Has("server") && !request.GetString("server", serverGroup))
    {
        return EncodeError(request.EncodedId(), ErrorCode::BadRequest,
                           "`server` is the name of a server group; omit it for the first one");
    }
    act = LoginAct::UpToLoginForm(Core::Text::FromUtf8(serverGroup));
    return {};
}

std::string Quit(const Request& request, std::unique_ptr<Act>&)
{
    // The main loop leaves on the next frame and ShutdownRuntime unlinks
    // the socket, so the caller gets its answer before the client goes.
    Destroy = true;

    json result;
    result["quitting"] = true;
    return EncodeResult(request.EncodedId(), result.dump());
}
} // namespace App::Control::Commands
