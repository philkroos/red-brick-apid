#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>

#include "ip_connection.h"
#include "brick_red.h"

#define HOST "localhost"
#define PORT 4223
#define UID "3hG6BK" // Change to your UID

#include "utils.c"

#define FILE_MAX_WRITE_UNCHECKED_BUFFER_LENGTH 61

uint64_t total_types = 0;
uint64_t st;
uint8_t buffer[FILE_MAX_WRITE_UNCHECKED_BUFFER_LENGTH];
uint16_t fid;
RED red;
int done = 0;
FILE *fp;

void write_burst();

void async_file_write(uint16_t file_id, uint8_t error_code, uint8_t length_written, void *user_data) {
	(void)length_written;
	(void)user_data;

	if (file_id != fid) {
		return;
	}

	printf("async_file_write -> ec %u\n", error_code);

	if (done) {
		uint64_t et = microseconds();
		float dur = (et - st) / 1000000.0;

		printf("uploaded %llu bytes in %f sec, %f kB/s\n", (unsigned long long)total_types, dur, total_types / dur / 1024);

		return;
	}

	write_burst();
}

void write_burst(void) {
	int rc;
	int i;

	printf("write_burst\n");

	for (i = 0; i < 30000; ++i) {
		rc = fread(buffer, 1, FILE_MAX_WRITE_UNCHECKED_BUFFER_LENGTH, fp);

		if (rc < 0) {
			printf("fread -> rc %d\n", rc);
			_exit(1); // FIXME
		}

		if (rc == 0) {
			done = 1;
			break;
		}

		total_types += rc;

		rc = red_write_file_unchecked(&red, fid, buffer, rc);
		if (rc < 0) {
			printf("red_write_file_unchecked -> rc %d\n", rc);
		}
	}

	rc = red_write_file_async(&red, fid, buffer, 0);
	if (rc < 0) {
		printf("red_write_file_async -> rc %d\n", rc);
	}
}

int main(int argc, char **argv) {
	uint8_t ec;
	int rc;

	if (argc != 3) {
		printf("usage: %s <local-input> <output-on-red-brick>\n", argv[0]);
		return 1;
	}

	fp = fopen(argv[1], "rb");

	if (fp == NULL) {
		printf("error: could not open '%s'\n", argv[1]);
		return 1;
	}

	printf("uploading '%s' to '%s'\n", argv[1], argv[2]);

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
	if (allocate_string(&red, argv[2], &sid)) {
		return -1;
	}

	rc = red_open_file(&red, sid, RED_FILE_FLAG_WRITE_ONLY | RED_FILE_FLAG_CREATE | RED_FILE_FLAG_NON_BLOCKING | RED_FILE_FLAG_TRUNCATE, 0755, 0, 0, &ec, &fid);
	if (rc < 0) {
		printf("red_open_file -> rc %d\n", rc);
		goto cleanup;
	}
	if (ec != 0) {
		printf("red_open_file -> ec %u\n", ec);
		goto cleanup;
	}
	printf("red_open_file -> fid %u\n", fid);

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

	fclose(fp);

	return 0;
}
