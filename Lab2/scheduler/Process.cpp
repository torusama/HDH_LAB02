#include "Process.h"
#include <iostream>
#include <iomanip>

Process::Process() {
    ProcessID = "";
    ArrivalTime = 0;
    BurstTime = 0;
    RemainingTime = 0;
    QueueID = "";
    CompletionTime = 0;
    WaitingTime = 0;
    StartTime = -1;
    TurnaroundTime = 0;
}

Process::Process(string id,int arrival,int burst,string queue) {
    ProcessID = id;
    ArrivalTime = arrival;
    BurstTime = burst;
    QueueID = queue;
    RemainingTime = burst;
    CompletionTime = 0;
    WaitingTime = 0;
    TurnaroundTime = 0;
    StartTime = -1;
}

string Process::getPID() {
    return ProcessID;
}

string Process::getQueueID() {
    return QueueID;
}

int Process::getArrivalTime() {
    return ArrivalTime;
}

int Process::getBurstTime() {
    return BurstTime;
}

int Process::getRemainingTime() {
    return RemainingTime;
}

int Process::getTurnaroundTime(){
    return TurnaroundTime;
}

int Process::getWaitingTime() {
    return WaitingTime;
}

int Process::getCompletionTime() {
    return CompletionTime;
}

int Process::getStartTime() {
    return StartTime;
}

bool Process::isCompleted() {
    if (RemainingTime == 0) return true;
    return false;
}

void Process::setCompletionTime(int Time) {
    CompletionTime = Time;
}

void Process::setRemainingTime(int Time) {
    RemainingTime = Time;
}

void Process::setStartTime(int Time) {
    StartTime = Time;
}

void Process::execute(int timeUnit) {
    if (RemainingTime >= timeUnit) {
        RemainingTime -= timeUnit;
    }
    else {
        RemainingTime = 0;
    }
}

void Process::calculateMetrics()
{
    // tổng thời gian của process = thời điểm process chạy xong - thời điểm process vào hệ thống
    TurnaroundTime = CompletionTime - ArrivalTime; 
    // tổng thời gian đợi = thời gian hoàn thành - thời gian vào - thời gian đợi cpu chạy xong process khác để đến lượt
    WaitingTime = CompletionTime - ArrivalTime - BurstTime;
}


void Process::print(){
    cout << "================================" << endl;
    cout << "Process ID       : " << ProcessID << endl;
    cout << "Queue ID         : " << QueueID << endl;
    cout << "Arrival Time     : " << ArrivalTime << endl;
    cout << "Burst Time       : " << BurstTime << endl;
    cout << "Remaining Time   : " << RemainingTime << endl;
    cout << "Start Time       : " << (StartTime == -1 ? "Not started" : to_string(StartTime)) << endl;
    cout << "Completion Time  : " << CompletionTime << endl;
    cout << "Turnaround Time  : " << TurnaroundTime << endl;
    cout << "Waiting Time     : " << WaitingTime << endl;
    cout << "Status           : " << (isCompleted() ? "Completed" : "Running/Waiting") << endl;
    cout << "================================" << endl;
}