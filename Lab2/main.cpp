#include <iostream>
#include "fat32/DiskReader.h"
#include "fat32/BootSector.h"
#include "fat32/FATTable.h"
#include "fat32/DirectoryEntry.h"

// TODO: include GUI header khi đã chọn framework
// #include "gui/MainWindow.h"

// Gợi ý luồng khởi động chương trình:
//
// 1. Khởi tạo GUI (Qt: QApplication, wxWidgets: wxApp, ...)
// 2. Hiển thị MainWindow
// 3. Người dùng chọn ổ đĩa → gọi DiskReader::open()
// 4. Gọi BootSector::load()  → hiển thị Chức năng 1
// 5. Gọi FATTable::load()
// 6. Gọi DirectoryEntry::findAllTxtFiles() → hiển thị Chức năng 2
// 7. Người dùng chọn file → đọc nội dung + parse → Chức năng 3
// 8. Chạy Scheduler → vẽ Gantt chart → Chức năng 4

int main(int argc, char* argv[]) {
    // TODO: Thay bằng khởi tạo GUI thực tế
    std::cout << "Lab 2 - FAT32 Reader\n";

    // Ví dụ test không GUI (dòng lệnh):
    if (argc < 2) {
        std::cout << "Usage: lab2.exe <drive_path>\n";
        std::cout << "  Windows: lab2.exe \\\\.\\E:\n";
        std::cout << "  Linux:   lab2.exe /dev/sdb\n";
        return 1;
    }

    DiskReader reader;
    if (!reader.open(argv[1])) {
        std::cerr << "Cannot open drive: " << argv[1] << "\n";
        std::cerr << "Make sure you run as Administrator.\n";
        return 1;
    }

    BootSector boot;
    if (!boot.load(reader)) {
        std::cerr << "Cannot read Boot Sector. Is this FAT32?\n";
        return 1;
    }

    FATTable fat;
    if (!fat.load(reader, boot)) {
        std::cerr << "Cannot load FAT table.\n";
        return 1;
    }

    DirectoryEntry dirScanner;
    auto txtFiles = dirScanner.findAllTxtFiles(reader, boot, fat);

    std::cout << "Found " << txtFiles.size() << " .txt files.\n";
    for (auto& f : txtFiles)
        std::cout << "  " << f.fullPath << " (" << f.fileSize << " bytes)\n";

    return 0;
}
