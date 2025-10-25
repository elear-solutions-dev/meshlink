#ifdef NDEBUG
#undef NDEBUG
#endif

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <assert.h>

#include "meshlink.h"
#include "devtools.h"
#include "utils.h"
#include <unistd.h>

// Note: We cannot directly access internal node_t->external_ip_address 
// because we can't include both meshlink.h and meshlink_internal.h.
// Instead, we'll infer the bug from:
// 1. meshlink_get_port() returning 0 (API bug)
// 2. Log messages showing "port 0" being advertised
// 3. The fact that direct connections fail due to port 0

static struct sync_flag bar_reachable;

static void status_cb(meshlink_handle_t *mesh, meshlink_node_t *node, bool reachable) {
	(void)mesh;

	if(reachable && !strcmp(node->name, "bar")) {
		set_sync_flag(&bar_reachable, true);
	}
}

int main(void) {
	init_sync_flag(&bar_reachable);

	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_cb);

	printf("\n=== Testing REQ_EXTERNAL with Port 0 ===\n");
	printf("Testing both BACKBONE+BACKBONE and BACKBONE+PORTABLE scenarios\n\n");

	// Clean up any previous test data
	assert(meshlink_destroy("req_external_conf.1"));
	assert(meshlink_destroy("req_external_conf.2"));

	// === SCENARIO 1: BACKBONE + BACKBONE (both send REQ_EXTERNAL) ===
	printf("=== SCENARIO 1: BACKBONE + BACKBONE ===\n");
	printf("Both nodes send REQ_EXTERNAL messages\n");

	// Open two meshlink instances (both backbone)
	meshlink_handle_t *mesh1 = meshlink_open("req_external_conf.1", "foo", "req-external", DEV_CLASS_BACKBONE);
	assert(mesh1);

	meshlink_handle_t *mesh2 = meshlink_open("req_external_conf.2", "bar", "req-external", DEV_CLASS_BACKBONE);
	assert(mesh2);

	// Disable local discovery
	meshlink_enable_discovery(mesh1, false);
	meshlink_enable_discovery(mesh2, false);

	// Set mesh1 to use port 0 (dynamic assignment)
	printf("\nConfiguring mesh1 (foo) to use port 0...\n");
	assert(meshlink_set_port(mesh1, 0));

	// Set up canonical addresses for both nodes
	assert(meshlink_set_canonical_address(mesh1, meshlink_get_self(mesh1), "localhost", NULL));
	assert(meshlink_set_canonical_address(mesh2, meshlink_get_self(mesh2), "localhost", NULL));

	// Import and export both side's data
	char *data = meshlink_export(mesh1);
	assert(data);
	assert(meshlink_import(mesh2, data));
	free(data);

	data = meshlink_export(mesh2);
	assert(data);
	assert(meshlink_import(mesh1, data));
	free(data);

	// Set up status callback for mesh1
	meshlink_set_node_status_cb(mesh1, status_cb);

	// Start both meshes
	printf("Starting mesh1 (foo) with port 0...\n");
	assert(meshlink_start(mesh1));

	// Get the actual port assigned to mesh1
	int foo_port = meshlink_get_port(mesh1);
	printf("mesh1 (foo) meshlink_get_port() returned: %d\n", foo_port);

	// 🔴 BUG: Currently returns 0 instead of actual port
	if(foo_port == 0) {
		printf("❌ BUG CONFIRMED: meshlink_get_port() returns 0\n");
	} else {
		printf("✅ BUG FIXED: meshlink_get_port() returns actual port %d\n", foo_port);
	}

	printf("Starting mesh2 (bar)...\n");
	assert(meshlink_start(mesh2));

	// Wait for bar to become reachable
	printf("Waiting for nodes to connect...\n");
	assert(wait_sync_flag(&bar_reachable, 20));
	printf("✅ Nodes connected successfully\n");

	// Give some time for REQ_EXTERNAL to be exchanged
	sleep(3);

	printf("✅ Verifying REQ_EXTERNAL bidirectional learning...\n");
	
	// Get nodes for testing
	meshlink_node_t *node_foo = meshlink_get_node(mesh2, "foo");
	meshlink_node_t *node_bar = meshlink_get_node(mesh1, "bar");
	assert(node_foo && node_bar);
	
	// Test bidirectional learning: bar should learn foo's external info
	devtool_node_status_t bar_view_of_foo;
	devtool_get_node_status(mesh1, node_foo, &bar_view_of_foo);
	
	// Test bidirectional learning: foo should learn bar's external info  
	devtool_node_status_t foo_view_of_bar;
	devtool_get_node_status(mesh2, node_bar, &foo_view_of_bar);
	
	// Verify that REQ_EXTERNAL data was received and parsed correctly
	assert(bar_view_of_foo.external_ip_address != NULL);
	assert(foo_view_of_bar.external_ip_address != NULL);
	
	// Parse and verify the external IP addresses
	char foo_ip[64], foo_port_str[16];
	char bar_ip[64], bar_port_str[16];
	
	assert(sscanf(bar_view_of_foo.external_ip_address, "%s %s", foo_ip, foo_port_str) == 2);
	assert(sscanf(foo_view_of_bar.external_ip_address, "%s %s", bar_ip, bar_port_str) == 2);
	
	int foo_port_learned = atoi(foo_port_str);
	int bar_port_learned = atoi(bar_port_str);
	
	// Verify the learned ports match the actual assigned ports
	assert(foo_port_learned == foo_port);
	assert(bar_port_learned > 0);  // bar should have a valid port
	
	printf("✅ REQ_EXTERNAL bidirectional learning verified:\n");
	printf("   bar learned foo's external address: %s %s\n", foo_ip, foo_port_str);
	printf("   foo learned bar's external address: %s %s\n", bar_ip, bar_port_str);
	printf("   foo's learned port (%d) matches actual port (%d)\n", foo_port_learned, foo_port);
	
	// Clean up allocated strings
	devtool_free_node_status(&bar_view_of_foo);
	devtool_free_node_status(&foo_view_of_bar);

	// Clean up scenario 1
	printf("\n=== Cleaning Up Scenario 1 ===\n");
	meshlink_stop(mesh1);
	meshlink_stop(mesh2);
	meshlink_close(mesh1);
	meshlink_close(mesh2);

	assert(meshlink_destroy("req_external_conf.1"));
	assert(meshlink_destroy("req_external_conf.2"));

	// === SCENARIO 2: BACKBONE + PORTABLE (only backbone sends REQ_EXTERNAL) ===
	printf("\n=== SCENARIO 2: BACKBONE + PORTABLE ===\n");
	printf("Only backbone node sends REQ_EXTERNAL messages\n");

	// Clean up any previous test data
	assert(meshlink_destroy("req_external_conf.1"));
	assert(meshlink_destroy("req_external_conf.2"));

	// Open two meshlink instances (backbone + portable)
	mesh1 = meshlink_open("req_external_conf.1", "foo", "req-external", DEV_CLASS_BACKBONE);
	assert(mesh1);

	mesh2 = meshlink_open("req_external_conf.2", "bar", "req-external", DEV_CLASS_PORTABLE);
	assert(mesh2);

	// Disable local discovery
	meshlink_enable_discovery(mesh1, false);
	meshlink_enable_discovery(mesh2, false);

	// Set mesh1 (backbone) to use port 0 (dynamic assignment)
	printf("\nConfiguring mesh1 (backbone) to use port 0...\n");
	assert(meshlink_set_port(mesh1, 0));

	// Set up canonical addresses for both nodes
	assert(meshlink_set_canonical_address(mesh1, meshlink_get_self(mesh1), "localhost", NULL));
	assert(meshlink_set_canonical_address(mesh2, meshlink_get_self(mesh2), "localhost", NULL));

	// Import and export both side's data
	char *data2 = meshlink_export(mesh1);
	assert(data2);
	assert(meshlink_import(mesh2, data2));
	free(data2);

	data2 = meshlink_export(mesh2);
	assert(data2);
	assert(meshlink_import(mesh1, data2));
	free(data2);

	// Set up status callback
	meshlink_set_node_status_cb(mesh1, status_cb);

	// Start both meshes
	printf("Starting mesh1 (backbone) with port 0...\n");
	assert(meshlink_start(mesh1));

	int foo_port2 = meshlink_get_port(mesh1);
	printf("mesh1 (backbone) meshlink_get_port() returned: %d\n", foo_port2);

	if(foo_port2 == 0) {
		printf("❌ BUG STILL EXISTS: mesh1's port is 0\n");
		printf("   REQ_EXTERNAL will send 'IP 0' to mesh2\n");
		printf("   mesh2 cannot establish direct UDP connections\n");
		printf("   Critical bug for NMN architecture with 2000 nodes using port 0\n");
		assert(false);
	} else {
		printf("✅ BUG FIXED: mesh1's port is %d\n", foo_port2);
		printf("   REQ_EXTERNAL will send 'IP %d' to mesh2\n", foo_port2);
		printf("   mesh2 can establish direct UDP connections\n");
		printf("   P2P optimization working correctly\n");
	}

	printf("Starting mesh2 (portable)...\n");
	assert(meshlink_start(mesh2));

	// Wait for connection
	printf("Waiting for nodes to connect...\n");
	assert(wait_sync_flag(&bar_reachable, 5));
	printf("✅ Nodes connected successfully\n");

	// Give more time for REQ_EXTERNAL messages to be exchanged
	printf("Waiting for REQ_EXTERNAL message exchange...\n");
	sleep(3);  // Give time for protocol messages to be sent

	printf("✅ Verifying REQ_EXTERNAL bidirectional learning...\n");
	
	// Get nodes for testing
	meshlink_node_t *node_foo2 = meshlink_get_node(mesh2, "foo");
	meshlink_node_t *node_bar2 = meshlink_get_node(mesh1, "bar");
	assert(node_foo2 && node_bar2);
	
	// Test bidirectional learning: bar should learn foo's external info
	devtool_node_status_t bar_view_of_foo2;
	devtool_get_node_status(mesh1, node_foo2, &bar_view_of_foo2);
	
	// Test unidirectional learning: bar (portable) should learn foo's (backbone) external info
	// In BACKBONE + PORTABLE scenario, only backbone sends REQ_EXTERNAL
	devtool_node_status_t foo_view_of_bar2;
	devtool_get_node_status(mesh2, node_bar2, &foo_view_of_bar2);
	
	// Verify that REQ_EXTERNAL data was received and parsed correctly
	assert(bar_view_of_foo2.external_ip_address != NULL);
	// Portable nodes don't send REQ_EXTERNAL, so foo shouldn't know bar's external address
	assert(foo_view_of_bar2.external_ip_address == NULL);
	
	// Parse and verify the external IP address that bar learned about foo
	char foo_ip2[64], foo_port_str2[16];
	assert(sscanf(bar_view_of_foo2.external_ip_address, "%s %s", foo_ip2, foo_port_str2) == 2);
	
	int foo_port_learned2 = atoi(foo_port_str2);
	
	// Verify the learned port matches the actual assigned port
	assert(foo_port_learned2 == foo_port2);
	
	printf("✅ REQ_EXTERNAL unidirectional learning verified (BACKBONE + PORTABLE):\n");
	printf("   bar (portable) learned foo's (backbone) external address: %s %s\n", foo_ip2, foo_port_str2);
	printf("   foo (backbone) does NOT know bar's (portable) external address (correct behavior)\n");
	printf("   foo's learned port (%d) matches actual port (%d)\n", foo_port_learned2, foo_port2);
	
	// Clean up allocated strings
	devtool_free_node_status(&bar_view_of_foo2);
	devtool_free_node_status(&foo_view_of_bar2);

	// Clean up scenario 2
	printf("\n=== Cleaning Up Scenario 2 ===\n");
	meshlink_stop(mesh1);
	meshlink_stop(mesh2);
	meshlink_close(mesh1);
	meshlink_close(mesh2);

	assert(meshlink_destroy("req_external_conf.1"));
	assert(meshlink_destroy("req_external_conf.2"));

	printf("\n=== REQ_EXTERNAL Port 0 Test Complete ===\n");
	printf("✅ Tested both BACKBONE+BACKBONE and BACKBONE+PORTABLE scenarios\n");
	printf("✅ Bug fix verified in both scenarios\n");

	return 0;
}

