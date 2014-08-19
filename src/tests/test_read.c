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
	if (allocate_string_object(&red, "/tmp/foobar", &sid)) {
		return -1;
	}

	uint16_t fid;
	rc = red_open_file(&red, sid, RED_FILE_FLAG_READ_ONLY, 0, 0, 0, &ec, &fid);
	if (rc < 0) {
		printf("red_open_file -> rc %d\n", rc);
		goto cleanup;
	}
	if (ec != 0) {
		printf("red_open_file -> ec %u\n", ec);
		goto cleanup;
	}
	printf("red_open_file -> fid %u\n", fid);

	uint8_t buffer[62];
	uint8_t buffer_ref[62];
	uint8_t length_read;
	uint64_t st = microseconds();

	memcpy(buffer_ref, "foobar x1\nfoobar y2\nfoobar z3\nfoobar u4\nfoobar v5\nfoobar w6\n\n", 61);

	int i;
	for (i = 0; i < 30000; ++i) {
		rc = red_read_file(&red, fid, 61, &ec, buffer, &length_read);
		if (rc < 0) {
			printf("red_read_file %d -> rc %d\n", i, rc);
		}
		if (ec != 0) {
			printf("red_read_file %d -> ec %u\n", i, ec);
		}

		if (length_read != 61) {
			printf("red_read_file %d -> length_read %d\n", i, length_read);
		}

		if (memcmp(buffer_ref, buffer, 61) != 0) {
			printf("red_read_file %d -> wrong data\n", i);
		}
	}

	uint64_t et = microseconds();
	float dur = (et - st) / 1000000.0;

	printf("30000x red_read_file in %f sec, %f kB/s\n", dur, 30000 * 61 / dur / 1024);

	rc = red_release_object(&red, fid, &ec);
	if (rc < 0) {
		printf("red_release_object/file -> rc %d\n", rc);
	}
	if (ec != 0) {
		printf("red_release_object/file -> ec %u\n", ec);
	}

cleanup:
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
