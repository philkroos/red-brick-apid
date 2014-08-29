/* ***********************************************************
 * This file was automatically generated on 2014-08-29.      *
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
#define RED_FUNCTION_OPEN_INVENTORY 2

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_GET_INVENTORY_TYPE 3

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_GET_NEXT_INVENTORY_ENTRY 4

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_REWIND_INVENTORY 5

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_ALLOCATE_STRING 6

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_TRUNCATE_STRING 7

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_GET_STRING_LENGTH 8

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_SET_STRING_CHUNK 9

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_GET_STRING_CHUNK 10

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_ALLOCATE_LIST 11

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_GET_LIST_LENGTH 12

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_GET_LIST_ITEM 13

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_APPEND_TO_LIST 14

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_REMOVE_FROM_LIST 15

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_OPEN_FILE 16

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_CREATE_PIPE 17

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_GET_FILE_TYPE 18

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_GET_FILE_NAME 19

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_GET_FILE_FLAGS 20

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_READ_FILE 21

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_READ_FILE_ASYNC 22

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_ABORT_ASYNC_FILE_READ 23

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_WRITE_FILE 24

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_WRITE_FILE_UNCHECKED 25

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_WRITE_FILE_ASYNC 26

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_SET_FILE_POSITION 27

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_GET_FILE_POSITION 28

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_GET_FILE_INFO 31

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_GET_SYMLINK_TARGET 32

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_OPEN_DIRECTORY 33

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_GET_DIRECTORY_NAME 34

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_GET_NEXT_DIRECTORY_ENTRY 35

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_REWIND_DIRECTORY 36

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_CREATE_DIRECTORY 37

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_SPAWN_PROCESS 38

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_KILL_PROCESS 39

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_GET_PROCESS_COMMAND 40

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_GET_PROCESS_ARGUMENTS 41

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_GET_PROCESS_ENVIRONMENT 42

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_GET_PROCESS_WORKING_DIRECTORY 43

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_GET_PROCESS_USER_ID 44

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_GET_PROCESS_GROUP_ID 45

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_GET_PROCESS_STDIN 46

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_GET_PROCESS_STDOUT 47

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_GET_PROCESS_STDERR 48

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_GET_PROCESS_STATE 49

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_DEFINE_PROGRAM 51

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_UNDEFINE_PROGRAM 52

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_GET_PROGRAM_IDENTIFIER 53

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_GET_PROGRAM_DIRECTORY 54

/**
 * \ingroup BrickRED
 */
#define RED_FUNCTION_GET_IDENTITY 255

/**
 * \ingroup BrickRED
 *
 * Signature: \code void callback(uint16_t file_id, uint8_t error_code, uint8_t ret_buffer[60], uint8_t length_read, void *user_data) \endcode
 * 
 * This callback reports the result of a call to the {@link red_read_file_async}
 * function.
 */
#define RED_CALLBACK_ASYNC_FILE_READ 29

/**
 * \ingroup BrickRED
 *
 * Signature: \code void callback(uint16_t file_id, uint8_t error_code, uint8_t length_written, void *user_data) \endcode
 * 
 * This callback reports the result of a call to the {@link red_write_file_async}
 * function.
 */
#define RED_CALLBACK_ASYNC_FILE_WRITE 30

/**
 * \ingroup BrickRED
 *
 * Signature: \code void callback(uint16_t process_id, uint8_t state, uint8_t exit_code, void *user_data) \endcode
 */
#define RED_CALLBACK_PROCESS_STATE_CHANGED 50


/**
 * \ingroup BrickRED
 */
#define RED_OBJECT_TYPE_INVENTORY 0

/**
 * \ingroup BrickRED
 */
#define RED_OBJECT_TYPE_STRING 1

/**
 * \ingroup BrickRED
 */
#define RED_OBJECT_TYPE_LIST 2

/**
 * \ingroup BrickRED
 */
#define RED_OBJECT_TYPE_FILE 3

/**
 * \ingroup BrickRED
 */
#define RED_OBJECT_TYPE_DIRECTORY 4

/**
 * \ingroup BrickRED
 */
#define RED_OBJECT_TYPE_PROCESS 5

/**
 * \ingroup BrickRED
 */
#define RED_OBJECT_TYPE_PROGRAM 6

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
#define RED_FILE_FLAG_NO_ACCESS_TIME 64

/**
 * \ingroup BrickRED
 */
#define RED_FILE_FLAG_NO_FOLLOW 128

/**
 * \ingroup BrickRED
 */
#define RED_FILE_FLAG_TRUNCATE 256

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
#define RED_PIPE_FLAG_NON_BLOCKING_READ 1

/**
 * \ingroup BrickRED
 */
#define RED_PIPE_FLAG_NON_BLOCKING_WRITE 2

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
#define RED_FILE_TYPE_PIPE 8

/**
 * \ingroup BrickRED
 */
#define RED_FILE_ORIGIN_BEGINNING 0

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
 */
#define RED_PROCESS_SIGNAL_INTERRUPT 2

/**
 * \ingroup BrickRED
 */
#define RED_PROCESS_SIGNAL_QUIT 3

/**
 * \ingroup BrickRED
 */
#define RED_PROCESS_SIGNAL_ABORT 6

/**
 * \ingroup BrickRED
 */
#define RED_PROCESS_SIGNAL_KILL 9

/**
 * \ingroup BrickRED
 */
#define RED_PROCESS_SIGNAL_USER1 10

/**
 * \ingroup BrickRED
 */
#define RED_PROCESS_SIGNAL_USER2 12

/**
 * \ingroup BrickRED
 */
#define RED_PROCESS_SIGNAL_TERMINATE 15

/**
 * \ingroup BrickRED
 */
#define RED_PROCESS_SIGNAL_CONTINUE 18

/**
 * \ingroup BrickRED
 */
#define RED_PROCESS_SIGNAL_STOP 19

/**
 * \ingroup BrickRED
 */
#define RED_PROCESS_STATE_UNKNOWN 0

/**
 * \ingroup BrickRED
 */
#define RED_PROCESS_STATE_RUNNING 1

/**
 * \ingroup BrickRED
 */
#define RED_PROCESS_STATE_EXITED 2

/**
 * \ingroup BrickRED
 */
#define RED_PROCESS_STATE_KILLED 3

/**
 * \ingroup BrickRED
 */
#define RED_PROCESS_STATE_STOPPED 4

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
 * Decreases the reference count of an object by one and returns the resulting
 * error code. If the reference count reaches zero the object is destroyed.
 */
int red_release_object(RED *red, uint16_t object_id, uint8_t *ret_error_code);

/**
 * \ingroup BrickRED
 *
 * Opens the inventory for a specific object type and allocates a new inventory
 * object for it.
 * 
 * Returns the object ID of the new directory object and the resulting error code.
 */
int red_open_inventory(RED *red, uint8_t type, uint8_t *ret_error_code, uint16_t *ret_inventory_id);

/**
 * \ingroup BrickRED
 *
 * Returns the object type of a inventory object and the resulting error code.
 */
int red_get_inventory_type(RED *red, uint16_t inventory_id, uint8_t *ret_error_code, uint8_t *ret_type);

/**
 * \ingroup BrickRED
 *
 * Returns the object ID of the next object in an inventory object and the
 * resulting error code. If there is not next object then error code
 * ``API_E_NO_MORE_DATA`` is returned. To rewind an inventory object call
 * {@link red_rewind_inventory}.
 */
int red_get_next_inventory_entry(RED *red, uint16_t inventory_id, uint8_t *ret_error_code, uint16_t *ret_object_id);

/**
 * \ingroup BrickRED
 *
 * Rewinds an inventory object and returns the resulting error code.
 */
int red_rewind_inventory(RED *red, uint16_t inventory_id, uint8_t *ret_error_code);

/**
 * \ingroup BrickRED
 *
 * Allocates a new string object, reserves ``length_to_reserve`` bytes memory
 * for it and sets up to the first 60 bytes. Set ``length_to_reserve`` to the
 * length of the string that should be stored in the string object.
 * 
 * Returns the object ID of the new string object and the resulting error code.
 */
int red_allocate_string(RED *red, uint32_t length_to_reserve, const char buffer[60], uint8_t *ret_error_code, uint16_t *ret_string_id);

/**
 * \ingroup BrickRED
 *
 * Truncates a string object to ``length`` bytes and returns the resulting
 * error code.
 */
int red_truncate_string(RED *red, uint16_t string_id, uint32_t length, uint8_t *ret_error_code);

/**
 * \ingroup BrickRED
 *
 * Returns the length of a string object in bytes and the resulting error code.
 */
int red_get_string_length(RED *red, uint16_t string_id, uint8_t *ret_error_code, uint32_t *ret_length);

/**
 * \ingroup BrickRED
 *
 * Sets a chunk of up to 58 bytes in a string object beginning at ``offset``.
 * 
 * Returns the resulting error code.
 */
int red_set_string_chunk(RED *red, uint16_t string_id, uint32_t offset, const char buffer[58], uint8_t *ret_error_code);

/**
 * \ingroup BrickRED
 *
 * Returns a chunk up to 63 bytes from a string object beginning at ``offset`` and
 * returns the resulting error code.
 */
int red_get_string_chunk(RED *red, uint16_t string_id, uint32_t offset, uint8_t *ret_error_code, char ret_buffer[63]);

/**
 * \ingroup BrickRED
 *
 * Allocates a new list object and reserves memory for ``length_to_reserve`` items.
 * Set ``length_to_reserve`` to the number of items that should be stored in the
 * list object.
 * 
 * Returns the object ID of the new list object and the resulting error code.
 */
int red_allocate_list(RED *red, uint16_t length_to_reserve, uint8_t *ret_error_code, uint16_t *ret_list_id);

/**
 * \ingroup BrickRED
 *
 * Returns the length of a list object in items and the resulting error code.
 */
int red_get_list_length(RED *red, uint16_t list_id, uint8_t *ret_error_code, uint16_t *ret_length);

/**
 * \ingroup BrickRED
 *
 * Returns the object ID of the object stored at ``index`` in a list object and
 * returns the resulting error code.
 */
int red_get_list_item(RED *red, uint16_t list_id, uint16_t index, uint8_t *ret_error_code, uint16_t *ret_item_object_id);

/**
 * \ingroup BrickRED
 *
 * Appends an object to a list object and increases the reference count of the
 * appended object by one.
 * 
 * Returns the resulting error code.
 */
int red_append_to_list(RED *red, uint16_t list_id, uint16_t item_object_id, uint8_t *ret_error_code);

/**
 * \ingroup BrickRED
 *
 * Removes the object stored at ``index`` from a list object and decreases the
 * reference count of the removed object by one.
 * 
 * Returns the resulting error code.
 */
int red_remove_from_list(RED *red, uint16_t list_id, uint16_t index, uint8_t *ret_error_code);

/**
 * \ingroup BrickRED
 *
 * Opens an existing file or creates a new file and allocates a new file object
 * for it.
 * 
 * The reference count of the name string object is increased by one. When the
 * file object is destroyed then the reference count of the name string object is
 * decreased by one. Also the name string object is locked and cannot be modified
 * while the file object holds a reference to it.
 * 
 * Returns the object ID of the new file object and the resulting error code.
 */
int red_open_file(RED *red, uint16_t name_string_id, uint16_t flags, uint16_t permissions, uint32_t user_id, uint32_t group_id, uint8_t *ret_error_code, uint16_t *ret_file_id);

/**
 * \ingroup BrickRED
 *
 * Creates a new pipe and allocates a new file object for it.
 * 
 * Returns the object ID of the new file object and the resulting error code.
 */
int red_create_pipe(RED *red, uint16_t flags, uint8_t *ret_error_code, uint16_t *ret_file_id);

/**
 * \ingroup BrickRED
 *
 * Returns the type of a file object and the resulting error code.
 */
int red_get_file_type(RED *red, uint16_t file_id, uint8_t *ret_error_code, uint8_t *ret_type);

/**
 * \ingroup BrickRED
 *
 * Returns the name of a file object and the resulting error code.
 */
int red_get_file_name(RED *red, uint16_t file_id, uint8_t *ret_error_code, uint16_t *ret_name_string_id);

/**
 * \ingroup BrickRED
 *
 * Returns the flags used to open or create a file object and the resulting
 * error code.
 */
int red_get_file_flags(RED *red, uint16_t file_id, uint8_t *ret_error_code, uint16_t *ret_flags);

/**
 * \ingroup BrickRED
 *
 * Reads up to 62 bytes from a file object.
 * 
 * Returns the read bytes and the resulting error code.
 */
int red_read_file(RED *red, uint16_t file_id, uint8_t length_to_read, uint8_t *ret_error_code, uint8_t ret_buffer[62], uint8_t *ret_length_read);

/**
 * \ingroup BrickRED
 *
 * Reads up to 2\ :sup:`63`\  - 1 bytes from a file object.
 * 
 * Returns the resulting error code.
 * 
 * Reports the read bytes in 60 byte chunks and the resulting error code of the
 * read operation via the {@link RED_CALLBACK_ASYNC_FILE_READ} callback.
 */
int red_read_file_async(RED *red, uint16_t file_id, uint64_t length_to_read, uint8_t *ret_error_code);

/**
 * \ingroup BrickRED
 *
 * Aborts a {@link red_read_file_async} operation in progress.
 * 
 * Returns the resulting error code.
 */
int red_abort_async_file_read(RED *red, uint16_t file_id, uint8_t *ret_error_code);

/**
 * \ingroup BrickRED
 *
 * Writes up to 61 bytes to a file object.
 * 
 * Returns the actual number of bytes written and the resulting error code.
 */
int red_write_file(RED *red, uint16_t file_id, uint8_t buffer[61], uint8_t length_to_write, uint8_t *ret_error_code, uint8_t *ret_length_written);

/**
 * \ingroup BrickRED
 *
 * Writes up to 61 bytes to a file object.
 * 
 * Does neither report the actual number of bytes written nor the resulting error
 * code.
 */
int red_write_file_unchecked(RED *red, uint16_t file_id, uint8_t buffer[61], uint8_t length_to_write);

/**
 * \ingroup BrickRED
 *
 * Writes up to 61 bytes to a file object.
 * 
 * Reports the actual number of bytes written and the resulting error code via the
 * {@link RED_CALLBACK_ASYNC_FILE_WRITE} callback.
 */
int red_write_file_async(RED *red, uint16_t file_id, uint8_t buffer[61], uint8_t length_to_write);

/**
 * \ingroup BrickRED
 *
 * Set the current seek position of a file object in bytes relative to ``origin``.
 * 
 * Returns the resulting absolute seek position and error code.
 */
int red_set_file_position(RED *red, uint16_t file_id, int64_t offset, uint8_t origin, uint8_t *ret_error_code, uint64_t *ret_position);

/**
 * \ingroup BrickRED
 *
 * Returns the current seek position of a file object in bytes and returns the
 * resulting error code.
 */
int red_get_file_position(RED *red, uint16_t file_id, uint8_t *ret_error_code, uint64_t *ret_position);

/**
 * \ingroup BrickRED
 *
 * Returns various information about a file and the resulting error code.
 * 
 * The information is obtained via the
 * `stat() <http://pubs.opengroup.org/onlinepubs/9699919799/functions/stat.html>`__
 * function. If ``follow_symlink`` is *false* then the
 * `lstat() <http://pubs.opengroup.org/onlinepubs/9699919799/functions/stat.html>`__
 * function is used instead.
 */
int red_get_file_info(RED *red, uint16_t name_string_id, bool follow_symlink, uint8_t *ret_error_code, uint8_t *ret_type, uint16_t *ret_permissions, uint32_t *ret_user_id, uint32_t *ret_group_id, uint64_t *ret_length, uint64_t *ret_access_time, uint64_t *ret_modification_time, uint64_t *ret_status_change_time);

/**
 * \ingroup BrickRED
 *
 * Returns the target of a symlink and the resulting error code.
 * 
 * If ``canonicalize`` is *false* then the target of the symlink is resolved one
 * level via the
 * `readlink() <http://pubs.opengroup.org/onlinepubs/9699919799/functions/readlink.html>`__
 * function, otherwise it is fully resolved using the
 * `realpath() <http://pubs.opengroup.org/onlinepubs/9699919799/functions/realpath.html>`__
 * function.
 */
int red_get_symlink_target(RED *red, uint16_t name_string_id, bool canonicalize, uint8_t *ret_error_code, uint16_t *ret_target_string_id);

/**
 * \ingroup BrickRED
 *
 * Opens an existing directory and allocates a new directory object for it.
 * 
 * The reference count of the name string object is increased by one. When the
 * directory object is destroyed then the reference count of the name string
 * object is decreased by one. Also the name string object is locked and cannot be
 * modified while the directory object holds a reference to it.
 * 
 * Returns the object ID of the new directory object and the resulting error code.
 */
int red_open_directory(RED *red, uint16_t name_string_id, uint8_t *ret_error_code, uint16_t *ret_directory_id);

/**
 * \ingroup BrickRED
 *
 * Returns the name of a directory object and the resulting error code.
 */
int red_get_directory_name(RED *red, uint16_t directory_id, uint8_t *ret_error_code, uint16_t *ret_name_string_id);

/**
 * \ingroup BrickRED
 *
 * Returns the next entry in a directory object and the resulting error code.
 * If there is not next entry then error code ``API_E_NO_MORE_DATA`` is returned.
 * To rewind a directory object call {@link red_rewind_directory}.
 */
int red_get_next_directory_entry(RED *red, uint16_t directory_id, uint8_t *ret_error_code, uint16_t *ret_name_string_id, uint8_t *ret_type);

/**
 * \ingroup BrickRED
 *
 * Rewinds a directory object and returns the resulting error code.
 */
int red_rewind_directory(RED *red, uint16_t directory_id, uint8_t *ret_error_code);

/**
 * \ingroup BrickRED
 *
 * 
 */
int red_create_directory(RED *red, uint16_t name_string_id, bool recursive, uint16_t permissions, uint32_t user_id, uint32_t group_id, uint8_t *ret_error_code);

/**
 * \ingroup BrickRED
 *
 * 
 */
int red_spawn_process(RED *red, uint16_t command_string_id, uint16_t arguments_list_id, uint16_t environment_list_id, uint16_t working_directory_string_id, uint32_t user_id, uint32_t group_id, uint16_t stdin_file_id, uint16_t stdout_file_id, uint16_t stderr_file_id, uint8_t *ret_error_code, uint16_t *ret_process_id);

/**
 * \ingroup BrickRED
 *
 * 
 */
int red_kill_process(RED *red, uint16_t process_id, uint8_t signal, uint8_t *ret_error_code);

/**
 * \ingroup BrickRED
 *
 * 
 */
int red_get_process_command(RED *red, uint16_t process_id, uint8_t *ret_error_code, uint16_t *ret_command_string_id);

/**
 * \ingroup BrickRED
 *
 * 
 */
int red_get_process_arguments(RED *red, uint16_t process_id, uint8_t *ret_error_code, uint16_t *ret_arguments_list_id);

/**
 * \ingroup BrickRED
 *
 * 
 */
int red_get_process_environment(RED *red, uint16_t process_id, uint8_t *ret_error_code, uint16_t *ret_environment_list_id);

/**
 * \ingroup BrickRED
 *
 * 
 */
int red_get_process_working_directory(RED *red, uint16_t process_id, uint8_t *ret_error_code, uint16_t *ret_working_directory_string_id);

/**
 * \ingroup BrickRED
 *
 * 
 */
int red_get_process_user_id(RED *red, uint16_t process_id, uint8_t *ret_error_code, uint32_t *ret_user_id);

/**
 * \ingroup BrickRED
 *
 * 
 */
int red_get_process_group_id(RED *red, uint16_t process_id, uint8_t *ret_error_code, uint32_t *ret_group_id);

/**
 * \ingroup BrickRED
 *
 * 
 */
int red_get_process_stdin(RED *red, uint16_t process_id, uint8_t *ret_error_code, uint16_t *ret_stdin_file_id);

/**
 * \ingroup BrickRED
 *
 * 
 */
int red_get_process_stdout(RED *red, uint16_t process_id, uint8_t *ret_error_code, uint16_t *ret_stdout_file_id);

/**
 * \ingroup BrickRED
 *
 * 
 */
int red_get_process_stderr(RED *red, uint16_t process_id, uint8_t *ret_error_code, uint16_t *ret_stderr_file_id);

/**
 * \ingroup BrickRED
 *
 * 
 */
int red_get_process_state(RED *red, uint16_t process_id, uint8_t *ret_error_code, uint8_t *ret_state, uint8_t *ret_exit_code);

/**
 * \ingroup BrickRED
 *
 * 
 */
int red_define_program(RED *red, uint16_t identifier_string_id, uint8_t *ret_error_code, uint16_t *ret_program_id);

/**
 * \ingroup BrickRED
 *
 * 
 */
int red_undefine_program(RED *red, uint16_t program_id, uint8_t *ret_error_code);

/**
 * \ingroup BrickRED
 *
 * 
 */
int red_get_program_identifier(RED *red, uint16_t program_id, uint8_t *ret_error_code, uint16_t *ret_identifier_string_id);

/**
 * \ingroup BrickRED
 *
 * 
 */
int red_get_program_directory(RED *red, uint16_t program_id, uint8_t *ret_error_code, uint16_t *ret_directory_string_id);

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
