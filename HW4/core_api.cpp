/* 046267 Computer Architecture - Spring 2020 - HW #4 */

#include "core_api.h"
#include "sim_api.h"
#include <vector>
#include <iostream>


#include <stdio.h>

using namespace std;
//class that contains information on one thread
class thread {
public:
    thread();
    thread(const thread& rhs);
    int exeCommand();//1 - simple command, 2-need cont_sw, 3-halt, 0-already finished
    void setThreadNum(int i);
    void setLatency(int load, int store);
    int getWaiting();
    bool getFinished();
    void waitCycles(int num);
    int getRegs(int i);

private:
    int numThread_;
    tcontext regs_;
    int lastCommand_;
    bool finished_;
    int waiting_;
    int loadCycles_;
    int storeCycles_;
};
vector<thread> blockThreadSim, finegrainedThreadSim;//global thread vectors
int blockInstructionNum, blockCycles, finegrainInstructionNum, finegrainCycles;//global counters

//initialisation
thread::thread() :numThread_(0), lastCommand_(0), finished_(false), waiting_(0), loadCycles_(0), storeCycles_(0) {
    for (int i = 0; i < REGS_COUNT; i++) {
        regs_.reg[i] = 0;
    }
}
//copy-constructor
thread::thread(const thread& rhs) :numThread_(rhs.numThread_), lastCommand_(rhs.lastCommand_), finished_(rhs.finished_), waiting_(rhs.waiting_), loadCycles_(rhs.loadCycles_), storeCycles_(rhs.storeCycles_) {
    for (int i = 0; i < REGS_COUNT; i++) {
        regs_.reg[i] = rhs.regs_.reg[i];
    }
}
//getters/setters
int thread::getRegs(int i) {
    return regs_.reg[i];
}
void thread::setThreadNum(int i) {
    numThread_ = i;
}
void thread::setLatency(int load, int store) {
    loadCycles_ = load;
    storeCycles_ = store;
}
int thread::getWaiting() {
    return waiting_;
}
bool thread::getFinished() {
    return finished_;
}

//make a thread wait number of cycles
void thread::waitCycles(int num) {
    if (finished_)return;
    waiting_ -= num;
    if (waiting_ < 0)waiting_ = 0;
    return;
}
//thread executing next command
int thread::exeCommand() {
    Instruction inst;
    int helper = 0;
    SIM_MemInstRead(lastCommand_, &inst, numThread_);
    if (finished_)return 0;
    switch (inst.opcode)
    {
    case (CMD_NOP):
        lastCommand_++;
        break;
    case (CMD_ADD):
        regs_.reg[inst.dst_index] = regs_.reg[inst.src1_index] + regs_.reg[inst.src2_index_imm];
        lastCommand_++;
        return 1;
        break;
    case (CMD_SUB):
        regs_.reg[inst.dst_index] = regs_.reg[inst.src1_index] - regs_.reg[inst.src2_index_imm];
        lastCommand_++;
        return 1;
        break;
    case (CMD_ADDI):
        regs_.reg[inst.dst_index] = regs_.reg[inst.src1_index] + inst.src2_index_imm;
        lastCommand_++;
        return 1;
        break;
    case (CMD_SUBI):
        regs_.reg[inst.dst_index] = regs_.reg[inst.src1_index] - inst.src2_index_imm;
        lastCommand_++;
        return 1;
        break;
    case (CMD_LOAD):
        if (inst.isSrc2Imm)helper = regs_.reg[inst.src1_index] + inst.src2_index_imm;
        else helper = regs_.reg[inst.src1_index] + regs_.reg[inst.src2_index_imm];
        SIM_MemDataRead(helper, &regs_.reg[inst.dst_index]);//addr,*dst
        lastCommand_++;
        waiting_ = loadCycles_;
        return 2;
        break;
    case (CMD_STORE):
        if (inst.isSrc2Imm)
            helper = regs_.reg[inst.dst_index] + inst.src2_index_imm;
        else
            helper = regs_.reg[inst.dst_index] + regs_.reg[inst.src2_index_imm];
        SIM_MemDataWrite(helper, regs_.reg[inst.src1_index]);//addr, val
        lastCommand_++;
        waiting_ = storeCycles_;
        return 2;
    case (CMD_HALT):
        finished_ = true;
        lastCommand_++;
        return 3;
    default:
        break;
    }
    lastCommand_++;
    return 0;
}
//returns is the system in idle state- no thread running
bool isIdle(vector<thread> threads, int numThreads) {
    for (int i = 0; i < numThreads; i++) {
        if (threads[i].getWaiting() > 0 || threads[i].getFinished())
            continue;
        return false;
    }
    return true;
}
//returns did all the threads hit HALT
bool allThreadsFinished(vector<thread> threads, int numThreads) {
    for (int i = 0; i < numThreads; i++) {
        if (threads[i].getFinished() == true)
            continue;
        return false;
    }
    return true;
}
//make all the threads in the system to wait a numCycles of cecles
void waitCycle(int numThreads, int numCycles, int num) {
    if (num == 1) {//for Blocked SM
        for (int i = 0; i < numThreads; i++) {
            blockThreadSim[i].waitCycles(numCycles);
        }
    }
    if (num == 2) {//for FineGrained SM
        for (int i = 0; i < numThreads; i++) {
            finegrainedThreadSim[i].waitCycles(numCycles);
        }
    }

}
// find next available thread
int findNextThread(int num, int numThreads, int currentThread) {
    if (num == 1) {//for Nlocked SM
        int i = (currentThread + 1) % numThreads;
        for (; i != currentThread; i = (i + 1) % numThreads) {
            if (blockThreadSim[i].getWaiting() > 0 || blockThreadSim[i].getFinished())
                continue;
            return i;
        }
        return currentThread;
    }
    else {//for FineGrained SM
        int i = (currentThread + 1) % numThreads;
        for (; i != currentThread; i = (i + 1) % numThreads) {
            if (finegrainedThreadSim[i].getWaiting() > 0 || finegrainedThreadSim[i].getFinished())
                continue;
            return i;
        }
        return currentThread;
    }
}

//execute the Blocked SM
void CORE_BlockedMT() {
    blockInstructionNum = 0;
    blockCycles = 0;
    int helper = 0;
    int loadLatency = SIM_GetLoadLat();
    int storeLatency = SIM_GetStoreLat();
    int switchCycles = SIM_GetSwitchCycles();
    int threadsNum = SIM_GetThreadsNum();
    thread tHelper = thread();
    int threadRunning = 0;
    int inst;
    for (int i = 0; i < threadsNum; i++) {
        tHelper.setThreadNum(i);
        tHelper.setLatency(loadLatency + 1, storeLatency + 1);
        blockThreadSim.push_back(tHelper);

    }
    while (!allThreadsFinished(blockThreadSim, threadsNum)) {
        // cout << "Entered loop\n";
        if (isIdle(blockThreadSim, threadsNum)) {
            waitCycle(threadsNum, 1, 1);
            blockCycles++;
            continue;
        }
        if (helper == -1) {
            if (blockThreadSim[threadRunning].getFinished() == false && blockThreadSim[threadRunning].getWaiting() == 0)
                helper = 0;
            else {
                threadRunning = findNextThread(1, threadsNum, threadRunning);
                helper = 0;
                waitCycle(threadsNum, switchCycles, 1);
                blockCycles += switchCycles;
            }
        }
        inst = blockThreadSim[threadRunning].exeCommand();
        if (inst == 1) {
            waitCycle(threadsNum, 1, 1);
            blockInstructionNum++;
            blockCycles++;
            continue;
        }
        else if (inst == 2) {
            waitCycle(threadsNum, 1, 1);
            blockInstructionNum++;
            blockCycles++;
            if (isIdle(blockThreadSim, threadsNum))
            {
                helper = -1;
                continue;
            }
            threadRunning = findNextThread(1, threadsNum, threadRunning);
            waitCycle(threadsNum, switchCycles, 1);
            blockCycles += switchCycles;
        }
        else if (inst == 3) {
            waitCycle(threadsNum, 1, 1);
            blockInstructionNum++;
            blockCycles++;
            if (isIdle(blockThreadSim, threadsNum))helper = -1;
            else {
                threadRunning = findNextThread(1, threadsNum, threadRunning);
                waitCycle(threadsNum, switchCycles, 1);
                blockCycles += switchCycles;
            }
        }
        else {
            threadRunning = findNextThread(1, threadsNum, threadRunning);
            blockCycles++;
        }
    }
}
//execute FineGrained SM
void CORE_FinegrainedMT() {
    int loadLatency = SIM_GetLoadLat();
    int storeLatency = SIM_GetStoreLat();
    int threadsNum = SIM_GetThreadsNum();
    finegrainInstructionNum = 0;
    finegrainCycles = 0;
    thread tHelper = thread();
    int threadRunning = 0;
    int inst;
    int helper = 2;
    for (int i = 0; i < threadsNum; i++) {
        tHelper.setThreadNum(i);
        tHelper.setLatency(loadLatency + 1, storeLatency + 1);
        finegrainedThreadSim.push_back(tHelper);
    }
    while (!allThreadsFinished(finegrainedThreadSim, threadsNum)) {
        finegrainCycles++;
        if (isIdle(finegrainedThreadSim, threadsNum)) {
            waitCycle(threadsNum, 1, 2);
            helper = 1;
            continue;
        }
        if (helper != 2) {
            threadRunning = findNextThread(2, threadsNum, threadRunning);
        }
        helper = 0;
        inst = finegrainedThreadSim[threadRunning].exeCommand();
        finegrainInstructionNum++;
        waitCycle(threadsNum, 1, 2);
    }
}

double CORE_BlockedMT_CPI() {
    if (blockInstructionNum != 0)
        return ((double)blockCycles / (double)blockInstructionNum);
    return 0;
}

double CORE_FinegrainedMT_CPI() {
    if (finegrainInstructionNum != 0)
        return ((double)finegrainCycles / (double)finegrainInstructionNum);
    return 0;
}

void CORE_BlockedMT_CTX(tcontext* context, int threadid) {
    for (int i = 0; i < REGS_COUNT; i++) {
        context[threadid].reg[i] = blockThreadSim[threadid].getRegs(i);
    }
}

void CORE_FinegrainedMT_CTX(tcontext* context, int threadid) {
    for (int i = 0; i < REGS_COUNT; i++) {
        context[threadid].reg[i] = finegrainedThreadSim[threadid].getRegs(i);
    }
}
