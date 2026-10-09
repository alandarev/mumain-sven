#include "GameLogic/AuctionHouse/AuctionHousePriceMemory.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <limits>
#include <sstream>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace GameLogic::AuctionHouse
{
namespace
{
constexpr int FileVersion = 1;
// How long an update waits for another client of the account to finish its own; the game thread waits, so a
// stalled holder (suspended, debugged) costs at most this, and the update then reports that it was not saved.
constexpr auto LockWait = std::chrono::milliseconds(500);
constexpr auto LockRetry = std::chrono::milliseconds(5);
constexpr char HexDigits[] = "0123456789abcdef";
constexpr std::uint64_t FnvOffset = 14695981039346656037ull;
constexpr std::uint64_t FnvPrime = 1099511628211ull;

std::string Hex(std::string_view text)
{
    std::string hex;
    hex.reserve(text.size() * 2);
    for (unsigned char byte : text)
    {
        hex += HexDigits[byte >> 4];
        hex += HexDigits[byte & 15];
    }
    return hex.empty() ? "-" : hex;
}

enum class LineRead : std::uint8_t
{
    Line,
    End,
    TooLong
};

// Like std::getline, but never holds more than MaximumLineLength characters of one line.
LineRead ReadLine(std::istream& in, std::string& line)
{
    line.clear();
    for (auto c = in.get(); c != std::char_traits<char>::eof(); c = in.get())
    {
        if (c == '\n')
            return LineRead::Line;
        if (line.size() == PriceMemory::MaximumLineLength)
            return LineRead::TooLong;
        line += static_cast<char>(c);
    }
    return line.empty() ? LineRead::End : LineRead::Line;
}

// An exclusive lock on `<store>.lock`, held across the read, change and replace of one update. The
// operating system drops it when the process ends, so a crash leaves no stale lock. Taking it never blocks
// longer than LockWait.
class StoreLock
{
public:
    explicit StoreLock(const std::filesystem::path& store)
    {
        auto path = store;
        path += ".lock";
#ifdef _WIN32
        m_Handle = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                               OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (m_Handle == INVALID_HANDLE_VALUE)
            return;
#else
        m_File = open(path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
        if (m_File < 0)
            return;
#endif
        const auto deadline = std::chrono::steady_clock::now() + LockWait;
        while (!(m_Locked = TryLock()) && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(LockRetry);
    }
    ~StoreLock()
    {
#ifdef _WIN32
        if (m_Handle != INVALID_HANDLE_VALUE)
            CloseHandle(m_Handle);
#else
        if (m_File >= 0)
            close(m_File);
#endif
    }
    StoreLock(const StoreLock&) = delete;
    StoreLock& operator=(const StoreLock&) = delete;
    bool Locked() const
    {
        return m_Locked;
    }

private:
    bool TryLock()
    {
#ifdef _WIN32
        OVERLAPPED overlapped{};
        return LockFileEx(m_Handle, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, 1, 0, &overlapped) != 0;
#else
        return flock(m_File, LOCK_EX | LOCK_NB) == 0;
#endif
    }

#ifdef _WIN32
    HANDLE m_Handle = INVALID_HANDLE_VALUE;
#else
    int m_File = -1;
#endif
    bool m_Locked = false;
};

unsigned long ProcessId()
{
#ifdef _WIN32
    return GetCurrentProcessId();
#else
    return static_cast<unsigned long>(getpid());
#endif
}

template <typename T> bool ReadField(std::istream& in, T& field, unsigned maximum)
{
    unsigned value = 0;
    if (!(in >> value) || value > maximum)
        return false;
    field = static_cast<T>(value);
    return true;
}

bool ReadEntry(const std::string& line, PriceMemory::Entry& entry)
{
    std::istringstream in(line);
    auto& id = entry.identity;
    std::uint64_t price = 0;
    if (!(in >> price) || price == 0 || price > std::numeric_limits<Zen>::max())
        return false;
    entry.price = static_cast<Zen>(price);
    constexpr unsigned Byte = 0xFF, Word = 0xFFFF;
    bool fields = ReadField(in, id.goods, 1) && ReadField(in, id.type, Word) && ReadField(in, id.level, Byte) &&
                  ReadField(in, id.excellentFlags, Byte) && ReadField(in, id.luck, 1) && ReadField(in, id.skill, 1) &&
                  ReadField(in, id.optionType, Byte) && ReadField(in, id.optionLevel, Byte) &&
                  ReadField(in, id.ancientDiscriminator, Byte) && ReadField(in, id.ancientBonus, Byte) &&
                  ReadField(in, id.socketCount, Byte);
    for (auto& socket : id.sockets)
        fields = fields && ReadField(in, socket, Byte);
    fields = fields && ReadField(in, id.harmonyOption, Word) && ReadField(in, id.harmonyLevel, Word) &&
             ReadField(in, id.option380, 1);
    std::string extra;
    return fields && !(in >> extra);
}

void WriteEntry(std::ostream& out, const PriceMemory::Entry& entry)
{
    const auto& id = entry.identity;
    auto number = [](auto value) { return static_cast<unsigned>(value); };
    out << entry.price << ' ' << number(id.goods) << ' ' << id.type << ' ' << number(id.level) << ' '
        << number(id.excellentFlags) << ' ' << number(id.luck) << ' ' << number(id.skill) << ' '
        << number(id.optionType) << ' ' << number(id.optionLevel) << ' ' << number(id.ancientDiscriminator) << ' '
        << number(id.ancientBonus) << ' ' << number(id.socketCount);
    for (auto socket : id.sockets)
        out << ' ' << number(socket);
    out << ' ' << id.harmonyOption << ' ' << id.harmonyLevel << ' ' << number(id.option380) << '\n';
}
} // namespace

PricingIdentity GoodsPricingIdentity(std::uint16_t type, std::uint8_t level)
{
    return {.goods = true, .type = type, .level = level};
}

std::optional<Zen> PriceMemory::Find(const PricingIdentity& identity) const
{
    const auto found = std::find_if(m_Entries.begin(), m_Entries.end(),
                                    [&identity](const Entry& entry) { return entry.identity == identity; });
    if (found == m_Entries.end())
        return std::nullopt;
    return found->price;
}

std::optional<Zen> PriceMemory::Use(const PricingIdentity& identity)
{
    const auto found = std::find_if(m_Entries.begin(), m_Entries.end(),
                                    [&identity](const Entry& entry) { return entry.identity == identity; });
    if (found == m_Entries.end())
        return std::nullopt;
    std::rotate(m_Entries.begin(), found, found + 1);
    std::erase(m_Uses, identity);
    m_Uses.push_back(identity);
    return m_Entries.front().price;
}

void PriceMemory::Remember(const PricingIdentity& identity, Zen price)
{
    Forget(identity);
    m_Entries.insert(m_Entries.begin(), {identity, price});
    if (m_Entries.size() > Capacity)
        m_Entries.resize(Capacity);
}

bool PriceMemory::Forget(const PricingIdentity& identity)
{
    return std::erase_if(m_Entries, [&identity](const Entry& entry) { return entry.identity == identity; }) != 0;
}

void PriceMemory::Clear()
{
    m_Entries.clear();
    m_Uses.clear();
}

bool PriceMemory::Load(const std::filesystem::path& path, std::string_view owner)
{
    m_Entries.clear();
    m_Uses.clear();
    std::ifstream file(path);
    if (!file)
        return false;
    std::string line;
    if (ReadLine(file, line) != LineRead::Line || line != std::to_string(FileVersion))
        return false;
    if (ReadLine(file, line) != LineRead::Line || line != Hex(owner))
        return false;
    std::vector<Entry> loaded;
    for (std::size_t records = 0; records < Capacity; ++records)
    {
        const auto read = ReadLine(file, line);
        if (read == LineRead::End)
            break;
        Entry entry;
        if (read == LineRead::TooLong || !ReadEntry(line, entry))
            return false;
        if (std::none_of(loaded.begin(), loaded.end(),
                         [&entry](const Entry& old) { return old.identity == entry.identity; }))
            loaded.push_back(entry);
    }
    m_Entries = std::move(loaded);
    return true;
}

bool PriceMemory::Save(const std::filesystem::path& path, std::string_view owner) const
{
    std::error_code error;
    if (!path.parent_path().empty())
        std::filesystem::create_directories(path.parent_path(), error);
    if (error)
        return false;
    const auto temporary = UniqueTemporaryPath(path);
    std::ofstream file(temporary, std::ios::trunc);
    file << FileVersion << '\n' << Hex(owner) << '\n';
    for (const auto& entry : m_Entries)
        WriteEntry(file, entry);
    file.close();
    bool replaced = false;
    if (file)
    {
#ifdef _WIN32
        replaced =
            MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
        std::filesystem::rename(temporary, path, error);
        replaced = !error;
#endif
    }
    if (!replaced)
        std::filesystem::remove(temporary, error);
    return replaced;
}

template <typename Change>
bool PriceMemory::Update(const std::filesystem::path& path, std::string_view owner, Change change)
{
    std::error_code error;
    if (!path.parent_path().empty())
        std::filesystem::create_directories(path.parent_path(), error);
    const StoreLock lock(path);
    if (!lock.Locked())
        return false;
    // The latest file, as other clients of the account left it; a missing or unreadable one is empty.
    PriceMemory latest;
    latest.Load(path, owner);
    for (const auto& identity : m_Uses)
        latest.Use(identity);
    change(latest);
    if (!latest.Save(path, owner))
        return false;
    m_Entries = std::move(latest.m_Entries);
    m_Uses.clear();
    return true;
}

bool PriceMemory::SavePublished(const std::filesystem::path& path, std::string_view owner,
                                const PricingIdentity& identity, Zen price)
{
    return Update(path, owner, [&identity, price](PriceMemory& store) { store.Remember(identity, price); });
}

bool PriceMemory::SaveForgotten(const std::filesystem::path& path, std::string_view owner,
                                const PricingIdentity& identity)
{
    return Update(path, owner, [&identity](PriceMemory& store) { store.Forget(identity); });
}

std::filesystem::path UniqueTemporaryPath(const std::filesystem::path& path)
{
    // The process id tells processes apart, the counter writers of one process, the clock a reused id.
    static std::atomic<std::uint64_t> counter{0};
    const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
    auto temporary = path;
    temporary += ".tmp-" + std::to_string(ProcessId()) + "-" + std::to_string(++counter) + "-" + std::to_string(ticks);
    return temporary;
}

std::string PriceMemoryOwner(std::string_view connectAddress, std::uint16_t connectPort, std::string_view serverName,
                             int serverIndex, std::string_view account)
{
    // Unit separators keep "a" + "bc" apart from "ab" + "c".
    std::string owner;
    owner.append(connectAddress).append(":").append(std::to_string(connectPort));
    owner.append("\x1f").append(serverName).append("\x1f").append(std::to_string(serverIndex));
    owner.append("\x1f").append(account);
    return owner;
}

std::filesystem::path PriceMemoryPath(std::string_view owner)
{
    // A short hash names the file; the owner inside the file confirms it.
    std::uint64_t hash = FnvOffset;
    for (unsigned char byte : owner)
    {
        hash ^= byte;
        hash *= FnvPrime;
    }
    std::string name;
    for (int shift = 60; shift >= 0; shift -= 4)
        name += HexDigits[(hash >> shift) & 15];
    return std::filesystem::path("Config/auction-prices") / (name + ".prices");
}
} // namespace GameLogic::AuctionHouse
