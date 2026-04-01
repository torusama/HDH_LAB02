#pragma once
#include <cstdint>
#include <vector>
#include "DiskReader.h"
#include "BootSector.h"

// Các giá trị đặc biệt trong FAT32 entry
namespace FAT32 {
    constexpr uint32_t FREE_CLUSTER    = 0x00000000;  // Cluster trống
    constexpr uint32_t BAD_CLUSTER     = 0x0FFFFFF7;  // Cluster lỗi
    constexpr uint32_t END_OF_CHAIN    = 0x0FFFFFF8;  // Kết thúc chuỗi (0xFF8..0xFFF)
    constexpr uint32_t ENTRY_MASK      = 0x0FFFFFFF;  // Mask 28 bit có nghĩa
}

// FATTable: nạp toàn bộ bảng FAT vào RAM và cung cấp API duyệt cluster chain
// Việc nạp 1 lần vào RAM giúp đọc chain nhanh hơn nhiều so với đọc sector từng lần
class FATTable {
public:
    FATTable();

    // Nạp FAT table #0 từ đĩa vào bộ nhớ
    // Phải gọi sau khi BootSector đã load thành công
    bool load(DiskReader& reader, const BootSector& boot);

    // Kiểm tra đã load chưa
    bool isLoaded() const;

    // Lấy giá trị entry tại cluster number (đã mask 28 bit)
    uint32_t getEntry(uint32_t cluster) const;

    // Kiểm tra cluster có phải end-of-chain không
    bool isEndOfChain(uint32_t cluster) const;

    // Kiểm tra cluster có trống không
    bool isFree(uint32_t cluster) const;

    // Kiểm tra cluster có lỗi không
    bool isBad(uint32_t cluster) const;

    // Lấy cluster tiếp theo trong chain (trả về 0 nếu là end-of-chain)
    uint32_t getNextCluster(uint32_t cluster) const;

    // Xây dựng toàn bộ cluster chain bắt đầu từ firstCluster
    // Trả về vector theo thứ tự: [firstCluster, next, next, ..., lastCluster]
    // Dùng để đọc nội dung file hoặc duyệt thư mục nhiều cluster
    std::vector<uint32_t> getClusterChain(uint32_t firstCluster) const;

    // Tổng số cluster entries trong FAT
    uint32_t getTotalEntries() const;

private:
    std::vector<uint32_t> _entries;  // Toàn bộ FAT entries trong RAM
    bool                  _loaded;
};
