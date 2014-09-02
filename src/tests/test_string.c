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

	uint16_t sid;
	rc = red_allocate_string(&red, 20, "", &ec, &sid);
	if (rc < 0) {
		printf("red_acquire_string -> rc %d\n", rc);
	}
	if (ec != 0) {
		printf("red_acquire_string -> ec %u\n", ec);
	}
	printf("red_acquire_string -> sid %u\n", sid);

	rc = red_set_string_chunk(&red, sid, 0, "A123456789B123456789C123456789D123456789", &ec);
	if (rc < 0) {
		printf("red_set_string_chunk -> rc %d\n", rc);
	}
	if (ec != 0) {
		printf("red_set_string_chunk -> ec %u\n", ec);
	}

	rc = red_set_string_chunk(&red, sid, 40, "E123456789F123456789G123456789H123456789", &ec);
	if (rc < 0) {
		printf("red_set_string_chunk -> rc %d\n", rc);
	}
	if (ec != 0) {
		printf("red_set_string_chunk -> ec %u\n", ec);
	}

	uint32_t length;
	rc = red_get_string_length(&red, sid, &ec, &length);
	if (rc < 0) {
		printf("red_get_string_length -> rc %d\n", rc);
	}
	if (ec != 0) {
		printf("red_get_string_length -> ec %u\n", ec);
	}
	printf("red_get_string_length -> length %u\n", length);

	char buffer[63 + 1];
	rc = red_get_string_chunk(&red, sid, 20, &ec, buffer);
	if (rc < 0) {
		printf("red_get_string_chunk -> rc %d\n", rc);
	}
	if (ec != 0) {
		printf("red_get_string_chunk -> ec %u\n", ec);
	}
	buffer[63] = '\0';
	printf("red_get_string_chunk -> buffer '%s'\n", buffer);

	release_object(&red, sid, "string");

	red_destroy(&red);
	ipcon_destroy(&ipcon);

	return 0;
}
