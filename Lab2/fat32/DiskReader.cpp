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

bool DiskReader::open(string& _drivePath) {
    if (_isOpen) {
        close();
    }
    else {
        if (_drivePath.length() > 4) {
           if(_drivePath.substr(0,4) == "\\\\.\\") {

           }
           else {
            _drivePath = string("\\\\.\\") + _drivePath;
           }
        }
        else {
            _drivePath = string("\\\\.\\") + _drivePath;
        }
    _handle = CreateFileA(_drivePath.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);
    if (_handle == INVALID_HANDLE_VALUE) {
        _isOpen = false;
        return false;
    }
    else {
        _isOpen = true;
        DWORD sectorsPerCluster;
        DWORD bytesPerSector;
        BOOL a = GetDiskFreeSpaceA(_drivePath.c_str(),&sectorsPerCluster,&bytesPerSector,NULL,NULL);
        _bytesPerSector = bytesPerSector;
    }
    }
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
    for (int i = 0; i < count - 1; i++) {
        if(!readSector(startSector + i,buffer + i * _bytesPerSector)) return false;
    }
    return true;
}

bool DiskReader::readCluster(uint32_t clusterNumber,uint8_t  sectorsPerCluster,uint64_t dataStartSector,uint8_t* buffer) { //Cluster số 3,8 sector mỗi clus,startSecctor = 100
    if (!_isOpen || buffer == NULL || clusterNumber <= 2 || sectorsPerCluster <= 0) return false;
    LARGE_INTEGER startSectorOfCluster;
    startSectorOfCluster.QuadPart = (clusterNumber - 2) * sectorsPerCluster + dataStartSector;//(3-2)*8 + 100 = 108
    bool check = readSectors(startSectorOfCluster.QuadPart,sectorsPerCluster,buffer);
    if(!check) return false;
    return true;
}

bool DiskReader::isOpen() {
    if(!isOpen) return false;
    return true;
}

uint32_t DiskReader::getBytesPerSector() {
    return _bytesPerSector;
}

string DiskReader::getDrivePath(){ 
    return _drivePath;
}