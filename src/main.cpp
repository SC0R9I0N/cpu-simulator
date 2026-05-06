#include "Simulator.h"
#include "XLSXEventLogger.h"
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#ifdef _WIN32
#include <getopt.h>
#else
#include <unistd.h>
#endif

void print_usage_info()
{
	printf("USAGE: sim [OPTIONS]\n");
	printf("Runs a CPU simulation given a CPU configuration file and a program file.\n\n");
	printf("  -h           this help screen.\n");
	printf("  -d           debug output (shows pipeline on each cycle).\n");
	printf("  -c file      [Required] uses file as configuration file.\n");
	printf("  -p file      [Required] uses file as program file.\n");
	printf("  -j file      [Optional] uses file as JSON output file.\n");
	printf("  -x file      [Optional] uses file as XLSX output file.\n");
}

int main(int argc, char *argv[])
{
	char *program_file_name = NULL;
	char *config_file_name = NULL;
	char *json_file_name = NULL;
	char *excel_file_name = NULL;
	char c;
	
	while ((c = getopt(argc, argv, "hvdc:p:j:x:")) != -1)
	{
		switch (c)
		{
		case 'h':
			print_usage_info();
			return 0;
		case 'd':
			debug = true;
			break;
		case 'p':
			program_file_name = optarg;
			break;
		case 'c':
			config_file_name = optarg;
			break;
		case 'j':
			json_file_name = optarg;
			break;
		case 'x':
			excel_file_name = optarg;
			break;
		case '?':
			if (optopt == 'j' || optopt == 'c' || optopt == 'p' || optopt == 'x')
				fprintf(stderr, "Option -%c requires an argument.\n", optopt);
			else if (isprint(optopt))
				fprintf(stderr, "Unknown option `-%c'.\n", optopt);
			else
				fprintf(stderr,
						"Unknown option character `\\x%x'.\n",
						optopt);
			return 1;
		default:
			abort();
		}
	}

	if (config_file_name == NULL || program_file_name == NULL)
	{
		print_usage_info();
		exit(1);
	}
	std::ifstream program(program_file_name);
	if (!program.is_open())
	{
		std::cerr << "Failed to open " << program_file_name << " for reading." << std::endl;
		return 1;
	}
	if (!parse_config(config_file_name))
	{
		fprintf(stderr, "Error while parsing config file %s.\n", config_file_name);
		exit(1);
	}

	Simulator *sim = new Simulator(&program, config);

	// Set up hard coded X0 "Zero" register with value 0.
	sim->archToPhysMap[ArchitecturalRegister{REG_X, 0}] = NUM_PHYS_REG;
	sim->physicalRegisterFile[NUM_PHYS_REG].value = 0;

	XLSXEventLogger *xlsx_logger = NULL;
	if (excel_file_name != NULL)
		xlsx_logger = new XLSXEventLogger(excel_file_name);
	

	// Run the simulator until completion
	sim->run();

	if (xlsx_logger)
	{
		xlsx_logger->close();
	}

	// Print out simulation results and statistics
	std::cout << "Simulation complete!" << std::endl;
	sim->printStats();
	sim->dump();
	if (json_file_name != NULL)
	{
		std::ofstream json_output(json_file_name);
		if (!json_output.is_open())
		{
			std::cerr << "Failed to open " << json_file_name << " for writing JSON." << std::endl;
			return 1;
		}
		sim->serializeJSON(&json_output);
		json_output.close();
	}
	if (xlsx_logger)
	{
		xlsx_logger->close();
		delete xlsx_logger;
	}
	delete sim;

	return 0;
}
