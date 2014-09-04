#include <stdio.h>
#include <time.h>
#include <sys/time.h>

#include "ip_connection.h"
#include "brick_red.h"

#define HOST "localhost"
#define PORT 4223
#define UID "3hG6BK" // Change to your UID

#include "utils.c"

RED red;

int main() {
	int rc;

	// Create IP connection
	IPConnection ipcon;
	ipcon_create(&ipcon);

	// Create device object
	red_create(&red, UID, &ipcon);

	// Connect to brickd
	rc = ipcon_connect(&ipcon, HOST, PORT);
	if (rc < 0) {
		printf("ipcon_connect -> rc %d\n", rc);
		return -1;
	}

	char uid[8];
	char connected_uid[8];
	char position;
	uint8_t hardware_version[3];
	uint8_t firmware_version[3];
	uint16_t device_identifier;
	rc = red_get_identity(&red, uid, connected_uid, &position, hardware_version, firmware_version, &device_identifier);
	if (rc < 0) {
		printf("red_get_identity -> rc %d\n", rc);
		goto cleanup;
	}

	printf("red_get_identity -> uid %s\n", uid);
	printf("red_get_identity -> connected_uid %s\n", connected_uid);
	printf("red_get_identity -> position %c\n", position);
	printf("red_get_identity -> hardware_version %u.%u.%u\n", hardware_version[0], hardware_version[1], hardware_version[2]);
	printf("red_get_identity -> firmware_version %u.%u.%u\n", firmware_version[0], firmware_version[1], firmware_version[2]);
	printf("red_get_identity -> device_identifier %u\n", device_identifier);

cleanup:
	red_destroy(&red);
	ipcon_destroy(&ipcon);

	return 0;
}
