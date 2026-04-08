#include "FATTable.h"

// Khởi tạo bảng FAT, ban đầu chưa load bảng FAT => _loaded = false
FATTable::FATTable() : _loaded(false) {}

bool FATTable::load(DiskReader& reader, const BootSector& boot)
{
    _loaded = false; // Chưa chắc bảng FAT load thành công nên cứ reset = false trc
    // _entries là mảng chứa toàn bộ bảng FAT sau khi đọc vào RAM.
    _entries.clear(); // Khi load lại FAT, ta xóa dữ liệu cũ

    // Check ổ đĩa phải đang mở, Boot Sector phải hợp lệ
    if (!reader.isOpen() || !boot.isValid()) return false;

    uint32_t fatSectors = boot.getSectorsPerFAT(); // đọc số sector của bảng FAT
    uint32_t bytesPerSector = boot.getBytesPerSector(); // đọc số byte của mỗi sector
    uint64_t fatStartSector = boot.getFATStartSector(); // đọc sector đầu tiên của FAT

    // tính kích thước FAT theo byte = số sector của FAT * số byte của mỗi sector
    // Tại sao ép kiểu uint64_t? Để tránh tràn số nếu FAT khá lớn.
    uint64_t fatBytes = static_cast<uint64_t>(fatSectors) * bytesPerSector;

    // Nếu bằng 0 thì chứng tỏ thông tin Boot Sector bất thường hoặc chưa load đúng.
    if (fatBytes == 0) return false;

    // Tạo buffer để chứa toàn bộ FAT, mỗi phần tử là 1 byte, và tổng số phần tử là fatBytes
    // Tạo một mảng byte trong RAM đủ lớn để chứa nguyên bảng FAT.
    // uint8_t: số nguyên k âm 8 bit = 1 byte
    std::vector<uint8_t> buffer(static_cast<size_t>(fatBytes));

    // reader đọc bảng FAT từ USB vào buffer
    // nếu đọc thất bại thì báo false luôn
    if (!reader.readSectors(fatStartSector, fatSectors, buffer.data())) {
        // bắt đầu đọc từ sector fatStartSector
        // đọc fatSectors sector
        // ghi dữ liệu vào buffer
        return false;
    }

    // Tính xem bảng FAT có tổng cộng bao nhiêu entry
    // 1 FAT entry chiếm 4 byte
    // mỗi FAT entry thì thường được lưu bằng uint32_t
    uint32_t totalEntries = static_cast<uint32_t>(fatBytes / 4);

    // vector _entries có đúng totalEntries phần tử.
    _entries.resize(totalEntries);

    for (uint32_t i = 0; i < totalEntries; ++i) {
        uint32_t value = // little endian, 1 entry = 4 byte => 4 ptu buffer
            static_cast<uint32_t>(buffer[i * 4]) | // lấy buffer 1
            (static_cast<uint32_t>(buffer[i * 4 + 1]) << 8) | // lấy buffer 2, dịch trái 8 bit
            (static_cast<uint32_t>(buffer[i * 4 + 2]) << 16) |
            (static_cast<uint32_t>(buffer[i * 4 + 3]) << 24);
            // 1 entry = 4 ptu buffer => entry thứ i bắt đầu từ ptu buffer thứ i * 4
            // value = OR lại 4 thằng buffer này => ghép lại thành số 32 bit
        _entries[i] = value & FAT32::ENTRY_MASK; // gán entry thứ i = value and với ENTRY_MASK: 0x0FFFFFFF
        // entry này tuy lưu trên 32 bit, nhưng chỉ 28 bit thấp là phần dữ liệu cluster có nghĩa cho bài toán này
    }

    _loaded = true;
    return true;
}

// hàm check coi bảng FAT đã đc load vào RAM hay chưa
bool FATTable::isLoaded() const
{
    return _loaded;
}

// hỏi bảng FAT xem cluster này đang trỏ tới đâu
uint32_t FATTable::getEntry(uint32_t cluster) const
{
    // nếu bảng FAT chưa load xong or 
    // cluster m hỏi vượt quá số entry hiện có trong bảng FAT
    if (!_loaded || cluster >= _entries.size()) return 0;

    return _entries[cluster] & FAT32::ENTRY_MASK; // Đây là entry đã parse của cluster đó.
}

// Kiểm tra xem giá trị FAT entry này có phải là kết thúc chuỗi (end of chain) không.
bool FATTable::isEndOfChain(uint32_t cluster) const
{
    // cluster nằm trong khoảng 0x0FFFFFF8 đến 0x0FFFFFFF được coi là đã tới cuối chain.
    return cluster >= FAT32::END_OF_CHAIN && cluster <= 0x0FFFFFFF;
}

// Kiểm tra xem FAT entry này có phải là cluster trống không.
bool FATTable::isFree(uint32_t cluster) const
{
    // Nếu entry = 0, nghĩa là cluster đó chưa được cấp phát cho file/thư mục nào.
    return cluster == FAT32::FREE_CLUSTER;
}

// Kiểm tra xem entry này có đánh dấu cluster lỗi / bad cluster không.
bool FATTable::isBad(uint32_t cluster) const
{
    return cluster == FAT32::BAD_CLUSTER;
}

// Tra cluster tiếp theo
uint32_t FATTable::getNextCluster(uint32_t cluster) const
{
    if (!_loaded) return 0; // chưa load xong k tra đc

    uint32_t value = getEntry(cluster);
    // lấy giá trị entry FAT tại vị trí cluster
    // vd getEntry(5) = 12 => cluster[5] đang trỏ tới cluster[12]
    
    if (isEndOfChain(value) || isBad(value) || isFree(value))
    {
        return 0;
    }

    return value;
}

// bắt đầu từ firstCluster, lần theo bảng FAT 
// để lấy ra toàn bộ danh sách cluster thuộc về file/thư mục đó.
std::vector<uint32_t> FATTable::getClusterChain(uint32_t firstCluster) const
{
    std::vector<uint32_t> chain;

    // Trong FAT32, cluster có số nhỏ hơn 2 
    // thường không phải data cluster hợp lệ để dùng cho file thường.
    if (!_loaded || firstCluster < 2) return chain;

    uint32_t current = firstCluster; // cluster đang xét tại mỗi bước.
    uint32_t guard = 0; // đếm số lần lặp

    // giới hạn tối đa số lần lặp = số entry trong bảng FAT
    // phòng trường hợp bảng FAT bị lỗi, tạo vòng lặp kiểu:
    // 5 -> 8 -> 12 -> 8 -> 12 -> 8 -> ...
    uint32_t maxGuard = static_cast<uint32_t>(_entries.size());

    while (current >= 2 && current < _entries.size() && guard < maxGuard)
    {
        // Mỗi lần đến 1 cluster, ta nhét nó vào danh sách kết quả.
        chain.push_back(current);

        // Lấy cluster kế tiếp
        // current = 5
        // getEntry(5) = 12
        // nghĩa là cluster tiếp theo là 12
        uint32_t next = getEntry(current);

        // Nếu next là:
        // End of Chain → tới cuối file rồi
        // Bad Cluster → vùng lỗi
        // Free Cluster → cluster trống, không đi tiếp được
        if (isEndOfChain(next) || isBad(next) || isFree(next)) {
            break;
        }

        current = next;
        ++guard;
    }

    return chain;
}

// chỉ trả về số phần tử hiện có trong vector _entries
uint32_t FATTable::getTotalEntries() const
{
    return static_cast<uint32_t>(_entries.size());
}