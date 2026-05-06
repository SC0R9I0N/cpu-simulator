/** Code by @author Wonsun Ahn
*
 * Functions used to parse and read in the processor configuration file.
 */

#include <assert.h>
#include "Config.h"

Config *config;
bool verbose = false;
bool debug = false;

int parse_config(const char *config_file_name)
{
	GKeyFile *keyfile;
	GError *error = NULL;

	/* Create a new GKeyFile object and a bitwise list of flags. */
	keyfile = g_key_file_new ();

	/* Load the GKeyFile from keyfile.conf or return. */
	if (!g_key_file_load_from_file (keyfile, config_file_name, G_KEY_FILE_NONE, &error))
	{
		g_error ("%s", error->message);
		return 0;
	}

	config = g_slice_new (Config);

	config->NF = g_key_file_get_integer (keyfile, "pipeline", "NF", NULL);
	assert(config->NF > 0);  // fetch width = 0 is not supported
	config->NI = g_key_file_get_integer (keyfile, "pipeline", "NI", NULL);
	assert(config->NI > 0);
	config->NW = g_key_file_get_integer (keyfile, "pipeline", "NW", NULL);
	assert(config->NW > 0);
	config->NR = g_key_file_get_integer (keyfile, "pipeline", "NR", NULL);
	assert(config->NR > 0);
	config->NB = g_key_file_get_integer (keyfile, "pipeline", "NB", NULL);
	assert(config->NB > 0);
	config->NC = g_key_file_get_integer (keyfile, "pipeline", "NC", NULL);
	assert(config->NC > 0);

	g_key_file_free(keyfile);

	return 1;
}