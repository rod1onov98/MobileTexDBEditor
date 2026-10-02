#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace texdb {

struct ImageRGBA {
    std::uint32_t width{};
    std::uint32_t height{};
    std::vector<std::uint8_t> pixels; // RGBA8

    [[nodiscard]] bool valid() const noexcept {
        return width > 0 && height > 0 && pixels.size() == static_cast<std::size_t>(width) * height * 4;
    }

    [[nodiscard]] bool hasAlpha() const noexcept;
};

struct TextureEntry {
    std::string name;
    std::string line;
    int originalIndex{-1};
    bool deleted{false};
    std::optional<ImageRGBA> replacement;
};

struct PlatformInfo {
    std::string tag; // dxt / etc / pvr / ...
    std::filesystem::path datPath;
    std::filesystem::path tocPath;
    std::filesystem::path tmbPath;
    std::uint32_t datSize{};
    std::vector<std::int32_t> offsets;
};

class TextureDatabase {
public:
    static TextureDatabase Open(const std::filesystem::path& txtFile);

    [[nodiscard]] const std::filesystem::path& sourceTxt() const noexcept {
        return m_txtPath;
    }
    [[nodiscard]] const std::filesystem::path& sourceDir() const noexcept {
        return m_sourceDir;
    }
    [[nodiscard]] const std::string& baseName() const noexcept {
        return m_baseName;
    }
    [[nodiscard]] const std::vector<TextureEntry>& entries() const noexcept {
        return m_entries;
    }
    [[nodiscard]] std::vector<TextureEntry>& entries() noexcept {
        return m_entries;
    }
    [[nodiscard]] const std::vector<PlatformInfo>& platforms() const noexcept {
        return m_platforms;
    }

    [[nodiscard]] int findByName(const std::string& name) const;
    [[nodiscard]] std::uint32_t widthOf(std::size_t index) const;
    [[nodiscard]] std::uint32_t heightOf(std::size_t index) const;
    [[nodiscard]] int alphaModeOf(std::size_t index) const;
    [[nodiscard]] bool mipModeOf(std::size_t index) const;
    [[nodiscard]] bool isAffiliate(std::size_t index) const;
    [[nodiscard]] bool isModified(std::size_t index) const;

    void replace(std::size_t index, ImageRGBA image);
    std::size_t add(std::string name, ImageRGBA image);
    void erase(std::size_t index);

    [[nodiscard]] ImageRGBA decode(std::size_t index, std::string* sourceDescription = nullptr) const;

    void saveAs(const std::filesystem::path& outputDir) const;

    [[nodiscard]] std::string validationSummary() const;

    static std::uint32_t hash32(const std::string& text) noexcept;
    static std::uint16_t hash16(const std::string& text) noexcept {
        return static_cast<std::uint16_t>(hash32(text) & 0xFFFF);
    }

private:
    struct Row {
        bool isTexture{false};
        int entryIndex{-1};
        std::string raw;
    };

    struct RecordHeader {
        std::uint16_t hash{};
        std::uint16_t encoding{};
        std::uint16_t width{};
        std::uint16_t heightMask{};
        std::uint32_t storedSize{};
        std::uint32_t rle{};
    };

    std::filesystem::path m_txtPath;
    std::filesystem::path m_sourceDir;
    std::string m_baseName;
    std::vector<Row> m_rows;
    std::vector<TextureEntry> m_entries;
    std::vector<PlatformInfo> m_platforms;

    static RecordHeader readHeader(const PlatformInfo& platform, int originalIndex);
    static std::uint64_t recordSpan(const PlatformInfo& platform, int originalIndex);
    static ImageRGBA decodeRecord(const PlatformInfo& platform, int originalIndex, const std::string& name);

    static std::string makeImageLine(const std::string& name, const ImageRGBA& image, int alphaMode = 2);
    static std::string updateImageLine(std::string line, const ImageRGBA& image);
};

} // namespace texdb
