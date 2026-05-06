# CPU Simulator

---

# Introduction

## Description

The purpose of this project is to evaluate this effect of different
architecture parameters on a CPU design by simulating a modified (and
simplified) version of the PowerPc 604 and 620 architectures. We will assume a
32-bit architecture that executes a subset of the RISC V ISA which consists of
the following 10 instructions: fld, fsd, add, addi, slt, fadd, fsub, fmul,
fdiv, bne. See Appendix A in the textbook for instructions' syntax and
semantics.

Your simulator should take an input file as a command line input. This input
file, for example, prog.dat, will contain a RISC V assembly language program
(code segment). Each line in the input file is a RISC V instruction from the
aforementioned 10 instructions. Your simulator should read this input file,
recognize the instructions, recognize the different fields of the instructions,
and simulate their execution on the architecture described below in this
handout. Your will have to implement the functional+timing simulator.

Please read the following description of the simulated architecture carefully
before you start constructing your simulator.

### Simulated Architecture

The simulated architecture is a speculative, multi-issue, out of order CPU where:

a. The fetch unit fetches up to NF=4 instructions every cycle (i.e., issue
width is 4).

b. A 2-bit dynamic branch predictor (initialized to predict weakly taken(t))
with 16-entry branch target buffer (BTB) is used. It hashes the address of a
branch, L, to an entry in the BTB using bits 7-4 of L.  The simulator is
simplified by not simulating mispredicted instructions (if it were to do so,
complex features like maintaining CPU checkpoints on every branch and restoring
them on a misprediction, and flushing mispredicted instructions would have to
be implemented).  Instead, on a branch mispredict, the simulator simply stalls
the fetch until the culprit branch is resolved in the EX stage, so that no
mispredicted instructions enter the pipeline.

c. The decode unit decodes (in a separate cycle) the instructions fetched by
the fetch unit and stores the decoded instructions in an instruction queue
which can hold up to NI=16 instructions.

d. Up to NW=4 instructions can be dispatched every clock cycle to reservation
stations. The architecture has the following functional units with the shown
latencies and number of reservation stations.

| Unit      | Latency (cycles) for operation         | Reservation stations           | Instructions executing on the unit |
|-----------|----------------------------------------|--------------------------------|------------------------------------|
| INT       | 1 (integer and logic operations)       | 4                              | add, addi, slt                     |
| Load/Store| 1 for address calculation              | 2 load buffer + 2 store buffer | fld, fsd                           |
| FPadd     | 3 (pipelined FP add)                   | 3                              | fadd, fsub                         |
| FPmult    | 4 (non-pipelined FP multiply)          | 2                              | fmul                               |
| FPdiv     | 6 (non-pipelined divide)               | 1                              | fdiv                               |
| BU        | 1 (condition and target evaluation)    | 2                              | bne                                |

During dispatch, you may encounter a stall because 1) the reservation stations
of a given unit are occupied, and 2) the reorder buffers are full.  The former
is called an "RS Stall" and the latter an "ROB Stall".  Your simulator should
keep track of both statistics to help identify performance bottlenecks in the
CPU architecture.  When a stall is due to both, it is categorized as an "RS
Stall".

e. One instruction can issue and execute in each Functional Unit (FU), for a
total of 6 instructions per cycle.  If there are more than one ready
instruction in the reservation stations for the FU, then only one instruction
can execute and others will suffer FU Stalls.  The reservation stations are
scanned from first to last (e.g. INT1 ~ INT4) and the first instruction that is
ready is issued.

f. Up to NB=4 results can be written back in the WB stage.  This depends on the
width of the Common Data Busses (CDB) that forwards the results to the
reservation stations, the physical register file, and the ROB.  All
instructions except branch instructions produce a result, including store
instructions.  Store instructions produce a store value that needs to be
written back to the store queue through the CDB.  When there is contention on
the CDB, write backs are prioritized in the following order: Load, INT, FPadd,
FPmult, FPdiv, Store.  The priority is loosely based on which instruction type
is likely to be on the critical path of execution: loads produce values that
computation needs to proceed so they are prioritized and stores consume values
that are drained to memory so they are rarely on the critical path.

g. A circular reorder buffer (ROB) with NR=16 entries is used and up to NC=4
instructions can be committed from the top of the ROB in one cycle.

h. You need to perform register renaming to eliminate the false dependences in
the decode stage. Assuming we have a total of 32 physical registers (p0, p1,
p2, …p31). You will need to implement a mapping table and a free list of the
physical register as we discussed in class. Also, assuming that all of the
physical registers can be used by either integer or floating point
instructions.

i. A dedicated/separate ALU is used for the effective address calculation in
the branch unit (BU) and simultaneously, a special hardware is used to evaluate
the branch condition. Also, a dedicated/separate ALU is used for the effective
address calculation in the load/store unit. You will also need to implement
forwarding in your simulation design.

The simulator should be parameterized so that one can experiment with different
values of NF, NI, NW, NR, NB, and NC (by reading a configuration file). To
simplify the simulation, we will assume that the instruction cache line
contains NF instructions and that the entire program fits in the instruction
cache (i.e., it always takes one cycle to read a cache line). Also, the data
cache (single ported) is very large so that writing or reading a word into the
data cache always takes one cycle (i.e., eliminating the cache effect in memory
accesses).

You simulation should be both functional and timing correct. For functional, we
check the register and memory contents. For timing, we check the execution
cycles.

### Project Language

Please use C/C++ to implement your project. If you are not familiar with C/C++ and absolutely need to use another language talk to me as soon as possible.

### Project Submission

* "Project Discussion" submission on GradeScope (due 4/29 11:59 PM) - 18 points
    * Answer performance comparison questions about various CPU configurations
* "Project" submission on GradeScope (due 5/3 11:59 PM) - 82 points
    * Submit project GitHub repository to autograder.  Can submit as many times as you want.
    * Autograder score (1 point per test case diff) - 77 points
    * "Report.pdf" added to the root of the repository - 5 points
        * Include a figure with detailed text to describe the module design of your code.
        * Mark and list the key data structures used in your code.

## Comparative Analysis

After running the benchmarks with the parameters specified above, perform the
following analysis:
1) Study the effect of changing the issue and commit width to 2. That is setting
   NW=NB=2 rather than 4.
2) Study the effect of changing the fetch/decode width. That is setting NF = 2 rather than 4.
3) Study the effect of changing the NI to 4 instead of 16.
4) Study the effect of changing the number of reorder buffer entries. That is setting NR =
   4, 8, and 32
   You need to provide the results and analysis in your project report.

## Test Benchmark

Use the following as an initial benchmark (i.e. content of the input file prog.dat).

    %All the registers have the initial value of 0.
    %memory content in the form of address, value.
    0, 111
    8, 14
    16, 5
    24, 10
    100, 2
    108, 27
    116, 3
    124, 8
    200, 12

        addi R1, R0, 24
        addi R2, R0, 124
        fld F2, 200(R0)

    loop:
        fld F0, 0(R1)
        fmul F0, F0, F2
        fld F4, 0(R2)
        fadd F0, F0, F4
        fsd F0, 0(R2)
        addi R1, R1, -8
        addi R2, R2, -8
        bne R1, $0, loop

(Note that this is just a testbench for you to verify your design. Your submission should support
ALL the instructions listed in the table and you should verify and ensure the simulation
correctness for different programs that use those nine instructions. When you submit your code,
we will use more complicated programs (with multiple branches and all instructions in the table)
to test your submission).

# Building and Running

## Directory Structure and Makefile Script

Here is an overview of the directory structure:

* src/ : Implementation .cpp files for the simulator.  Start by reading main.cpp.
* include/ : Header .h and .hpp files for the simulator.  Start by reading Simulator.h.
* Makefile : The build script for the Make tool.
* configs/ : Directory where processor configuration files are.
* **diffs/** : Directory where **differences** between outputs/ and outputs_solution/ are stored.
* outputs/ : Directory where outputs after running simare stored.
* outputs_solution/ : Directory where outputs produced by sim are stored.
* outputs_verbose/ : Directory where the detailed simulation state for each cycle for the solution simulator is stored.
* outputs_xlsx/ : Directory where your .xlsx event timeline spreadsheets after running sim are stored, if you run 'make xlsx'.
* outputs_xlsx_solution/ : Directory where the solution .xlsx event timeline spreadsheets are stored.
* programs/ : Directory where RISC-V assembly programs used to test the simulator are stored.

If you are unsure about expected behavior, I suggest that you read the solution
timeline under outputs_xlsx_solution/ first, and if you need more understanding
of what happened at a particular cycle in the timeline, read the cycle-by-cycle
log under the outputs_verbose/ folder, and look into the cycle in question in
detail.

In order to build the project and run the simulations, you only need to do 'make' to invoke the 'Makefile' script:

```
make
```

The output should look like:

```
g++  -fsanitize=address -g -c -Wall -Wextra -std=c++11 -Iinclude/ `pkg-config --cflags glib-2.0` -c  src/Config.cpp -o obj/Config.o
g++  -fsanitize=address -g -c -Wall -Wextra -std=c++11 -Iinclude/ `pkg-config --cflags glib-2.0` -c  src/main.cpp -o obj/main.o
g++  -fsanitize=address -g -c -Wall -Wextra -std=c++11 -Iinclude/ `pkg-config --cflags glib-2.0` -c  src/Simulator.cpp -o obj/Simulator.o
src/Simulator.cpp: In constructor ‘Simulator::Simulator(std::ifstream*, Config*)’:
src/Simulator.cpp:6:37: warning: unused parameter ‘program’ [-Wunused-parameter]
    6 | Simulator::Simulator(std::ifstream* program, Config *c)
      |                      ~~~~~~~~~~~~~~~^~~~~~~
src/Simulator.cpp:6:54: warning: unused parameter ‘c’ [-Wunused-parameter]
    6 | Simulator::Simulator(std::ifstream* program, Config *c)
      |                                              ~~~~~~~~^
g++  -fsanitize=address -g -c -Wall -Wextra -std=c++11 -Iinclude/ `pkg-config --cflags glib-2.0` -c  src/SimulatorDebug.cpp -o obj/SimulatorDebug.o
g++  obj/Config.o obj/main.o obj/Simulator.o obj/SimulatorDebug.o -fsanitize=address `pkg-config --libs glib-2.0` -o sim
```

If successful, it will produce the executable "sim" as the above.

By invoking the "run" target, you can run the test programs against the configurations, and compare the output with the solution output:

```
make run
```

This will generate the following output:

```
Running ./sim -p programs/prog-loop.dat -c configs/1-wide.cfg -j outputs/prog-loop.1-wide.json -d > outputs/prog-loop.1-wide.out
Running ./sim -p programs/prog-noloop.dat -c configs/1-wide.cfg -j outputs/prog-noloop.1-wide.json -d > outputs/prog-noloop.1-wide.out
Running ./sim -p programs/prog-loop.dat -c configs/2-wide.cfg -j outputs/prog-loop.2-wide.json -d > outputs/prog-loop.2-wide.out
...
```

You can write your own programs under the programs/ folder and also add new
configurations under the configs/ folder and they will be added to the test
suite.  The results are stored in the outputs/ directory and also diffs with
the outputs_solution/ directory are generated and stored in the diffs/
directory.  When you debug the program, you will find these diffs useful.

If you wish to remove all files generated from your build, invoke the "clean" target:

```
make clean
```

Optionally, if you implement XLSX timeline logging of your own, your .xlsx files are generated using the 'xlsx' target:

```
make xlsx
```

## Debugging

Here are some suggestions for debugging your simulator.

### VSCode Debugger

You can use the VSCode debugger to run the simulator in debug mode.  If you
have never used the VSCode debugger before, here is a tutorial:
https://code.visualstudio.com/docs/debugtest/debugging.

You can change the debugger launch configurations by editing the
[launch.json](.vscode/launch.json) file.  Currently it is configured to run the
2-wide.cfg configuration on the prog-noloop.dat program.

You can use the standard debugging techniques such as setting break points,
stepping through code, and watching values of variables and expressions.


### Google Address Sanitizer

You might have noticed that the Makefile passes the "-fsanitize=address" flag
to gcc when you compile the program.  What this flag does is to instrument your
binary with memory access checking code so that whenever you have an illegal
memory access or memory leak it throws an exception, much like how Java throws
an exception on an illegal access.  It is meant to help you and make C++
programming feel more like Java programming, as the biggest headache with C/C++
programming often is memory management.

This instrumentation pass and associated library is called the Gooogle Address
Sanitizer (ASAN).  You can read more about it here:
https://github.com/google/sanitizers/wiki/addresssanitizer.

The expectation is that your simulator has no memory errors when submitted.  If
it does, then it is going to be a very flaky simulator that is unusable.

### Logging

You will benefit from having logging functions that output internal state of
the CPU so that you can view what is happening at each cycle.  Writing these
logging functions is a part of writing a good simulator.  Ideally, you can
enable logging by passing a command line flag.
