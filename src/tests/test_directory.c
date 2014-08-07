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
	rc = red_allocate_string(&red, 20, &ec, &sid);
	if (rc < 0) {
		printf("red_allocate_string -> rc %d\n", rc);
	}
	if (ec != 0) {
		printf("red_allocate_string -> ec %u\n", ec);
	}
	printf("red_allocate_string -> sid %u\n", sid);

	rc = red_set_string_chunk(&red, sid, 0, "/lib/", &ec);
	if (rc < 0) {
		printf("red_set_string_chunk -> rc %d\n", rc);
	}
	if (ec != 0) {
		printf("red_set_string_chunk -> ec %u\n", ec);
	}

	uint16_t did;
	rc = red_open_directory(&red, sid, &ec, &did);
	if (rc < 0) {
		printf("red_open_directory -> rc %d\n", rc);
	}
	if (ec != 0) {
		printf("red_open_directory -> ec %u\n", ec);
	}
	printf("red_open_directory -> did %u\n", did);

	uint64_t st = microseconds();

	while (1) {
		uint16_t nid;
		uint8_t type;

		rc = red_get_next_directory_entry(&red, did, &ec, &nid, &type);
		if (rc < 0) {
			printf("red_get_next_directory_entry -> rc %d\n", rc);
			break;
		}
		if (ec != 0) {
			printf("red_get_next_directory_entry -> ec %u\n", ec);
			break;
		}

		char buffer[63 + 1];

		rc = red_get_string_chunk(&red, nid, 0, &ec, buffer);
		if (rc < 0) {
			printf("red_get_string_chunk -> rc %d\n", rc);
		}
		if (ec != 0) {
			printf("red_get_string_chunk -> ec %u\n", ec);
		}

		buffer[63] = '\0';
		printf("* %s", buffer);

		if (type == RED_FILE_TYPE_SYMLINK) {
			uint16_t tid;

			rc = red_get_symlink_target(&red, nid, false, &ec, &tid);
			if (rc < 0) {
				printf("red_get_symlink_target -> rc %d\n", rc);
			}
			if (ec != 0) {
				printf("red_get_symlink_target -> ec %u\n", ec);
			}

			rc = red_get_string_chunk(&red, tid, 0, &ec, buffer);
			if (rc < 0) {
				printf("red_get_string_chunk -> rc %d\n", rc);
			}
			if (ec != 0) {
				printf("red_get_string_chunk -> ec %u\n", ec);
			}

			buffer[63] = '\0';
			printf(" ==> %s", buffer);

			rc = red_release_object(&red, tid, &ec);
			if (rc < 0) {
				printf("red_release_object/string -> rc %d\n", rc);
			}
			if (ec != 0) {
				printf("red_release_object/string -> ec %u\n", ec);
			}
		}
		printf("\n");

		/*uint8_t type;
		uint16_t permissions;
		uint32_t user_id;
		uint32_t group_id;
		uint64_t length;
		uint64_t access_time;
		uint64_t modification_time;
		uint64_t status_change_time;
		rc = red_get_file_info(&red, nid, true, &ec, &type, &permissions, &user_id, &group_id, &length, &access_time, &modification_time, &status_change_time);
		if (rc < 0) {
			printf("red_get_file_info -> rc %d\n", rc);
		}
		if (ec != 0) {
			printf("red_get_file_info -> ec %u\n", ec);
		}

		printf("%u %o %u %u %lu %lu %lu %lu\n", type, permissions, user_id, group_id, length, access_time, modification_time, status_change_time);*/

		rc = red_release_object(&red, nid, &ec);
		if (rc < 0) {
			printf("red_release_object/string -> rc %d\n", rc);
		}
		if (ec != 0) {
			printf("red_release_object/string -> ec %u\n", ec);
		}
	}

	uint64_t et = microseconds();
	float dur = (et - st) / 1000000.0;

	printf("red_get_next_directory_entry in %f sec\n", dur);

	rc = red_release_object(&red, did, &ec);
	if (rc < 0) {
		printf("red_release_object/directory -> rc %d\n", rc);
	}
	if (ec != 0) {
		printf("red_release_object/directory -> ec %u\n", ec);
	}

	rc = red_release_object(&red, sid, &ec);
	if (rc < 0) {
		printf("red_release_object/string -> rc %d\n", rc);
	}
	if (ec != 0) {
		printf("red_release_object/string -> ec %u\n", ec);
	}

	red_destroy(&red);
	ipcon_destroy(&ipcon);

	return 0;
}
