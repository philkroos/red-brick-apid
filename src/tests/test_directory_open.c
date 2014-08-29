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
	if (allocate_string(&red, "/lib/", &sid)) {
		return -1;
	}

	uint16_t did;
	rc = red_open_directory(&red, sid, &ec, &did);
	if (rc < 0) {
		printf("red_open_directory -> rc %d\n", rc);
		goto cleanup;
	}
	if (ec != 0) {
		printf("red_open_directory -> ec %u\n", ec);
		goto cleanup;
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
			break;
		}
		if (ec != 0) {
			printf("red_get_string_chunk -> ec %u\n", ec);
			break;
		}

		buffer[63] = '\0';
		printf("* %s", buffer);

		if (type == RED_FILE_TYPE_SYMLINK) {
			uint16_t tid;

			rc = red_get_symlink_target(&red, nid, true, &ec, &tid);
			if (rc < 0) {
				printf("red_get_symlink_target -> rc %d\n", rc);
				break;
			}
			if (ec != 0) {
				printf("red_get_symlink_target -> ec %u\n", ec);
				break;
			}

			rc = red_get_string_chunk(&red, tid, 0, &ec, buffer);
			if (rc < 0) {
				printf("red_get_string_chunk -> rc %d\n", rc);
				break;
			}
			if (ec != 0) {
				printf("red_get_string_chunk -> ec %u\n", ec);
				break;
			}

			buffer[63] = '\0';
			printf(" ==> %s", buffer);

			release_object(&red, tid, "string");
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

		release_object(&red, nid, "string");
	}

	uint64_t et = microseconds();
	float dur = (et - st) / 1000000.0;

	printf("red_get_next_directory_entry in %f sec\n", dur);

cleanup:
	release_object(&red, did, "directory");
	release_object(&red, sid, "string");

	red_destroy(&red);
	ipcon_destroy(&ipcon);

	return 0;
}
