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

	uint16_t iid;
	rc = red_open_inventory(&red, RED_OBJECT_TYPE_INVENTORY, &ec, &iid);
	if (rc < 0) {
		printf("red_open_inventory -> rc %d\n", rc);
		return -1;
	}
	if (ec != 0) {
		printf("red_open_inventory -> ec %u\n", ec);
		return -1;
	}
	printf("red_open_inventory -> iid %u\n", iid);

	uint64_t st = microseconds();

	while (1) {
		uint16_t oid;

		rc = red_get_next_inventory_entry(&red, iid, &ec, &oid);
		if (rc < 0) {
			printf("red_get_next_inventory_entry -> rc %d\n", rc);
			break;
		}
		if (ec != 0) {
			printf("red_get_next_inventory_entry -> ec %u\n", ec);
			break;
		}
		printf("red_get_next_inventory_entry -> oid %u\n", oid);

		rc = red_release_object(&red, oid, &ec);
		if (rc < 0) {
			printf("red_release_object -> rc %d\n", rc);
		}
		if (ec != 0) {
			printf("red_release_object -> ec %u\n", ec);
		}
	}

	uint64_t et = microseconds();
	float dur = (et - st) / 1000000.0;

	printf("red_get_next_inventory_entry in %f sec\n", dur);

	rc = red_release_object(&red, iid, &ec);
	if (rc < 0) {
		printf("red_release_object/inventory -> rc %d\n", rc);
	}
	if (ec != 0) {
		printf("red_release_object/inventory -> ec %u\n", ec);
	}

	red_destroy(&red);
	ipcon_destroy(&ipcon);

	return 0;
}
