# LAB 02 - FAT32 Schedule Explorer

## Team Members

- Vo Tan An - 24127318
- Doan Vo Ngoc Lam - 24127435

## Overview

This project is a Windows GUI application for reading and displaying FAT32 information directly from a USB drive or disk partition formatted as FAT32.

The application currently focuses on:

- Reading and displaying Boot Sector information
- Listing all `.txt` files across the entire FAT32 volume, including subdirectories
- Showing detailed information for a selected `.txt` file
- Parsing scheduling input files and displaying the process table

## Implemented Features

### Function 1: Display Boot Sector Information

The program shows the following FAT32 Boot Sector information in table format:

- Bytes per sector
- Sectors per cluster
- Number of sectors in the Boot Sector region
- Number of FAT tables
- Number of sectors per FAT table
- Number of sectors for the RDET
- Total number of sectors on the disk

### Function 2: List All `.txt` Files

- Scans the FAT32 volume recursively
- Lists all `.txt` files, including files inside subdirectories
- Displays the result as a flat list, without showing the directory tree

### Function 3: View File Details

When a `.txt` file is selected, the application displays:

- File name
- Full path
- Creation date
- Creation time
- Total file size
- File content preview
- Process information table:
  - Process ID
  - Arrival Time
  - CPU Burst Time
  - Queue ID
  - Time Slice
  - Scheduling Algorithm Name

## Current Limitations

At the current stage, the source code does not yet implement:

- Scheduling diagram drawing
- Turnaround Time calculation
- Waiting Time calculation

If your submission only focuses on the FAT32 GUI reading and display requirements, this README reflects the current source code accurately.

## Project Structure

```text
Lab2/
|-- main_gui.cpp
|-- README.md
|-- fat32/
|   |-- BootSector.h
|   |-- BootSector.cpp
|   |-- DiskReader.h
|   |-- DiskReader.cpp
|   |-- FATTable.h
|   |-- FATTable.cpp
|   |-- DirectoryEntry.h
|   |-- DirectoryEntry.cpp
```

## Environment

- Windows
- A USB drive or disk partition formatted as FAT32
- `g++` with WinAPI support, such as MSYS2 MinGW UCRT64

## Build Instructions

Open PowerShell in the project directory and run:

```powershell
g++ main_gui.cpp fat32/BootSector.cpp fat32/DiskReader.cpp fat32/FATTable.cpp fat32/DirectoryEntry.cpp scheduler/Process.cpp scheduler/Queue.cpp scheduler/Scheduler.cpp -std=c++17 -mwindows -lcomctl32 -o lab2_gui.exe
```

If you get this error:

```text
cannot open output file lab2_gui.exe: Permission denied
```

it usually means `lab2_gui.exe` is still running. Close the application first, or run:

```powershell
Get-Process lab2_gui -ErrorAction SilentlyContinue | Stop-Process -Force
```

and then build again.

## Run Instructions

### Option 1: Run normally

```powershell
.\lab2_gui.exe
```

### Option 2: Start with a preselected drive

```powershell
.\lab2_gui.exe E:
```

## How To Use

1. Run the application, preferably as Administrator if needed.
2. Select a FAT32 drive from the combo box.
3. Click `Scan`.
4. View Boot Sector information on the left panel.
5. Select a `.txt` file from the file list.
6. View:
   - file details
   - file content
   - scheduling process table

## Important Notes

- The selected drive should be formatted as FAT32.
- The program reads FAT32 data directly from the device, so Administrator permission may be required.
- If a `.txt` file does not match the expected scheduling format, the application may still display the file content, but the process table can remain empty.
- If FAT32 metadata is inconsistent, the application includes a best-effort fallback to recover text content for preview.

## Source Notes

- The GUI entry point is in `main_gui.cpp`.
- FAT32-related logic is implemented inside the `fat32/` folder.

## Quick Build And Run Example

```powershell
g++ main_gui.cpp fat32/BootSector.cpp fat32/DiskReader.cpp fat32/FATTable.cpp fat32/DirectoryEntry.cpp -std=c++17 -mwindows -lcomctl32 -o lab2_gui.exe
.\lab2_gui.exe
```
