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

uint64_t st;
uint64_t length_to_read = 20130671;
int64_t length_to_read_block = 60 * 30000;
uint64_t total_length_read = 0;
FILE *fp;
RED red;
uint16_t fid;
int async_reads = 0;

void async_file_read(uint16_t file_id, uint8_t error_code, uint8_t *buffer, uint8_t length_read, void *user_data) {
	(void)file_id;
	(void)buffer;
	(void)user_data;

	if (error_code != 0) {
		printf("async_file_read %d -> ec %u\n", async_reads, error_code);
		return;
	}

	fwrite(buffer, 1, length_read, fp);

	total_length_read += length_read;

	if (total_length_read >= length_to_read) {
		printf("async_file_read DONE total_length_read %lu, length_read %d\n", total_length_read, length_read);
		uint64_t et = microseconds();
		float dur = (et - st) / 1000000.0;

		printf("red_read_file_async in %f sec, %f kB/s\n", dur, length_to_read / dur / 1024);
	} else if ((total_length_read % length_to_read_block) == 0) {
		uint8_t ec;

		printf("red_read_file_async... %d started\n", async_reads+1);
		int rc = red_read_file_async(&red, fid, length_to_read_block, &ec);

		if (rc < 0) {
			printf("red_read_file_async -> rc %d\n", rc);
		}
		if (ec != 0) {
			printf("red_read_file_async -> ec %u\n", ec);
		}

		++async_reads;
	}
}

int main() {
	uint8_t ec;
	int rc;

	// Create IP connection
	IPConnection ipcon;
	ipcon_create(&ipcon);

	fp = fopen("foobar", "wb");

	if (fp == NULL) {
		fprintf(stderr, "Could not fopen\n");
		exit(1);
	}

	// Create device object
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
		printf("red_acquire_string -> rc %d\n", rc);
	}
	if (ec != 0) {
		printf("red_acquire_string -> ec %u\n", ec);
	}
	printf("red_acquire_string -> sid %u\n", sid);

	rc = red_set_string_chunk(&red, sid, 0, "/tmp/foobar2", &ec);
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

	rc = red_open_file(&red, sid, RED_FILE_FLAG_READ_ONLY, 0, 0, 0, &ec, &fid);
	if (rc < 0) {
		printf("red_open_file -> rc %d\n", rc);
	}
	if (ec != 0) {
		printf("red_open_file -> ec %u\n", ec);
	}
	printf("red_open_file -> fid %u\n", fid);

	red_register_callback(&red, RED_CALLBACK_ASYNC_FILE_READ, async_file_read, NULL);

	st = microseconds();

	printf("red_read_file_async... %d started\n", async_reads+1);
	rc = red_read_file_async(&red, fid, length_to_read_block, &ec);
	if (rc < 0) {
		printf("red_read_file_async %d -> rc %d\n", async_reads, rc);
	}
	if (!ec) {
		printf("red_read_file_async %d -> ec %u\n", async_reads, ec);
	}

	++async_reads;

	printf("waiting... 1\n");
	getchar();

	//printf("red_abort_async_file_read...\n");
	rc = red_abort_async_file_read(&red, fid, &ec);
	if (rc < 0) {
		printf("red_abort_async_file_read -> rc %d\n", rc);
	}
	if (!ec) {
		printf("red_abort_async_file_read -> ec %u\n", ec);
	}

	printf("waiting... 2\n");
	getchar();

	printf("red_set_file_position...\n");
	uint64_t position;
	rc = red_set_file_position(&red, fid, -130671, RED_FILE_ORIGIN_CURRENT, &ec, &position);
	if (rc < 0) {
		printf("red_set_file_position -> rc %d\n", rc);
	}
	if (!ec) {
		printf("red_set_file_position -> ec %u\n", ec);
	}
	printf("red_set_file_position -> position %lu\n", position);

	printf("red_get_file_position...\n");
	position = 1234;
	rc = red_get_file_position(&red, fid, &ec, &position);
	if (rc < 0) {
		printf("red_get_file_position -> rc %d\n", rc);
	}
	if (!ec) {
		printf("red_get_file_position -> ec %u\n", ec);
	}
	printf("red_get_file_position -> position %lu\n", position);

	//printf("red_close_file...\n");
	rc = red_release_object(&red, fid, &ec);
	if (rc < 0) {
		printf("red_release_object/file -> rc %d\n", rc);
	}
	if (ec != 0) {
		printf("red_release_object/file -> ec %u\n", ec);
	}

	//printf("red_release_string...\n");
	rc = red_release_object(&red, sid, &ec);
	if (rc < 0) {
		printf("red_release_object/string -> rc %d\n", rc);
	}
	if (ec != 0) {
		printf("red_release_object/string -> ec %u\n", ec);
	}

	//printf("red_destroy...\n");
	red_destroy(&red);
	//printf("ipcon_destroy...\n");
	ipcon_destroy(&ipcon);

	printf("fclose...\n");
	fclose(fp);
}
