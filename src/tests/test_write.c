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
	// Create IP connection
	IPConnection ipcon;
	ipcon_create(&ipcon);

	// Create device object
	RED red;
	red_create(&red, UID, &ipcon);

	// Connect to brickd
	if(ipcon_connect(&ipcon, HOST, PORT) < 0) {
		fprintf(stderr, "Could not connect\n");
		exit(1);
	}
	// Don't use device before ipcon is connected

	uint16_t sid;
	if (red_acquire_string(&red, 20, &sid) < 0) {
		printf("red_acquire_string failed\n");
	}
	printf("red_acquire_string -> sid %u\n", sid);
	bool success;
	int rc = red_set_string_chunk(&red, sid, 0, "/tmp/foobar3", &success);

	if (rc < 0) {
		printf("red_set_string_chunk -> rc %d\n", rc);
	}
	if (!success) {
		printf("red_set_string_chunk -> success false\n");
	}

	int32_t length;
	if (red_get_string_length(&red, sid, &length)< 0) {
		printf("red_set_string_length failed\n");
	}
	printf("red_set_string_length -> length %d\n", length);

	uint16_t fid;
	if (red_open_file(&red, sid, RED_FILE_FLAG_WRITE_ONLY | RED_FILE_FLAG_CREATE | RED_FILE_FLAG_TRUNCATE, 0755, 0, 0, &fid) < 0) {
		printf("red_open_file failed\n");
	}
	printf("red_open_file -> fid %u\n", fid);

	uint32_t ec;
	if (red_get_last_error(&red, &ec) < 0) {
		printf("red_get_last_error failed\n");
	}
	printf("red_get_last_error -> ec %u\n", ec);

	uint8_t buffer[61];
	int8_t length_written;
	uint64_t st = microseconds();

	memcpy(buffer, "foobar x1\nfoobar y2\nfoobar z3\nfoobar u4\nfoobar v5\nfoobar w6\n\n", 61);

	for (int i = 0; i < 30000; ++i) {
		rc = red_write_file(&red, fid, buffer, 61, &length_written);

		if (rc < 0) {
			printf("red_write_file -> rc %d\n", rc);
		}

		if (length_written != 61) {
			printf("red_write_file -> length_written %d\n", length_written);
		}
	}

	uint64_t et = microseconds();

	printf("30000x red_write_file in %f sec\n", (et - st) / 1000000.0);

	if (red_close_file(&red, fid, &success) < 0) {
		printf("red_close_file failed\n");
	}
	if (!success) {
		printf("red_close_file -> success false\n");
	}

	if (red_release_string(&red, sid, &success) < 0) {
		printf("red_release_string failed\n");
	}
	if (!success) {
		printf("red_release_string -> success false\n");
	}

	red_destroy(&red);
	ipcon_destroy(&ipcon);
}
