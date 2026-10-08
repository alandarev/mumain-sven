#include "GameLogic/AuctionHouse/AuctionHousePriceMemory.h"

#include <algorithm>
#include <fstream>
#include <limits>
#include <sstream>
#ifdef _WIN32
#include <windows.h>
#endif

namespace GameLogic::AuctionHouse
{
namespace
{
constexpr int FileVersion = 1;
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
}

bool PriceMemory::Load(const std::filesystem::path& path, std::string_view owner)
{
    m_Entries.clear();
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
    auto temporary = path;
    temporary += ".tmp";
    std::ofstream file(temporary, std::ios::trunc);
    file << FileVersion << '\n' << Hex(owner) << '\n';
    for (const auto& entry : m_Entries)
        WriteEntry(file, entry);
    file.close();
    if (!file)
        return false;
#ifdef _WIN32
    return MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    std::filesystem::rename(temporary, path, error);
    return !error;
#endif
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
