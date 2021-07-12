/*
** Zabbix
** Copyright (C) 2001-2021 Zabbix SIA
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
**/

#include "log.h"
#include "zbxjson.h"
#include "db.h"
#include "dbcache.h"

#include "audit.h"

#define	AUDIT_DETAILS_KEY_LEN	100

static void	add_str_json(struct zbx_json *json, const char *key, const char *val)
{
	zbx_json_addarray(json, key);
	zbx_json_addstring(json, NULL, "add", ZBX_JSON_TYPE_STRING);
	zbx_json_addstring(json, NULL, val, ZBX_JSON_TYPE_STRING);
	zbx_json_close(json);
}

static	void add_uint64_json(struct zbx_json *json, const char *key, const uint64_t val)
{
	zbx_json_addarray(json, key);
	zbx_json_addstring(json, NULL, "add", ZBX_JSON_TYPE_STRING);
	zbx_json_adduint64(json, NULL, val);
	zbx_json_close(json);
}

/******************************************************************************
 *                                                                            *
 * Function: auditlog_global_script                                           *
 *                                                                            *
 * Purpose: record global script execution results into audit log             *
 *                                                                            *
 * Comments: 'hostid' should be always > 0. 'eventid' is > 0 in case of       *
 *           "manual script on event"                                         *
 *                                                                            *
 ******************************************************************************/
int	zbx_auditlog_global_script(unsigned char script_type, unsigned char script_execute_on,
		const char *script_command_orig, zbx_uint64_t hostid, const char *hostname, zbx_uint64_t eventid,
		zbx_uint64_t proxy_hostid, zbx_uint64_t userid, const char *username, const char *clientip,
		const char *output, const char *error)
{
	int	ret = SUCCEED;
	char	auditid_cuid[CUID_LEN], execute_on_s[MAX_ID_LEN + 1], hostid_s[MAX_ID_LEN + 1],
		eventid_s[MAX_ID_LEN + 1], proxy_hostid_s[MAX_ID_LEN + 1];

	struct zbx_json	details_json;
	zbx_config_t	cfg;

	zabbix_log(LOG_LEVEL_DEBUG, "In %s()", __func__);

	zbx_config_get(&cfg, ZBX_CONFIG_FLAGS_AUDIT_LOGGING_ENABLED);

	if (ZBX_AUDIT_LOGGING_ENABLED != cfg.audit_logging_enabled)
		goto out;

	zbx_new_cuid(auditid_cuid);

	zbx_json_initarray(&details_json, ZBX_JSON_STAT_BUF_LEN);
	zbx_json_addobject(&details_json, NULL);

	zbx_snprintf(execute_on_s, sizeof(execute_on_s), "%hhu", script_execute_on);

	add_str_json(&details_json, "script.execute_on", execute_on_s);

	if (0 != eventid)
	{
		zbx_snprintf(eventid_s, sizeof(eventid_s), ZBX_FS_UI64, eventid);
		add_str_json(&details_json, "script.eventid", eventid_s);
	}

	zbx_snprintf(hostid_s, sizeof(hostid_s), ZBX_FS_UI64, hostid);
	add_str_json(&details_json, "script.hostid", hostid_s);

	if (0 != proxy_hostid)
	{
		zbx_snprintf(proxy_hostid_s, sizeof(proxy_hostid_s), ZBX_FS_UI64, proxy_hostid);
		add_str_json(&details_json, "script.proxy_hostid", proxy_hostid_s);
	}

	if (ZBX_SCRIPT_TYPE_WEBHOOK != script_type)
		add_str_json(&details_json, "script.command", script_command_orig);

	if (NULL != output)
		add_str_json(&details_json, "script.output", output);

	if (NULL != error)
		add_str_json(&details_json, "script.error", error);

	zbx_json_close(&details_json);

	if (ZBX_DB_OK > DBexecute("insert into auditlog (auditid,userid,username,clock,action,ip,resourceid,"
			"resourcename,resourcetype,recordsetid,details) values ('%s'," ZBX_FS_UI64 ",'%s',%d,'%d','%s',"
			ZBX_FS_UI64 ",'%s',%d,'%s','%s' )", auditid_cuid, userid, username, (int)time(NULL),
			AUDIT_ACTION_EXECUTE, clientip, hostid, hostname, AUDIT_RESOURCE_SCRIPT, auditid_cuid,
			details_json.buffer))
	{
		ret = FAIL;
	}

	zbx_json_free(&details_json);
out:
	zabbix_log(LOG_LEVEL_DEBUG, "End of %s():%s", __func__, zbx_result_string(ret));

	return ret;
}

zbx_hashset_t zbx_audit;

typedef struct zbx_audit_entry
{
	zbx_uint64_t	id;
	char		*name;
	struct zbx_json	details_json;
	int		audit_action;
	int		resource_type;
} zbx_audit_entry_t;

static unsigned	zbx_audit_hash_func(const void *data)
{
	const zbx_audit_entry_t	* const *audit_entry = (const zbx_audit_entry_t * const *)data;

	return ZBX_DEFAULT_UINT64_HASH_ALGO(&((*audit_entry)->id), sizeof((*audit_entry)->id),
			ZBX_DEFAULT_HASH_SEED);
}

static int	zbx_audit_compare_func(const void *d1, const void *d2)
{
	const zbx_audit_entry_t	* const *audit_entry_1 = (const zbx_audit_entry_t * const *)d1;
	const zbx_audit_entry_t	* const *audit_entry_2 = (const zbx_audit_entry_t * const *)d2;

	ZBX_RETURN_IF_NOT_EQUAL((*audit_entry_1)->id, (*audit_entry_2)->id);

	return 0;
}

static void	zbx_audit_clean(void)
{
	zbx_hashset_iter_t	iter;
	zbx_audit_entry_t	**audit_entry;

	zbx_hashset_iter_reset(&zbx_audit, &iter);

	while (NULL != (audit_entry = (zbx_audit_entry_t **)zbx_hashset_iter_next(&iter)))
	{
		zbx_json_free(&((*audit_entry)->details_json));
		zbx_free((*audit_entry)->name);
		zbx_free(*audit_entry);
	}

	zbx_hashset_destroy(&zbx_audit);
}

void	zbx_audit_init(void)
{
#define AUDIT_HASHSET_DEF_SIZE	100
	zbx_hashset_create(&zbx_audit, AUDIT_HASHSET_DEF_SIZE, zbx_audit_hash_func, zbx_audit_compare_func);
#undef AUDIT_HASHSET_DEF_SIZE
}

void	zbx_audit_flush(void)
{
	char			audit_cuid[CUID_LEN], recsetid_cuid[CUID_LEN];
	zbx_hashset_iter_t	iter;
	zbx_audit_entry_t	**audit_entry;
	zbx_db_insert_t		db_insert_audit;

	zbx_new_cuid(recsetid_cuid);
	zbx_hashset_iter_reset(&zbx_audit, &iter);

	zbx_db_insert_prepare(&db_insert_audit, "auditlog", "auditid", "userid", "clock", "action", "ip",
			"resourceid","resourcename","resourcetype","recordsetid","details", NULL);

	while (NULL != (audit_entry = (zbx_audit_entry_t **)zbx_hashset_iter_next(&iter)))
	{
		zbx_new_cuid(audit_cuid);

		zabbix_log(LOG_LEVEL_INFORMATION, "BADGER BUFFER: ->%s<-", (*audit_entry)->details_json.buffer);

		if (AUDIT_ACTION_UPDATE != (*audit_entry)->audit_action ||
				0 != strcmp((*audit_entry)->details_json.buffer, "{}"))
		{
			zbx_json_close(&((*audit_entry)->details_json));
			zabbix_log(LOG_LEVEL_INFORMATION, "AAAAAAA: ->%s<-", audit_cuid);
			zbx_db_insert_add_values(&db_insert_audit, audit_cuid, USER_TYPE_SUPER_ADMIN,
					(int)time(NULL), (*audit_entry)->audit_action, "", (*audit_entry)->id,
					(*audit_entry)->name, (*audit_entry)->resource_type,
					recsetid_cuid, (*audit_entry)->details_json.buffer);
		}
	}

	zbx_db_insert_execute(&db_insert_audit);
	zbx_db_insert_clean(&db_insert_audit);

	zbx_audit_clean();
}

void	zbx_audit_update_json_add_string(const zbx_uint64_t id, const char *key, const char *value)
{
	zbx_audit_entry_t	local_audit_entry, **found_audit_entry;
	zbx_audit_entry_t	*local_audit_entry_x = &local_audit_entry;

	local_audit_entry.id = id;

	found_audit_entry = (zbx_audit_entry_t**)zbx_hashset_search(&zbx_audit,
			&(local_audit_entry_x));

	if (NULL == found_audit_entry)
	{
		THIS_SHOULD_NEVER_HAPPEN;
		exit(EXIT_FAILURE);
	}

	add_str_json(&((*found_audit_entry)->details_json), key, value);
}

void	zbx_audit_update_json_add_uint64(const zbx_uint64_t id, const char *key, const uint64_t value)
{
	zbx_audit_entry_t	local_audit_entry, **found_audit_entry;
	zbx_audit_entry_t	*local_audit_entry_x = &local_audit_entry;

	local_audit_entry.id = id;

	found_audit_entry = (zbx_audit_entry_t**)zbx_hashset_search(&zbx_audit,
			&(local_audit_entry_x));
	if (NULL == found_audit_entry)
	{
		THIS_SHOULD_NEVER_HAPPEN;
		exit(EXIT_FAILURE);
	}

	add_uint64_json(&((*found_audit_entry)->details_json), key, value);
}

void	zbx_audit_host_add_interfaces(zbx_uint64_t hostid, zbx_uint64_t interfaceid, zbx_uint64_t main_,
		zbx_uint64_t type, zbx_uint64_t useip, const char *ip, const char *dns, zbx_uint64_t port)
{
	char	audit_key_main[AUDIT_DETAILS_KEY_LEN], audit_key_type[AUDIT_DETAILS_KEY_LEN],
		audit_key_useip[AUDIT_DETAILS_KEY_LEN], audit_key_ip[AUDIT_DETAILS_KEY_LEN],
		audit_key_dns[AUDIT_DETAILS_KEY_LEN], audit_key_port[AUDIT_DETAILS_KEY_LEN];

	zbx_snprintf(audit_key_main,  AUDIT_DETAILS_KEY_LEN, "host.interfaces[%lu].main", interfaceid);
	zbx_snprintf(audit_key_type,  AUDIT_DETAILS_KEY_LEN, "host.interfaces[%lu].type", interfaceid);
	zbx_snprintf(audit_key_useip, AUDIT_DETAILS_KEY_LEN, "host.interfaces[%lu].useip", interfaceid);
	zbx_snprintf(audit_key_ip,    AUDIT_DETAILS_KEY_LEN, "host.interfaces[%lu].ip", interfaceid);
	zbx_snprintf(audit_key_dns,   AUDIT_DETAILS_KEY_LEN, "host.interfaces[%lu].dns", interfaceid);
	zbx_snprintf(audit_key_port,  AUDIT_DETAILS_KEY_LEN, "host.interfaces[%lu].port", interfaceid);

	zbx_audit_update_json_add_uint64(hostid, audit_key_main, main_);
	zbx_audit_update_json_add_uint64(hostid, audit_key_type, type);
	zbx_audit_update_json_add_uint64(hostid, audit_key_useip, useip);
	zbx_audit_update_json_add_string(hostid, audit_key_ip, ip);
	zbx_audit_update_json_add_string(hostid, audit_key_dns, dns);
	zbx_audit_update_json_add_uint64(hostid, audit_key_port, port);
}


void	zbx_audit_host_update_snmp_interfaces(zbx_uint64_t hostid, zbx_uint64_t version, zbx_uint64_t bulk,
		const char *community, const char *securityname, zbx_uint64_t securitylevel, const char *authpassphrase,
		const char *privpassphrase, zbx_uint64_t authprotocol, zbx_uint64_t privprotocol,
		const char *contextname, zbx_uint64_t interfaceid)
{
	char	audit_key_version[AUDIT_DETAILS_KEY_LEN], audit_key_bulk[AUDIT_DETAILS_KEY_LEN],
		audit_key_community[AUDIT_DETAILS_KEY_LEN], audit_key_securityname[AUDIT_DETAILS_KEY_LEN],
		audit_key_securitylevel[AUDIT_DETAILS_KEY_LEN], audit_key_authpassphrase[AUDIT_DETAILS_KEY_LEN],
		audit_key_privpassphrase[AUDIT_DETAILS_KEY_LEN], audit_key_authprotocol[AUDIT_DETAILS_KEY_LEN],
		audit_key_privprotocol[AUDIT_DETAILS_KEY_LEN], audit_key_contextname[AUDIT_DETAILS_KEY_LEN];

	zbx_snprintf(audit_key_version,  AUDIT_DETAILS_KEY_LEN, "host.interfaces[%lu].details.version", interfaceid);
	zbx_snprintf(audit_key_bulk,  AUDIT_DETAILS_KEY_LEN, "host.interfaces[%lu].details.bulk", interfaceid);
	zbx_snprintf(audit_key_community, AUDIT_DETAILS_KEY_LEN, "host.interfaces[%lu].details.community", interfaceid);
	zbx_snprintf(audit_key_securityname,    AUDIT_DETAILS_KEY_LEN, "host.interfaces[%lu].details.securityname",
			interfaceid);
	zbx_snprintf(audit_key_securitylevel,   AUDIT_DETAILS_KEY_LEN, "host.interfaces[%lu].details.securitylevel",
			interfaceid);
	zbx_snprintf(audit_key_authpassphrase,  AUDIT_DETAILS_KEY_LEN, "host.interfaces[%lu].details.authpassphrase",
			interfaceid);
	zbx_snprintf(audit_key_privpassphrase,  AUDIT_DETAILS_KEY_LEN, "host.interfaces[%lu].details.privpassphrase",
			interfaceid);
	zbx_snprintf(audit_key_authprotocol,  AUDIT_DETAILS_KEY_LEN, "host.interfaces[%lu].details.authprotocol",
			interfaceid);
	zbx_snprintf(audit_key_privprotocol,  AUDIT_DETAILS_KEY_LEN, "host.interfaces[%lu].details.privprotocol",
			interfaceid);
	zbx_snprintf(audit_key_contextname,  AUDIT_DETAILS_KEY_LEN, "host.interfaces[%lu].details.contextname",
			interfaceid);

	zbx_audit_update_json_add_uint64(hostid, audit_key_version, version);
	zbx_audit_update_json_add_uint64(hostid, audit_key_bulk, bulk);
	zbx_audit_update_json_add_string(hostid, audit_key_community, community);
	zbx_audit_update_json_add_string(hostid, audit_key_securityname, securityname);
	zbx_audit_update_json_add_uint64(hostid, audit_key_securitylevel, securitylevel);
	zbx_audit_update_json_add_string(hostid, audit_key_authpassphrase, authpassphrase);
	zbx_audit_update_json_add_string(hostid, audit_key_privpassphrase, privpassphrase);
	zbx_audit_update_json_add_uint64(hostid, audit_key_authprotocol, authprotocol);
	zbx_audit_update_json_add_uint64(hostid, audit_key_privprotocol, privprotocol);
	zbx_audit_update_json_add_string(hostid, audit_key_contextname, contextname);
}

void	zbx_audit_host_update_json_add_tls_and_psk(zbx_uint64_t hostid, int tls_connect, int tls_accept,
		const char *psk_identity, const char *psk)
{
	zbx_audit_update_json_add_uint64(hostid, "host.tls_connect", (zbx_uint64_t)tls_connect);
	zbx_audit_update_json_add_uint64(hostid, "host.tls_accept", (zbx_uint64_t)tls_accept);
	zbx_audit_update_json_add_string(hostid, "host.psk_identity", psk_identity);
	zbx_audit_update_json_add_string(hostid, "host.psk", psk);
}

void	zbx_audit_host_create_entry(int audit_action, zbx_uint64_t hostid, const char *name)
{
	zbx_audit_entry_t	*local_audit_host_entry;

	local_audit_host_entry = (zbx_audit_entry_t*)zbx_malloc(NULL, sizeof(zbx_audit_entry_t));
	local_audit_host_entry->id = hostid;
	local_audit_host_entry->name = zbx_strdup(NULL, name);
	local_audit_host_entry->audit_action = audit_action;
	local_audit_host_entry->resource_type = AUDIT_RESOURCE_HOST;
	zbx_json_initarray(&(local_audit_host_entry->details_json), ZBX_JSON_STAT_BUF_LEN);
	zbx_json_addobject(&(local_audit_host_entry->details_json), NULL);
	zbx_hashset_insert(&zbx_audit, &local_audit_host_entry, sizeof(local_audit_host_entry));
}

void	zbx_audit_host_add_groups(const char *audit_details_action, zbx_uint64_t hostid, zbx_uint64_t groupid)
{
	char	audit_key_groupid[AUDIT_DETAILS_KEY_LEN];

	zbx_snprintf(audit_key_groupid, AUDIT_DETAILS_KEY_LEN, "host.groups[%lu]", groupid);
	zbx_audit_update_json_add_string(hostid, audit_key_groupid, audit_details_action);
}

void	zbx_audit_host_del(zbx_uint64_t hostid, const char *hostname)
{
	char		recsetid_cuid[CUID_LEN];

	zbx_new_cuid(recsetid_cuid);
	zbx_audit_host_create_entry(AUDIT_ACTION_DELETE, hostid, hostname);
}
