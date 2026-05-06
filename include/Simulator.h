#ifndef SIMULATOR_H
#define SIMULATOR_H

#include <climits>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <queue>
#include <set>
#include <sys/types.h>
#include <vector>

#include "Config.h"
#include "json.hpp"
using namespace nlohmann::literals;

#define MAX_MEM_SIZE 1024
#define NUM_PHYS_REG 32
#define NUM_ARCH_REG_X 32
#define NUM_ARCH_REG_F 32
#define NUM_BTB_ENTRIES 16
#define BTB_SIZE 16
#define NUM_PHYS_REG_INCLUDING_X0 (NUM_PHYS_REG + 1)
#define NO_ROB -1
#define NO_REG -1

class XLSXEventLogger;

// Legacy opcode enum (not used by refactored core, kept for compatibility)
enum OpCode {
    OP_ADDI, OP_ADD, OP_SLT,
    OP_FLD, OP_FSD,
    OP_FADD, OP_FSUB, OP_FMUL, OP_FDIV,
    OP_BNE,
    OP_INVALID
};

// Core opcode enum used by the simulator
enum Opcode {
    FLD, FSD, ADD, ADDI, SLT, FADD, FSUB, FMUL, FDIV, BNE, NOP
};

enum RegisterType {
    REG_X,
    REG_F
};

struct ArchitecturalRegister {
    RegisterType type;
    int32_t      num;

    bool operator<(const ArchitecturalRegister &o) const {
        return (type < o.type) || (type == o.type && num < o.num);
    }

    static constexpr RegisterType X = REG_X;
    static constexpr RegisterType F = REG_F;
};

enum FUType {
    INT,
    LOAD,
    STORE,
    FPADD,
    FPMULT,
    FPDIV,
    BU
};

enum DebugArg {
    DEBUG_DCACHE,
    DEBUG_REGISTERS
};

struct RSEntry {
    bool   busy;
    int    seq;
    int    pc;
    Opcode opcode;
    FUType fuType;
    std::string raw;
    int    robTag;
    int    opIndex;
    int    destPhysReg;
    double value;
    bool   resultReady;
    bool   executing;
    int    qj;
    int    qk;
    double vj;
    double vk;
    bool   vjReady;
    bool   vkReady;
    int    imm;
    bool   hasImm;
    int    effectiveAddr;
    int    cyclesLeft;

    RSEntry()
        : busy(false),
          seq(-1),
          pc(0),
          opcode(NOP),
          fuType(INT),
          robTag(-1),
          opIndex(-1),
          destPhysReg(-1),
          value(0.0),
          resultReady(false),
          executing(false),
          qj(-1),
          qk(-1),
          vj(0.0),
          vk(0.0),
          vjReady(true),
          vkReady(true),
          imm(0),
          hasImm(false),
          effectiveAddr(0),
          cyclesLeft(0) {}
};

struct FU {
    FUType              type;
    int                 latency;
    bool                pipelined;
    int                 rsSize;
    std::vector<RSEntry> rs;
};

using FunctionalUnit = FU;

struct ROBEntry {
    bool   busy;
    int    seq;
    Opcode opcode;
    FUType fuType;
    std::string raw;
    int    pc;

    int destPR;        // legacy, unused by refactored core
    int destPhysReg;
    int destAR;        // legacy, unused by refactored core
    ArchitecturalRegister archDst;
    RegisterType destARType;
    bool   hasDst;

    bool   ready;
    bool   done;
    double value;
    double result;
    uint32_t PC;
    bool   mispredict;

    int  oldPR;        // legacy, unused by refactored core
    int  oldPhysReg;
    RegisterType oldPRType;

    bool     isBranch;
    uint32_t targetPC;
    uint32_t actualTarget;
    bool     predictedTaken;
    bool     actualTaken;
    bool     mispredicted;
    uint32_t predictedTarget;

    bool isLoad;
    bool isStore;
    int  effectiveAddr;

    ROBEntry()
        : busy(false),
          seq(-1),
          opcode(NOP),
          fuType(INT),
          pc(0),
          destPR(-1),
          destPhysReg(-1),
          destAR(-1),
          destARType(REG_X),
          hasDst(false),
          ready(false),
          done(false),
          value(0.0),
          result(0.0),
          PC(0),
          mispredict(false),
          oldPR(-1),
          oldPhysReg(-1),
          oldPRType(REG_X),
          isBranch(false),
          targetPC(0),
          actualTarget(0),
          predictedTaken(false),
          actualTaken(false),
          mispredicted(false),
          predictedTarget(0),
          isLoad(false),
          isStore(false),
          effectiveAddr(0) {}
};

struct BranchPredictorEntry {
    bool     valid;
    uint32_t target;
    int      counter;
};

struct DecodedInstruction {
    int         pc;
    int         seq;
    std::string raw;
    Opcode      opcode;
    FUType      fuType;

    ArchitecturalRegister archDst;
    ArchitecturalRegister archSrc1;
    ArchitecturalRegister archSrc2;

    bool hasDst;
    bool hasSrc1;
    bool hasSrc2;

    int  imm;
    bool hasImm;

    bool predictedTaken;
    int  predictedTarget;

    DecodedInstruction()
        : pc(0),
          seq(-1),
          opcode(NOP),
          fuType(INT),
          hasDst(false),
          hasSrc1(false),
          hasSrc2(false),
          imm(0),
          hasImm(false),
          predictedTaken(false),
          predictedTarget(0) {}
};

struct RegisterFileEntry {
    double value;
    bool   ready;
    int    robTag;
};

class Simulator {
public:
    // Configuration
    Config *configuration;

    // Memory
    double dataMemory[MAX_MEM_SIZE] = {0};

    // Physical register file
    RegisterFileEntry physicalRegisterFile[NUM_PHYS_REG_INCLUDING_X0];

    // Architectural → physical mapping and free list
    std::map<ArchitecturalRegister, int> archToPhysMap;
    std::deque<int>                      availablePhysRegs;
    int                                  nextAllocPR;

    // Branch predictor
    BranchPredictorEntry btb[NUM_BTB_ENTRIES];

    // Program
    std::vector<DecodedInstruction> program;
    int                             programSize = 0;

    // Fetch / decode queues
    std::deque<DecodedInstruction> fetchBuffer;
    std::deque<DecodedInstruction> instrQueue;

    // Instruction sequencing
    int                 nextSeq      = 0;
    std::vector<ROBEntry> reorderBuffer;

    // Reservation stations / FUs
    FunctionalUnit fuINT;
    FunctionalUnit fuLoad;
    FunctionalUnit fuStore;
    FunctionalUnit fuFPAdd;
    FunctionalUnit fuFPMult;
    FunctionalUnit fuFPDiv;
    FunctionalUnit fuBU;

    // Pipeline state
    bool fetchDone   = false;
    bool squashing   = false;
    int  cycleCounter = 0;

    int robDepth    = 0;
    int robReadPtr  = 0;
    int robWritePtr = 0;

    // CDB results
    struct CDBResult {
        int    seq;
        int    robTag;
        int    destPhys;
        double value;

        CDBResult()
            : seq(-1),
              robTag(-1),
              destPhys(-1),
              value(0.0) {}
    };
    std::deque<CDBResult> pendingCDB;

    // Statistics
    uint64_t totalCycles = 0;
    uint64_t rsStalls    = 0;
    uint64_t robStalls   = 0;
    uint64_t cdbUsed     = 0;
    uint64_t cdbTotal    = 0;

    // PC
    uint32_t pc = 0;

    // Constructor
    Simulator(std::ifstream *programFile, Config *c);

    void dump();
    void dump(int argc...);

    // Top-level interface
    void run(XLSXEventLogger *logger = nullptr);
    void printStats();
    void serializeJSON(std::ofstream *output);

    // Pipeline stages (refactored names)
    void fetch(XLSXEventLogger *logger);
    void decode(XLSXEventLogger *logger);
    void issue(XLSXEventLogger *logger);
    void wakeup();
    void execute(XLSXEventLogger *logger);
    void writeback(XLSXEventLogger *logger);
    void commit(XLSXEventLogger *logger);

    // Program parsing
    bool               parseProgram(std::ifstream *f);
    DecodedInstruction parseInstruction(const std::string &line, int bytePC);
    ArchitecturalRegister parseRegister(const std::string &token) const;
    FUType             opToFU(Opcode op) const;

    // FU helpers
    int           latency(FUType t) const;
    bool          pipelineStatus(FUType t) const;
    FunctionalUnit *getFU(FUType t);
    RSEntry      *findFreeRS(FUType fuType);
    bool          hasReadyRS(FUType fuType) const;

    // ROB / PR helpers
    int  allocReorderBuffer();
    void commitReorderBufferHead();
    int  allocPhysReg();

    // BTB helpers
    bool btbPredictTaken(int pc) const;
    int  btbTarget(int pc) const;
    void btbUpdate(int pc, bool taken, int target);

    // Recovery
    void squash(int robIdx);

    // Inline helpers
    inline bool robFull() const { return robDepth >= static_cast<int>(reorderBuffer.size()); }
    inline bool robEmpty() const { return robDepth == 0; }
    inline int  robNext(int idx) const { return (idx + 1) % static_cast<int>(reorderBuffer.size()); }
    inline int  btbIndex(int pc) const { return pc % BTB_SIZE; }
};

#endif // SIMULATOR_H
