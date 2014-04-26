/*
    node.c -- node tree management
    Copyright (C) 2014 Guus Sliepen <guus@meshlink.io>,

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "system.h"

#include "hash.h"
#include "logger.h"
#include "meshlink_internal.h"
#include "net.h"
#include "netutl.h"
#include "node.h"
#include "splay_tree.h"
#include "utils.h"
#include "xalloc.h"

static hash_t *node_udp_cache;

static int node_compare(const node_t *a, const node_t *b) {
	return strcmp(a->name, b->name);
}

void init_nodes(void) {
	mesh->nodes = splay_alloc_tree((splay_compare_t) node_compare, (splay_action_t) free_node);
	node_udp_cache = hash_alloc(0x100, sizeof(sockaddr_t));
}

void exit_nodes(void) {
	hash_free(node_udp_cache);
	splay_delete_tree(mesh->nodes);
}

node_t *new_node(void) {
	node_t *n = xzalloc(sizeof *n);

	if(replaywin) n->late = xzalloc(replaywin);
	n->edge_tree = new_edge_tree();
	n->mtu = MTU;
	n->maxmtu = MTU;

	return n;
}

void free_node(node_t *n) {
	if(n->edge_tree)
		free_edge_tree(n->edge_tree);

	sockaddrfree(&n->address);

	ecdsa_free(n->ecdsa);
	sptps_stop(&n->sptps);

	timeout_del(&mesh->loop, &n->mtutimeout);

	if(n->hostname)
		free(n->hostname);

	if(n->name)
		free(n->name);

	if(n->late)
		free(n->late);

	free(n);
}

void node_add(node_t *n) {
	splay_insert(mesh->nodes, n);
}

void node_del(node_t *n) {
	for splay_each(edge_t, e, n->edge_tree)
		edge_del(e);

	splay_delete(mesh->nodes, n);
}

node_t *lookup_node(char *name) {
	node_t n = {NULL};

	n.name = name;

	return splay_search(mesh->nodes, &n);
}

node_t *lookup_node_udp(const sockaddr_t *sa) {
	return hash_search(node_udp_cache, sa);
}

void update_node_udp(node_t *n, const sockaddr_t *sa) {
	if(n == mesh->self) {
		logger(DEBUG_ALWAYS, LOG_WARNING, "Trying to update UDP address of mesh->self!");
		return;
	}

	hash_insert(node_udp_cache, &n->address, NULL);

	if(sa) {
		n->address = *sa;
		n->sock = 0;
		for(int i = 0; i < mesh->listen_sockets; i++) {
			if(mesh->listen_socket[i].sa.sa.sa_family == sa->sa.sa_family) {
				n->sock = i;
				break;
			}
		}
		hash_insert(node_udp_cache, sa, n);
		free(n->hostname);
		n->hostname = sockaddr2hostname(&n->address);
		logger(DEBUG_PROTOCOL, LOG_DEBUG, "UDP address of %s set to %s", n->name, n->hostname);
	}
}
