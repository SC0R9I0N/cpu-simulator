#include "../include/Simulator.h"
#include <cstdarg>
#include <iomanip>
#include <iostream>

template <typename T>
void printElement(T t, const int &width)
{
	std::cout << std::left << std::setw(width) << std::setfill(' ') << t;
}

std::string getPRegName(int regNum)
{
	if (regNum == NUM_PHYS_REG)
		return "$ZERO";
	return "$" + std::to_string(regNum);
}

void dumpDataMemory(double *dataMemory)
{
	std::cout << "Data Cache\n";
	std::cout << "\tAddress\tValue" << std::endl;
	for (int i = 0; i < MAX_MEM_SIZE - 1; i++)
	{
		if (dataMemory[i] != 0)
			std::cout << "\t" << i << ":\t" << (int)dataMemory[i] << "\n";
	}
}

void dumpRegisters(RegisterFileEntry *registers, std::map<ArchitecturalRegister, int> archToPhysMap)
{
	// std::cout
	// 	<< "Register Mapping\nNote: only registers that are written to (at least once) are shown."
	// 	<< "\nFor registers used only as operands (not written to), assume they are mapped to "
	// 	   "register with value = 0."
	// 	<< std::endl;

	std::cout << "Register Mapping" << std::endl;
	for (auto &v : archToPhysMap)
	{
		switch (v.first.type)
		{
		case ArchitecturalRegister::X:
			std::cout << "\tX" << v.first.num << " -> " << getPRegName(v.second) << " = "
					  << registers[v.second].value << std::endl;
			break;
		case ArchitecturalRegister::F:
			std::cout << "\tF" << v.first.num << " -> " << getPRegName(v.second) << " = "
					  << registers[v.second].value << std::endl;
			break;
		}
	}

	if (debug)
	{
		std::cout << "=========Physical Registers=========" << std::endl;
		std::cout << "\tRegister ,  Value" << std::endl;
		std::cout << std::boolalpha;
		for (int i = 0; i < NUM_PHYS_REG; i++)
		{
			std::cout << "\t";
			printElement(i, 10);
			// printElement(registers[i].busy, 6);
			// std::cout << " ";
			// printElement(registers[i].ROBNum, 5);
			std::cout << " ";
			printElement(registers[i].value, 5);
			std::cout << std::endl;
		}
	}
}

/**
 * Prints the current state of the data and instruction cache
 * Used for debugging purposes (no-argument version)
 */
void Simulator::dump()
{
	dump(2, DEBUG_DCACHE, DEBUG_REGISTERS);
}

/**
 * Prints the current state of the data and instruction cache
 * Used for debugging purposes (variadic version)
 */
void Simulator::dump(int argc...)
{
	va_list args;
	va_start(args, argc);
	while (argc > 0)
	{
		argc--;
		enum DebugArg arg = (enum DebugArg)va_arg(args, int);
		switch (arg)
		{
		case DEBUG_DCACHE:
			dumpDataMemory(this->dataMemory);
			break;
		case DEBUG_REGISTERS:
			dumpRegisters(this->physicalRegisterFile, this->archToPhysMap);
			break;
		}
	}
	va_end(args);
}