#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <cstring>
using namespace std;

#ifdef _WIN32
    #include <windows.h>
#else
    #include <fcntl.h>
    #include <unistd.h>
#endif

// DiskReader: Mở và đọc raw bytes từ thiết bị USB/thẻ nhớ
// Không dùng fopen/fstream — đọc trực tiếp qua handle hệ điều hành
// Yêu cầu quyền Administrator trên Windows
class DiskReader {
public:
    DiskReader();
    ~DiskReader();

    // Mở thiết bị theo đường dẫn (Windows: "\\\\.\\E:", Linux: "/dev/sdb")
    // Trả về true nếu mở thành công
    bool open(string& drivePath);

    // Đóng handle thiết bị
    void close();

    // Đọc đúng 1 sector (512 bytes) tại vị trí sectorIndex
    // buffer phải được cấp phát sẵn ít nhất getBytesPerSector() bytes
    bool readSector(uint64_t sectorIndex, uint8_t* buffer);

    // Đọc nhiều sector liên tiếp bắt đầu từ startSector
    bool readSectors(uint64_t startSector, uint32_t count, uint8_t* buffer);

    // Đọc đúng 1 cluster bắt đầu từ clusterNumber
    // Cần biết sectorsPerCluster và dataStartSector (lấy từ BootSector)
    bool readCluster(uint32_t clusterNumber,uint8_t  sectorsPerCluster,uint64_t dataStartSector,uint8_t* buffer);

    // Kiểm tra thiết bị đã mở chưa
    bool isOpen();

    // Trả về kích thước sector (thường 512, đọc từ thiết bị)
    uint32_t getBytesPerSector();

    // Lấy đường dẫn đang mở (dùng để hiển thị trên GUI)
    string getDrivePath();

    // Liệt kê các ổ đĩa removable trên hệ thống (dùng để populate combobox GUI)
    // Trả về list đường dẫn, vd: ["E:", "F:"] trên Windows
    static vector<std::string> listRemovableDrives(); //->Hàm này dùng cho GUI

private:
#ifdef _WIN32
    HANDLE  _handle;
#else
    int     _fd;
#endif
    uint32_t    _bytesPerSector;
    string _drivePath;
    bool        _isOpen;
};
