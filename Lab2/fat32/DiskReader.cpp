#include "DiskReader.h"

void DiskReader::close() {
    if (_isOpen) {
        CloseHandle(_handle);
        _handle = INVALID_HANDLE_VALUE;
    }
    _drivePath = "";
    _bytesPerSector = 0;
    _isOpen = false;
}

DiskReader::DiskReader() {
    _handle = INVALID_HANDLE_VALUE;
    _bytesPerSector = 0;
    _drivePath = "";
    _isOpen = false;
}

DiskReader::~DiskReader() {
    if (_isOpen == true) {
        close();
    }
}

bool DiskReader::open(const std::string& drivePath) {
    if (_isOpen) {
        close();
    }

    // raw path cho CreateFileA, ví dụ "\\\\.\\E:"
    std::string rawPath = drivePath;
    if (rawPath.rfind("\\\\.\\", 0) != 0) {
        rawPath = std::string("\\\\.\\") + rawPath;
    }

    _handle = CreateFileA(rawPath.c_str(),
                          GENERIC_READ,
                          FILE_SHARE_READ | FILE_SHARE_WRITE,
                          NULL,
                          OPEN_EXISTING,
                          0,
                          NULL);

    if (_handle == INVALID_HANDLE_VALUE) {
        _isOpen = false;
        return false;
    }

    _isOpen = true;
    _drivePath = rawPath;

    // volume path cho GetDiskFreeSpaceA, ví dụ "E:\\"
    std::string volumePath = drivePath;
    if (volumePath.rfind("\\\\.\\", 0) == 0) {
        volumePath = volumePath.substr(4); // từ "\\\\.\\E:" thành "E:"
    }
    if (volumePath.size() == 2 && volumePath[1] == ':') {
        volumePath += "\\";
    }

    DWORD sectorsPerCluster = 0;
    DWORD bytesPerSector = 0;
    BOOL ok = GetDiskFreeSpaceA(volumePath.c_str(),
                                &sectorsPerCluster,
                                &bytesPerSector,
                                NULL,
                                NULL);

    if (!ok) {
        close();
        return false;
    }

    _bytesPerSector = bytesPerSector;
    return true;
}

bool DiskReader::readSector(uint64_t sectorIndex, uint8_t* buffer) {
    if (!_isOpen || buffer == NULL) return false;
    LARGE_INTEGER Offset;
    Offset.QuadPart = (LONGLONG)sectorIndex * _bytesPerSector;
    BOOL a = SetFilePointerEx(_handle,Offset,NULL,FILE_BEGIN);
    if(!a) return false;
    else {
        DWORD bytesRead;
        BOOL b = ReadFile(_handle,buffer,_bytesPerSector,&bytesRead,NULL);
        if (!b || bytesRead != _bytesPerSector) return false;
        return true;
    }
}

bool DiskReader::readSectors(uint64_t startSector, uint32_t count, uint8_t* buffer) {
    if (!_isOpen || count <= 0 || buffer == NULL) return false;
    LARGE_INTEGER Offset,totalBytes;
    Offset.QuadPart = (LONGLONG)startSector * _bytesPerSector;
    for (uint32_t i = 0; i < count; i++) {
        if(!readSector(startSector + i,buffer + i * _bytesPerSector)) return false;
    }
    return true;
}

bool DiskReader::readCluster(uint32_t clusterNumber,uint8_t  sectorsPerCluster,uint64_t dataStartSector,uint8_t* buffer) { //Cluster số 3,8 sector mỗi clus,startSecctor = 100
    if (!_isOpen || buffer == NULL || clusterNumber < 2 || sectorsPerCluster == 0) return false;
    LARGE_INTEGER startSectorOfCluster;
    startSectorOfCluster.QuadPart = (clusterNumber - 2) * sectorsPerCluster + dataStartSector;//(3-2)*8 + 100 = 108
    bool check = readSectors(startSectorOfCluster.QuadPart,sectorsPerCluster,buffer);
    if(!check) return false;
    return true;
}

bool DiskReader::isOpen() const {
    return _isOpen;
}

uint32_t DiskReader::getBytesPerSector() const {
    return _bytesPerSector;
}

std::string DiskReader::getDrivePath() const {
    return _drivePath;
}
