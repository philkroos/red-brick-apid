#include <stdio.h>
#include <time.h>
#include <sys/time.h>

#include "ip_connection.h"
#include "brick_red.h"

#define HOST "localhost"
#define PORT 4223
#define UID "3hG6BK" // Change to your UID

#include "utils.c"

void process_state_changed(uint16_t process_id, uint8_t state, uint8_t exit_code, void *user_data) {
	(void)process_id;
	(void)user_data;

	printf("process_state_changed state %u, exit_code %u\n", state, exit_code);
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

	uint16_t command_sid;
	if (allocate_string(&red, "/tmp/blubb.sh", &command_sid)) {
		return -1;
	}

	uint16_t arguments_lid;
	rc = red_allocate_list(&red, 20, &ec, &arguments_lid);
	if (rc < 0) {
		printf("red_allocate_list -> rc %d\n", rc);
	}
	if (ec != 0) {
		printf("red_allocate_list -> ec %u\n", ec);
	}
	printf("red_allocate_list -> sid %u\n", arguments_lid);

	uint16_t argument_sid;
	if (allocate_string(&red, "whatever", &argument_sid)) {
		return -1;
	}

	rc = red_append_to_list(&red, arguments_lid, argument_sid, &ec);
	if (rc < 0) {
		printf("red_append_to_list -> rc %d\n", rc);
	}
	if (ec != 0) {
		printf("red_append_to_list -> ec %u\n", ec);
	}

	uint16_t environment_lid;
	rc = red_allocate_list(&red, 20, &ec, &environment_lid);
	if (rc < 0) {
		printf("red_allocate_list -> rc %d\n", rc);
	}
	if (ec != 0) {
		printf("red_allocate_list -> ec %u\n", ec);
	}
	printf("red_allocate_list -> sid %u\n", environment_lid);

	uint16_t working_directory_sid;
	if (allocate_string(&red, "/tmp", &working_directory_sid)) {
		return -1;
	}

	uint16_t null_sid;
	if (allocate_string(&red, "/dev/null", &null_sid)) {
		return -1;
	}

	uint16_t stdin_fid;
	rc = red_open_file(&red, null_sid, RED_FILE_FLAG_READ_ONLY, 0, 0, 0, &ec, &stdin_fid);
	if (rc < 0) {
		printf("red_open_file -> rc %d\n", rc);
	}
	if (ec != 0) {
		printf("red_open_file -> ec %u\n", ec);
	}
	printf("red_open_file -> fid %u\n", stdin_fid);

	uint16_t stdout_fid;
	rc = red_open_file(&red, null_sid, RED_FILE_FLAG_WRITE_ONLY, 0, 0, 0, &ec, &stdout_fid);
	if (rc < 0) {
		printf("red_open_file -> rc %d\n", rc);
	}
	if (ec != 0) {
		printf("red_open_file -> ec %u\n", ec);
	}
	printf("red_open_file -> fid %u\n", stdout_fid);

	red_register_callback(&red, RED_CALLBACK_PROCESS_STATE_CHANGED, process_state_changed, &red);

	uint16_t pid;
	rc = red_spawn_process(&red, command_sid, arguments_lid, environment_lid, working_directory_sid, 0, 0, stdin_fid, stdout_fid, stdout_fid, &ec, &pid);
	if (rc < 0) {
		printf("red_spawn_process -> rc %d\n", rc);
	}
	if (ec != 0) {
		printf("red_spawn_process -> ec %u\n", ec);
	}
	printf("red_spawn_process -> pid %u\n", pid);

	getchar();

	release_object(&red, command_sid, "string");
	release_object(&red, arguments_lid, "list");
	release_object(&red, argument_sid, "string");
	release_object(&red, environment_lid, "list");
	release_object(&red, working_directory_sid, "string");
	release_object(&red, null_sid, "string");
	release_object(&red, stdin_fid, "string");
	release_object(&red, stdout_fid, "string");
	release_object(&red, pid, "process");

	red_destroy(&red);
	ipcon_destroy(&ipcon);

	return 0;
}
