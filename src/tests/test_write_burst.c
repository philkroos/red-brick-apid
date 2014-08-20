#include <stdio.h>
#include <time.h>
#include <sys/time.h>

#include "ip_connection.h"
#include "brick_red.h"

//#define HOST "192.168.178.27"
#define HOST "localhost"
#define PORT 4223
#define UID "3hG4aq"
//#define UID "3hG6BK"

#include "utils.c"

uint64_t st;
uint8_t buffer[61];
uint16_t fid;
RED red;
int k = 10;

void write_burst();

void async_file_write(uint16_t file_id, uint8_t error_code, uint8_t length_written, void *user_data) {
	(void)user_data;

	printf("async_file_write k %d -> ec %u\n", k, error_code);

	if (k > 0) {
		--k;
		write_burst();
	} else {
		uint64_t et = microseconds();
		float dur = (et - st) / 1000000.0;

		printf("RED_CALLBACK_ASYNC_FILE_WRITE file_id %u, length_written %d, in %f sec, %f kB/s\n", file_id, length_written, dur, 10 * 30001 * 61 / dur / 1024);
	}
}

void write_burst(void) {
	int rc;

	printf("write_burst k %d\n", k);

	int i;
	for (i = 0; i < 30000; ++i) {
		rc = red_write_file_unchecked(&red, fid, buffer, 61);
		if (rc < 0) {
			printf("red_write_file_unchecked -> rc %d\n", rc);
		}
	}

	rc = red_write_file_async(&red, fid, buffer, 61);
	if (rc < 0) {
		printf("red_write_file_async -> rc %d\n", rc);
	}
}

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

	uint16_t sid;
	if (allocate_string(&red, "/tmp/foobar", &sid)) {
		return -1;
	}

	rc = red_open_file(&red, sid, RED_FILE_FLAG_WRITE_ONLY | RED_FILE_FLAG_CREATE | RED_FILE_FLAG_TRUNCATE, 0755, 0, 0, &ec, &fid);
	if (rc < 0) {
		printf("red_open_file -> rc %d\n", rc);
		goto cleanup;
	}
	if (ec != 0) {
		printf("red_open_file -> ec %u\n", ec);
		goto cleanup;
	}
	printf("red_open_file -> fid %u\n", fid);

	memcpy(buffer, "foobar x1\nfoobar x2\nfoobar x3\nfoobar x4\nfoobar x5\nfoobar x6\n\n", 61);

	red_register_callback(&red, RED_CALLBACK_ASYNC_FILE_WRITE, async_file_write, NULL);

	st = microseconds();

	write_burst();

	printf("waiting...\n");
	getchar();

	release_object(&red, fid, "file");

cleanup:
	release_object(&red, sid, "string");

	red_destroy(&red);
	ipcon_destroy(&ipcon);

	return 0;
}
