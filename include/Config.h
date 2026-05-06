#ifndef CONFIG_H
#define CONFIG_H

#include <glib.h>
#include <glib/gprintf.h>
#include <cstdio>

typedef struct
{
	int NF; // Fetch Width
	int NI; // Instruction Queue Size
	int NW; // Number of Instructions that can be dispatched per cycle
	int NR; // Reorder Buffer Size
	int NB; // Width of Common Data Buses (CDB)
	int NC; // Number of Instructions that can commit from the ROB per cycle
} Config;

int parse_config(const char *config_file_name);

extern Config *config;
extern bool verbose;
extern bool debug;

#endif /* #define CONFIG_H */
