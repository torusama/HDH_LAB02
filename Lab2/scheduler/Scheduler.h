#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <string>
#include <vector>
#include <fstream>
#include "Queue.h"
#include "Process.h"
#include <iomanip>
using namespace std;

// Struct để lưu timeline (Gantt chart)
struct ScheduleEvent {
    int StartTime;
    int EndTime;
    string QueueID;
    string ProcessID;
    
    // Constructor
    ScheduleEvent(int start, int end, string qid, string pid) 
        : StartTime(start), EndTime(end), QueueID(qid), ProcessID(pid) {}
};

class Scheduler {
private:
    vector<Queue> Queues;                  // Danh sách các queue (Q1, Q2, Q3)
    vector<Process> Processes;             // Danh sách tất cả process
    vector<ScheduleEvent> Timeline;        // Lịch sử thực thi (Gantt chart)
    int CurrentTime;                       // Thời gian hiện tại của hệ thống
    int CurrentQueueIndex;                 // Queue nào đang được xét (Round Robin)

public:
    Scheduler();
    void addQueue(Queue q);
    void addProcess(Process p);
    void runScheduling();
    void distributeProcessesToQueues();
    
    // Kiểm tra tất cả process đã xong chưa
    bool allProcessesCompleted();
    
    // Thực thi 1 process trong 1 khoảng thời gian
    void executeProcess(Process* p, Queue& q, int timeToRun);
    
    // Ghi lại event vào timeline
    void recordEvent(int start, int end, string queueID, string processID);
    
    // Lấy timeline để in output
    vector<ScheduleEvent> getTimeline() const;
    
    // Lấy danh sách process để tính metrics
    vector<Process> getProcesses() const;
    
    // In thông tin scheduler (optional)
    void print();

    // Đọc file
    void read(const string& fileName);

    // Ghi file
    void write(const string& fileName);
};

#endif