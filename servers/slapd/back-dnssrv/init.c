/* init.c - initialize ldap backend */
/* $OpenLDAP$ */
/*
 * Copyright 2000 The OpenLDAP Foundation, All Rights Reserved.
 * COPYING RESTRICTIONS APPLY, see COPYRIGHT file
 */

#include "portable.h"

#include <stdio.h>

#include <ac/socket.h>

#include "slap.h"
#include "back-dnssrv.h"

#ifdef SLAPD_DNSSRV_DYNAMIC

int back_dnssrv_LTX_init_module(int argc, char *argv[]) {
    BackendInfo bi;

    memset( &bi, 0, sizeof(bi) );
    bi.bi_type = "dnssrv";
    bi.bi_init = dnssrv_back_initialize;

    backend_add(&bi);
    return 0;
}

#endif /* SLAPD_DNSSRV_DYNAMIC */

int
dnssrv_back_initialize(
    BackendInfo	*bi
)
{
	bi->bi_open = 0;
	bi->bi_config = 0;
	bi->bi_close = 0;
	bi->bi_destroy = 0;

	bi->bi_db_init = dnssrv_back_db_init;
	bi->bi_db_config = dnssrv_back_db_config;
	bi->bi_db_open = 0;
	bi->bi_db_close = 0;
	bi->bi_db_destroy = dnssrv_back_db_destroy;

	bi->bi_op_bind = dnssrv_back_bind;
	bi->bi_op_unbind = 0;
	bi->bi_op_search = dnssrv_back_search;
	bi->bi_op_compare = dnssrv_back_compare;
	bi->bi_op_modify = dnssrv_back_modify;
	bi->bi_op_modrdn = dnssrv_back_modrdn;
	bi->bi_op_add = dnssrv_back_add;
	bi->bi_op_delete = dnssrv_back_delete;
	bi->bi_op_abandon = 0;

	bi->bi_extended = 0;

	bi->bi_acl_group = 0;

#ifdef HAVE_CYRUS_SASL
	bi->bi_sasl_authorize = 0;
	bi->bi_sasl_getsecret = 0;
	bi->bi_sasl_putsecret = 0;
#endif /* HAVE_CYRUS_SASL */

	bi->bi_connection_init = 0;
	bi->bi_connection_destroy = 0;

	return 0;
}

int
dnssrv_back_db_init(
    Backend	*be
)
{
#if 0
	struct ldapinfo	*li;

	li = (struct ldapinfo *) ch_calloc( 1, sizeof(struct ldapinfo) );
	ldap_pvt_thread_mutex_init( &li->conn_mutex );

	be->be_private = li;

	return li == NULL;
#else
	return 0;
#endif
}

int
dnssrv_back_db_destroy(
    Backend	*be
)
{
#if 0
	struct ldapinfo	*li;

	if (be->be_private) {
		li = (struct ldapinfo *)be->be_private;
		if (li->host) {
			free(li->host);
			li->host = NULL;
		}
		ldap_pvt_thread_mutex_destroy( &li->conn_mutex );
	}

	free( be->be_private );
	return 0;
#else
	return 0;
#endif
}
