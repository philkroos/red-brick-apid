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

int allocate_string_object(RED *red, const char *string, uint16_t *object_id) {
	uint8_t ec;
	uint16_t sid;
	int rc;

	rc = red_allocate_string(red, 20, string, &ec, &sid);
	if (rc < 0) {
		printf("red_allocate_string '%s' -> rc %d\n", string, rc);
		return -1;
	}
	if (ec != 0) {
		printf("red_allocate_string '%s' -> ec %u\n", string, ec);
		return -1;
	}
	printf("red_allocate_string '%s' -> sid %u\n", string, sid);

	/*rc = red_set_string_chunk(red, sid, 0, string, &ec);
	if (rc < 0) {
		printf("red_set_string_chunk '%s' -> rc %d\n", string, rc);
		return -1;
	}
	if (ec != 0) {
		printf("red_set_string_chunk '%s' -> ec %u\n", string, ec);
		return -1;
	}*/

	*object_id = sid;

	return 0;
}
