#include "BootSector.h"

//1. reset _valid = false
//2. kiểm tra reader.isOpen()
//3. tạo buffer đủ 1 sector
//4. reader.readSector(0, buffer)
//5. copy buffer vào _raw
//6. validate một số field quan trọng
//7. nếu ổn -> _valid = true, return true
//8. nếu không -> return false
bool BootSector::load(DiskReader& reader) {
    _valid = false;
    if (!reader.isOpen()) return false;
    uint8_t* buffer = new uint8_t[reader.getBytesPerSector()];
    if(!reader.readSector(0,buffer)){
        delete[] buffer;
        return false;
    } 
    memcpy(&_raw,buffer,sizeof(BootSectorRaw));
    delete []buffer;
    if(_raw.BPB_BytsPerSec <= 0) return false;
    if(_raw.BPB_SecPerClus <= 0) return false;
    if(_raw.BPB_RsvdSecCnt <= 0) return false;
    if(_raw.BPB_NumFATs <= 0) return false;
    if(_raw.BPB_FATSz32 <= 0) return false;
    if(_raw.BPB_TotSec32 <= 0) return false;
    if(_raw.BPB_RootClus < 2) return false;
    _valid = true;
    return true;
}

BootSector::BootSector(){
    _valid = false;
};

bool BootSector::isValid() const {
    if (!_valid) return false;
    return true;
}

uint16_t BootSector::getBytesPerSector() const {
    return _raw.BPB_BytsPerSec;
}

uint8_t BootSector::getSectorsPerCluster() const {
    return _raw.BPB_SecPerClus;
}

uint16_t BootSector::getReservedSectors() const {
    return _raw.BPB_RsvdSecCnt;
}

uint8_t BootSector::getNumFATs() const {
    return _raw.BPB_NumFATs;
}

uint32_t BootSector::getSectorsPerFAT() const {
    return _raw.BPB_FATSz32;
}

uint32_t BootSector::getTotalSectors() const {
    return _raw.BPB_TotSec32;
}

uint32_t BootSector::getRootCluster() const {
    return _raw.BPB_RootClus;
}

uint32_t BootSector::getRDETSectors() const {
    return 0;
}

 uint64_t BootSector::getFATStartSector() const {
    return _raw.BPB_RsvdSecCnt;
 }

 uint64_t BootSector::getDataStartSector() const {
    return _raw.BPB_RsvdSecCnt + (_raw.BPB_FATSz32 * _raw.BPB_NumFATs);
 }

 uint32_t BootSector::getClusterSize() const {
    return _raw.BPB_SecPerClus * _raw.BPB_BytsPerSec;
 }

 uint64_t BootSector::clusterToSector(uint32_t cluster) const {
    uint32_t DataStartSector = _raw.BPB_NumFATs * _raw.BPB_FATSz32 + _raw.BPB_RsvdSecCnt;
    return (cluster - 2) * _raw.BPB_SecPerClus + DataStartSector;
 }

 const BootSectorRaw& BootSector::getRaw() const {
    return _raw;
 }