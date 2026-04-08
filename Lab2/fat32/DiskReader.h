#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <fcntl.h>
    #include <unistd.h>
#endif

// DiskReader: Mo va doc raw bytes tu thiet bi USB/the nho.
// Khong dung fopen/fstream, doc truc tiep qua handle he dieu hanh.
class DiskReader {
public:
    DiskReader();
    ~DiskReader();

    // Mo thiet bi theo duong dan (Windows: "\\\\.\\E:", Linux: "/dev/sdb").
    bool open(const std::string& drivePath);

    // Dong handle thiet bi.
    void close();

    // Doc dung 1 sector tai vi tri sectorIndex.
    bool readSector(uint64_t sectorIndex, uint8_t* buffer);

    // Doc nhieu sector lien tiep bat dau tu startSector.
    bool readSectors(uint64_t startSector, uint32_t count, uint8_t* buffer);

    // Doc dung 1 cluster bat dau tu clusterNumber.
    bool readCluster(uint32_t clusterNumber,
                     uint8_t sectorsPerCluster,
                     uint64_t dataStartSector,
                     uint8_t* buffer);

    bool isOpen() const;
    uint32_t getBytesPerSector() const;
    std::string getDrivePath() const;

    static std::vector<std::string> listRemovableDrives();

private:
#ifdef _WIN32
    HANDLE _handle;
#else
    int _fd;
#endif
    uint32_t _bytesPerSector;
    std::string _drivePath;
    bool _isOpen;
};
