#include <stdio.h>
#include <time.h>
#include <sys/time.h>

#include "ip_connection.h"
#include "brick_red.h"

#define HOST "localhost"
#define PORT 4223
#define UID "3hG4aq" // Change to your UID

#include "utils.c"

#define STRING_MAX_GET_CHUNK_BUFFER_LENGTH 63

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
		goto cleanup2;
	}
	if (ec != 0) {
		printf("red_define_program -> ec %u\n", ec);
		goto cleanup2;
	}
	printf("red_define_program -> pid %u\n", pid);

	uint16_t pdsid;
	rc = red_get_program_directory(&red, pid, &ec, &pdsid);
	if (rc < 0) {
		printf("red_get_program_directory -> rc %d\n", rc);
		goto cleanup1;
	}
	if (ec != 0) {
		printf("red_get_program_directory -> ec %u\n", ec);
		goto cleanup1;
	}
	printf("red_get_program_directory -> pdsid %u\n", pdsid);

	char buffer[STRING_MAX_GET_CHUNK_BUFFER_LENGTH + 1];
	rc = red_get_string_chunk(&red, pdsid, 0, &ec, buffer);
	if (rc < 0) {
		printf("red_get_string_chunk -> rc %d\n", rc);
		goto cleanup1;
	}
	if (ec != 0) {
		printf("red_get_string_chunk -> ec %u\n", ec);
		goto cleanup1;
	}

	buffer[STRING_MAX_GET_CHUNK_BUFFER_LENGTH] = '\0';
	printf("red_get_string_chunk '%s'\n", buffer);

	rc = red_undefine_program(&red, pid, &ec);
	if (rc < 0) {
		printf("red_undefine_program -> rc %d\n", rc);
		goto cleanup0;
	}
	if (ec != 0) {
		printf("red_undefine_program -> ec %u\n", ec);
		goto cleanup0;
	}

cleanup0:
	release_object(&red, pdsid, "string");

cleanup1:
	release_object(&red, pid, "program");

cleanup2:
	release_object(&red, sid, "string");

	red_destroy(&red);
	ipcon_destroy(&ipcon);

	return 0;
}
