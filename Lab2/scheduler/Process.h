#ifndef PROCESS_H
#define PROCESS_H

#include <string>
using namespace std;

class Process {
private:
    string ProcessID;
    int ArrivalTime;
    int BurstTime;
    int RemainingTime;
    string QueueID;
    int CompletionTime;
    int WaitingTime;
    int TurnaroundTime;
    int StartTime;
public:
    Process();
    Process(string id, int arrival, int burst,string queue);
    
    // Getters
    string getPID();
    int getArrivalTime();
    int getBurstTime();
    int getRemainingTime();
    string getQueueID();
    int getCompletionTime();
    int getTurnaroundTime();
    int getWaitingTime();
    int getStartTime();
    
    // Setters
    void setRemainingTime(int time);
    void setCompletionTime(int time);
    void setStartTime(int time);
    
    // Methods
    void execute(int timeUnits);           // Thực thi process trong timeUnits
    bool isCompleted();              
    void calculateMetrics();               
    
    // Debug/Display
    void print();

};

#endif