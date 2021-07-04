/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Anthony Minessale II <anthm@freeswitch.org>
 * Neal Horman <neal at wanlink dot com>
 *
 *
 * mod_find_channel.c -- Framework Demo Module
 *
 */
#include <switch.h>

#include <sys/ioctl.h>
#include <linux/if.h>
#include <linux/if_tun.h>

/* Prototypes */
SWITCH_MODULE_LOAD_FUNCTION(mod_find_channel_load);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_find_channel_runtime);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_find_channel_shutdown);

/* SWITCH_MODULE_DEFINITION(name, load, shutdown, runtime)
 * Defines a switch_loadable_module_function_table_t and a static const char[] modname
 */
SWITCH_MODULE_DEFINITION(mod_find_channel, mod_find_channel_load, mod_find_channel_shutdown, mod_find_channel_runtime);

static struct {
    int bAlive;
} globals;

struct stream_format {
	char *http;           /* http cmd (from xmlrpc)                                              */
	char *query;          /* http query (cmd args)                                               */
	switch_bool_t api;    /* flag: define content type for http reply e.g. text/html or text/xml */
	switch_bool_t html;   /* flag: format as html                                                */
	char *nl;             /* newline to use: html "<br>\n" or just "\n"                          */
};
typedef struct stream_format stream_format;

struct holder {
	char * delim;
	char * var_name;
	char * var_value;
	switch_stream_handle_t *stream;
	uint32_t count;
	int print_title;
	switch_xml_t xml;
	cJSON *json;
	int rows;
	int justcount;
	stream_format *format;
};

static int find_channel_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	struct holder *holder = (struct holder *) pArg;
	int x;
	switch_core_session_t *psession = NULL;

	char *uuid = switch_str_nil(argv[0]);
	if ((psession = switch_core_session_locate(uuid))) {
		switch_channel_t *channel = switch_core_session_get_channel(psession);
		const char *value = switch_channel_get_variable(channel, holder->var_name);
		if (value) {
			holder->stream->write_function(holder->stream, "Compare: %s = %s ? %s\n", holder->var_name, holder->var_value, value);
			if (strcasecmp(value, holder->var_value) == 0) {
				for (x = 0; x < argc ; x++) {
					char *val = switch_str_nil(argv[x]);
					holder->stream->write_function(holder->stream, "%s%s", val, x == (argc - 1) ? "\n" : holder->delim);
				}
			}
		}
	}

	holder->count++;
	return 0;
}

static switch_status_t do_config(switch_bool_t reload)
{
	memset(&globals, 0, sizeof(globals));

//	if (switch_xml_config_parse_module_settings("find_chennel.conf", reload, instructions) != SWITCH_STATUS_SUCCESS) {
//		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Could not open vpn.conf\n");
//		return SWITCH_STATUS_FALSE;
//	}

	return SWITCH_STATUS_SUCCESS;
}

#define FIND_CHANNEL_SYNTAX "find_channel <variable_name> <variable_value>"
SWITCH_STANDARD_API(find_channel_function)
{
	char *errmsg = NULL;
	switch_cache_db_handle_t *db;
	struct holder holder = { 0 };
	char sql[1024];
	char *mydata = NULL, *argv[6] = { 0 };
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (switch_core_db_handle(&db) != SWITCH_STATUS_SUCCESS) {
		stream->write_function(stream, "%s", "-ERR Database error!\n");
		return SWITCH_STATUS_SUCCESS;
	}

	holder.stream = stream;
	holder.justcount = 0;
	holder.delim = ",";

	if (cmd && *cmd && (mydata = strdup(cmd))) {
		switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
		holder.var_name = argv[0];
		holder.var_value = argv[1];
	}

	if (!holder.var_name && !holder.var_value) {
		stream->write_function(stream, "-USAGE: %s\n", FIND_CHANNEL_SYNTAX);
		goto end;
	}

	switch_snprintfv(sql, sizeof(sql), "select * from channels where hostname='%q' order by created_epoch", switch_core_get_switchname());

	switch_cache_db_execute_sql_callback(db, sql, find_channel_callback, &holder, &errmsg);

	if (errmsg) {
		stream->write_function(stream, "-ERR SQL Error [%s]\n", errmsg);
		free(errmsg);
		errmsg = NULL;
		goto end;
	}

end:
	switch_safe_free(mydata);
	switch_cache_db_release_db_handle(&db);

	return status;
}

/* Macro expands to: switch_status_t mod_find_channel_load(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool) */
SWITCH_MODULE_LOAD_FUNCTION(mod_find_channel_load)
{
	switch_api_interface_t *api_interface;
	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	// Set global runtime params
	globals.bAlive = TRUE;

	do_config(SWITCH_FALSE);

	SWITCH_ADD_API(api_interface, "find_channel", "Find channel uuid by variable API", find_channel_function, FIND_CHANNEL_SYNTAX);

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

/*
  Called when the system shuts down
  Macro expands to: switch_status_t mod_find_channel_shutdown() */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_find_channel_shutdown)
{
	/* Cleanup dynamically allocated config settings */
	globals.bAlive = FALSE;
	return SWITCH_STATUS_SUCCESS;
}


/*
  If it exists, this is called in it's own thread when the module-load completes
  If it returns anything but SWITCH_STATUS_TERM it will be called again automatically
  Macro expands to: switch_status_t mod_find_channel_runtime()
*/
SWITCH_MODULE_RUNTIME_FUNCTION(mod_find_channel_runtime)
{
	while(globals.bAlive)
	{
		switch_cond_next();
	}

	return SWITCH_STATUS_TERM;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet
 */
