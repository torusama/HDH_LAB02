#ifndef QUEUE_H
#define QUEUE_H

#include <string>
#include <vector>
#include "Process.h"
using namespace std;

class Queue {
private:
    string QueueID;              // VD: "Q1", "Q2", "Q3"
    int TimeSlice;               // VD: 8, 5, 3
    string Policy;               // "SJF" hoặc "SRTN"
    vector<Process*> Processes;  // Danh sách các process trong queue này

public:
    Queue();
    Queue(string qid,int time,string policy);
    string getQid();
    int getTimeSlice();
    string getPolicy();
    int getProcessCount();
    void addProcess(Process* p);
    bool isEmpty();
    bool hasReadyProcess(int currentTime);
    Process* getNextProcess(int currentTime);
};

#endif