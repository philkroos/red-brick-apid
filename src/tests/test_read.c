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
	if (ipcon_connect(&ipcon, HOST, PORT) < 0) {
		fprintf(stderr, "Could not connect\n");
		exit(1);
	}
	// Don't use device before ipcon is connected

	uint16_t sid;
	rc = red_allocate_string(&red, 20, &ec, &sid);
	if (rc < 0) {
		printf("red_allocate_string -> rc %d\n", rc);
	}
	if (ec != 0) {
		printf("red_allocate_string -> ec %u\n", ec);
	}
	printf("red_allocate_string -> sid %u\n", sid);

	rc = red_set_string_chunk(&red, sid, 0, "/tmp/foobar", &ec);
	if (rc < 0) {
		printf("red_set_string_chunk -> rc %d\n", rc);
	}
	if (ec != 0) {
		printf("red_set_string_chunk -> ec %u\n", ec);
	}

	uint16_t fid;
	rc = red_open_file(&red, sid, RED_FILE_FLAG_READ_ONLY, 0, 0, 0, &ec, &fid);
	if (rc < 0) {
		printf("red_open_file -> rc %d\n", rc);
	}
	if (ec != 0) {
		printf("red_open_file -> ec %u\n", ec);
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
