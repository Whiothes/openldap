/* dn2id.c - routines to deal with the dn2id index */
/* $OpenLDAP$ */
/*
 * Copyright 1998-2000 The OpenLDAP Foundation, All Rights Reserved.
 * COPYING RESTRICTIONS APPLY, see COPYRIGHT file
 */

#include "portable.h"

#include <stdio.h>
#include <ac/string.h>

#include "back-bdb.h"
#include "idl.h"

#ifndef BDB_HIER
int
bdb_dn2id_add(
	BackendDB	*be,
	DB_TXN *txn,
	const char	*pdn,
	Entry		*e )
{
	int		rc;
	DBT		key, data;
	struct bdb_info *bdb = (struct bdb_info *) be->be_private;
	DB *db = bdb->bi_dn2id->bdi_db;

	Debug( LDAP_DEBUG_TRACE, "=> bdb_dn2id_add( \"%s\", 0x%08lx )\n",
		e->e_ndn, (long) e->e_id, 0 );
	assert( e->e_id != NOID );

	DBTzero( &key );
	key.size = strlen( e->e_ndn ) + 2;
	key.data = ch_malloc( key.size );
	((char *)key.data)[0] = DN_BASE_PREFIX;
	AC_MEMCPY( &((char *)key.data)[1], e->e_ndn, key.size - 1 );

	DBTzero( &data );
	data.data = (char *) &e->e_id;
	data.size = sizeof( e->e_id );

	/* store it -- don't override */
	rc = db->put( db, txn, &key, &data, DB_NOOVERWRITE );
	if( rc != 0 ) {
		Debug( LDAP_DEBUG_ANY, "=> bdb_dn2id_add: put failed: %s %d\n",
			db_strerror(rc), rc, 0 );
		goto done;
	}

	{
		((char *)(key.data))[0] = DN_ONE_PREFIX;

		if( pdn != NULL ) {
			key.size = strlen( pdn ) + 2;
			AC_MEMCPY( &((char*)key.data)[1],
				pdn, key.size - 1 );

			rc = bdb_idl_insert_key( be, db, txn, &key, e->e_id );

			if( rc != 0 ) {
				Debug( LDAP_DEBUG_ANY,
					"=> bdb_dn2id_add: parent (%s) insert failed: %d\n",
					pdn, rc, 0 );
				goto done;
			}
		}
	}

	{
		char **subtree = dn_subtree( be, e->e_ndn );

		if( subtree != NULL ) {
			int i;
			((char *)key.data)[0] = DN_SUBTREE_PREFIX;
			for( i=0; subtree[i] != NULL; i++ ) {
				if( be_issuffix( be, subtree[i] ))
					continue;
				key.size = strlen( subtree[i] ) + 2;
				AC_MEMCPY( &((char *)key.data)[1],
					subtree[i], key.size - 1 );

				rc = bdb_idl_insert_key( be, db, txn, &key,
					e->e_id );

				if( rc != 0 ) {
					Debug( LDAP_DEBUG_ANY,
						"=> bdb_dn2id_add: subtree (%s) insert failed: %d\n",
						subtree[i], rc, 0 );
					break;
				}
			}

			charray_free( subtree );
		}
	}

done:
	ch_free( key.data );
	Debug( LDAP_DEBUG_TRACE, "<= bdb_dn2id_add: %d\n", rc, 0, 0 );
	return rc;
}

int
bdb_dn2id_delete(
	BackendDB	*be,
	DB_TXN *txn,
	const char	*pdn,
	const char	*dn,
	ID		id )
{
	int		rc;
	DBT		key;
	struct bdb_info *bdb = (struct bdb_info *) be->be_private;
	DB *db = bdb->bi_dn2id->bdi_db;

	Debug( LDAP_DEBUG_TRACE, "=> bdb_dn2id_delete( \"%s\", 0x%08lx )\n",
		dn, id, 0 );

	DBTzero( &key );
	key.size = strlen( dn ) + 2;
	key.data = ch_malloc( key.size );
	key.flags = DB_DBT_USERMEM;
	((char *)key.data)[0] = DN_BASE_PREFIX;
	AC_MEMCPY( &((char *)key.data)[1], dn, key.size - 1 );

	/* delete it */
	rc = db->del( db, txn, &key, 0 );
	if( rc != 0 ) {
		Debug( LDAP_DEBUG_ANY, "=> bdb_dn2id_delete: delete failed: %s %d\n",
			db_strerror(rc), rc, 0 );
		goto done;
	}

	{
		((char *)(key.data))[0] = DN_ONE_PREFIX;

		if( pdn != NULL ) {
			key.size = strlen( pdn ) + 2;
			AC_MEMCPY( &((char*)key.data)[1],
				pdn, key.size - 1 );

			rc = bdb_idl_delete_key( be, db, txn, &key, id );

			if( rc != 0 ) {
				Debug( LDAP_DEBUG_ANY,
					"=> bdb_dn2id_delete: parent (%s) delete failed: %d\n",
					pdn, rc, 0 );
				goto done;
			}
		}
	}

	{
		char **subtree = dn_subtree( be, dn );

		if( subtree != NULL ) {
			int i;
			((char *)key.data)[0] = DN_SUBTREE_PREFIX;
			for( i=0; subtree[i] != NULL; i++ ) {
				key.size = strlen( subtree[i] ) + 2;
				AC_MEMCPY( &((char *)key.data)[1],
					subtree[i], key.size - 1 );

				rc = bdb_idl_delete_key( be, db, txn, &key, id );

				if( rc != 0 ) {
					Debug( LDAP_DEBUG_ANY,
						"=> bdb_dn2id_delete: subtree (%s) delete failed: %d\n",
						subtree[i], rc, 0 );
					charray_free( subtree );
					goto done;
				}
			}

			charray_free( subtree );
		}
	}

done:
	ch_free( key.data );
	Debug( LDAP_DEBUG_TRACE, "<= bdb_dn2id_delete %d\n", rc, 0, 0 );
	return rc;
}

int
bdb_dn2id(
	BackendDB	*be,
	DB_TXN *txn,
	const char	*dn,
	ID *id )
{
	int		rc;
	DBT		key, data;
	struct bdb_info *bdb = (struct bdb_info *) be->be_private;
	DB *db = bdb->bi_dn2id->bdi_db;

	Debug( LDAP_DEBUG_TRACE, "=> bdb_dn2id( \"%s\" )\n", dn, 0, 0 );

	DBTzero( &key );
	key.size = strlen( dn ) + 2;
	key.data = ch_malloc( key.size );
	((char *)key.data)[0] = DN_BASE_PREFIX;
	AC_MEMCPY( &((char *)key.data)[1], dn, key.size - 1 );

	/* store the ID */
	DBTzero( &data );
	data.data = id;
	data.ulen = sizeof(ID);
	data.flags = DB_DBT_USERMEM;

	/* fetch it */
	rc = db->get( db, txn, &key, &data, bdb->bi_db_opflags );

	if( rc != 0 ) {
		Debug( LDAP_DEBUG_TRACE, "<= bdb_dn2id: get failed: %s (%d)\n",
			db_strerror( rc ), rc, 0 );
	} else {
		Debug( LDAP_DEBUG_TRACE, "<= bdb_dn2id: got id=0x%08lx\n",
			*id, 0, 0 );
	}

	ch_free( key.data );
	return rc;
}

int
bdb_dn2id_matched(
	BackendDB	*be,
	DB_TXN *txn,
	const char	*in,
	ID *id,
	char **matchedDN )
{
	int		rc;
	DBT		key, data;
	struct bdb_info *bdb = (struct bdb_info *) be->be_private;
	DB *db = bdb->bi_dn2id->bdi_db;
	const char *dn = in;
	char *tmp = NULL;

	Debug( LDAP_DEBUG_TRACE, "=> bdb_dn2id_matched( \"%s\" )\n", dn, 0, 0 );

	DBTzero( &key );
	key.size = strlen( dn ) + 2;
	key.data = ch_malloc( key.size );
	((char *)key.data)[0] = DN_BASE_PREFIX;

	/* store the ID */
	DBTzero( &data );
	data.data = id;
	data.ulen = sizeof(ID);
	data.flags = DB_DBT_USERMEM;

	*matchedDN = NULL;

	while(1) {
		AC_MEMCPY( &((char *)key.data)[1], dn, key.size - 1 );

		*id = NOID;

		/* fetch it */
		rc = db->get( db, txn, &key, &data, bdb->bi_db_opflags );

		if( rc == DB_NOTFOUND ) {
			char *pdn = dn_parent( be, dn );
			ch_free( tmp );
			tmp = NULL;

			if( pdn == NULL || *pdn == '\0' ) {
				Debug( LDAP_DEBUG_TRACE,
					"<= bdb_dn2id_matched: no match\n",
					0, 0, 0 );
				ch_free( pdn );
				break;
			}

			dn = pdn;
			tmp = pdn;
			key.size = strlen( dn ) + 2;

		} else if ( rc == 0 ) {
			if( data.size != sizeof( ID ) ) {
				Debug( LDAP_DEBUG_ANY,
					"<= bdb_dn2id_matched: get size mismatch: "
					"expected %ld, got %ld\n",
					(long) sizeof(ID), (long) data.size, 0 );
				ch_free( tmp );
			}

			if( in != dn ) {
				*matchedDN = (char *) dn;
			}

			Debug( LDAP_DEBUG_TRACE,
				"<= bdb_dn2id_matched: id=0x%08lx: %s %s\n",
				(long) *id, *matchedDN == NULL ? "entry" : "matched", dn );
			break;

		} else {
			Debug( LDAP_DEBUG_ANY,
				"<= bdb_dn2id_matched: get failed: %s (%d)\n",
				db_strerror(rc), rc, 0 );
			ch_free( tmp );
			break;
		}
	}

	ch_free( key.data );
	return rc;
}

int
bdb_dn2id_children(
	BackendDB	*be,
	DB_TXN *txn,
	const char *dn )
{
	int		rc;
	DBT		key, data;
	struct bdb_info *bdb = (struct bdb_info *) be->be_private;
	DB *db = bdb->bi_dn2id->bdi_db;
	ID		id;

	Debug( LDAP_DEBUG_TRACE, "=> bdb_dn2id_children( %s )\n",
		dn, 0, 0 );

	DBTzero( &key );
	key.size = strlen( dn ) + 2;
	key.data = ch_malloc( key.size );
	((char *)key.data)[0] = DN_ONE_PREFIX;
	AC_MEMCPY( &((char *)key.data)[1], dn, key.size - 1 );

	/* we actually could do a empty get... */
	DBTzero( &data );
	data.data = &id;
	data.ulen = sizeof(id);
	data.flags = DB_DBT_USERMEM;
	data.doff = 0;
	data.dlen = sizeof(id);

	rc = db->get( db, txn, &key, &data, bdb->bi_db_opflags );

	Debug( LDAP_DEBUG_TRACE, "<= bdb_dn2id_children( %s ): %schildren (%d)\n",
		dn,
		rc == 0 ? "" : ( rc == DB_NOTFOUND ? "no " :
			db_strerror(rc) ), rc );

	return rc;
}

int
bdb_dn2idl(
	BackendDB	*be,
	const char	*dn,
	int prefix,
	ID *ids )
{
	int		rc;
	DBT		key, data;
	struct bdb_info *bdb = (struct bdb_info *) be->be_private;
	DB *db = bdb->bi_dn2id->bdi_db;

	Debug( LDAP_DEBUG_TRACE, "=> bdb_dn2idl( \"%s\" )\n", dn, 0, 0 );

	if (prefix == DN_SUBTREE_PREFIX && be_issuffix(be, dn))
	{
		BDB_IDL_ALL(bdb, ids);
		return 0;
	}

	DBTzero( &key );
	key.size = strlen( dn ) + 2;
	key.data = ch_malloc( key.size );
	((char *)key.data)[0] = prefix;
	AC_MEMCPY( &((char *)key.data)[1], dn, key.size - 1 );

	rc = bdb_idl_fetch_key( be, db, NULL, &key, ids );

	if( rc != 0 ) {
		Debug( LDAP_DEBUG_TRACE,
			"<= bdb_dn2idl: get failed: %s (%d)\n",
			db_strerror( rc ), rc, 0 );

	} else {
		Debug( LDAP_DEBUG_TRACE,
			"<= bdb_dn2idl: id=%ld first=%ld last=%ld\n",
			(long) ids[0],
			(long) BDB_IDL_FIRST( ids ), (long) BDB_IDL_LAST( ids ) );
	}

	ch_free( key.data );
	return rc;
}
#else	/* BDB_HIER */

/* Experimental management routines for a hierarchically structured backend.
 *
 * Unsupported! Use at your own risk!
 *
 * Instead of a dn2id database, we use an id2parent database. Each entry in
 * this database is a struct diskNode, containing the ID of the node's parent
 * and the RDN of the node.
 */
typedef struct diskNode {
	ID parent;
	struct berval rdn;
	struct berval nrdn;
} diskNode;

/* In bdb_db_open() we call bdb_build_tree() which reads the entire id2parent
 * database into memory (into an AVL tree). Next we iterate through each node
 * of this tree, connecting each child to its parent. The nodes in this AVL
 * tree are a struct idNode. The immediate (Onelevel) children of a node are
 * referenced in the i_kids AVL tree. With this arrangement, there is no need
 * to maintain the DN_ONE_PREFIX or DN_SUBTREE_PREFIX database keys. Note that
 * the DN of an entry is constructed by walking up the list of i_parent
 * pointers, so no full DN is stored on disk anywhere. This makes modrdn
 * extremely efficient, even when operating on a populated subtree.
 *
 * The idNode tree is searched directly from the root when performing id to
 * entry lookups. The tree is traversed using the i_kids subtrees when
 * performing dn to id lookups.
 */
typedef struct idNode {
	ID i_id;
	struct idNode *i_parent;
	diskNode *i_rdn;
	Avlnode *i_kids;
	ldap_pvt_thread_rdwr_t i_kids_rdwr;
} idNode;

/* strcopy is like strcpy except it returns a pointer to the trailing NUL of
 * the result string. This allows fast construction of catenated strings
 * without the overhead of strlen/strcat.
 */
char *
bdb_strcopy(
	char *a,
	char *b
)
{
	if (!a || !b)
		return a;
	
	while (*a++ = *b++) ;
	return a-1;
}

/* The main AVL tree is sorted in ID order. The i_kids AVL trees are
 * sorted in lexical order. These are the various helper routines used
 * for the searches and sorts.
 */
static int
node_find_cmp(
	ID id,
	idNode *n
)
{
	return id - n->i_id;
}

static int
node_frdn_cmp(
	char *nrdn,
	idNode *n
)
{
	return strcmp(nrdn, n->i_rdn->nrdn.bv_val);
}

static int
node_add_cmp(
	idNode *a,
	idNode *b
)
{
	return a->i_id - b->i_id;
}

static int
node_rdn_cmp(
	idNode *a,
	idNode *b
)
{
	return strcmp(a->i_rdn->nrdn.bv_val, b->i_rdn->nrdn.bv_val);
}

idNode * bdb_find_id_node(
	ID id,
	Avlnode *tree
)
{
	return avl_find(tree, (const void *)id, (AVL_CMP)node_find_cmp);
}

idNode * bdb_find_rdn_node(
	char *nrdn,
	Avlnode *tree
)
{
	return avl_find(tree, (const void *)nrdn, (AVL_CMP)node_frdn_cmp);
}

/* This function links a node into its parent's i_kids tree. */
int bdb_insert_kid(
	idNode *a,
	Avlnode *tree
)
{
	int rc;

	if (a->i_rdn->parent == 0)
		return 0;
	a->i_parent = bdb_find_id_node(a->i_rdn->parent, tree);
	if (!a->i_parent)
		return -1;
	ldap_pvt_thread_rdwr_wlock(&a->i_parent->i_kids_rdwr);
	rc = avl_insert( &a->i_parent->i_kids, (caddr_t) a,
		(AVL_CMP)node_rdn_cmp, (AVL_DUP) avl_dup_error );
	ldap_pvt_thread_rdwr_wunlock(&a->i_parent->i_kids_rdwr);
	return rc;
}

/* This function adds a node into the main AVL tree */
idNode *bdb_add_node(
	ID id,
	char *d,
	struct bdb_info *bdb
)
{
	idNode *node;

	node = (idNode *)ch_malloc(sizeof(idNode));
	node->i_id = id;
	node->i_parent = NULL;
	node->i_kids = NULL;
	node->i_rdn = (diskNode *)d;
	node->i_rdn->rdn.bv_val += (long)d;
	node->i_rdn->nrdn.bv_val += (long)d;
	ldap_pvt_thread_rdwr_init(&node->i_kids_rdwr);
	avl_insert( &bdb->bi_tree, (caddr_t) node,
			(AVL_CMP)node_add_cmp, (AVL_DUP) avl_dup_error );
	if (id == 1)
		bdb->bi_troot = node;
	return node;
}

/* This function initializes the trees at startup time. */
int bdb_build_tree(
	Backend *be
)
{
	struct bdb_info *bdb = (struct bdb_info *) be->be_private;
	int i, rc;
	DBC *cursor;
	DBT key, data;
	ID id;
	idNode *node;
	char **rdns;

	bdb->bi_tree = NULL;

	rc = bdb->bi_id2parent->bdi_db->cursor(
		bdb->bi_id2parent->bdi_db, NULL, &cursor,
		bdb->bi_db_opflags );
	if( rc != 0 ) {
		return NOID;
	}

	/* When be_suffix is turned into struct berval or LDAPDN
	 * life will get a lot easier... Since no DNs live on disk, we
	 * need to operate on the be_suffix to fully qualify our DNs.
	 * We need to know how many components are in the suffix DN,
	 * so we can tell where the suffix ends and our nodes begin.
	 *
	 * Note that this code always uses be_suffix[0], so defining
	 * multiple suffixes for a single backend won't work!
	 */
	bdb->bi_sufflen = strlen(be->be_suffix[0]);
	bdb->bi_nsufflen = strlen(be->be_nsuffix[0]);

	rdns = ldap_explode_dn(be->be_nsuffix[0], 0);
	for (i=0; rdns[i]; i++);
	bdb->bi_nrdns = i;
	charray_free(rdns);

	DBTzero( &key );
	DBTzero( &data );
	key.data = (char *)&id;
	key.ulen = sizeof( id );
	key.flags = DB_DBT_USERMEM;
	data.flags = DB_DBT_MALLOC;

	while (cursor->c_get( cursor, &key, &data, DB_NEXT ) == 0) {
		bdb_add_node( id, data.data, bdb );
	}
	cursor->c_close( cursor );

	rc = avl_apply(bdb->bi_tree, (AVL_APPLY)bdb_insert_kid, bdb->bi_tree,
		-1, AVL_INORDER );

	return rc;
}

/* This function constructs a full DN for a given id. We really should
 * be passing idNodes directly, to save some effort...
 */
int bdb_fix_dn(
	BackendDB *be,
	ID id,
	Entry *e
)
{
	struct bdb_info *bdb = (struct bdb_info *) be->be_private;
	idNode *n, *o;
	int rlen, nrlen;
	char *ptr, *nptr;
	
	ldap_pvt_thread_rdwr_rlock(&bdb->bi_tree_rdwr);
	o = bdb_find_id_node(id, bdb->bi_tree);
	rlen = bdb->bi_sufflen + 1;
	nrlen = bdb->bi_nsufflen + 1;
	for (n = o; n; n=n->i_parent) {
		rlen += n->i_rdn->rdn.bv_len + 1;
		nrlen += n->i_rdn->nrdn.bv_len + 1;
	}
	e->e_dn = ch_malloc(rlen + nrlen);
	e->e_ndn = e->e_dn + rlen;
	ptr = e->e_dn;
	nptr = e->e_ndn;
	for (n = o; n; n=n->i_parent) {
		ptr = bdb_strcopy(ptr, n->i_rdn->rdn.bv_val);
		*ptr++ = ',';
		nptr = bdb_strcopy(nptr, n->i_rdn->nrdn.bv_val);
		*nptr++ = ',';
	}
	ldap_pvt_thread_rdwr_runlock(&bdb->bi_tree_rdwr);

	ptr--;
	nptr--;
	strcpy(ptr, be->be_suffix[0]);
	strcpy(nptr, be->be_nsuffix[0]);

	return 0;
}

int
bdb_dn2id_add(
	BackendDB	*be,
	DB_TXN *txn,
	const char	*pdn,
	Entry		*e )
{
	struct bdb_info *bdb = (struct bdb_info *) be->be_private;
	int		rc, rlen, nrlen;
	DBT		key, data;
	DB *db = bdb->bi_id2parent->bdi_db;
	char		*nrdn = dn_rdn( be, e->e_ndn );
	char		*rdn;
	diskNode *d;
	idNode *n;

	if (nrdn == NULL) {
		nrdn = "";
		rdn = "";
	} else {
		rdn = dn_rdn( be, e->e_dn );
	}

	nrlen = strlen(nrdn);
	rlen = strlen(rdn);
	d = ch_malloc(sizeof(diskNode) + rlen + nrlen + 2);
	d->rdn.bv_len = rlen;
	d->nrdn.bv_len = nrlen;
	d->rdn.bv_val = (char *)(d+1);
	d->nrdn.bv_val = bdb_strcopy(d->rdn.bv_val, rdn) + 1;
	strcpy(d->nrdn.bv_val, nrdn);
	d->rdn.bv_val -= (long)d;
	d->nrdn.bv_val -= (long)d;

	if (nrdn[0]) free(nrdn);
	if (rdn[0]) free(rdn);

	if (pdn) {
		bdb_dn2id(be, txn, pdn, &d->parent);
	} else {
		d->parent = 0;
	}

	DBTzero(&key);
	DBTzero(&data);
	key.data = &e->e_id;
	key.size = sizeof(ID);
	key.flags = DB_DBT_USERMEM;

	data.data = d;
	data.size = sizeof(diskNode) + rlen + nrlen + 2;
	data.flags = DB_DBT_USERMEM;

	rc = db->put( db, txn, &key, &data, DB_NOOVERWRITE );

	if (rc == 0) {
		ldap_pvt_thread_rdwr_wlock(&bdb->bi_tree_rdwr);
		n = bdb_add_node( e->e_id, data.data, bdb);
		ldap_pvt_thread_rdwr_wunlock(&bdb->bi_tree_rdwr);

		if (d->parent) {
			ldap_pvt_thread_rdwr_rlock(&bdb->bi_tree_rdwr);
			bdb_insert_kid(n, bdb->bi_tree);
			ldap_pvt_thread_rdwr_runlock(&bdb->bi_tree_rdwr);
		}
	} else {
		free(d);
	}
	return rc;
}

int
bdb_dn2id_delete(
	BackendDB	*be,
	DB_TXN *txn,
	const char	*pdn,
	const char	*dn,
	ID		id )
{
	struct bdb_info *bdb = (struct bdb_info *) be->be_private;
	int rc;
	DBT		key;
	DB *db = bdb->bi_id2parent->bdi_db;
	idNode *n;

	DBTzero(&key);
	key.size = sizeof(id);
	key.data = &id;

	rc = db->del( db, txn, &key, 0);

	ldap_pvt_thread_rdwr_wlock(&bdb->bi_tree_rdwr);
	n = avl_delete(&bdb->bi_tree, (void *)id, (AVL_CMP)node_find_cmp);
	if (n) {
		if (n->i_parent) {
			ldap_pvt_thread_rdwr_wlock(&n->i_parent->i_kids_rdwr);
			avl_delete(&n->i_parent->i_kids, n->i_rdn->nrdn.bv_val,
				(AVL_CMP)node_frdn_cmp);
			ldap_pvt_thread_rdwr_wunlock(&n->i_parent->i_kids_rdwr);
		}
		free(n->i_rdn);
		ldap_pvt_thread_rdwr_destroy(&n->i_kids_rdwr);
		free(n);
	}
	if (id == 1)
		bdb->bi_troot = NULL;
	ldap_pvt_thread_rdwr_wunlock(&bdb->bi_tree_rdwr);

	return rc;
}

int
bdb_dn2id_matched(
	BackendDB	*be,
	DB_TXN *txn,
	const char	*in,
	ID *id,
	char **matchedDN )
{
	struct bdb_info *bdb = (struct bdb_info *) be->be_private;
	int		i;
	char		**rdns;
	idNode *n, *p;

	if (!bdb->bi_troot)
		return DB_NOTFOUND;

	p = bdb->bi_troot;
	if (be_issuffix(be, in)) {
		*id = p->i_id;
		return 0;
	}

	rdns = ldap_explode_dn(in, 0);
	for (i=0; rdns[i]; i++);
	i -= bdb->bi_nrdns;
	if (i < 0)
		return -1;
	n = p;
	ldap_pvt_thread_rdwr_rlock(&bdb->bi_tree_rdwr);
	for (--i; i>=0; i--) {
		ldap_pvt_thread_rdwr_rlock(&p->i_kids_rdwr);
		n = bdb_find_rdn_node(rdns[i], p->i_kids);
		ldap_pvt_thread_rdwr_runlock(&p->i_kids_rdwr);
		if (!n) break;
		p = n;
	}
	ldap_pvt_thread_rdwr_runlock(&bdb->bi_tree_rdwr);

	if (n) {
		*id = n->i_id;
	} else if (matchedDN) {
		int len = 0, j;
		char *ptr;
		++i;
		for (j=i; rdns[j]; j++)
			len += strlen(rdns[j]) + 1;
		ptr = ch_malloc(len);
		*matchedDN = ptr;
		for (;rdns[i]; i++) {
			ptr = bdb_strcopy(ptr, rdns[i]);
			*ptr++ = ',';
		}
		ptr[-1] = '\0';
	}
	return n ? 0 : DB_NOTFOUND;
}

int
bdb_dn2id(
	BackendDB	*be,
	DB_TXN *txn,
	const char	*dn,
	ID *id )
{
	return bdb_dn2id_matched(be, txn, dn, id, NULL);
}

int
bdb_dn2id_children(
	BackendDB	*be,
	DB_TXN *txn,
	const char *dn )
{
	int		rc;
	struct bdb_info *bdb = (struct bdb_info *) be->be_private;
	ID		id;
	idNode *n;

	rc = bdb_dn2id(be, txn, dn, &id);
	if (rc != 0)
		return rc;

	ldap_pvt_thread_rdwr_rlock(&bdb->bi_tree_rdwr);
	n = bdb_find_id_node(id, bdb->bi_tree);
	ldap_pvt_thread_rdwr_runlock(&bdb->bi_tree_rdwr);

	if (!n->i_kids)
		return DB_NOTFOUND;
	else
		return 0;
}

/* Since we don't store IDLs for onelevel or subtree, we have to construct
 * them on the fly... Perhaps the i_kids tree ought to just be an IDL?
 */
static int
insert_one(
	idNode *n,
	ID *ids
)
{
	return bdb_idl_insert(ids, n->i_id);
}

static int
insert_sub(
	idNode *n,
	ID *ids
)
{
	int rc;

	rc = bdb_idl_insert(ids, n->i_id);
	if (rc == 0) {
		ldap_pvt_thread_rdwr_rlock(&n->i_kids_rdwr);
		rc = avl_apply(n->i_kids, (AVL_APPLY)insert_sub, ids, -1,
			AVL_INORDER);
		ldap_pvt_thread_rdwr_runlock(&n->i_kids_rdwr);
	}
	return rc;
}

int
bdb_dn2idl(
	BackendDB	*be,
	const char	*dn,
	int prefix,
	ID *ids )
{
	struct bdb_info *bdb = (struct bdb_info *) be->be_private;
	int		rc;
	ID		id;
	idNode		*n;

	if (prefix == DN_SUBTREE_PREFIX && be_issuffix(be, dn)) {
		BDB_IDL_ALL(bdb, ids);
		return 0;
	}

	rc = bdb_dn2id(be, NULL, dn, &id);
	if (rc) return rc;

	ldap_pvt_thread_rdwr_rlock(&bdb->bi_tree_rdwr);
	n = bdb_find_id_node(id, bdb->bi_tree);
	ldap_pvt_thread_rdwr_runlock(&bdb->bi_tree_rdwr);

	ids[0] = 0;
	ldap_pvt_thread_rdwr_rlock(&n->i_kids_rdwr);
	if (prefix == DN_ONE_PREFIX) {
		rc = avl_apply(n->i_kids, (AVL_APPLY)insert_one, ids, -1,
			AVL_INORDER);
	} else {
		rc = avl_apply(n->i_kids, (AVL_APPLY)insert_sub, ids, -1,
			AVL_INORDER);
	}
	ldap_pvt_thread_rdwr_runlock(&n->i_kids_rdwr);
	return rc;
}
#endif	/* BDB_HIER */
