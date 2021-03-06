#pragma once

#include <stdint.h>

#define FOS_MAILBOX_STATUS_OK (0)
#define FOS_MAILBOX_STATUS_ALLOCATION_ERROR (1)
#define FOS_MAILBOX_STATUS_KERNEL_ERROR (2)
#define FOS_MAILBOX_STATUS_PERMISSIONS_ERROR (3)
#define FOS_MAILBOX_STATUS_GENERAL_ERROR (4)
#define FOS_MAILBOX_STATUS_CAPABILITY_NOT_FOUND_ERROR (5)
#define FOS_MAILBOX_STATUS_NAME_CLASH_ERROR (6)
#define FOS_MAILBOX_STATUS_UNINITIALIZED_ERROR (7)
#define FOS_MAILBOX_STATUS_NO_SPACE_ERROR (8)
#define FOS_MAILBOX_STATUS_INVALID_ALIAS_ERROR (9)
#define FOS_MAILBOX_STATUS_EMPTY_ERROR (10)
#define FOS_MAILBOX_STATUS_INVALID_MAILBOX_ERROR (11)
#define FOS_MAILBOX_STATUS_INVALID_NAMESPACE_ERROR (12)
#define FOS_MAILBOX_STATUS_BAD_FREE_ERROR (13)
#define FOS_MAILBOX_STATUS_BAD_PARAMETER_ERROR (14)
#define FOS_MAILBOX_STATUS_RESOURCE_UNAVAILABLE (15)
#define FOS_MAILBOX_STATUS_ALIAS_DISABLED_ERROR (16)
#define FOS_MAILBOX_STATUS_UNIMPLEMENTED_ERROR (17)
#define FOS_MAILBOX_STATUS_INCONSISTENCY_ERROR (18)
#define FOS_MAILBOX_STATUS_RPC_ERROR (19)
#define FOS_MAILBOX_STATUS_UNEXPECTED_MESSAGE_ERROR (20)
#define FOS_MAILBOX_STATUS_CHANNEL_KILLED (21)
#define FOS_MAILBOX_STATUS_BAD_OPERATION_ERROR (22)
#define FOS_MAILBOX_STATUS_REMOTE_EXISTS_ERROR (23)
#define FOS_MAILBOX_STATUS_CONNECTION_LOST_ERROR (25)
#define FOS_MAILBOX_STATUS_NOT_FOUND_ERROR (26)
#define FOS_MAILBOX_STATUS_NOT_DIR_ERROR (27)
#define FOS_MAILBOX_STATUS_NOT_EMPTY_ERROR (28)
#define FOS_MAILBOX_STATUS_INVALID_ERROR (29)
#define FOS_MAILBOX_STATUS_NOENT_ERROR (30)
#define FOS_MAILBOX_STATUS_EXIST_ERROR (31)
#define FOS_MAILBOX_STATUS_IS_DIR_ERROR (32)

typedef uint32_t FosStatus;

int statusToErrno(FosStatus status);
int _statusToErrno(FosStatus status);
