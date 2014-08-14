#!/bin/sh -x
gcc -Wall -Wextra -O2 -pthread ip_connection.c brick_red.c $1
