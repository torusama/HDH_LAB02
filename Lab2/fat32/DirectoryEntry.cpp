#include "DirectoryEntry.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <sstream>
#include <iomanip>

DirectoryEntry::DirectoryEntry() {}

namespace {
std::vector<uint8_t> trimTrailingNulls(std::vector<uint8_t> data) {
    while (!data.empty() && data.back() == 0) {
        data.pop_back();
    }
    return data;
}

std::string applyShortNameCase(std::string text, bool toLower) {
    if (!toLower) return text;

    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}
}

// ========================= FATDateTime helpers =========================

std::string FATDateTime::toDateString() const {
    std::ostringstream oss;
    oss << std::setfill('0')
        << std::setw(2) << day << "/"
        << std::setw(2) << month << "/"
        << std::setw(4) << year;
    return oss.str();
}

std::string FATDateTime::toTimeString() const {
    std::ostringstream oss;
    oss << std::setfill('0')
        << std::setw(2) << hour << ":"
        << std::setw(2) << minute << ":"
        << std::setw(2) << second;
    return oss.str();
}

// ========================= Public API =========================

std::vector<FileInfo> DirectoryEntry::readDirectory(uint32_t firstCluster,
                                                    DiskReader& reader,
                                                    const BootSector& boot,
                                                    const FATTable& fat) {
    std::vector<FileInfo> results;

    if (!reader.isOpen() || !boot.isValid() || !fat.isLoaded() || firstCluster < 2) {
        return results;
    }

    std::vector<uint32_t> chain = fat.getClusterChain(firstCluster);
    if (chain.empty()) return results;

    const uint32_t clusterSize = boot.getClusterSize();
    std::vector<uint8_t> clusterBuffer(clusterSize);

    std::vector<RawLFNEntry> lfnEntries;

    for (uint32_t cluster : chain) {
        if (!reader.readCluster(cluster,
                                boot.getSectorsPerCluster(),
                                boot.getDataStartSector(),
                                clusterBuffer.data())) {
            continue;
        }

        const uint32_t entryCount = clusterSize / sizeof(RawDirEntry);

        for (uint32_t i = 0; i < entryCount; ++i) {
            const uint8_t* ptr = clusterBuffer.data() + i * sizeof(RawDirEntry);
            const RawDirEntry& entry = *reinterpret_cast<const RawDirEntry*>(ptr);

            if (isEndOfDirectory(entry)) {
                return results;
            }

            if (isDeleted(entry)) {
                lfnEntries.clear();
                continue;
            }

            // LFN entry
            if (entry.DIR_Attr == Attr::LFN) {
                const RawLFNEntry& lfn = *reinterpret_cast<const RawLFNEntry*>(ptr);
                lfnEntries.push_back(lfn);
                continue;
            }

            // Bỏ volume label
            if (entry.DIR_Attr & Attr::VOLUME_ID) {
                lfnEntries.clear();
                continue;
            }

            FileInfo info;
            info.name = decodeName(lfnEntries, entry);
            info.isDirectory = (entry.DIR_Attr & Attr::DIRECTORY) != 0;
            info.firstCluster = getFirstCluster(entry);
            info.fileSize = entry.DIR_FileSize;
            info.createdAt = decodeDateTime(entry.DIR_CrtDate, entry.DIR_CrtTime, entry.DIR_CrtTimeTenth);

            // extension
            std::size_t dotPos = info.name.find_last_of('.');
            if (!info.isDirectory && dotPos != std::string::npos && dotPos + 1 < info.name.size()) {
                info.extension = info.name.substr(dotPos + 1);
                std::transform(info.extension.begin(), info.extension.end(), info.extension.begin(),
                               [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
            } else {
                info.extension.clear();
            }

            info.fullPath.clear(); // sẽ được gán ở scanDirectory

            // bỏ "." và ".."
            if (info.isDirectory && (info.name == "." || info.name == "..")) {
                lfnEntries.clear();
                continue;
            }

            results.push_back(info);
            lfnEntries.clear();
        }
    }

    return results;
}

std::vector<FileInfo> DirectoryEntry::findAllTxtFiles(DiskReader& reader,
                                                      const BootSector& boot,
                                                      const FATTable& fat) {
    std::vector<FileInfo> results;

    if (!reader.isOpen() || !boot.isValid() || !fat.isLoaded()) {
        return results;
    }

    scanDirectory(boot.getRootCluster(), "/", reader, boot, fat, results);
    return results;
}

std::vector<uint8_t> DirectoryEntry::readFileContent(const FileInfo& file,
                                                     DiskReader& reader,
                                                     const BootSector& boot,
                                                     const FATTable& fat) {
    std::vector<uint8_t> content;

    if (!reader.isOpen() || !boot.isValid() || !fat.isLoaded()) return content;
    if (file.isDirectory || file.firstCluster < 2) return content;

    std::vector<uint32_t> chain = fat.getClusterChain(file.firstCluster);
    if (chain.empty()) return content;

    const uint32_t clusterSize = boot.getClusterSize();
    std::vector<uint8_t> clusterBuffer(clusterSize);

    for (uint32_t cluster : chain) {
        if (!reader.readCluster(cluster,
                                boot.getSectorsPerCluster(),
                                boot.getDataStartSector(),
                                clusterBuffer.data())) {
            break;
        }

        content.insert(content.end(), clusterBuffer.begin(), clusterBuffer.end());

        if (content.size() >= file.fileSize) {
            break;
        }
    }

    if (file.fileSize == 0) {
        // Some directory entries can report zero size even though the cluster chain
        // still contains text data. In that case, keep a best-effort payload so the
        // GUI can still preview the file content.
        return trimTrailingNulls(std::move(content));
    }

    if (content.size() > file.fileSize) {
        content.resize(file.fileSize);
    }

    return content;
}

// ========================= Private helpers =========================

std::string DirectoryEntry::decodeName(const std::vector<RawLFNEntry>& lfnEntries,
                                       const RawDirEntry& shortEntry) {
    if (!lfnEntries.empty()) {
        std::string longName;

        // LFN được đọc theo thứ tự ngược trong thư mục, nên duyệt reverse để ráp đúng
        for (auto it = lfnEntries.rbegin(); it != lfnEntries.rend(); ++it) {
            longName += utf16leToUtf8(it->LDIR_Name1, 5);
            longName += utf16leToUtf8(it->LDIR_Name2, 6);
            longName += utf16leToUtf8(it->LDIR_Name3, 2);
        }

        if (!longName.empty()) return longName;
    }

    // Fallback 8.3 short name
    std::string base;
    std::string ext;

    for (int i = 0; i < 8; ++i) {
        if (shortEntry.DIR_Name[i] == ' ') break;
        base.push_back(static_cast<char>(shortEntry.DIR_Name[i]));
    }

    for (int i = 8; i < 11; ++i) {
        if (shortEntry.DIR_Name[i] == ' ') break;
        ext.push_back(static_cast<char>(shortEntry.DIR_Name[i]));
    }

    // Trim spaces cuối nếu có
    while (!base.empty() && base.back() == ' ') base.pop_back();
    while (!ext.empty() && ext.back() == ' ') ext.pop_back();

    const bool lowerBase = (shortEntry.DIR_NTRes & 0x08) != 0;
    const bool lowerExt = (shortEntry.DIR_NTRes & 0x10) != 0;
    base = applyShortNameCase(base, lowerBase);
    ext = applyShortNameCase(ext, lowerExt);

    if (!ext.empty()) return base + "." + ext;
    return base;
}

std::string DirectoryEntry::utf16leToUtf8(const uint16_t* chars, int count) {
    std::string result;

    for (int i = 0; i < count; ++i) {
        uint16_t ch = chars[i];

        // 0x0000 = kết thúc chuỗi, 0xFFFF = padding
        if (ch == 0x0000 || ch == 0xFFFF) break;

        // UTF-16 BMP -> UTF-8
        if (ch <= 0x7F) {
            result.push_back(static_cast<char>(ch));
        } else if (ch <= 0x7FF) {
            result.push_back(static_cast<char>(0xC0 | ((ch >> 6) & 0x1F)));
            result.push_back(static_cast<char>(0x80 | (ch & 0x3F)));
        } else {
            result.push_back(static_cast<char>(0xE0 | ((ch >> 12) & 0x0F)));
            result.push_back(static_cast<char>(0x80 | ((ch >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (ch & 0x3F)));
        }
    }

    return result;
}

FATDateTime DirectoryEntry::decodeDateTime(uint16_t fatDate, uint16_t fatTime, uint8_t tenths) {
    FATDateTime dt{0, 0, 0, 0, 0, 0};

    dt.year   = 1980 + ((fatDate >> 9) & 0x7F);
    dt.month  = (fatDate >> 5) & 0x0F;
    dt.day    = fatDate & 0x1F;

    dt.hour   = (fatTime >> 11) & 0x1F;
    dt.minute = (fatTime >> 5) & 0x3F;
    dt.second = (fatTime & 0x1F) * 2;

    // Có thể cộng tenths để làm mịn giây nếu muốn, nhưng ở đây giữ đơn giản
    (void)tenths;

    return dt;
}

uint32_t DirectoryEntry::getFirstCluster(const RawDirEntry& entry) {
    return (static_cast<uint32_t>(entry.DIR_FstClusHI) << 16) |
           static_cast<uint32_t>(entry.DIR_FstClusLO);
}

bool DirectoryEntry::isTxtFile(const RawDirEntry& entry) {
    if (entry.DIR_Attr & Attr::DIRECTORY) return false;
    if (entry.DIR_Attr & Attr::VOLUME_ID) return false;

    char ext[4] = {0};
    ext[0] = static_cast<char>(entry.DIR_Name[8]);
    ext[1] = static_cast<char>(entry.DIR_Name[9]);
    ext[2] = static_cast<char>(entry.DIR_Name[10]);

    for (int i = 0; i < 3; ++i) {
        ext[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(ext[i])));
    }

    return ext[0] == 'T' && ext[1] == 'X' && ext[2] == 'T';
}

bool DirectoryEntry::isValidDirectory(const RawDirEntry& entry) {
    if (!(entry.DIR_Attr & Attr::DIRECTORY)) return false;
    if (entry.DIR_Attr & Attr::VOLUME_ID) return false;
    if (entry.DIR_Name[0] == 0x00 || entry.DIR_Name[0] == 0xE5) return false;

    std::string shortName;
    for (int i = 0; i < 11; ++i) {
        if (entry.DIR_Name[i] == ' ') continue;
        shortName.push_back(static_cast<char>(entry.DIR_Name[i]));
    }

    return shortName != "." && shortName != "..";
}

bool DirectoryEntry::isDeleted(const RawDirEntry& entry) {
    return entry.DIR_Name[0] == 0xE5;
}

bool DirectoryEntry::isEndOfDirectory(const RawDirEntry& entry) {
    return entry.DIR_Name[0] == 0x00;
}

void DirectoryEntry::scanDirectory(uint32_t firstCluster,
                                   const std::string& currentPath,
                                   DiskReader& reader,
                                   const BootSector& boot,
                                   const FATTable& fat,
                                   std::vector<FileInfo>& results) {
    std::vector<FileInfo> entries = readDirectory(firstCluster, reader, boot, fat);

    for (auto& item : entries) {
        // Gán full path
        if (currentPath == "/") {
            item.fullPath = "/" + item.name;
        } else {
            item.fullPath = currentPath + "/" + item.name;
        }

        if (item.isDirectory) {
            if (item.firstCluster >= 2) {
                scanDirectory(item.firstCluster, item.fullPath, reader, boot, fat, results);
            }
        } else {
            // Ưu tiên extension parse từ tên đầy đủ
            std::string ext = item.extension;
            std::transform(ext.begin(), ext.end(), ext.begin(),
                           [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

            if (ext == "TXT") {
                results.push_back(item);
            }
        }
    }
}