#include <climits>
#include "Queue.h"

Queue::Queue() {
    QueueID = "";
    TimeSlice = 0;
    Policy = "";
}

Queue::Queue(string qid,int time,string policy) {
    QueueID = qid;
    TimeSlice = time;
    Policy = policy;
}

string Queue::getQid() {
    return QueueID;
}

string Queue::getPolicy() {
    return Policy;
}

int Queue::getTimeSlice() {
    return TimeSlice;
}

int Queue::getProcessCount() {
    return Processes.size();
}

void Queue::addProcess(Process* p) {
    Processes.push_back(p);
}

bool Queue::isEmpty() {
    return Processes.empty();
}

//Check 2 điều kiện p đã xong chưa? và time đến <= time hiện tại
bool Queue::hasReadyProcess(int currentTime) {
    for (Process* p : Processes) {
        if (!p->isCompleted() && p->getArrivalTime() <= currentTime) {
            return true;
        }
    }
    return false;
}

Process* Queue::getNextProcess(int currentTime) {
    Process* p = NULL;
    if (Policy == "SJF") {
        int minBurst = INT_MAX;
        for (Process* a : Processes) {
            if(!a->isCompleted() && a->getArrivalTime() <= currentTime) {
                if (minBurst > a->getBurstTime()) {
                    minBurst = a->getBurstTime();
                    p = a;
                }
            }
        }
    }
    else if (Policy == "SRTN") {
        int minRemaining = INT_MAX;
         for (Process* a : Processes) {
            if(!a->isCompleted() && a->getArrivalTime() <= currentTime) {
                if (minRemaining > a->getRemainingTime()) {
                    minRemaining = a->getRemainingTime();
                    p = a;
                }
            }
        }
    }
    return p;
}

