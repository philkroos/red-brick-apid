/*
 * redapid
 * Copyright (C) 2015 Matthias Bolte <matthias@tinkerforge.com>
 *
 * acl.h: Access Control Lists helper functions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <pwd.h>
#include <stdbool.h>
#include <string.h>
#include <sys/acl.h>

// sets errno on error
static int acl_remove_user_entry(acl_t acl, uid_t uid) {
	int rc;
	acl_entry_t entry;
	acl_tag_t tag;
	void *qualifier;

	rc = acl_get_entry(acl, ACL_FIRST_ENTRY, &entry);

	if (rc < 0) {
		return -1;
	}

	while (rc > 0) {
		if (acl_get_tag_type(entry, &tag) < 0) {
			return -1;
		}

		if (tag == ACL_USER) {
			qualifier = acl_get_qualifier(entry);

			if (qualifier != NULL && *(uid_t *)qualifier == uid) {
				if (acl_delete_entry(acl, entry) < 0) {
					return -1;
				}

				break;
			}
		}

		rc = acl_get_entry(acl, ACL_NEXT_ENTRY, &entry);

		if (rc < 0) {
			return -1;
		}
	}

	return 0;
}

// sets errno on error
static int acl_entry_set_permissions(acl_entry_t entry, const char *permissions) {
	acl_permset_t permset;

	if (acl_get_permset(entry, &permset) < 0) {
		return -1;
	}

	if (acl_clear_perms(permset) < 0) {
		return -1;
	}

	if (strchr(permissions, 'r') != NULL && acl_add_perm(permset, ACL_READ) < 0) {
		return -1;
	}

	if (strchr(permissions, 'w') != NULL && acl_add_perm(permset, ACL_WRITE) < 0) {
		return -1;
	}

	if (strchr(permissions, 'x') != NULL && acl_add_perm(permset, ACL_EXECUTE) < 0) {
		return -1;
	}

	if (acl_set_permset(entry, permset) < 0)  {
		return -1;
	}

	return 0;
}

// sets errno on error
int acl_add_user(const char *directory, const char *user, const char *permissions) {
	bool success = false;
	struct passwd *pw;
	acl_t acl = NULL;
	acl_entry_t entry;

	pw = getpwnam(user);

	if (pw == NULL) {
		goto cleanup;
	}

	// get access ACL
	acl = acl_get_file(directory, ACL_TYPE_ACCESS);

	if (acl == NULL) {
		goto cleanup;
	}

	// remove USER entry, if existing
	if (acl_remove_user_entry(acl, pw->pw_uid) < 0) {
		goto cleanup;
	}

	// add USER entry
	if (acl_create_entry(&acl, &entry) < 0) {
		goto cleanup;
	}

	if (acl_entry_set_permissions(entry, permissions) < 0) {
		goto cleanup;
	}

	if (acl_set_tag_type(entry, ACL_USER) < 0) {
		goto cleanup;
	}

	if (acl_set_qualifier(entry, &pw->pw_uid) < 0) {
		goto cleanup;
	}

	if (acl_calc_mask(&acl) < 0) {
		goto cleanup;
	}

	// set access ACL
	if (acl_set_file(directory, ACL_TYPE_ACCESS, acl) < 0) {
		goto cleanup;
	}

	// set default ACL from access ACL
	if (acl_set_file(directory, ACL_TYPE_DEFAULT, acl) < 0) {
		goto cleanup;
	}

	success = true;

cleanup:
	if (acl != NULL) {
		acl_free(acl);
	}

	return success ? 0 : -1;
}
