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
	if (allocate_string(&red, "blubb", &sid)) {
		return -1;
	}

	uint16_t pid;
	rc = red_define_program(&red, sid, &ec, &pid);
	if (rc < 0) {
		printf("red_define_program -> rc %d\n", rc);
		goto cleanup;
	}
	if (ec != 0) {
		printf("red_define_program -> ec %u\n", ec);
		goto cleanup;
	}
	printf("red_define_program -> pid %u\n", pid);

	rc = red_undefine_program(&red, pid, &ec);
	if (rc < 0) {
		printf("red_undefine_program -> rc %d\n", rc);
		goto cleanup;
	}
	if (ec != 0) {
		printf("red_undefine_program -> ec %u\n", ec);
		goto cleanup;
	}

cleanup:
	release_object(&red, pid, "program");
	release_object(&red, sid, "string");

	red_destroy(&red);
	ipcon_destroy(&ipcon);

	return 0;
}
