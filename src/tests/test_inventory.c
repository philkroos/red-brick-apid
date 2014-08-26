#include <stdio.h>
#include <time.h>
#include <sys/time.h>

#include "ip_connection.h"
#include "brick_red.h"

#define HOST "localhost"
#define PORT 4223
#define UID "3hG4aq" // Change to your UID

#include "utils.c"

int main() {
	uint8_t ec;
	int rc;

	// Create IP connection
	IPConnection ipcon;
	ipcon_create(&ipcon);

	// Create device object
	RED red;
	red_create(&red, UID, &ipcon);

	// Connect to brickd
	rc = ipcon_connect(&ipcon, HOST, PORT);
	if (rc < 0) {
		printf("ipcon_connect -> rc %d\n", rc);
		return -1;
	}

	uint8_t type;
	for (type = RED_OBJECT_TYPE_INVENTORY; type <= RED_OBJECT_TYPE_PROGRAM; ++type) {
		uint16_t iid;
		rc = red_open_inventory(&red, type, &ec, &iid);
		if (rc < 0) {
			printf("red_open_inventory(%u) -> rc %d\n", type, rc);
			return -1;
		}
		if (ec != 0) {
			printf("red_open_inventory(%u) -> ec %u\n", type, ec);
			return -1;
		}
		printf("red_open_inventory(%u) -> iid %u\n", type, iid);

		for (;;) {
			uint16_t oid;

			rc = red_get_next_inventory_entry(&red, iid, &ec, &oid);
			if (rc < 0) {
				printf("red_get_next_inventory_entry(%u) -> rc %d\n", type, rc);
				break;
			}
			if (ec != 0) {
				printf("red_get_next_inventory_entry(%u) -> ec %u\n", type, ec);
				break;
			}
			printf("red_get_next_inventory_entry(%u) -> oid %u\n", type, oid);

			release_object(&red, oid, "object");
		}

		release_object(&red, iid, "inventory");
	}

	red_destroy(&red);
	ipcon_destroy(&ipcon);

	return 0;
}
