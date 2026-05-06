#include "Simulator.h"
#include "XLSXEventLogger.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <climits>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdarg.h>
#include <string>
#include <unordered_set>

// GLOBAL STALL ACCOUNTING AND TRACKING
// ==========================================================================
// These globals track stalls and resource allocation on a per-cycle basis.
// They are reset at the beginning of each cycle in run() to accurately
// measure pipeline statistics.

// Per-cycle ROB stall accounting for speculative instructions.
// Reset at the beginning of each cycle in run().
static int g_cycleRobStalls = 0;

// Per-cycle RS stall accounting for speculative instructions.
// Reset at the beginning of each cycle in run().
static int g_cycleRsStalls = 0;

// ROB tags of instructions dispatched in the current cycle.
// Used to break ties between loads and stores contending for the LS unit.
// This prevents the LS unit from receiving multiple dispatches in a single cycle
// when a load and store are both ready, ensuring fair arbitration.
static std::unordered_set<int> g_cycleIssuedRobTags;

// Stall counter per instruction sequence number.
// Tracks how many RS stalls each individual instruction has accumulated.
// Incremented when instruction stalls in issue(), and used to adjust total stall
// counts when instructions are squashed due to branch mispredictions.
static std::map<int, int> g_instructionRsStalls;

// UTILITY HELPER FUNCTIONS
// ==========================================================================
// These local helper functions are used for string manipulation during
// instruction parsing. They support trimming whitespace and splitting
// instruction fields by delimiters.

namespace {

std::string trim(const std::string &s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    const auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

std::vector<std::string> splitAndTrim(const std::string &s, char delim) {
    std::vector<std::string> out;
    std::istringstream iss(s);
    std::string token;
    while (std::getline(iss, token, delim)) {
        out.push_back(trim(token));
    }
    return out;
}

} // namespace

// CONSTRUCTOR AND INITIALIZATION
// ==========================================================================
// Initializes the simulator with program data and configuration. Sets up:
// - Reorder buffer with configured size
// - Physical register file with all registers marked ready
// - BTB (Branch Target Buffer) with 2-bit saturating counters
// - All functional units (INT, LOAD, STORE, FPADD, FPMULT, FPDIV, BU)
// - Program parsing and storage

Simulator::Simulator(std::ifstream *programFile, Config *cfg)
    : configuration(cfg) {

    reorderBuffer.resize(configuration->NR);

    // Initialize physical register allocator with all registers available
    nextAllocPR = 0;
    availablePhysRegs.clear();
    for (int i = 0; i < NUM_PHYS_REG; ++i) {
        availablePhysRegs.push_back(i);
    }

    // Set up the physical register file. All registers start ready and unused.
    // X0 (the zero register) maps to a dedicated physical register.
    for (int i = 0; i < NUM_PHYS_REG_INCLUDING_X0; ++i) {
        physicalRegisterFile[i].value  = 0.0;
        physicalRegisterFile[i].ready  = true;
        physicalRegisterFile[i].robTag = NO_ROB;
    }

    // X0 always maps to the dedicated zero physical register (NUM_PHYS_REG).
    // This ensures X0 reads always return 0 and writes to X0 are ignored.
    archToPhysMap[{ArchitecturalRegister::X, 0}] = NUM_PHYS_REG;

    // Initialize the Branch Target Buffer with 2-bit saturating counters.
    // Counter value: 0=strongly not taken, 1=weakly not taken, 2=weakly taken, 3=strongly taken.
    // Predictions are made based on counter >= 2 (weakly taken or higher).
    for (int i = 0; i < BTB_SIZE; ++i) {
        btb[i].valid   = false;
        btb[i].target  = 0;
        btb[i].counter = 2; // weakly taken
    }

    // Initialize all functional units with configured latencies, pipelining, and reservation station sizes.
    // Each FU has a latency (in cycles), pipelined flag (true if multiple ops can overlap),
    // and a vector of reservation station entries for holding pending operations.
    fuINT    = {FUType::INT,    1, true,  4, std::vector<RSEntry>(4)};
    fuLoad   = {FUType::LOAD,   1, false, 2, std::vector<RSEntry>(2)};
    fuStore  = {FUType::STORE,  1, false, 2, std::vector<RSEntry>(2)};
    fuFPAdd  = {FUType::FPADD,  3, true,  3, std::vector<RSEntry>(3)};
    fuFPMult = {FUType::FPMULT, 4, false, 2, std::vector<RSEntry>(2)};
    fuFPDiv  = {FUType::FPDIV,  6, false, 1, std::vector<RSEntry>(1)};
    fuBU     = {FUType::BU,     1, true,  2, std::vector<RSEntry>(2)};

    parseProgram(programFile);
    programSize = static_cast<int>(program.size());
}

// PROGRAM PARSING
// ==========================================================================
// Reads the program file and populates the instruction stream. Supports:
// - Data memory initialization (address, value pairs)
// - Label resolution for branch targets
// - Instruction decoding for all supported opcodes
// The file format uses '%' for comments and ':' to denote labels.

bool Simulator::parseProgram(std::ifstream *f) {
    std::string line;
    int bytePC = 0;
    bool inCode = false;
    std::map<std::string, int> labelMap;

    // First pass: collect labels and data memory values
    while (std::getline(*f, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '%') continue;

        // Try to parse as data memory initialization (format: "address, value")
        if (!inCode) {
            std::istringstream test(line);
            int addr;
            char comma;
            double val;
            if (test >> addr >> comma >> val && comma == ',') {
                if (addr >= 0 && addr < MAX_MEM_SIZE) {
                    dataMemory[addr] = val;
                }
                continue;
            }
        }

        // Parse labels (marked by ':') and extract the executable instruction
        std::string work = line;
        const auto colonPos = work.find(':');
        if (colonPos != std::string::npos) {
            const std::string label = trim(work.substr(0, colonPos));
            labelMap[label] = bytePC;
            work = trim(work.substr(colonPos + 1));
        }
        if (work.empty()) continue;

        inCode = true;
        program.push_back(parseInstruction(work, bytePC));
        bytePC += 4;
    }

    // Second pass: resolve branch label targets to their byte addresses
    for (auto &inst : program) {
        if (inst.opcode == Opcode::BNE && !inst.hasImm) {
            std::istringstream ss(inst.raw);
            std::string op, rest;
            ss >> op;
            std::getline(ss, rest);
            auto parts = splitAndTrim(rest, ',');
            if (parts.size() >= 3) {
                auto it = labelMap.find(parts[2]);
                if (it != labelMap.end()) {
                    inst.imm    = it->second;
                    inst.hasImm = true;
                }
            }
        }
    }

    return true;
}

// INSTRUCTION DECODING
// ==========================================================================
// Converts a single instruction string into a DecodedInstruction structure.
// Supports all ISA opcodes and parses register operands and immediates.
// Memory operands use offset(base) addressing format.

DecodedInstruction Simulator::parseInstruction(const std::string &line, int bytePC) {
    DecodedInstruction inst;
    inst.pc  = bytePC;
    inst.raw = line;

    std::istringstream ss(line);
    std::string op;
    ss >> op;
    for (char &c : op) c = static_cast<char>(std::tolower(c));

    // Determine opcode from mnemonic
    if      (op == "fld")  inst.opcode = Opcode::FLD;
    else if (op == "fsd")  inst.opcode = Opcode::FSD;
    else if (op == "add")  inst.opcode = Opcode::ADD;
    else if (op == "addi") inst.opcode = Opcode::ADDI;
    else if (op == "slt")  inst.opcode = Opcode::SLT;
    else if (op == "fadd") inst.opcode = Opcode::FADD;
    else if (op == "fsub") inst.opcode = Opcode::FSUB;
    else if (op == "fmul") inst.opcode = Opcode::FMUL;
    else if (op == "fdiv") inst.opcode = Opcode::FDIV;
    else if (op == "bne")  inst.opcode = Opcode::BNE;
    else {
        inst.opcode = Opcode::NOP;
        return inst;
    }

    // Map opcode to functional unit type
    inst.fuType = opToFU(inst.opcode);

    std::string rest;
    std::getline(ss, rest);
    rest = trim(rest);

    // Helper lambda to parse memory operands in offset(base) format
    auto parseMem = [&](const std::string &s,
                        ArchitecturalRegister &base,
                        int &imm,
                        bool &hasImm) {
        const auto lp = s.find('(');
        const auto rp = s.find(')');
        if (lp != std::string::npos && rp != std::string::npos && rp > lp) {
            imm    = std::stoi(trim(s.substr(0, lp)));
            hasImm = true;
            base   = parseRegister(s.substr(lp + 1, rp - lp - 1));
        }
    };

    // Parse operands based on instruction format
    switch (inst.opcode) {
    case Opcode::FLD: {
        auto parts = splitAndTrim(rest, ',');
        if (parts.size() >= 2) {
            inst.archDst = parseRegister(parts[0]);
            inst.hasDst  = true;
            parseMem(parts[1], inst.archSrc1, inst.imm, inst.hasImm);
            inst.hasSrc1 = true;
        }
        break;
    }
    case Opcode::FSD: {
        auto parts = splitAndTrim(rest, ',');
        if (parts.size() >= 2) {
            inst.archSrc2 = parseRegister(parts[0]);
            inst.hasSrc2  = true;
            parseMem(parts[1], inst.archSrc1, inst.imm, inst.hasImm);
            inst.hasSrc1 = true;
        }
        break;
    }
    case Opcode::ADD:
    case Opcode::SLT: {
        auto parts = splitAndTrim(rest, ',');
        if (parts.size() >= 3) {
            inst.archDst  = parseRegister(parts[0]);
            inst.archSrc1 = parseRegister(parts[1]);
            inst.archSrc2 = parseRegister(parts[2]);
            inst.hasDst   = true;
            inst.hasSrc1  = true;
            inst.hasSrc2  = true;
        }
        break;
    }
    case Opcode::ADDI: {
        auto parts = splitAndTrim(rest, ',');
        if (parts.size() >= 3) {
            inst.archDst  = parseRegister(parts[0]);
            inst.archSrc1 = parseRegister(parts[1]);
            inst.imm      = std::stoi(parts[2]);
            inst.hasDst   = true;
            inst.hasSrc1  = true;
            inst.hasImm   = true;
        }
        break;
    }
    case Opcode::FADD:
    case Opcode::FSUB:
    case Opcode::FMUL:
    case Opcode::FDIV: {
        auto parts = splitAndTrim(rest, ',');
        if (parts.size() >= 3) {
            inst.archDst  = parseRegister(parts[0]);
            inst.archSrc1 = parseRegister(parts[1]);
            inst.archSrc2 = parseRegister(parts[2]);
            inst.hasDst   = true;
            inst.hasSrc1  = true;
            inst.hasSrc2  = true;
        }
        break;
    }
    case Opcode::BNE: {
        auto parts = splitAndTrim(rest, ',');
        if (parts.size() >= 2) {
            inst.archSrc1 = parseRegister(parts[0]);
            inst.archSrc2 = parseRegister(parts[1]);
            inst.hasSrc1  = true;
            inst.hasSrc2  = true;
        }
        // Third operand can be a numeric immediate or label (resolved in parseProgram)
        if (parts.size() >= 3) {
            const std::string &immStr = parts[2];
            const bool numeric = !immStr.empty() &&
                                 (std::isdigit(static_cast<unsigned char>(immStr[0])) ||
                                  immStr[0] == '-');
            if (numeric) {
                inst.imm    = std::stoi(immStr);
                inst.hasImm = true;
            }
        }
        break;
    }
    default:
        break;
    }

    return inst;
}

// REGISTER PARSING AND OPCODE MAPPING
// ==========================================================================
// Helper functions to convert string representations to enum types.

ArchitecturalRegister Simulator::parseRegister(const std::string &token) const {
    std::string t = trim(token);
    for (char &c : t) c = static_cast<char>(std::toupper(c));
    if (t.empty()) return {ArchitecturalRegister::X, 0};

    // Parse floating-point registers (F0-F31)
    if (t[0] == 'F') {
        return {ArchitecturalRegister::F, std::stoi(t.substr(1))};
    }
    // Parse integer registers (X0-X31, R0-R31, or $0-$31 shorthand)
    if (t[0] == 'X' || t[0] == 'R' || t[0] == '$') {
        return {ArchitecturalRegister::X, std::stoi(t.substr(1))};
    }
    return {ArchitecturalRegister::X, 0};
}

// Map opcode to the functional unit that executes it
FUType Simulator::opToFU(Opcode op) const {
    switch (op) {
    case Opcode::ADD:
    case Opcode::ADDI:
    case Opcode::SLT:
        return FUType::INT;
    case Opcode::FLD:
        return FUType::LOAD;
    case Opcode::FSD:
        return FUType::STORE;
    case Opcode::FADD:
    case Opcode::FSUB:
        return FUType::FPADD;
    case Opcode::FMUL:
        return FUType::FPMULT;
    case Opcode::FDIV:
        return FUType::FPDIV;
    case Opcode::BNE:
        return FUType::BU;
    default:
        return FUType::INT;
    }
}

// FUNCTIONAL UNIT HELPERS
// ==========================================================================
// Query and manage functional unit properties and reservation station availability.

// Latency in cycles for each functional unit type
int Simulator::latency(FUType t) const {
    switch (t) {
    case FUType::INT:    return 1;
    case FUType::LOAD:   return 2;
    case FUType::STORE:  return 2;
    case FUType::FPADD:  return 3;
    case FUType::FPMULT: return 4;
    case FUType::FPDIV:  return 6;
    case FUType::BU:     return 1;
    }
    return 1;
}

// Check if a functional unit is fully pipelined.
// Non-pipelined units (FPMULT, FPDIV) can only have one instruction executing at a time.
bool Simulator::pipelineStatus(FUType t) const {
    return !(t == FUType::FPMULT || t == FUType::FPDIV);
}

// Get pointer to the functional unit struct for a given FU type
FunctionalUnit *Simulator::getFU(FUType t) {
    switch (t) {
    case FUType::INT:    return &fuINT;
    case FUType::LOAD:   return &fuLoad;
    case FUType::STORE:  return &fuStore;
    case FUType::FPADD:  return &fuFPAdd;
    case FUType::FPMULT: return &fuFPMult;
    case FUType::FPDIV:  return &fuFPDiv;
    case FUType::BU:     return &fuBU;
    }
    return &fuINT;
}

// Find a free (non-busy) reservation station entry in the given FU
RSEntry *Simulator::findFreeRS(FUType t) {
    auto *fu = getFU(t);
    for (auto &rs : fu->rs) {
        if (!rs.busy) return &rs;
    }
    return nullptr;
}

// Check if the given FU has at least one free reservation station
bool Simulator::hasReadyRS(FUType t) const {
    const FunctionalUnit *fu = nullptr;
    switch (t) {
    case FUType::INT:    fu = &fuINT;    break;
    case FUType::LOAD:   fu = &fuLoad;   break;
    case FUType::STORE:  fu = &fuStore;  break;
    case FUType::FPADD:  fu = &fuFPAdd;  break;
    case FUType::FPMULT: fu = &fuFPMult; break;
    case FUType::FPDIV:  fu = &fuFPDiv;  break;
    case FUType::BU:     fu = &fuBU;     break;
    }
    if (!fu) return false;
    for (const auto &rs : fu->rs) {
        if (!rs.busy) return true;
    }
    return false;
}


// REORDER BUFFER AND PHYSICAL REGISTER ALLOCATION
// ==========================================================================
// Manages ROB entries and physical register free list. The ROB tracks
// instruction order and enables in-order commit despite out-of-order execution.

// Allocate a new ROB entry at robWritePtr and advance the write pointer
int Simulator::allocReorderBuffer() {
    if (robFull()) return NO_ROB;
    const int idx = robWritePtr;
    reorderBuffer[idx] = ROBEntry{};
    reorderBuffer[idx].busy = true;
    robWritePtr = robNext(robWritePtr);
    ++robDepth;
    return idx;
}

// Commit the head of the ROB and advance the read pointer
void Simulator::commitReorderBufferHead() {
    reorderBuffer[robReadPtr].busy = false;
    robReadPtr = robNext(robReadPtr);
    --robDepth;
}

// Allocate a free physical register from the available list
int Simulator::allocPhysReg() {
    if (availablePhysRegs.empty()) return NO_REG;
    const int pr = availablePhysRegs.front();
    availablePhysRegs.pop_front();
    return pr;
}

// BRANCH TARGET BUFFER (BTB)
// ==========================================================================
// Implements a simple set-associative BTB with 2-bit saturating counters
// for branch prediction. Counter values: 0=SN, 1=WN, 2=WT, 3=ST.
// Predictions are made when counter >= 2 (weakly taken or higher).

// Check if BTB predicts the branch at bpc as taken
bool Simulator::btbPredictTaken(int bpc) const {
    const int idx = btbIndex(bpc);
    if (!btb[idx].valid) return false;
    return btb[idx].counter >= 2;
}

// Get the predicted target address for a branch at bpc
int Simulator::btbTarget(int bpc) const {
    const int idx = btbIndex(bpc);
    if (!btb[idx].valid) return bpc + 4;
    return btb[idx].target;
}

// Update the BTB entry with the actual branch outcome and target
void Simulator::btbUpdate(int bpc, bool taken, int target) {
    const int idx = btbIndex(bpc);
    btb[idx].valid  = true;
    btb[idx].target = target;
    int &ctr = btb[idx].counter;
    // Update saturating counter: increment if taken, decrement if not taken
    if (taken) {
        ctr = std::min(ctr + 1, 3);
    } else {
        ctr = std::max(ctr - 1, 0);
    }
}


// MAIN SIMULATION LOOP
// ==========================================================================
// The main run() method orchestrates the 5-stage pipeline in reverse order
// within each cycle (writeback -> commit -> wakeup -> issue -> execute -> 
// decode -> fetch). This ordering ensures that data from one stage flows 
// correctly to earlier stages without timing violations.
//
// The simulation terminates when all instructions have been fetched, decoded,
// executed, and committed, with the reorder buffer empty.

void Simulator::run(XLSXEventLogger *logger) {
    while (true) {
        // Exit when all instructions have completed execution
        if (fetchDone &&
            fetchBuffer.empty() &&
            instrQueue.empty() &&
            robEmpty()) {
            break;
        }

        // Safety limit to prevent infinite loops
        if (cycleCounter >= 100000) {
            std::cerr << "[ERROR] Exceeded 100000 cycles - aborting.\n";
            break;
        }

        ++cycleCounter;
        squashing       = false;
        
        // Reset per-cycle accounting for stalls and issued instructions
        g_cycleRobStalls = 0;
        g_cycleRsStalls = 0;
        g_cycleIssuedRobTags.clear();

        if (logger) logger->startCycle();

        // Execute pipeline stages in reverse order within the cycle
        writeback(logger);
        commit(logger);
        wakeup();
        issue(logger);
        execute(logger);
        decode(logger);
        fetch(logger);
    }

    totalCycles = cycleCounter;
}

// FETCH STAGE
// ==========================================================================
// Reads up to NF instructions from the program and places them in the
// fetch buffer. Applies branch prediction: on BNE, uses the BTB to predict
// the target and updates the PC accordingly. Fetch stops when a predicted
// taken branch is encountered in this cycle.

void Simulator::fetch(XLSXEventLogger *logger) {
    if (fetchDone || squashing) return;
    if (!fetchBuffer.empty())  return;

    // Do not fetch if instruction queue is full
    const int iqFree = configuration->NI - static_cast<int>(instrQueue.size());
    if (iqFree <= 0) return;

    const int fetchBudget = std::min(configuration->NF, iqFree);
    int fetched = 0;

    while (fetched < fetchBudget) {
        const int idx = pc / 4;
        if (idx >= programSize) {
            fetchDone = true;
            break;
        }

        DecodedInstruction inst = program[idx];
        inst.seq = nextSeq++;

        // Apply branch prediction for BNE instructions
        if (inst.opcode == Opcode::BNE) {
            const bool predTaken  = btbPredictTaken(inst.pc);
            const int  predTarget = btbTarget(inst.pc);
            inst.predictedTaken   = predTaken;
            inst.predictedTarget  = predTarget;
            // Update PC based on prediction
            pc = predTaken ? predTarget : (pc + 4);
        } else {
            pc += 4;
        }

        if (logger) {
            logger->logFetchedInstruction(inst.seq, inst.pc, inst.raw);
            logger->markStage(inst.seq, "IF");
        }

        fetchBuffer.push_back(inst);
        ++fetched;

        // Stop fetching after a predicted taken branch
        if (inst.opcode == Opcode::BNE && inst.predictedTaken) {
            break;
        }
    }
}

// DECODE STAGE
// ==========================================================================
// Transfers instructions from the fetch buffer to the instruction queue.
// Clears the fetch buffer if squashing due to a branch misprediction.

void Simulator::decode(XLSXEventLogger *logger) {
    if (squashing) {
        fetchBuffer.clear();
        return;
    }

    int decoded = 0;
    auto it = fetchBuffer.begin();

    // Transfer up to NF instructions to the instruction queue
    while (it != fetchBuffer.end() && decoded < configuration->NF) {
        if (static_cast<int>(instrQueue.size()) >= configuration->NI) break;

        if (logger) logger->markStage(it->seq, "ID");
        instrQueue.push_back(*it);
        it = fetchBuffer.erase(it);
        ++decoded;
    }
}


// ISSUE STAGE
// ==========================================================================
// Dispatches instructions from the instruction queue to reservation stations.
// Performs register renaming and checks for structural hazards:
// - Reservation station availability
// - Physical register availability
// - Reorder buffer availability
//
// Register renaming maps architectural registers to physical registers,
// enabling out-of-order execution while maintaining instruction dependencies.

void Simulator::issue(XLSXEventLogger *logger) {
    if (squashing) return;

    int issued = 0;
    auto it = instrQueue.begin();

    while (it != instrQueue.end() && issued < configuration->NW) {
        DecodedInstruction &inst = *it;

        // Stall if reservation station for this FU is not available
        if (!hasReadyRS(inst.fuType)) {
            ++rsStalls;
            ++g_cycleRsStalls;
            ++g_instructionRsStalls[inst.seq];
            if (logger) logger->markStage(inst.seq, "RS Stall");
            break;
        }

        // Stall if instruction has a destination but no free physical register
        if (inst.hasDst && availablePhysRegs.empty()) {
            ++rsStalls;
            ++g_cycleRsStalls;
            ++g_instructionRsStalls[inst.seq];
            if (logger) logger->markStage(inst.seq, "RS Stall");
            break;
        }

        // Stall if reorder buffer is full
        if (robFull()) {
            ++robStalls;
            ++g_cycleRobStalls;
            if (logger) logger->markStage(inst.seq, "ROB Stall");
            break;
        }

        // Allocate ROB entry and record for load/store arbitration
        const int robIdx = allocReorderBuffer();
        g_cycleIssuedRobTags.insert(robIdx);

        // Fill ROB entry with instruction metadata
        ROBEntry &re = reorderBuffer[robIdx];
        re.seq      = inst.seq;
        re.pc       = inst.pc;
        re.opcode   = inst.opcode;
        re.fuType   = inst.fuType;
        re.raw      = inst.raw;
        re.hasDst   = inst.hasDst;
        re.archDst  = inst.archDst;
        re.effectiveAddr = -1;
        re.isBranch = (inst.opcode == Opcode::BNE);
        re.isStore  = (inst.opcode == Opcode::FSD);
        re.isLoad   = (inst.opcode == Opcode::FLD);
        re.predictedTaken  = inst.predictedTaken;
        re.predictedTarget = inst.predictedTarget;

        // Allocate and initialize reservation station entry
        RSEntry *rs = findFreeRS(inst.fuType);
        *rs = RSEntry{};
        rs->busy        = true;
        rs->seq         = inst.seq;
        rs->pc          = inst.pc;
        rs->opcode      = inst.opcode;
        rs->fuType      = inst.fuType;
        rs->raw         = inst.raw;
        rs->robTag      = robIdx;
        rs->imm         = inst.imm;
        rs->hasImm      = inst.hasImm;
        rs->effectiveAddr = -1;

        // Helper to resolve source operands using rename table
        auto resolveSrc = [&](const ArchitecturalRegister &ar,
                              double &val,
                              int &tag) {
            auto mit = archToPhysMap.find(ar);
            if (mit == archToPhysMap.end()) {
                val = 0.0;
                tag = NO_REG;
                return;
            }
            const int pr = mit->second;
            if (physicalRegisterFile[pr].ready) {
                // Physical register ready: forward value
                val = physicalRegisterFile[pr].value;
                tag = NO_REG;
            } else {
                // Physical register not ready: record CDB dependency
                val = 0.0;
                tag = pr;
            }
        };

        // Resolve source operands for register renaming
        if (inst.hasSrc1) {
            resolveSrc(inst.archSrc1, rs->vj, rs->qj);
        } else {
            rs->vj = 0.0;
            rs->qj = NO_REG;
        }

        if (inst.hasSrc2) {
            resolveSrc(inst.archSrc2, rs->vk, rs->qk);
        } else {
            rs->vk = 0.0;
            rs->qk = NO_REG;
        }

        // Allocate new physical register if instruction has a destination
        int newPR = NO_REG;
        int oldPR = NO_REG;

        if (inst.hasDst) {
            newPR = allocPhysReg();
            auto mit = archToPhysMap.find(inst.archDst);
            oldPR = (mit != archToPhysMap.end()) ? mit->second : NO_REG;
            // Update rename table: architectural register now maps to new physical register
            archToPhysMap[inst.archDst] = newPR;
            physicalRegisterFile[newPR].ready  = false;
            physicalRegisterFile[newPR].robTag = robIdx;
        }

        re.destPhysReg = newPR;
        re.oldPhysReg  = oldPR;
        rs->destPhysReg = newPR;

        if (logger) logger->markStage(inst.seq, "IS");

        it = instrQueue.erase(it);
        ++issued;
    }
}

// WAKEUP STAGE
// ==========================================================================
// Scans reservation stations and updates operands when their physical
// registers become ready. This stage enables rapid operand forwarding
// and reduces the latency for dependent instructions to become ready.

void Simulator::wakeup() {
    auto wakeupFU = [&](FunctionalUnit &fu) {
        for (auto &rs : fu.rs) {
            if (!rs.busy || rs.executing || rs.resultReady) continue;

            // Check if source operand 1 has become ready
            if (rs.qj != NO_REG && physicalRegisterFile[rs.qj].ready) {
                rs.vj = physicalRegisterFile[rs.qj].value;
                rs.qj = NO_REG;
            }
            // Check if source operand 2 has become ready
            if (rs.qk != NO_REG && physicalRegisterFile[rs.qk].ready) {
                rs.vk = physicalRegisterFile[rs.qk].value;
                rs.qk = NO_REG;
            }
        }
    };

    // Wakeup all functional units
    wakeupFU(fuINT);
    wakeupFU(fuLoad);
    wakeupFU(fuStore);
    wakeupFU(fuFPAdd);
    wakeupFU(fuFPMult);
    wakeupFU(fuFPDiv);
    wakeupFU(fuBU);
}


// EXECUTE STAGE
// ==========================================================================
// Dispatches ready instructions to functional units and computes results.
// Key features:
// - Memory ordering: loads check for conflicting stores and forward values
// - Branch resolution: detects mispredictions and triggers squashing
// - Pipelining: respects latencies and handles non-pipelined units
// - Load/store arbitration: fair scheduling using dispatch order
//
// Instructions become ready when all source operands are available (qj, qk == NO_REG).
// Results are placed on the Common Data Bus (CDB) for writeback.

void Simulator::execute(XLSXEventLogger *logger) {
    // Check if load/store unit is currently busy with an executing instruction
    bool lsUnitBusy = false;
    for (auto &rs : fuLoad.rs) {
        if (rs.busy && rs.executing) {
            lsUnitBusy = true;
            break;
        }
    }
    for (auto &rs : fuStore.rs) {
        if (rs.busy && rs.executing) {
            lsUnitBusy = true;
            break;
        }
    }

    bool lsStartedThisCycle = false;

    // Execute functional unit: dispatches ready instructions and completes executing ones
    auto execFU = [&](FunctionalUnit &fu,
                      bool externalBusy = false,
                      bool *sharedStarted = nullptr) {
        bool nonPipeActive = externalBusy;
        
        // Non-pipelined units can have at most one instruction executing at a time
        if (!fu.pipelined) {
            for (auto &rs : fu.rs) {
                if (rs.busy && rs.executing) {
                    nonPipeActive = true;
                    break;
                }
            }
        }

        bool startedThisCycle = false;

        // Collect all active instructions and sort by sequence number (in-order dispatch)
        std::vector<RSEntry*> active;
        for (auto &rs : fu.rs) {
            if (rs.busy && !rs.resultReady) {
                active.push_back(&rs);
            }
        }
        std::sort(active.begin(), active.end(),
                  [](RSEntry *a, RSEntry *b) { return a->seq < b->seq; });

        for (auto *ptr : active) {
            RSEntry &rs = *ptr;

            // Store address calculation: compute effective address for FSD
            if (rs.opcode == Opcode::FSD &&
                rs.effectiveAddr == -1 &&
                rs.qj == NO_REG) {
                const int addr = static_cast<int>(rs.vj) + rs.imm;
                rs.effectiveAddr = addr;
                if (rs.robTag != NO_ROB &&
                    rs.robTag < static_cast<int>(reorderBuffer.size())) {
                    reorderBuffer[rs.robTag].effectiveAddr = addr;
                }
            }

            // Check if instruction is ready to execute (all operands available)
            const bool ready = (rs.qj == NO_REG) && (rs.qk == NO_REG);

            // Dispatch ready instruction to functional unit
            if (!rs.executing && ready) {
                // Non-pipelined units and shared units have dispatch restrictions
                const bool pairedOccupied = (sharedStarted && *sharedStarted);
                if ((!fu.pipelined && nonPipeActive) ||
                    startedThisCycle ||
                    pairedOccupied) {
                    if (logger) logger->markStage(rs.seq, "FU Stall");
                    continue;
                }

                // Start execution with latency counter
                rs.executing  = true;
                rs.cyclesLeft = fu.latency;
                startedThisCycle = true;
                if (sharedStarted) *sharedStarted = true;
                if (!fu.pipelined) nonPipeActive = true;
            }

            if (!rs.executing) continue;

            if (logger) logger->markStage(rs.seq, "EX");
            
            // Decrement cycle counter; continue when cycles remain
            --rs.cyclesLeft;
            if (rs.cyclesLeft > 0) continue;

            // Execute instruction and compute result
            double result = 0.0;

            switch (rs.opcode) {
            case Opcode::FLD: {
                const int addr = static_cast<int>(rs.vj) + rs.imm;
                bool safeToLoad = true;
                bool foundStore = false;
                double forwarded = 0.0;

                // Memory ordering: check for conflicting stores to the same address
                // Walk backwards through ROB entries (older instructions) until ROB read pointer
                int checkIdx = rs.robTag;
                while (checkIdx != robReadPtr) {
                    checkIdx = (checkIdx == 0)
                               ? static_cast<int>(reorderBuffer.size()) - 1
                               : (checkIdx - 1);
                    ROBEntry &older = reorderBuffer[checkIdx];

                    if (older.busy && older.isStore) {
                        if (older.effectiveAddr == -1) {
                            // Conflicting store with unknown address: cannot safely load
                            safeToLoad = false;
                            break;
                        }
                        if (older.effectiveAddr == addr) {
                            // Found store to same address
                            foundStore = true;
                            if (older.done) {
                                // Store completed: forward value from store
                                forwarded = older.result;
                            } else {
                                // Store not yet completed: wait for it
                                safeToLoad = false;
                            }
                            break;
                        }
                    }
                }

                if (!safeToLoad) {
                    // Stall load and re-execute next cycle
                    ++rs.cyclesLeft;
                    rs.executing = false;
                    continue;
                }

                // Load is safe to execute
                rs.effectiveAddr = addr;
                if (rs.robTag != NO_ROB &&
                    rs.robTag < static_cast<int>(reorderBuffer.size())) {
                    reorderBuffer[rs.robTag].effectiveAddr = addr;
                }

                // Forward value from conflicting store or read from memory
                if (foundStore) {
                    result = forwarded;
                } else {
                    result = (addr >= 0 && addr < MAX_MEM_SIZE)
                             ? dataMemory[addr]
                             : 0.0;
                }
                break;
            }
            case Opcode::FSD: {
                const int addr = static_cast<int>(rs.vj) + rs.imm;
                rs.effectiveAddr = addr;
                if (rs.robTag != NO_ROB &&
                    rs.robTag < static_cast<int>(reorderBuffer.size())) {
                    reorderBuffer[rs.robTag].effectiveAddr = addr;
                    reorderBuffer[rs.robTag].result        = rs.vk;
                }
                result = rs.vk;
                break;
            }
            case Opcode::BNE: {
                // Branch resolution: compare operands and check prediction
                const bool taken = (rs.vj != rs.vk);
                const int target = rs.hasImm ? rs.imm : (rs.pc + 4);
                const int actual = taken ? target : (rs.pc + 4);

                // Update BTB with branch outcome
                btbUpdate(rs.pc, taken, target);

                // Detect misprediction
                bool mispred = false;
                if (rs.robTag != NO_ROB &&
                    rs.robTag < static_cast<int>(reorderBuffer.size())) {
                    ROBEntry &re = reorderBuffer[rs.robTag];
                    re.actualTaken  = taken;
                    re.actualTarget = actual;
                    re.mispredicted =
                        (taken != re.predictedTaken) ||
                        (taken && actual != re.predictedTarget);
                    mispred = re.mispredicted;
                }

                result = taken ? 1.0 : 0.0;

                // On misprediction: flush pipeline and squash speculative instructions
                if (mispred) {
                    pc        = actual;
                    fetchDone = false;
                    squash(rs.robTag);
                    squashing = true;
                }
                break;
            }
            case Opcode::ADD:
                result = rs.vj + rs.vk;
                break;
            case Opcode::ADDI:
                result = rs.vj + static_cast<double>(rs.imm);
                break;
            case Opcode::SLT:
                result = (rs.vj < rs.vk) ? 1.0 : 0.0;
                break;
            case Opcode::FADD:
                result = rs.vj + rs.vk;
                break;
            case Opcode::FSUB:
                result = rs.vj - rs.vk;
                break;
            case Opcode::FMUL:
                result = rs.vj * rs.vk;
                break;
            case Opcode::FDIV:
                result = (rs.vk != 0.0) ? (rs.vj / rs.vk) : 0.0;
                break;
            default:
                break;
            }

            // Broadcast result on Common Data Bus (CDB)
            CDBResult cdb;
            cdb.seq      = rs.seq;
            cdb.robTag   = rs.robTag;
            cdb.destPhys = rs.destPhysReg;
            cdb.value    = result;

            pendingCDB.push_back(cdb);
            rs.resultReady = true;
        }
    };

    // Execute INT unit (fully pipelined)
    execFU(fuINT);

    // Determine which load/store to execute first
    // Priority: execute stores first if both were just dispatched, else loads first
    auto oldestReadyRobTag = [&](const FunctionalUnit &fu) -> int {
        int bestSeq = INT_MAX;
        int bestTag = NO_ROB;
        for (const auto &rs : fu.rs) {
            if (rs.busy &&
                !rs.executing &&
                !rs.resultReady &&
                rs.qj == NO_REG &&
                rs.qk == NO_REG) {
                if (rs.seq < bestSeq) {
                    bestSeq = rs.seq;
                    bestTag = rs.robTag;
                }
            }
        }
        return bestTag;
    };

    const int loadTag  = oldestReadyRobTag(fuLoad);
    const int storeTag = oldestReadyRobTag(fuStore);

    const bool loadJustDispatched  = (loadTag  != NO_ROB) && g_cycleIssuedRobTags.count(loadTag);
    const bool storeJustDispatched = (storeTag != NO_ROB) && g_cycleIssuedRobTags.count(storeTag);

    // Arbitrate between load and store for the shared LS unit
    if (!storeJustDispatched && loadJustDispatched) {
        execFU(fuStore, lsUnitBusy, &lsStartedThisCycle);
        execFU(fuLoad,  lsUnitBusy, &lsStartedThisCycle);
    } else {
        execFU(fuLoad,  lsUnitBusy, &lsStartedThisCycle);
        execFU(fuStore, lsUnitBusy, &lsStartedThisCycle);
    }

    // Execute remaining functional units
    execFU(fuFPAdd);
    execFU(fuFPMult);
    execFU(fuFPDiv);
    execFU(fuBU);
}


// WRITEBACK STAGE
// ==========================================================================
// Processes results on the Common Data Bus (CDB) and updates the machine state.
// Prioritizes branch results to enable rapid recovery from mispredictions,
// then processes other results according to functional unit priority.
// 
// Each cycle, up to NB results can be written back. Unwritten results remain
// in pendingCDB for later processing.

void Simulator::writeback(XLSXEventLogger *logger) {
    // Track CDB utilization for statistics
    cdbTotal += configuration->NB;

    // Free reservation station for instruction that just wrote back
    auto freeRSFor = [&](const CDBResult &r) {
        auto sweep = [&](FunctionalUnit &fu) {
            for (auto &rs : fu.rs) {
                if (rs.busy &&
                    rs.robTag == r.robTag &&
                    rs.resultReady) {
                    rs.busy        = false;
                    rs.executing   = false;
                    rs.resultReady = false;
                }
            }
        };
        sweep(fuINT);
        sweep(fuLoad);
        sweep(fuStore);
        sweep(fuFPAdd);
        sweep(fuFPMult);
        sweep(fuFPDiv);
        sweep(fuBU);
    };

    // Commit result to physical register and ROB
    auto commitResult = [&](const CDBResult &r) {
        if (r.robTag != NO_ROB &&
            r.robTag < static_cast<int>(reorderBuffer.size()) &&
            reorderBuffer[r.robTag].busy) {
            reorderBuffer[r.robTag].result = r.value;
            reorderBuffer[r.robTag].done   = true;
        }
        if (r.destPhys != NO_REG &&
            r.destPhys < NUM_PHYS_REG_INCLUDING_X0) {
            physicalRegisterFile[r.destPhys].value = r.value;
            physicalRegisterFile[r.destPhys].ready = true;
        }
    };

    // High priority: write back branch results to enable immediate squashing on misprediction
    {
        auto it = pendingCDB.begin();
        while (it != pendingCDB.end()) {
            const CDBResult &r = *it;
            const bool isBranchWB =
                (r.destPhys == NO_REG) &&
                (r.robTag != NO_ROB &&
                 r.robTag < static_cast<int>(reorderBuffer.size()) &&
                 reorderBuffer[r.robTag].isBranch);
            if (isBranchWB) {
                commitResult(r);
                freeRSFor(r);
                if (logger) logger->markStage(r.seq, "WB");
                it = pendingCDB.erase(it);
            } else {
                ++it;
            }
        }
    }

    // Assign functional unit priorities for fairness in CDB arbitration
    auto fuPriority = [](FUType t) -> int {
        switch (t) {
        case FUType::LOAD:   return 0;  // Highest: feeds dependent instructions
        case FUType::INT:    return 1;
        case FUType::FPADD:  return 2;
        case FUType::FPMULT: return 3;
        case FUType::FPDIV:  return 4;
        case FUType::STORE:  return 5;  // Lowest: no dependent instructions
        default:             return 6;
        }
    };

    // Check if a CDB result enables a branch to become ready
    auto isBranchEnabling = [&](const CDBResult &r) -> bool {
        if (r.destPhys == NO_REG ||
            r.destPhys >= NUM_PHYS_REG_INCLUDING_X0) {
            return false;
        }
        for (const auto &rs : fuBU.rs) {
            if (rs.busy &&
                !rs.resultReady &&
                (rs.qj == r.destPhys || rs.qk == r.destPhys)) {
                return true;
            }
        }
        return false;
    };

    // Sort pending CDB results by: FU priority, branch-enabling status, sequence number
    std::sort(pendingCDB.begin(), pendingCDB.end(),
              [&](const CDBResult &a, const CDBResult &b) {
                  int pa = 6;
                  int pb = 6;
                  if (a.robTag != NO_ROB &&
                      a.robTag < static_cast<int>(reorderBuffer.size())) {
                      pa = fuPriority(reorderBuffer[a.robTag].fuType);
                  }
                  if (b.robTag != NO_ROB &&
                      b.robTag < static_cast<int>(reorderBuffer.size())) {
                      pb = fuPriority(reorderBuffer[b.robTag].fuType);
                  }
                  if (pa != pb) return pa < pb;

                  const bool ba = isBranchEnabling(a);
                  const bool bb = isBranchEnabling(b);
                  if (ba != bb) return ba > bb;

                  return a.seq < b.seq;
              });

    // Write back up to NB results
    int used = 0;
    for (auto &r : pendingCDB) {
        if (used >= configuration->NB) break;
        commitResult(r);
        freeRSFor(r);
        if (logger) logger->markStage(r.seq, "WB");
        ++cdbUsed;
        ++used;
    }

    // Log CDB stalls for remaining results
    if (logger) {
        for (int i = used; i < static_cast<int>(pendingCDB.size()); ++i) {
            logger->markStage(pendingCDB[i].seq, "CDB Stall");
        }
    }

    // Remove written-back results from pending queue
    pendingCDB.erase(pendingCDB.begin(),
                     pendingCDB.begin() + used);
}

// COMMIT STAGE
// ==========================================================================
// Commits up to NC instructions in-order from the head of the reorder buffer.
// Writes store data to memory and reclaims old physical registers.

void Simulator::commit(XLSXEventLogger *logger) {
    int committed = 0;
    while (committed < configuration->NC && !robEmpty()) {
        ROBEntry &re = reorderBuffer[robReadPtr];
        if (!re.done) break;

        // Store commits write data to memory
        if (re.isStore) {
            const int addr = re.effectiveAddr;
            if (addr >= 0 && addr < MAX_MEM_SIZE) {
                dataMemory[addr] = re.result;
            }
        }

        // Reclaim old physical register freed by this instruction
        if (re.hasDst && re.destPhysReg != NO_REG) {
            // Reclaim the old physical register
            // Check re.oldPhysReg != NUM_PHYS_REG (X0) and ensure it's a valid PR
            if (re.oldPhysReg != NO_REG && re.oldPhysReg < NUM_PHYS_REG) {
                // Add to BACK: it is now available for future use
                availablePhysRegs.push_back(re.oldPhysReg);
                physicalRegisterFile[re.oldPhysReg].ready = true;
                physicalRegisterFile[re.oldPhysReg].robTag = NO_ROB;
            }
        }

        if (logger) logger->markStage(re.seq, "CT");
        commitReorderBufferHead();
        ++committed;
    }
}

// SQUASH / BRANCH MISPREDICTION RECOVERY
// ==========================================================================
// On branch misprediction, flushes all speculative instructions younger than
// the branch. Restores the rename table and reclaims physical registers.
// Resets fetch and clears the instruction queue.

void Simulator::squash(int mispredROBIdx) {
    int idx = robWritePtr;

    // Collect sequence numbers of instructions being squashed from ROB
    std::set<int> squashedSeqs;
    while (idx != robNext(mispredROBIdx)) {
        idx = (idx == 0) ? static_cast<int>(reorderBuffer.size()) - 1 : (idx - 1);

        ROBEntry &re = reorderBuffer[idx];
        if (!re.busy) continue;
        
        squashedSeqs.insert(re.seq);

        // Restore rename table for destination registers
        if (re.hasDst && re.destPhysReg != NO_REG) {
            auto it = archToPhysMap.find(re.archDst);
            if (it != archToPhysMap.end() && it->second == re.destPhysReg) {
                // Restore the previous mapping
                if (re.oldPhysReg != NO_REG) {
                    it->second = re.oldPhysReg;
                } else {
                    archToPhysMap.erase(it);
                }
            }

            // CRITICAL: Push to FRONT to maintain deterministic allocation order
            // Also, X0 (NUM_PHYS_REG) should never be in the free list
            if (re.destPhysReg < NUM_PHYS_REG) {
                physicalRegisterFile[re.destPhysReg].ready = true;
                physicalRegisterFile[re.destPhysReg].robTag = NO_ROB;
                // Re-insert at the FRONT so the next instruction re-uses it
                availablePhysRegs.push_front(re.destPhysReg);
            }
        }
        re.busy = false;
        --robDepth;
    }

    // Reset the write pointer to the instruction right after the branch
    robWritePtr = robNext(mispredROBIdx);

    // Flush reservation stations of squashed instructions
    auto flushFU = [&](FunctionalUnit &fu) {
        for (auto &rs : fu.rs) {
            if (rs.busy &&
                !reorderBuffer[rs.robTag].busy) {
                rs.busy        = false;
                rs.resultReady = false;
                rs.executing   = false;
            }
        }
    };

    flushFU(fuINT);
    flushFU(fuLoad);
    flushFU(fuStore);
    flushFU(fuFPAdd);
    flushFU(fuFPMult);
    flushFU(fuFPDiv);
    flushFU(fuBU);

    // Also mark any in-flight wrong-path instructions in IQ and fetch buffer as squashed.
    for (const auto &inst : instrQueue) {
        squashedSeqs.insert(inst.seq);
    }
    for (const auto &inst : fetchBuffer) {
        squashedSeqs.insert(inst.seq);
    }

    instrQueue.clear();
    fetchBuffer.clear();

    // Remove stalls for all squashed instructions (ROB, IQ, and fetch buffer).
    int stallsToRemove = 0;
    for (int seq : squashedSeqs) {
        auto it = g_instructionRsStalls.find(seq);
        if (it != g_instructionRsStalls.end()) {
            stallsToRemove += it->second;
            g_instructionRsStalls.erase(it);
        }
    }
    rsStalls -= stallsToRemove;

    // ROB stalls from this cycle's speculative instructions are still removed.
    robStalls -= g_cycleRobStalls;
    g_cycleRobStalls = 0;
    g_cycleRsStalls = 0;

    // Remove pending CDB entries for squashed instructions
    auto it = pendingCDB.begin();
    while (it != pendingCDB.end()) {
        if (!reorderBuffer[it->robTag].busy) {
            it = pendingCDB.erase(it);
        } else {
            ++it;
        }
    }
}


// STATISTICS AND SERIALIZATION
// ==========================================================================
// Output simulation results for analysis and debugging.

void Simulator::printStats() {
    std::cout << "Total CCs:  " << totalCycles << "\n";
    std::cout << "RS Stalls:  " << rsStalls    << "\n";
    std::cout << "ROB Stalls: " << robStalls   << "\n";
}

// Serialize final machine state to JSON format for external analysis
void Simulator::serializeJSON(std::ofstream *output) {
    nlohmann::json j;
    
    // Store cycle and stall statistics
    j["total_cycles"] = totalCycles;
    j["rs_stalls"]    = rsStalls;
    j["rob_stalls"]   = robStalls;
    j["cdb_utilized"] = cdbUsed;
    j["cdb_total"]    = cdbTotal;

    // Store data memory state
    for (int i = 0; i < MAX_MEM_SIZE; ++i) {
        j["data_memory"][i] = dataMemory[i];
    }
    
    // Store physical register file state
    for (int i = 0; i < NUM_PHYS_REG; ++i) {
        j["register_file"][i] = physicalRegisterFile[i].value;
    }

    // Store architectural register state (resolved through rename table)
    for (const auto &kv : archToPhysMap) {
        const std::string name =
            (kv.first.type == ArchitecturalRegister::X ? "X" : "F") +
            std::to_string(kv.first.num);
        j["arch_registers"][name] = physicalRegisterFile[kv.second].value;
    }

    *output << j.dump(4) << "\n";
}
