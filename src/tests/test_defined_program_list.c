#include <stdio.h>
#include <time.h>
#include <sys/time.h>

#include "ip_connection.h"
#include "brick_red.h"

#define HOST "localhost"
#define PORT 4223
#define UID "3hG6BK" // Change to your UID

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

	uint16_t dplid;
	rc = red_get_defined_programs(&red, &ec, &dplid);
	if (rc < 0) {
		printf("red_get_defined_programs -> rc %d\n", rc);
		return -1;
	}
	if (ec != 0) {
		printf("red_get_defined_programs -> ec %u\n", ec);
		return -1;
	}
	printf("red_get_defined_programs -> dplid %u\n", dplid);

	uint16_t length;
	rc = red_get_list_length(&red, dplid, &ec, &length);
	if (rc < 0) {
		printf("red_get_list_length -> rc %d\n", rc);
		goto cleanup;
	}
	if (ec != 0) {
		printf("red_get_list_length -> ec %u\n", ec);
		goto cleanup;
	}

	uint16_t i;
	for (i = 0; i < length; ++i) {
		uint16_t dpid;
		uint8_t type;

		rc = red_get_list_item(&red, dplid, i, &ec, &dpid, &type);
		if (rc < 0) {
			printf("red_get_list_item -> rc %d\n", rc);
			break;
		}
		if (ec != 0) {
			printf("red_get_list_item -> ec %u\n", ec);
			break;
		}
		printf("red_get_list_item -> dpid %u\n", dpid);

		release_object(&red, dpid, "program");
	}

cleanup:
	release_object(&red, dplid, "defined programs list");

	red_destroy(&red);
	ipcon_destroy(&ipcon);

	return 0;
}
