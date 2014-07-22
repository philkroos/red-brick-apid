#include <stdio.h>
#include <time.h>
#include <sys/time.h>

#include "ip_connection.h"
#include "brick_red.h"

#define HOST "localhost"
#define PORT 4223
#define UID "3hG4aq" // Change to your UID

uint64_t microseconds(void) {
	struct timeval tv;

	// FIXME: use a monotonic source such as clock_gettime(CLOCK_MONOTONIC),
	//        QueryPerformanceCounter() or mach_absolute_time()
	if (gettimeofday(&tv, NULL) < 0) {
		return 0;
	} else {
		return tv.tv_sec * 1000000 + tv.tv_usec;
	}
}

RED red;

int main() {
	uint8_t ec;
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

	uint16_t lid;
	rc = red_allocate_list(&red, 20, &ec, &lid);
	if (rc < 0) {
		printf("red_allocate_list -> rc %d\n", rc);
	}
	if (ec != 0) {
		printf("red_allocate_list -> ec %u\n", ec);
	}
	printf("red_allocate_list -> sid %u\n", lid);

	int i;
	for (i = 0; i < 30; ++i) {
		uint16_t sid;
		rc = red_allocate_string(&red, 20, &ec, &sid);
		if (rc < 0) {
			printf("red_allocate_string -> rc %d\n", rc);
		}
		if (ec != 0) {
			printf("red_allocate_string -> ec %u\n", ec);
		}
		printf("red_allocate_string -> sid %u\n", sid);

		rc = red_set_string_chunk(&red, sid, 0, "A123456789B123456789C123456789D123456789", &ec);
		if (rc < 0) {
			printf("red_set_string_chunk -> rc %d\n", rc);
		}
		if (ec != 0) {
			printf("red_set_string_chunk -> ec %u\n", ec);
		}

		rc = red_append_to_list(&red, lid, sid, &ec);
		if (rc < 0) {
			printf("red_append_to_list -> rc %d\n", rc);
		}
		if (ec != 0) {
			printf("red_append_to_list -> ec %u\n", ec);
		}

		rc = red_release_object(&red, sid, &ec);
		if (rc < 0) {
			printf("red_release_object/string -> rc %d\n", rc);
		}
		if (ec != 0) {
			printf("red_release_object/string -> ec %u\n", ec);
		}
	}

	uint16_t length;
	rc = red_get_list_length(&red, lid, &ec, &length);
	if (rc < 0) {
		printf("red_get_list_length -> rc %d\n", rc);
	}
	if (ec != 0) {
		printf("red_get_list_length -> ec %u\n", ec);
	}
	printf("red_get_list_length -> length %u\n", length);

	rc = red_remove_from_list(&red, lid, 5, &ec);
	if (rc < 0) {
		printf("red_remove_from_list -> rc %d\n", rc);
	}
	if (ec != 0) {
		printf("red_remove_from_list -> ec %u\n", ec);
	}

	rc = red_get_list_length(&red, lid, &ec, &length);
	if (rc < 0) {
		printf("red_get_list_length -> rc %d\n", rc);
	}
	if (ec != 0) {
		printf("red_get_list_length -> ec %u\n", ec);
	}
	printf("red_get_list_length -> length %u\n", length);

	/*rc = red_release_object(&red, lid, &ec);
	if (rc < 0) {
		printf("red_release_object/list -> rc %d\n", rc);
	}
	if (ec != 0) {
		printf("red_release_object/list -> ec %u\n", ec);
	}*/

	//printf("red_destroy...\n");
	red_destroy(&red);
	//printf("ipcon_destroy...\n");
	ipcon_destroy(&ipcon);
}
