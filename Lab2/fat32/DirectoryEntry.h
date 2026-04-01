#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "DiskReader.h"
#include "BootSector.h"
#include "FATTable.h"

// ─── Raw structs (32 bytes mỗi entry) ────────────────────────────────────────

// Attribute bits (byte 0x0B trong entry)
namespace Attr {
    constexpr uint8_t READ_ONLY  = 0x01;
    constexpr uint8_t HIDDEN     = 0x02;
    constexpr uint8_t SYSTEM     = 0x04;
    constexpr uint8_t VOLUME_ID  = 0x08;
    constexpr uint8_t DIRECTORY  = 0x10;
    constexpr uint8_t ARCHIVE    = 0x20;
    constexpr uint8_t LFN        = 0x0F;  // Long File Name entry (READ_ONLY|HIDDEN|SYSTEM|VOLUME_ID)
}

#pragma pack(push, 1)
// Standard 8.3 Directory Entry (32 bytes)
struct RawDirEntry {
    uint8_t  DIR_Name[11];       // 0x00 - Tên 8.3 (8 tên + 3 extension), padded bằng space
    uint8_t  DIR_Attr;           // 0x0B - Attribute
    uint8_t  DIR_NTRes;          // 0x0C - Reserved
    uint8_t  DIR_CrtTimeTenth;   // 0x0D - Phần trăm giây khi tạo (0-199)
    uint16_t DIR_CrtTime;        // 0x0E - ★ Giờ tạo (bit: 15-11=hour, 10-5=min, 4-0=sec/2)
    uint16_t DIR_CrtDate;        // 0x10 - ★ Ngày tạo (bit: 15-9=year+1980, 8-5=month, 4-0=day)
    uint16_t DIR_LstAccDate;     // 0x12 - Ngày truy cập lần cuối
    uint16_t DIR_FstClusHI;      // 0x14 - High 16 bit của first cluster
    uint16_t DIR_WrtTime;        // 0x16 - Giờ sửa lần cuối
    uint16_t DIR_WrtDate;        // 0x18 - Ngày sửa lần cuối
    uint16_t DIR_FstClusLO;      // 0x1A - Low 16 bit của first cluster
    uint32_t DIR_FileSize;       // 0x1C - ★ Kích thước file (byte)
};

// Long File Name Entry (cũng 32 bytes, nhận diện khi Attr == LFN)
struct RawLFNEntry {
    uint8_t  LDIR_Ord;           // 0x00 - Thứ tự (bit 6 = 1 nếu là entry cuối trong LFN sequence)
    uint16_t LDIR_Name1[5];      // 0x01 - 5 ký tự UTF-16LE đầu
    uint8_t  LDIR_Attr;          // 0x0B - = 0x0F (LFN marker)
    uint8_t  LDIR_Type;          // 0x0C - = 0
    uint8_t  LDIR_Chksum;        // 0x0D - Checksum của tên 8.3
    uint16_t LDIR_Name2[6];      // 0x0E - 6 ký tự UTF-16LE tiếp
    uint16_t LDIR_FstClusLO;     // 0x1A - = 0
    uint16_t LDIR_Name3[2];      // 0x1C - 2 ký tự UTF-16LE cuối
};
#pragma pack(pop)

// ─── Parsed structs (dùng trong chương trình) ─────────────────────────────────

// Thời gian đã decode từ FAT format
struct FATDateTime {
    int year, month, day;
    int hour, minute, second;

    std::string toDateString() const;  // "DD/MM/YYYY"
    std::string toTimeString() const;  // "HH:MM:SS"
};

// Thông tin đầy đủ của 1 file/thư mục sau khi parse
struct FileInfo {
    std::string  name;           // Tên đầy đủ (ưu tiên LFN nếu có, fallback 8.3)
    std::string  extension;      // Extension (không có dấu chấm)
    bool         isDirectory;
    uint32_t     firstCluster;   // Cluster bắt đầu (để đọc nội dung)
    uint32_t     fileSize;       // Kích thước byte
    FATDateTime  createdAt;      // ★ Ngày/giờ tạo (Chức năng 3)
    std::string  fullPath;       // Đường dẫn đầy đủ vd: "/docs/schedule.txt"
};

// ─── DirectoryEntry class ─────────────────────────────────────────────────────

// DirectoryEntry: đọc và duyệt các entry trong 1 thư mục,
// hỗ trợ cả 8.3 short name và Long File Name (LFN)
class DirectoryEntry {
public:
    DirectoryEntry();

    // Đọc tất cả entries trong 1 thư mục bắt đầu tại firstCluster
    // Tự động theo cluster chain qua FATTable nếu thư mục nhiều cluster
    // Trả về list file/folder trong thư mục đó (không bao gồm "." và "..")
    std::vector<FileInfo> readDirectory(uint32_t          firstCluster,
                                        DiskReader&        reader,
                                        const BootSector&  boot,
                                        const FATTable&    fat);

    // Tìm kiếm đệ quy toàn bộ đĩa, trả về tất cả file .txt
    // Bắt đầu từ Root Cluster (boot.getRootCluster())
    // Chức năng 2: không hiển thị cây thư mục, chỉ lấy danh sách phẳng
    std::vector<FileInfo> findAllTxtFiles(DiskReader&        reader,
                                          const BootSector&  boot,
                                          const FATTable&    fat);

    // Đọc nội dung thô của 1 file theo cluster chain
    // Trả về vector<uint8_t> chứa toàn bộ bytes của file
    // Dùng cho Chức năng 3: parse nội dung file .txt
    std::vector<uint8_t> readFileContent(const FileInfo&    file,
                                         DiskReader&        reader,
                                         const BootSector&  boot,
                                         const FATTable&    fat);

private:
    // Decode tên từ các RawLFNEntry tích lũy + RawDirEntry 8.3
    // LFN entries được lưu theo thứ tự ngược (entry có Ord cao nhất đứng trước)
    std::string decodeName(const std::vector<RawLFNEntry>& lfnEntries,
                           const RawDirEntry&              shortEntry);

    // Convert UTF-16LE (5/6/2 chars trong LFN) sang UTF-8 string
    std::string utf16leToUtf8(const uint16_t* chars, int count);

    // Decode ngày giờ FAT → FATDateTime
    FATDateTime decodeDateTime(uint16_t fatDate, uint16_t fatTime, uint8_t tenths);

    // Lấy first cluster từ RawDirEntry (ghép FstClusHI và FstClusLO)
    uint32_t getFirstCluster(const RawDirEntry& entry);

    // Kiểm tra entry có phải file .txt không (extension == "TXT")
    bool isTxtFile(const RawDirEntry& entry);

    // Kiểm tra entry có phải thư mục hợp lệ không (không phải "." hay "..")
    bool isValidDirectory(const RawDirEntry& entry);

    // Kiểm tra entry đã xóa (byte đầu == 0xE5) hay hết entries (byte đầu == 0x00)
    bool isDeleted(const RawDirEntry& entry);
    bool isEndOfDirectory(const RawDirEntry& entry);

    // Hàm đệ quy nội bộ cho findAllTxtFiles
    void scanDirectory(uint32_t           firstCluster,
                       const std::string& currentPath,
                       DiskReader&        reader,
                       const BootSector&  boot,
                       const FATTable&    fat,
                       std::vector<FileInfo>& results);
};
