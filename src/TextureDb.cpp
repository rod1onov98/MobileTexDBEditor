#include "TextureDb.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace texdb {
namespace {

using std::uint8_t;
using std::uint16_t;
using std::uint32_t;
using std::uint64_t;

constexpr uint16_t ENC_RGBA8888 = 0x1401;
constexpr uint16_t ENC_RGBA4444 = 0x8033;
constexpr uint16_t ENC_RGB565 = 0x8363;
constexpr uint16_t ENC_DXT1 = 0x83F0;
constexpr uint16_t ENC_DXT5 = 0x83F3;

std::vector<uint8_t> readAll(const std::filesystem::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) {
        throw std::runtime_error("Cannot open file: " + p.string());
    }
    f.seekg(0, std::ios::end);
    const auto n = f.tellg();
    f.seekg(0, std::ios::beg);
    if (n < 0) {
        throw std::runtime_error("Cannot determine file size: " + p.string());
    }
    std::vector<uint8_t> b(static_cast<std::size_t>(n));
    if (!b.empty()) {
        f.read(reinterpret_cast<char*>(b.data()), static_cast<std::streamsize>(b.size()));
    }
    if (!f && !b.empty()) {
        throw std::runtime_error("Cannot read file: " + p.string());
    }
    return b;
}

std::string readText(const std::filesystem::path& p) {
    auto b = readAll(p);
    if (b.size() >= 3 && b[0] == 0xEF && b[1] == 0xBB && b[2] == 0xBF) {
        return std::string(reinterpret_cast<const char*>(b.data() + 3), b.size() - 3);
    }
    return std::string(reinterpret_cast<const char*>(b.data()), b.size());
}

std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> out;
    std::size_t pos = 0;
    while (pos <= text.size()) {
        const std::size_t e = text.find('\n', pos);
        std::string line = text.substr(pos, e == std::string::npos ? std::string::npos : e - pos);
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        out.push_back(std::move(line));
        if (e == std::string::npos) {
            break;
        }
        pos = e + 1;
    }
    if (!out.empty() && out.back().empty() && !text.empty() && text.back() == '\n') {
        out.pop_back();
    }
    return out;
}

bool startsWith(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && std::equal(prefix.begin(), prefix.end(), s.begin());
}

bool endsWith(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() && std::equal(suffix.rbegin(), suffix.rend(), s.rbegin());
}

std::string parseTextureName(const std::string& line) {
    if (line.size() < 3 || line.front() != '"') return {};
    const auto q = line.find('"', 1);
    if (q == std::string::npos) return {};
    return line.substr(1, q - 1);
}

std::optional<std::string> getProperty(const std::string& line, const std::string& key) {
    const std::string needle = key + "=";
    std::size_t pos = 0;
    while ((pos = line.find(needle, pos)) != std::string::npos) {
        if (pos > 0) {
            const char prev = line[pos - 1];
            if (prev != ' ' && prev != '\t' && prev != '"') {
                pos += needle.size();
                continue;
            }
        }
        std::size_t begin = pos + needle.size();
        if (begin < line.size() && line[begin] == '"') {
            const auto end = line.find('"', begin + 1);
            if (end == std::string::npos) {
                return line.substr(begin + 1);
            }
            return line.substr(begin + 1, end - begin - 1);
        }
        std::size_t end = begin;
        while (end < line.size() && line[end] != ' ' && line[end] != '\t' && line[end] != '"') {
            ++end;
        }
        return line.substr(begin, end - begin);
    }
    return std::nullopt;
}

int getIntProperty(const std::string& line, const std::string& key, int fallback) {
    const auto v = getProperty(line, key);
    if (!v || v->empty()) {
        return fallback;
    }
    int result = fallback;
    const char* first = v->data();
    const char* last = first + v->size();
    const auto [ptr, ec] = std::from_chars(first, last, result);
    return (ec == std::errc{} && ptr == last) ? result : fallback;
}

std::string removeProperty(std::string line, const std::string& key) {
    const std::string quotedNeedle = "\"" + key + "=";
    if (const auto qp = line.find(quotedNeedle); qp != std::string::npos) {
        const auto qe = line.find('"', qp + quotedNeedle.size());
        if (qe != std::string::npos) {
            std::size_t from = qp;
            if (from > 0 && line[from - 1] == ' ') {
                --from;
            }
            line.erase(from, qe - from + 1);
        }
    }

    const std::string needle = key + "=";
    std::size_t p = line.find(needle);
    if (p == std::string::npos) {
        return line;
    }
    std::size_t from = p;
    while (from > 0 && line[from - 1] == ' ') {
        --from;
    }
    std::size_t to = p + needle.size();
    while (to < line.size() && line[to] != ' ' && line[to] != '\t' && line[to] != '"') {
        ++to;
    }
    line.erase(from, to - from);
    return line;
}

std::string setProperty(std::string line, const std::string& key, const std::string& value) {
    const std::string needle = key + "=";
    std::size_t p = line.find(needle);
    if (p != std::string::npos) {
        std::size_t begin = p + needle.size();
        std::size_t end = begin;
        while (end < line.size() && line[end] != ' ' && line[end] != '\t' && line[end] != '"') {
            ++end;
        }
        line.replace(begin, end - begin, value);
        return line;
    }
    if (!line.empty() && line.back() != ' ') {
        line.push_back(' ');
    }
    line += key + "=" + value;
    return line;
}

uint16_t rd16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0] | (static_cast<uint16_t>(p[1]) << 8));
}
uint32_t rd32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

void wr16(std::ostream& o, uint16_t v) {
    const std::array<uint8_t, 2> b{static_cast<uint8_t>(v), static_cast<uint8_t>(v >> 8)};
    o.write(reinterpret_cast<const char*>(b.data()), b.size());
}
void wr32(std::ostream& o, uint32_t v) {
    const std::array<uint8_t, 4> b { static_cast<uint8_t>(v), static_cast<uint8_t>(v >> 8), static_cast<uint8_t>(v >> 16), static_cast<uint8_t>(v >> 24) };
    o.write(reinterpret_cast<const char*>(b.data()), b.size());
}

std::string lowerAscii(std::string s) {
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return s;
}

std::size_t encodedMipSize(uint16_t encoding, uint32_t w, uint32_t h) {
    switch (encoding) {
    case ENC_RGBA8888:
        return static_cast<std::size_t>(w) * h * 4;
    case ENC_RGB565:
    case ENC_RGBA4444:
        return static_cast<std::size_t>(w) * h * 2;
    case ENC_DXT1:
        return static_cast<std::size_t>((w + 3) / 4) * ((h + 3) / 4) * 8;
    case ENC_DXT5:
        return static_cast<std::size_t>((w + 3) / 4) * ((h + 3) / 4) * 16;
    default:
        return 0;
    }
}

std::size_t totalEncodedSize(uint16_t encoding, uint32_t w, uint32_t h, bool hasMipChain) {
    std::size_t total = encodedMipSize(encoding, w, h);
    if (!hasMipChain || total == 0) {
        return total;
    }

    if (encoding == ENC_DXT1 || encoding == ENC_DXT5) {
        while (w > 4 && h > 4) {
            w = std::max<uint32_t>(1, w / 2);
            h = std::max<uint32_t>(1, h / 2);
            total += encodedMipSize(encoding, w, h);
        }
        return total;
    }

    while (w > 1 || h > 1) {
        w = std::max<uint32_t>(1, w / 2);
        h = std::max<uint32_t>(1, h / 2);
        total += encodedMipSize(encoding, w, h);
    }
    return total;
}

std::size_t rlePixelBlockSize(uint16_t encoding) {
    switch (encoding) {
    case ENC_DXT1:
        return 8;
    case ENC_DXT5:
        return 16;
    case ENC_RGBA8888:
        return 4;
    case ENC_RGB565:
    case ENC_RGBA4444:
        return 2;
    default:
        return 4;
    }
}

std::vector<uint8_t> rleDecompress(const std::vector<uint8_t>& stored, std::size_t expected, std::size_t pixelBlockSize) {
    if (stored.size() < 4) {
        throw std::runtime_error("Broken RLE payload");
    }
    const uint8_t control = stored[0];
    const std::size_t block = std::max<std::size_t>(4, pixelBlockSize);
    std::vector<uint8_t> out;
    out.reserve(expected);

    std::size_t in = 4;
    while (in < stored.size() && out.size() < expected) {
        if (stored[in] != control) {
            const std::size_t copy = std::min(block, expected - out.size());
            if (in + copy > stored.size()) {
                throw std::runtime_error("Truncated RLE stream");
            }
            out.insert(out.end(), stored.begin() + static_cast<std::ptrdiff_t>(in), stored.begin() + static_cast<std::ptrdiff_t>(in + copy));
            in += block;
        } else {
            if (in + 2 + block > stored.size()) {
                throw std::runtime_error("Truncated RLE repeat");
            }
            const uint8_t count = stored[in + 1];
            in += 2;
            for (uint32_t n = 0; n < count && out.size() < expected; ++n) {
                const std::size_t copy = std::min(block, expected - out.size());
                out.insert(out.end(), stored.begin() + static_cast<std::ptrdiff_t>(in), stored.begin() + static_cast<std::ptrdiff_t>(in + copy));
            }
            in += block;
        }
    }
    if (out.size() < expected) {
        throw std::runtime_error("RLE stream decoded to fewer bytes than expected");
    }
    out.resize(expected);
    return out;
}

struct Color { uint8_t r, g, b, a; };

Color rgb565(uint16_t c) {
    const uint8_t r5 = static_cast<uint8_t>((c >> 11) & 31);
    const uint8_t g6 = static_cast<uint8_t>((c >> 5) & 63);
    const uint8_t b5 = static_cast<uint8_t>(c & 31);
    return Color{static_cast<uint8_t>((r5 << 3) | (r5 >> 2)), static_cast<uint8_t>((g6 << 2) | (g6 >> 4)), static_cast<uint8_t>((b5 << 3) | (b5 >> 2)), 255};
}

void putPixel(ImageRGBA& img, uint32_t x, uint32_t y, const Color& c) {
    if (x >= img.width || y >= img.height) {
        return;
    }
    const std::size_t p = (static_cast<std::size_t>(y) * img.width + x) * 4;
    img.pixels[p + 0] = c.r;
    img.pixels[p + 1] = c.g;
    img.pixels[p + 2] = c.b;
    img.pixels[p + 3] = c.a;
}

ImageRGBA decodeDxt1(const std::vector<uint8_t>& data, uint32_t w, uint32_t h) {
    const std::size_t need = encodedMipSize(ENC_DXT1, w, h);
    if (data.size() < need) {
        throw std::runtime_error("DXT1 payload is truncated");
    }
    ImageRGBA out{w, h, std::vector<uint8_t>(static_cast<std::size_t>(w) * h * 4)};
    std::size_t p = 0;
    for (uint32_t by = 0; by < (h + 3) / 4; ++by) {
        for (uint32_t bx = 0; bx < (w + 3) / 4; ++bx) {
            const uint16_t c0 = rd16(&data[p]);
            const uint16_t c1 = rd16(&data[p + 2]);
            const uint32_t bits = rd32(&data[p + 4]);
            p += 8;
            std::array<Color, 4> pal{};
            pal[0] = rgb565(c0);
            pal[1] = rgb565(c1);
            if (c0 > c1) {
                pal[2] = Color{static_cast<uint8_t>((2 * pal[0].r + pal[1].r) / 3), static_cast<uint8_t>((2 * pal[0].g + pal[1].g) / 3), static_cast<uint8_t>((2 * pal[0].b + pal[1].b) / 3), 255};
                pal[3] = Color{static_cast<uint8_t>((pal[0].r + 2 * pal[1].r) / 3), static_cast<uint8_t>((pal[0].g + 2 * pal[1].g) / 3), static_cast<uint8_t>((pal[0].b + 2 * pal[1].b) / 3), 255};
            } else {
                pal[2] = Color{static_cast<uint8_t>((pal[0].r + pal[1].r) / 2), static_cast<uint8_t>((pal[0].g + pal[1].g) / 2), static_cast<uint8_t>((pal[0].b + pal[1].b) / 2), 255};
                pal[3] = Color{0, 0, 0, 0};
            }
            for (uint32_t py = 0; py < 4; ++py) {
                for (uint32_t px = 0; px < 4; ++px) {
                    const uint32_t idx = (bits >> (2 * (py * 4 + px))) & 3u;
                    putPixel(out, bx * 4 + px, by * 4 + py, pal[idx]);
                }
            }
        }
    }
    return out;
}

ImageRGBA decodeDxt5(const std::vector<uint8_t>& data, uint32_t w, uint32_t h) {
    const std::size_t need = encodedMipSize(ENC_DXT5, w, h);
    if (data.size() < need) {
        throw std::runtime_error("DXT5 payload is truncated");
    }
    ImageRGBA out{w, h, std::vector<uint8_t>(static_cast<std::size_t>(w) * h * 4)};
    std::size_t p = 0;
    for (uint32_t by = 0; by < (h + 3) / 4; ++by) {
        for (uint32_t bx = 0; bx < (w + 3) / 4; ++bx) {
            const uint8_t a0 = data[p + 0];
            const uint8_t a1 = data[p + 1];
            uint64_t abits = 0;
            for (int i = 0; i < 6; ++i) {
                abits |= static_cast<uint64_t>(data[p + 2 + i]) << (8 * i);
            }
            std::array<uint8_t, 8> alpha{};
            alpha[0] = a0; alpha[1] = a1;
            if (a0 > a1) {
                for (int i = 1; i <= 6; ++i)
                    alpha[i + 1] = static_cast<uint8_t>(((7 - i) * a0 + i * a1) / 7);
            } else {
                for (int i = 1; i <= 4; ++i)
                    alpha[i + 1] = static_cast<uint8_t>(((5 - i) * a0 + i * a1) / 5);
                alpha[6] = 0; alpha[7] = 255;
            }

            const uint16_t c0 = rd16(&data[p + 8]);
            const uint16_t c1 = rd16(&data[p + 10]);
            const uint32_t cbits = rd32(&data[p + 12]);
            p += 16;
            std::array<Color, 4> pal{};
            pal[0] = rgb565(c0); pal[1] = rgb565(c1);
            pal[2] = Color{static_cast<uint8_t>((2 * pal[0].r + pal[1].r) / 3), static_cast<uint8_t>((2 * pal[0].g + pal[1].g) / 3), static_cast<uint8_t>((2 * pal[0].b + pal[1].b) / 3), 255};
            pal[3] = Color{static_cast<uint8_t>((pal[0].r + 2 * pal[1].r) / 3), static_cast<uint8_t>((pal[0].g + 2 * pal[1].g) / 3), static_cast<uint8_t>((pal[0].b + 2 * pal[1].b) / 3), 255};

            for (uint32_t py = 0; py < 4; ++py) {
                for (uint32_t px = 0; px < 4; ++px) {
                    const uint32_t n = py * 4 + px;
                    const uint32_t ci = (cbits >> (2 * n)) & 3;
                    const uint32_t ai = static_cast<uint32_t>((abits >> (3 * n)) & 7);
                    Color c = pal[ci]; c.a = alpha[ai];
                    putPixel(out, bx * 4 + px, by * 4 + py, c);
                }
            }
        }
    }
    return out;
}

ImageRGBA decode565(const std::vector<uint8_t>& data, uint32_t w, uint32_t h) {
    const std::size_t need = static_cast<std::size_t>(w) * h * 2;
    if (data.size() < need) {
        throw std::runtime_error("RGB565 payload is truncated");
    }
    ImageRGBA out{w, h, std::vector<uint8_t>(static_cast<std::size_t>(w) * h * 4)};
    for (std::size_t i = 0; i < static_cast<std::size_t>(w) * h; ++i) {
        const auto c = rgb565(rd16(&data[i * 2]));
        out.pixels[i * 4 + 0] = c.r;
        out.pixels[i * 4 + 1] = c.g;
        out.pixels[i * 4 + 2] = c.b;
        out.pixels[i * 4 + 3] = 255;
    }
    return out;
}

ImageRGBA decode4444(const std::vector<uint8_t>& data, uint32_t w, uint32_t h) {
    const std::size_t need = static_cast<std::size_t>(w) * h * 2;
    if (data.size() < need) {
        throw std::runtime_error("RGBA4444 payload is truncated");
    }
    ImageRGBA out{w, h, std::vector<uint8_t>(static_cast<std::size_t>(w) * h * 4)};
    for (std::size_t i = 0; i < static_cast<std::size_t>(w) * h; ++i) {
        const uint16_t v = rd16(&data[i * 2]);
        const uint8_t r = static_cast<uint8_t>((v >> 12) & 0xF);
        const uint8_t g = static_cast<uint8_t>((v >> 8) & 0xF);
        const uint8_t b = static_cast<uint8_t>((v >> 4) & 0xF);
        const uint8_t a = static_cast<uint8_t>(v & 0xF);
        out.pixels[i * 4 + 0] = static_cast<uint8_t>(r * 17);
        out.pixels[i * 4 + 1] = static_cast<uint8_t>(g * 17);
        out.pixels[i * 4 + 2] = static_cast<uint8_t>(b * 17);
        out.pixels[i * 4 + 3] = static_cast<uint8_t>(a * 17);
    }
    return out;
}

std::string encodingName(uint16_t e) {
    switch (e) {
    case ENC_RGBA8888:
        return "RGBA8888";
    case ENC_RGBA4444:
        return "RGBA4444";
    case ENC_RGB565:
        return "RGB565";
    case ENC_DXT1:
        return "DXT1";
    case ENC_DXT5:
        return "DXT5";
    case 0x8C02:
        return "PVRTC";
    default: {
        std::ostringstream ss;
        ss << "0x" << std::hex << std::uppercase << e;
        return ss.str();
    }
    }
}

void copyRange(std::ifstream& in, std::ofstream& out, uint64_t offset, uint64_t count) {
    constexpr std::size_t BUF = 1 << 20;
    std::vector<char> buffer(BUF);
    in.clear();
    in.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    if (!in) {
        throw std::runtime_error("Seek failed while copying DAT record");
    }
    while (count) {
        const std::size_t n = static_cast<std::size_t>(std::min<uint64_t>(count, buffer.size()));
        in.read(buffer.data(), static_cast<std::streamsize>(n));
        if (in.gcount() != static_cast<std::streamsize>(n)) {
            throw std::runtime_error("Unexpected EOF in DAT");
        }
        out.write(buffer.data(), static_cast<std::streamsize>(n));
        if (!out) {
            throw std::runtime_error("Write failed while rebuilding DAT");
        }
        count -= n;
    }
}

} // namespace

bool ImageRGBA::hasAlpha() const noexcept {
    if (!valid()) {
        return false;
    }
    for (std::size_t i = 3; i < pixels.size(); i += 4) {
        if (pixels[i] != 255) {
            return true;
        }
    }
    return false;
}

std::uint32_t TextureDatabase::hash32(const std::string& text) noexcept {
    uint32_t h = 0;
    for (const unsigned char c : text) {
        h += (h << 5);
        h += static_cast<uint32_t>(c);
    }
    h += (h >> 5);
    return h;
}

TextureDatabase TextureDatabase::Open(const std::filesystem::path& txtFile) {
    TextureDatabase db;
    db.m_txtPath = std::filesystem::absolute(txtFile);
    db.m_sourceDir = db.m_txtPath.parent_path();
    db.m_baseName = db.m_txtPath.stem().string();

    if (!std::filesystem::exists(db.m_txtPath))
        throw std::runtime_error("TXT database file does not exist");

    const auto lines = splitLines(readText(db.m_txtPath));
    int textureIndex = 0;
    for (const auto& line : lines) {
        const std::string name = parseTextureName(line);
        if (!name.empty()) {
            TextureEntry e;
            e.name = name;
            e.line = line;
            e.originalIndex = textureIndex;
            db.m_entries.push_back(std::move(e));
            db.m_rows.push_back(Row{true, textureIndex, {}});
            ++textureIndex;
        } else {
            db.m_rows.push_back(Row{false, -1, line});
        }
    }
    if (db.m_entries.empty()) {
        throw std::runtime_error("No texture entries found in TXT file");
    }

    const std::string prefix = db.m_baseName + ".";
    for (const auto& de : std::filesystem::directory_iterator(db.m_sourceDir)) {
        if (!de.is_regular_file()) {
            continue;
        }
        const std::string fn = de.path().filename().string();
        if (!startsWith(fn, prefix) || !endsWith(lowerAscii(fn), ".dat")) {
            continue;
        }
        const std::size_t tagStart = prefix.size();
        const std::size_t tagLen = fn.size() - prefix.size() - 4;
        if (tagLen == 0) {
            continue;
        }
        const std::string tag = fn.substr(tagStart, tagLen);
        const auto toc = db.m_sourceDir / (db.m_baseName + "." + tag + ".toc");
        if (!std::filesystem::exists(toc)) {
            continue;
        }

        PlatformInfo p;
        p.tag = tag;
        p.datPath = de.path();
        p.tocPath = toc;
        p.tmbPath = db.m_sourceDir / (db.m_baseName + "." + tag + ".tmb");

        auto tb = readAll(toc);
        if (tb.size() < 4 || (tb.size() - 4) % 4 != 0)
            throw std::runtime_error("Malformed TOC: " + toc.string());
        p.datSize = rd32(tb.data());
        const std::size_t count = (tb.size() - 4) / 4;
        if (count != db.m_entries.size()) {
            std::ostringstream ss;
            ss << "TOC entry count mismatch for " << tag << ": TXT=" << db.m_entries.size() << ", TOC=" << count;
            throw std::runtime_error(ss.str());
        }
        p.offsets.resize(count);
        for (std::size_t i = 0; i < count; ++i)
            p.offsets[i] = static_cast<std::int32_t>(rd32(tb.data() + 4 + i * 4));

        const auto realSize = std::filesystem::file_size(p.datPath);
        if (realSize > std::numeric_limits<uint32_t>::max())
            throw std::runtime_error("DAT is larger than 4 GiB and is not supported by this editor");
        if (p.datSize != realSize) {
            std::ostringstream ss;
            ss << "TOC/DAT size mismatch for " << tag << ": TOC=" << p.datSize << ", DAT=" << realSize;
            throw std::runtime_error(ss.str());
        }
        db.m_platforms.push_back(std::move(p));
    }

    if (db.m_platforms.empty())
        throw std::runtime_error("No matching <name>.<platform>.dat/.toc pairs were found");

    std::sort(db.m_platforms.begin(), db.m_platforms.end(), [](const auto& a, const auto& b) {
        auto rank = [](const std::string& t) {
            const auto l = lowerAscii(t);
            if (l == "dxt") {
                return 0;
            }
            if (l == "etc") {
                return 1;
            }
            if (l == "pvr") {
                return 2;
            }
            return 3;
        };
        const int ra = rank(a.tag), rb = rank(b.tag);
        return ra != rb ? ra < rb : a.tag < b.tag;
    });

    return db;
}

int TextureDatabase::findByName(const std::string& name) const {
    for (std::size_t i = 0; i < m_entries.size(); ++i) {
        if (!m_entries[i].deleted && m_entries[i].name == name) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

std::uint32_t TextureDatabase::widthOf(std::size_t index) const {
    const auto& e = m_entries.at(index);
    if (e.replacement) {
        return e.replacement->width;
    }
    return static_cast<uint32_t>(std::max(0, getIntProperty(e.line, "width", 0)));
}
std::uint32_t TextureDatabase::heightOf(std::size_t index) const {
    const auto& e = m_entries.at(index);
    if (e.replacement) {
        return e.replacement->height;
    }
    return static_cast<uint32_t>(std::max(0, getIntProperty(e.line, "height", 0)));
}
int TextureDatabase::alphaModeOf(std::size_t index) const {
    return getIntProperty(m_entries.at(index).line, "alphamode", 0);
}
bool TextureDatabase::mipModeOf(std::size_t index) const {
    return getIntProperty(m_entries.at(index).line, "mipmode", 1) != 0;
}
bool TextureDatabase::isAffiliate(std::size_t index) const {
    return getProperty(m_entries.at(index).line, "affiliate").has_value();
}
bool TextureDatabase::isModified(std::size_t index) const {
    const auto& e = m_entries.at(index);
    return e.deleted || e.replacement.has_value() || e.originalIndex < 0;
}

std::string TextureDatabase::makeImageLine(const std::string& name, const ImageRGBA& image, int alphaMode) {
    std::ostringstream ss;
    ss << '"' << name << '"' << " width=" << image.width << " height=" << image.height << " img=00000000 mipmode=0 alphamode=" << alphaMode;
    return ss.str();
}

std::string TextureDatabase::updateImageLine(std::string line, const ImageRGBA& image) {
    line = removeProperty(std::move(line), "affiliate");
    line = setProperty(std::move(line), "width", std::to_string(image.width));
    line = setProperty(std::move(line), "height", std::to_string(image.height));
    line = setProperty(std::move(line), "mipmode", "0");
    if (!getProperty(line, "alphamode")) {
        line = setProperty(std::move(line), "alphamode", "2");
    }
    return line;
}

void TextureDatabase::replace(std::size_t index, ImageRGBA image) {
    if (!image.valid()) {
        throw std::runtime_error("Replacement image is invalid");
    }
    if (image.width > 32767 || image.height > 32767)
        throw std::runtime_error("Texture dimensions must fit into 15 bits");
    auto& e = m_entries.at(index);
    if (e.deleted) {
        throw std::runtime_error("Cannot replace a deleted texture");
    }
    e.line = updateImageLine(e.line, image);
    e.replacement = std::move(image);
}

std::size_t TextureDatabase::add(std::string name, ImageRGBA image) {
    if (name.empty()) {
        throw std::runtime_error("Texture name is empty");
    }
    if (name.find('"') != std::string::npos) {
        throw std::runtime_error("Texture name cannot contain quotes");
    }
    if (findByName(name) >= 0) {
        throw std::runtime_error("A texture with this name already exists");
    }
    if (!image.valid()) {
        throw std::runtime_error("Image is invalid");
    }
    if (image.width > 32767 || image.height > 32767)
        throw std::runtime_error("Texture dimensions must fit into 15 bits");

    TextureEntry e;
    e.name = std::move(name);
    e.originalIndex = -1;
    e.line = makeImageLine(e.name, image, 2);
    e.replacement = std::move(image);
    m_entries.push_back(std::move(e));
    return m_entries.size() - 1;
}

void TextureDatabase::erase(std::size_t index) {
    m_entries.at(index).deleted = true;
}

TextureDatabase::RecordHeader TextureDatabase::readHeader(const PlatformInfo& p, int originalIndex) {
    if (originalIndex < 0 || static_cast<std::size_t>(originalIndex) >= p.offsets.size()) 
        throw std::runtime_error("Invalid original texture index");
    const std::int32_t off = p.offsets[static_cast<std::size_t>(originalIndex)];
    if (off < 0) {
        throw std::runtime_error("Affiliate texture has no DAT record");
    }
    std::ifstream f(p.datPath, std::ios::binary);
    if (!f) {
        throw std::runtime_error("Cannot open DAT: " + p.datPath.string());
    }
    f.seekg(off, std::ios::beg);
    std::array<uint8_t, 16> b{};
    f.read(reinterpret_cast<char*>(b.data()), b.size());
    if (f.gcount() != static_cast<std::streamsize>(b.size())) {
        throw std::runtime_error("Truncated DAT header");
    }
    return RecordHeader{ rd16(b.data()), rd16(b.data() + 2), rd16(b.data() + 4), rd16(b.data() + 6), rd32(b.data() + 8), rd32(b.data() + 12) };
}

std::uint64_t TextureDatabase::recordSpan(const PlatformInfo& p, int originalIndex) {
    const std::size_t i = static_cast<std::size_t>(originalIndex);
    const auto off = p.offsets.at(i);
    if (off < 0) {
        return 0;
    }
    std::uint64_t next = p.datSize;
    for (std::size_t j = i + 1; j < p.offsets.size(); ++j) {
        if (p.offsets[j] >= 0) { next = static_cast<uint32_t>(p.offsets[j]); break; }
    }
    if (next < static_cast<uint32_t>(off)) {
        throw std::runtime_error("TOC offsets are not ordered");
    }
    return next - static_cast<uint32_t>(off);
}

ImageRGBA TextureDatabase::decodeRecord(const PlatformInfo& p, int originalIndex, const std::string& name) {
    const RecordHeader h = readHeader(p, originalIndex);
    const std::int32_t off = p.offsets.at(static_cast<std::size_t>(originalIndex));
    const uint32_t w = h.width;
    const uint32_t hh = h.heightMask & 0x7FFF;
    if (!w || !hh) {
        throw std::runtime_error("Texture has zero dimensions");
    }

    const bool hasMipChain = (h.heightMask & 0x8000) != 0;
    const std::size_t expected = totalEncodedSize(h.encoding, w, hh, hasMipChain);
    if (expected == 0) {
        throw std::runtime_error("Preview decoder does not support encoding " + encodingName(h.encoding));
    }
    if (h.storedSize < 4) {
        throw std::runtime_error("Invalid stored size in DAT record");
    }

    std::ifstream f(p.datPath, std::ios::binary);
    if (!f) {
        throw std::runtime_error("Cannot open DAT: " + p.datPath.string());
    }
    f.seekg(static_cast<std::streamoff>(off) + 12, std::ios::beg);
    std::vector<uint8_t> stored(h.storedSize);
    f.read(reinterpret_cast<char*>(stored.data()), static_cast<std::streamsize>(stored.size()));
    if (f.gcount() != static_cast<std::streamsize>(stored.size()))
        throw std::runtime_error("Truncated DAT record: " + name);

    std::vector<uint8_t> encoded;
    if (h.rle == 0) {
        encoded.assign(stored.begin() + 4, stored.end());
    } else {
        encoded = rleDecompress(stored, expected, rlePixelBlockSize(h.encoding));
    }

    const std::size_t firstMip = encodedMipSize(h.encoding, w, hh);
    if (encoded.size() < firstMip) {
        throw std::runtime_error("Texture payload is shorter than the first mip");
    }
    encoded.resize(firstMip);

    switch (h.encoding) {
    case ENC_RGBA8888:
        return ImageRGBA{w, hh, std::move(encoded)};
    case ENC_RGB565:
        return decode565(encoded, w, hh);
    case ENC_RGBA4444:
        return decode4444(encoded, w, hh);
    case ENC_DXT1:
        return decodeDxt1(encoded, w, hh);
    case ENC_DXT5:
        return decodeDxt5(encoded, w, hh);
    default:
        throw std::runtime_error("Preview decoder does not support encoding " + encodingName(h.encoding));
    }
}

ImageRGBA TextureDatabase::decode(std::size_t index, std::string* sourceDescription) const {
    const auto& e = m_entries.at(index);
    if (e.deleted) {
        throw std::runtime_error("Texture is deleted");
    }
    if (e.replacement) {
        if (sourceDescription) {
            *sourceDescription = "edited RGBA8888";
        }
        return *e.replacement;
    }
    if (e.originalIndex < 0) {
        throw std::runtime_error("New texture has no image data");
    }

    std::string errors;
    for (const auto& p : m_platforms) {
        if (p.offsets.at(static_cast<std::size_t>(e.originalIndex)) < 0) {
            continue;
        }
        try {
            auto img = decodeRecord(p, e.originalIndex, e.name);
            if (sourceDescription) {
                const auto h = readHeader(p, e.originalIndex);
                *sourceDescription = p.tag + " / " + encodingName(h.encoding);
            }
            return img;
        } catch (const std::exception& ex) {
            if (!errors.empty()) {
                errors += "; ";
            }
            errors += p.tag + ": " + ex.what();
        }
    }
    throw std::runtime_error(errors.empty() ? "Texture cannot be decoded" : errors);
}

void TextureDatabase::saveAs(const std::filesystem::path& outputDir) const {
    if (m_platforms.empty()) {
        throw std::runtime_error("Database has no platform files");
    }
    const auto out = std::filesystem::absolute(outputDir);
    std::filesystem::create_directories(out);

    if (std::filesystem::equivalent(out, m_sourceDir)) {
        throw std::runtime_error("Refusing to overwrite the source database. Choose another output folder.");
    }

    std::vector<const TextureEntry*> finalEntries;
    finalEntries.reserve(m_entries.size());
    for (const auto& row : m_rows) {
        if (!row.isTexture) {
            continue;
        }
        const auto& e = m_entries.at(static_cast<std::size_t>(row.entryIndex));
        if (!e.deleted) {
            finalEntries.push_back(&e);
        }
    }
    for (const auto& e : m_entries) {
        if (e.originalIndex < 0 && !e.deleted) {
            finalEntries.push_back(&e);
        }
    }

    for (const auto& p : m_platforms) {
        const auto outDat = out / (m_baseName + "." + p.tag + ".dat");
        const auto outToc = out / (m_baseName + "." + p.tag + ".toc");
        std::filesystem::path tmpDat = outDat;
        std::filesystem::path tmpToc = outToc;
        tmpDat += L".tmp";
        tmpToc += L".tmp";

        std::ifstream in(p.datPath, std::ios::binary);
        if (!in) {
            throw std::runtime_error("Cannot open source DAT: " + p.datPath.string());
        }
        std::ofstream dat(tmpDat, std::ios::binary | std::ios::trunc);
        if (!dat) {
            throw std::runtime_error("Cannot create output DAT: " + outDat.string());
        }

        std::vector<std::int32_t> newOffsets;
        newOffsets.reserve(finalEntries.size());

        for (const TextureEntry* ep : finalEntries) {
            const auto& e = *ep;
            const bool originalAffiliate = e.originalIndex >= 0 && p.offsets.at(static_cast<std::size_t>(e.originalIndex)) < 0;
            const bool finalAffiliate = !e.replacement && originalAffiliate;
            if (finalAffiliate) {
                newOffsets.push_back(-1);
                continue;
            }

            const auto currentPos = dat.tellp();
            if (currentPos < 0 || static_cast<uint64_t>(currentPos) > std::numeric_limits<std::int32_t>::max())
                throw std::runtime_error("Output DAT exceeds signed 32-bit TOC offset range");
            newOffsets.push_back(static_cast<std::int32_t>(currentPos));

            if (e.replacement) {
                const auto& img = *e.replacement;
                const uint64_t pixelBytes64 = static_cast<uint64_t>(img.width) * img.height * 4;
                if (pixelBytes64 > std::numeric_limits<uint32_t>::max() - 4)
                    throw std::runtime_error("Texture is too large: " + e.name);
                wr16(dat, hash16(e.name));
                wr16(dat, ENC_RGBA8888);
                wr16(dat, static_cast<uint16_t>(img.width));
                wr16(dat, static_cast<uint16_t>(img.height));
                wr32(dat, static_cast<uint32_t>(pixelBytes64) + 4);
                wr32(dat, 0); 
                dat.write(reinterpret_cast<const char*>(img.pixels.data()), static_cast<std::streamsize>(img.pixels.size()));
                if (!dat) {
                    throw std::runtime_error("Failed writing texture: " + e.name);
                }
            } else {
                if (e.originalIndex < 0)
                    throw std::runtime_error("Internal error: new texture without replacement image");
                const auto off = p.offsets.at(static_cast<std::size_t>(e.originalIndex));
                if (off < 0) {
                    throw std::runtime_error("Internal error: affiliate without replacement");
                }
                copyRange(in, dat, static_cast<uint32_t>(off), recordSpan(p, e.originalIndex));
            }
        }

        dat.flush();
        if (!dat) {
            throw std::runtime_error("Failed finalizing output DAT");
        }
        const auto datSizePos = dat.tellp();
        dat.close();
        if (datSizePos < 0 || static_cast<uint64_t>(datSizePos) > std::numeric_limits<uint32_t>::max())
            throw std::runtime_error("Output DAT is larger than 4 GiB");
        const uint32_t newDatSize = static_cast<uint32_t>(datSizePos);

        std::ofstream toc(tmpToc, std::ios::binary | std::ios::trunc);
        if (!toc) {
            throw std::runtime_error("Cannot create output TOC: " + outToc.string());
        }
        wr32(toc, newDatSize);
        for (const auto off : newOffsets) {
            wr32(toc, static_cast<uint32_t>(off));
        }
        toc.close();

        std::error_code ec;
        std::filesystem::remove(outDat, ec);
        ec.clear();
        std::filesystem::rename(tmpDat, outDat, ec);
        if (ec) {
            throw std::runtime_error("Cannot finalize DAT: " + ec.message());
        }
        std::filesystem::remove(outToc, ec);
        ec.clear();
        std::filesystem::rename(tmpToc, outToc, ec);
        if (ec) {
            throw std::runtime_error("Cannot finalize TOC: " + ec.message());
        }
    }

    const auto outTxt = out / (m_baseName + ".txt");
    std::ofstream tf(outTxt, std::ios::binary | std::ios::trunc);
    if (!tf) {
        throw std::runtime_error("Cannot create output TXT");
    }
    for (const auto& row : m_rows) {
        if (row.isTexture) {
            const auto& e = m_entries.at(static_cast<std::size_t>(row.entryIndex));
            if (e.deleted) {
                continue;
            }
            tf << e.line << "\r\n";
        } else {
            tf << row.raw << "\r\n";
        }
    }
    for (const auto& e : m_entries) {
        if (e.originalIndex < 0 && !e.deleted) {
            tf << e.line << "\r\n";
        }
    }
    tf.close();

    const std::string prefix = lowerAscii(m_baseName) + ".";
    for (const auto& de : std::filesystem::directory_iterator(m_sourceDir)) {
        if (!de.is_regular_file()) {
            continue;
        }
        const auto fn = de.path().filename().string();
        const auto lfn = lowerAscii(fn);
        if (startsWith(lfn, prefix) && endsWith(lfn, ".tmb")) {
            std::error_code ec;
            std::filesystem::copy_file(de.path(), out / de.path().filename(), std::filesystem::copy_options::overwrite_existing, ec);
        }
    }
}

std::string TextureDatabase::validationSummary() const {
    std::ostringstream ss;
    std::size_t affiliates = 0;
    for (const auto& e : m_entries) {
        if (getProperty(e.line, "affiliate")) {
            ++affiliates;
        }
    }
    ss << m_entries.size() << " textures, " << affiliates << " affiliates, " << m_platforms.size() << " platform DB(s)";

    std::size_t hashMismatch = 0;
    for (const auto& p : m_platforms) {
        for (std::size_t i = 0; i < m_entries.size(); ++i) {
            if (p.offsets[i] < 0) {
                continue;
            }
            try {
                if (readHeader(p, static_cast<int>(i)).hash != hash16(m_entries[i].name)) {
                    ++hashMismatch;
                }
            } catch (...) {
                ++hashMismatch;
            }
        }
    }
    if (hashMismatch) {
        ss << ", " << hashMismatch << " header/hash warning(s)";
    }
    else ss << ", headers OK";
    return ss.str();
}

} // namespace texdb
