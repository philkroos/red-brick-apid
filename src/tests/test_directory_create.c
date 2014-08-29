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

	uint16_t sid;
	if (allocate_string(&red, "/tmp/d1/d2", &sid)) {
		return -1;
	}

	rc = red_create_directory(&red, sid, true, 0755, 1000, 1000, &ec);
	if (rc < 0) {
		printf("red_create_directory -> rc %d\n", rc);
		goto cleanup;
	}
	if (ec != 0) {
		printf("red_create_directory -> ec %u\n", ec);
		goto cleanup;
	}

cleanup:
	release_object(&red, sid, "string");

	red_destroy(&red);
	ipcon_destroy(&ipcon);

	return 0;
}
