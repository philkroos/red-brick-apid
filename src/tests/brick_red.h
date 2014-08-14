/* ***********************************************************
 * This file was automatically generated on 2014-08-14.      *
 *                                                           *
 * Bindings Version 2.1.4                                    *
 *                                                           *
 * If you have a bugfix for this file and want to commit it, *
 * please fix the bug in the generator. You can find a link  *
 * to the generator git on tinkerforge.com                   *
 *************************************************************/

#ifndef BRICK_RED_H
#define BRICK_RED_H

#include "ip_connection.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \defgroup BrickRED RED Brick
 */

/**
 * \ingroup BrickRED
 *
 * Device for running user programs standalone on the stack
 */
typedef Device RED;

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_RELEASE_OBJECT 1

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_GET_NEXT_OBJECT_TABLE_ENTRY 2

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_REWIND_OBJECT_TABLE 3

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_ALLOCATE_STRING 4

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_TRUNCATE_STRING 5

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_GET_STRING_LENGTH 6

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_SET_STRING_CHUNK 7

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_GET_STRING_CHUNK 8

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_ALLOCATE_LIST 9

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_GET_LIST_LENGTH 10

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_GET_LIST_ITEM 11

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_APPEND_TO_LIST 12

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_REMOVE_FROM_LIST 13

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_OPEN_FILE 14

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_GET_FILE_NAME 15

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_GET_FILE_TYPE 16

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_WRITE_FILE 17

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_WRITE_FILE_UNCHECKED 18

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_WRITE_FILE_ASYNC 19

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_READ_FILE 20

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_READ_FILE_ASYNC 21

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_ABORT_ASYNC_FILE_READ 22

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_SET_FILE_POSITION 23

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_GET_FILE_POSITION 24

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_GET_FILE_INFO 27

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_GET_SYMLINK_TARGET 28

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_OPEN_DIRECTORY 29

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_GET_DIRECTORY_NAME 30

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_GET_NEXT_DIRECTORY_ENTRY 31

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_REWIND_DIRECTORY 32

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_START_PROCESS 33

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_GET_IDENTITY 255

/**
 * \ingroup BrickRED
 *
 * Signature: \code void callback(uint16_t file_id, uint8_t error_code, uint8_t length_written, void *user_data) \endcode
 */
#define RED_CALLBACK_ASYNC_FILE_WRITE 25

/**
 * \ingroup BrickRED
 *
 * Signature: \code void callback(uint16_t file_id, uint8_t error_code, uint8_t ret_buffer[60], uint8_t length_read, void *user_data) \endcode
 */
#define RED_CALLBACK_ASYNC_FILE_READ 26


/**
 * \ingroup BrickRED
 */
#define RED_OBJECT_TYPE_STRING 0

/**
 * \ingroup BrickRED
 */
#define RED_OBJECT_TYPE_LIST 1

/**
 * \ingroup BrickRED
 */
#define RED_OBJECT_TYPE_FILE 2

/**
 * \ingroup BrickRED
 */
#define RED_OBJECT_TYPE_DIRECTORY 3

/**
 * \ingroup BrickRED
 */
#define RED_OBJECT_TYPE_PROCESS 4

/**
 * \ingroup BrickRED
 */
#define RED_OBJECT_TYPE_PROGRAM 5

/**
 * \ingroup BrickRED
 */
#define RED_FILE_FLAG_READ_ONLY 1

/**
 * \ingroup BrickRED
 */
#define RED_FILE_FLAG_WRITE_ONLY 2

/**
 * \ingroup BrickRED
 */
#define RED_FILE_FLAG_READ_WRITE 4

/**
 * \ingroup BrickRED
 */
#define RED_FILE_FLAG_APPEND 8

/**
 * \ingroup BrickRED
 */
#define RED_FILE_FLAG_CREATE 16

/**
 * \ingroup BrickRED
 */
#define RED_FILE_FLAG_EXCLUSIVE 32

/**
 * \ingroup BrickRED
 */
#define RED_FILE_FLAG_TRUNCATE 64

/**
 * \ingroup BrickRED
 */
#define RED_FILE_PERMISSION_USER_ALL 448

/**
 * \ingroup BrickRED
 */
#define RED_FILE_PERMISSION_USER_READ 256

/**
 * \ingroup BrickRED
 */
#define RED_FILE_PERMISSION_USER_WRITE 128

/**
 * \ingroup BrickRED
 */
#define RED_FILE_PERMISSION_USER_EXECUTE 64

/**
 * \ingroup BrickRED
 */
#define RED_FILE_PERMISSION_GROUP_ALL 56

/**
 * \ingroup BrickRED
 */
#define RED_FILE_PERMISSION_GROUP_READ 32

/**
 * \ingroup BrickRED
 */
#define RED_FILE_PERMISSION_GROUP_WRITE 16

/**
 * \ingroup BrickRED
 */
#define RED_FILE_PERMISSION_GROUP_EXECUTE 8

/**
 * \ingroup BrickRED
 */
#define RED_FILE_PERMISSION_OTHERS_ALL 7

/**
 * \ingroup BrickRED
 */
#define RED_FILE_PERMISSION_OTHERS_READ 4

/**
 * \ingroup BrickRED
 */
#define RED_FILE_PERMISSION_OTHERS_WRITE 2

/**
 * \ingroup BrickRED
 */
#define RED_FILE_PERMISSION_OTHERS_EXECUTE 1

/**
 * \ingroup BrickRED
 */
#define RED_FILE_TYPE_UNKNOWN 0

/**
 * \ingroup BrickRED
 */
#define RED_FILE_TYPE_REGULAR 1

/**
 * \ingroup BrickRED
 */
#define RED_FILE_TYPE_DIRECTORY 2

/**
 * \ingroup BrickRED
 */
#define RED_FILE_TYPE_CHARACTER 3

/**
 * \ingroup BrickRED
 */
#define RED_FILE_TYPE_BLOCK 4

/**
 * \ingroup BrickRED
 */
#define RED_FILE_TYPE_FIFO 5

/**
 * \ingroup BrickRED
 */
#define RED_FILE_TYPE_SYMLINK 6

/**
 * \ingroup BrickRED
 */
#define RED_FILE_TYPE_SOCKET 7

/**
 * \ingroup BrickRED
 */
#define RED_FILE_ORIGIN_SET 0

/**
 * \ingroup BrickRED
 */
#define RED_FILE_ORIGIN_CURRENT 1

/**
 * \ingroup BrickRED
 */
#define RED_FILE_ORIGIN_END 2

/**
 * \ingroup BrickRED
 *
 * This constant is used to identify a RED Brick.
 *
 * The {@link red_get_identity} function and the
 * {@link IPCON_CALLBACK_ENUMERATE} callback of the IP Connection have a
 * \c device_identifier parameter to specify the Brick's or Bricklet's type.
 */
#define RED_DEVICE_IDENTIFIER 17

/**
 * \ingroup BrickRED
 *
 * Creates the device object \c red with the unique device ID \c uid and adds
 * it to the IPConnection \c ipcon.
 */
void red_create(RED *red, const char *uid, IPConnection *ipcon);

/**
 * \ingroup BrickRED
 *
 * Removes the device object \c red from its IPConnection and destroys it.
 * The device object cannot be used anymore afterwards.
 */
void red_destroy(RED *red);

/**
 * \ingroup BrickRED
 *
 * Returns the response expected flag for the function specified by the
 * \c function_id parameter. It is *true* if the function is expected to
 * send a response, *false* otherwise.
 *
 * For getter functions this is enabled by default and cannot be disabled,
 * because those functions will always send a response. For callback
 * configuration functions it is enabled by default too, but can be disabled
 * via the red_set_response_expected function. For setter functions it is
 * disabled by default and can be enabled.
 *
 * Enabling the response expected flag for a setter function allows to
 * detect timeouts and other error conditions calls of this setter as well.
 * The device will then send a response for this purpose. If this flag is
 * disabled for a setter function then no response is send and errors are
 * silently ignored, because they cannot be detected.
 */
int red_get_response_expected(RED *red, uint8_t function_id, bool *ret_response_expected);

/**
 * \ingroup BrickRED
 *
 * Changes the response expected flag of the function specified by the
 * \c function_id parameter. This flag can only be changed for setter
 * (default value: *false*) and callback configuration functions
 * (default value: *true*). For getter functions it is always enabled and
 * callbacks it is always disabled.
 *
 * Enabling the response expected flag for a setter function allows to detect
 * timeouts and other error conditions calls of this setter as well. The device
 * will then send a response for this purpose. If this flag is disabled for a
 * setter function then no response is send and errors are silently ignored,
 * because they cannot be detected.
 */
int red_set_response_expected(RED *red, uint8_t function_id, bool response_expected);

/**
 * \ingroup BrickRED
 *
 * Changes the response expected flag for all setter and callback configuration
 * functions of this device at once.
 */
int red_set_response_expected_all(RED *red, bool response_expected);

/**
 * \ingroup BrickRED
 *
 * Registers a callback with ID \c id to the function \c callback. The
 * \c user_data will be given as a parameter of the callback.
 */
void red_register_callback(RED *red, uint8_t id, void *callback, void *user_data);

/**
 * \ingroup BrickRED
 *
 * Returns the API version (major, minor, release) of the bindings for this
 * device.
 */
int red_get_api_version(RED *red, uint8_t ret_api_version[3]);

/**
 * \ingroup BrickRED
 *
 * 
 */
int red_release_object(RED *red, uint16_t object_id, uint8_t *ret_error_code);

/**
 * \ingroup BrickRED
 *
 * 
 */
int red_get_next_object_table_entry(RED *red, uint8_t type, uint8_t *ret_error_code, uint16_t *ret_object_id);

/**
 * \ingroup BrickRED
 *
 * 
 */
int red_rewind_object_table(RED *red, uint8_t type, uint8_t *ret_error_code);

/**
 * \ingroup BrickRED
 *
 * 
 */
int red_allocate_string(RED *red, uint32_t length_to_reserve, const char buffer[60], uint8_t *ret_error_code, uint16_t *ret_string_id);

/**
 * \ingroup BrickRED
 *
 * 
 */
int red_truncate_string(RED *red, uint16_t string_id, uint32_t length, uint8_t *ret_error_code);

/**
 * \ingroup BrickRED
 *
 * 
 */
int red_get_string_length(RED *red, uint16_t string_id, uint8_t *ret_error_code, uint32_t *ret_length);

/**
 * \ingroup BrickRED
 *
 * 
 */
int red_set_string_chunk(RED *red, uint16_t string_id, uint32_t offset, const char buffer[58], uint8_t *ret_error_code);

/**
 * \ingroup BrickRED
 *
 * 
 */
int red_get_string_chunk(RED *red, uint16_t string_id, uint32_t offset, uint8_t *ret_error_code, char ret_buffer[63]);

/**
 * \ingroup BrickRED
 *
 * 
 */
int red_allocate_list(RED *red, uint16_t length_to_reserve, uint8_t *ret_error_code, uint16_t *ret_list_id);

/**
 * \ingroup BrickRED
 *
 * 
 */
int red_get_list_length(RED *red, uint16_t list_id, uint8_t *ret_error_code, uint16_t *ret_length);

/**
 * \ingroup BrickRED
 *
 * 
 */
int red_get_list_item(RED *red, uint16_t list_id, uint16_t index, uint8_t *ret_error_code, uint16_t *ret_item_object_id);

/**
 * \ingroup BrickRED
 *
 * 
 */
int red_append_to_list(RED *red, uint16_t list_id, uint16_t item_object_id, uint8_t *ret_error_code);

/**
 * \ingroup BrickRED
 *
 * 
 */
int red_remove_from_list(RED *red, uint16_t list_id, uint16_t index, uint8_t *ret_error_code);

/**
 * \ingroup BrickRED
 *
 * 
 */
int red_open_file(RED *red, uint16_t name_string_id, uint16_t flags, uint16_t permissions, uint32_t user_id, uint32_t group_id, uint8_t *ret_error_code, uint16_t *ret_file_id);

/**
 * \ingroup BrickRED
 *
 * 
 */
int red_get_file_name(RED *red, uint16_t file_id, uint8_t *ret_error_code, uint16_t *ret_name_string_id);

/**
 * \ingroup BrickRED
 *
 * 
 */
int red_get_file_type(RED *red, uint16_t file_id, uint8_t *ret_error_code, uint8_t *ret_type);

/**
 * \ingroup BrickRED
 *
 * 
 */
int red_write_file(RED *red, uint16_t file_id, uint8_t buffer[61], uint8_t length_to_write, uint8_t *ret_error_code, uint8_t *ret_length_written);

/**
 * \ingroup BrickRED
 *
 * 
 */
int red_write_file_unchecked(RED *red, uint16_t file_id, uint8_t buffer[61], uint8_t length_to_write);

/**
 * \ingroup BrickRED
 *
 * 
 */
int red_write_file_async(RED *red, uint16_t file_id, uint8_t buffer[61], uint8_t length_to_write);

/**
 * \ingroup BrickRED
 *
 * 
 */
int red_read_file(RED *red, uint16_t file_id, uint8_t length_to_read, uint8_t *ret_error_code, uint8_t ret_buffer[62], uint8_t *ret_length_read);

/**
 * \ingroup BrickRED
 *
 * 
 */
int red_read_file_async(RED *red, uint16_t file_id, uint64_t length_to_read, uint8_t *ret_error_code);

/**
 * \ingroup BrickRED
 *
 * 
 */
int red_abort_async_file_read(RED *red, uint16_t file_id, uint8_t *ret_error_code);

/**
 * \ingroup BrickRED
 *
 * 
 */
int red_set_file_position(RED *red, uint16_t file_id, int64_t offset, uint8_t origin, uint8_t *ret_error_code, uint64_t *ret_position);

/**
 * \ingroup BrickRED
 *
 * 
 */
int red_get_file_position(RED *red, uint16_t file_id, uint8_t *ret_error_code, uint64_t *ret_position);

/**
 * \ingroup BrickRED
 *
 * 
 */
int red_get_file_info(RED *red, uint16_t name_string_id, bool follow_symlink, uint8_t *ret_error_code, uint8_t *ret_type, uint16_t *ret_permissions, uint32_t *ret_user_id, uint32_t *ret_group_id, uint64_t *ret_length, uint64_t *ret_access_time, uint64_t *ret_modification_time, uint64_t *ret_status_change_time);

/**
 * \ingroup BrickRED
 *
 * 
 */
int red_get_symlink_target(RED *red, uint16_t name_string_id, bool canonicalize, uint8_t *ret_error_code, uint16_t *ret_target_string_id);

/**
 * \ingroup BrickRED
 *
 * 
 */
int red_open_directory(RED *red, uint16_t name_string_id, uint8_t *ret_error_code, uint16_t *ret_directory_id);

/**
 * \ingroup BrickRED
 *
 * 
 */
int red_get_directory_name(RED *red, uint16_t directory_id, uint8_t *ret_error_code, uint16_t *ret_name_string_id);

/**
 * \ingroup BrickRED
 *
 * 
 */
int red_get_next_directory_entry(RED *red, uint16_t directory_id, uint8_t *ret_error_code, uint16_t *ret_name_string_id, uint8_t *ret_type);

/**
 * \ingroup BrickRED
 *
 * 
 */
int red_rewind_directory(RED *red, uint16_t directory_id, uint8_t *ret_error_code);

/**
 * \ingroup BrickRED
 *
 * 
 */
int red_start_process(RED *red, uint16_t command_string_id, uint16_t argument_string_ids[20], uint8_t argument_count, uint16_t environment_string_ids[8], uint8_t environment_count, bool merge_stdout_and_stderr, uint8_t *ret_error_code, uint16_t *ret_process_id);

/**
 * \ingroup BrickRED
 *
 * Returns the UID, the UID where the Brick is connected to, 
 * the position, the hardware and firmware version as well as the
 * device identifier.
 * 
 * The position can be '0'-'8' (stack position).
 * 
 * The device identifier numbers can be found :ref:`here <device_identifier>`.
 * |device_identifier_constant|
 */
int red_get_identity(RED *red, char ret_uid[8], char ret_connected_uid[8], char *ret_position, uint8_t ret_hardware_version[3], uint8_t ret_firmware_version[3], uint16_t *ret_device_identifier);

#ifdef __cplusplus
}
#endif

#endif
