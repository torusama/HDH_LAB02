#pragma once
#include <cstdint>
#include <string>
#include <cstring>
#include "DiskReader.h"

// Struct ánh xạ trực tiếp lên 512 bytes đầu của đĩa FAT32 (BPB - BIOS Parameter Block)
// Các trường được đặt đúng thứ tự theo chuẩn FAT32 specification
// Dùng #pragma pack để tránh compiler tự thêm padding bytes
#pragma pack(push, 1)
struct BootSectorRaw {
    uint8_t  BS_jmpBoot[3];      // 0x00 - Jump instruction
    uint8_t  BS_OEMName[8];      // 0x03 - OEM Name

    // -- BPB (BIOS Parameter Block) --
    uint16_t BPB_BytsPerSec;     // 0x0B - mỗi sector bao nhiu byte
    uint8_t  BPB_SecPerClus;     // 0x0D - số sector của 1 cluster
    uint16_t BPB_RsvdSecCnt;     // 0x0E - ★ Số sector vùng Reserved (Boot Sector region)
    uint8_t  BPB_NumFATs;        // 0x10 - số lượng bảng FAT
    uint16_t BPB_RootEntCnt;     // 0x11 - số entry của thư mục gốc
    uint16_t BPB_TotSec16;       // 0x13 - Tổng sector (FAT32 = 0, dùng TotSec32)
    uint8_t  BPB_Media;          // 0x15 - Media type
    uint16_t BPB_FATSz16;        // 0x16 - số sector mỗi bảng FAT
    uint16_t BPB_SecPerTrk;      // 0x18 - số sector mỗi track
    uint16_t BPB_NumHeads;       // 0x1A - Number of heads
    uint32_t BPB_HiddSec;        // 0x1C - Hidden sectors
    uint32_t BPB_TotSec32;       // 0x20 - ★ Tổng số sector trên đĩa

    // -- FAT32 Extended BPB --
    uint32_t BPB_FATSz32;        // 0x24 - ★ Số sector mỗi bảng FAT
    uint16_t BPB_ExtFlags;       // 0x28 - Flags
    uint16_t BPB_FSVer;          // 0x2A - Phiên bản FAT32
    uint32_t BPB_RootClus;       // 0x2C - Cluster đầu tiên của Root Directory
    uint16_t BPB_FSInfo;         // 0x30 - Sector FSInfo
    uint16_t BPB_BkBootSec;      // 0x32 - Sector backup Boot
    uint8_t  BPB_Reserved[12];   // 0x34 - Reserved

    uint8_t  BS_DrvNum;          // 0x40 - Drive number
    uint8_t  BS_Reserved1;       // 0x41
    uint8_t  BS_BootSig;         // 0x42 - Boot signature (0x29)
    uint32_t BS_VolID;           // 0x43 - Volume serial number
    uint8_t  BS_VolLab[11];      // 0x47 - Volume label
    uint8_t  BS_FilSysType[8];   // 0x52 - "FAT32   "
};
#pragma pack(pop)

// BootSector: wrapper gọn gàng cho BootSectorRaw,
// cung cấp các giá trị đã tính sẵn dùng trong toàn bộ chương trình
class BootSector {
public:
    BootSector();

    // Đọc và parse Boot Sector từ DiskReader
    // Trả về false nếu đọc thất bại hoặc không phải FAT32
    bool load(DiskReader& reader);

    // Kiểm tra đã load thành công chưa
    bool isValid() const; // const để cho nó chỉ đc đọc

    // ─── Getters cho Chức năng 1 (hiển thị bảng) ───────────────────────────

    uint16_t getBytesPerSector() const;  // BPB_BytsPerSec
    uint8_t  getSectorsPerCluster() const;  // BPB_SecPerClus
    uint16_t getReservedSectors() const;  // BPB_RsvdSecCnt  (Boot Sector region)
    uint8_t  getNumFATs() const;  // BPB_NumFATs
    uint32_t getSectorsPerFAT() const;  // BPB_FATSz32
    uint32_t getTotalSectors() const;  // BPB_TotSec32
    uint32_t getRootCluster() const;  // BPB_RootClus

    // Số sector của vùng RDET (Root Dir)
    // FAT32: root nằm trong Data region, bắt đầu tại RootClus
    // = SectorsPerCluster (tính cho 1 cluster, có thể mở rộng theo chain)
    uint32_t getRDETSectors() const;

    // ─── Giá trị tính sẵn (dùng nội bộ cho FATTable, DirectoryEntry) ────────

    // Sector đầu tiên của FAT table 1
    uint64_t getFATStartSector() const;  // = RsvdSecCnt

    // Sector đầu tiên của vùng Data
    uint64_t getDataStartSector() const;  // = RsvdSecCnt + NumFATs * FATSz32

    // Kích thước 1 cluster tính bằng byte
    uint32_t getClusterSize() const;  // = BytsPerSec * SecPerClus

    // Convert cluster number → sector number
    uint64_t clusterToSector(uint32_t cluster) const;

    // Trả về raw struct (dùng khi cần truy cập trường ít dùng)
    const BootSectorRaw& getRaw() const;

private:
    BootSectorRaw _raw;
    bool          _valid;
};
