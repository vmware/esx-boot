/*******************************************************************************
 * Copyright (c) 2008-2011,2015 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * error.h -- Generic status values
 */

#ifndef ERROR_H_
#define ERROR_H_

/*
 * A mapping for these error must be defined by the firmware library.
 */

#define ERROR_TABLE \
D(ERR_SUCCESS, "Success") \
D(ERR_UNKNOWN, "Unknown") \
D(ERR_LOAD_ERROR, "Load error") \
D(ERR_INVALID_PARAMETER, "Invalid parameter") \
D(ERR_UNSUPPORTED, "Unsupported") \
D(ERR_BAD_BUFFER_SIZE, "Bad buffer size") \
D(ERR_BUFFER_TOO_SMALL, "Buffer too small") \
D(ERR_NOT_READY, "Not ready") \
D(ERR_DEVICE_ERROR, "Device error") \
D(ERR_WRITE_PROTECTED, "Write protected") \
D(ERR_OUT_OF_RESOURCES, "Out of resources") \
D(ERR_VOLUME_CORRUPTED, "Volume corrupted") \
D(ERR_VOLUME_FULL, "Volume full") \
D(ERR_NO_MEDIA, "No media") \
D(ERR_MEDIA_CHANGED, "Media changed") \
D(ERR_NOT_FOUND, "Not found") \
D(ERR_ACCESS_DENIED, "Access denied") \
D(ERR_NO_RESPONSE, "No response") \
D(ERR_NO_MAPPING, "No mapping") \
D(ERR_TIMEOUT, "Timeout") \
D(ERR_NOT_STARTED, "Not started") \
D(ERR_ALREADY_STARTED, "Already started") \
D(ERR_ABORTED, "Aborted") \
D(ERR_ICMP_ERROR, "ICMP error") \
D(ERR_TFTP_ERROR, "TFTP error") \
D(ERR_PROTOCOL_ERROR, "Protocol error") \
D(ERR_INCOMPATIBLE_VERSION, "Incompatible version") \
D(ERR_SECURITY_VIOLATION, "Security violation") \
D(ERR_CRC_ERROR, "CRC error") \
D(ERR_END_OF_MEDIA, "End of media") \
D(ERR_END_OF_FILE, "End of file") \
D(ERR_INVALID_LANGUAGE, "Invalid language") \
D(ERR_SYNTAX, "Syntax") \
D(ERR_INCONSISTENT_DATA, "Inconsistent data") \
D(ERR_UNEXPECTED_EOF, "Unexpected EOF") \
D(ERR_BAD_ARCH, "Bad arch") \
D(ERR_BAD_TYPE, "Bad type") \
D(ERR_BAD_HEADER, "Bad header") \
D(ERR_NOT_EXECUTABLE, "Not executable") \
D(ERR_INSECURE, "Secure boot failed") \

#define D(symbol, string) symbol,
enum {
   ERROR_TABLE
   ERROR_NUMBER
};
#undef D

#define WARNING(_status_)     ((_status_) | 0x80000000)
#define IS_WARNING(_status_)  ((_status_) & 0x80000000)

#endif /* ERROR_H_ */
