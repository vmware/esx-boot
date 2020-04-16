/*******************************************************************************
 * Copyright (c) 2008-2011,2015,2019 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * error.h -- Generic status values and their EFI equivalents.
 */

#ifndef ERROR_H_
#define ERROR_H_

#define ERROR_TABLE \
D(ERR_SUCCESS,              EFI_SUCCESS,              "Success")              \
D(ERR_UNKNOWN,              EFI_UNDEFINED_ERROR,      "Unknown")              \
D(ERR_LOAD_ERROR,           EFI_LOAD_ERROR,           "Load error")           \
D(ERR_INVALID_PARAMETER,    EFI_INVALID_PARAMETER,    "Invalid parameter")    \
D(ERR_UNSUPPORTED,          EFI_UNSUPPORTED,          "Unsupported")          \
D(ERR_BAD_BUFFER_SIZE,      EFI_BAD_BUFFER_SIZE,      "Bad buffer size")      \
D(ERR_BUFFER_TOO_SMALL,     EFI_BUFFER_TOO_SMALL,     "Buffer too small")     \
D(ERR_NOT_READY,            EFI_NOT_READY,            "Not ready")            \
D(ERR_DEVICE_ERROR,         EFI_DEVICE_ERROR,         "Device error")         \
D(ERR_WRITE_PROTECTED,      EFI_WRITE_PROTECTED,      "Write protected")      \
D(ERR_OUT_OF_RESOURCES,     EFI_OUT_OF_RESOURCES,     "Out of resources")     \
D(ERR_VOLUME_CORRUPTED,     EFI_VOLUME_CORRUPTED,     "Volume corrupted")     \
D(ERR_VOLUME_FULL,          EFI_VOLUME_FULL,          "Volume full")          \
D(ERR_NO_MEDIA,             EFI_NO_MEDIA,             "No media")             \
D(ERR_MEDIA_CHANGED,        EFI_MEDIA_CHANGED,        "Media changed")        \
D(ERR_NOT_FOUND,            EFI_NOT_FOUND,            "Not found")            \
D(ERR_ACCESS_DENIED,        EFI_ACCESS_DENIED,        "Access denied")        \
D(ERR_NO_RESPONSE,          EFI_NO_RESPONSE,          "No response")          \
D(ERR_NO_MAPPING,           EFI_NO_MAPPING,           "No mapping")           \
D(ERR_TIMEOUT,              EFI_TIMEOUT,              "Timeout")              \
D(ERR_NOT_STARTED,          EFI_NOT_STARTED,          "Not started")          \
D(ERR_ALREADY_STARTED,      EFI_ALREADY_STARTED,      "Already started")      \
D(ERR_ABORTED,              EFI_ABORTED,              "Aborted")              \
D(ERR_ICMP_ERROR,           EFI_ICMP_ERROR,           "ICMP error")           \
D(ERR_TFTP_ERROR,           EFI_TFTP_ERROR,           "TFTP error")           \
D(ERR_PROTOCOL_ERROR,       EFI_PROTOCOL_ERROR,       "Protocol error")       \
D(ERR_INCOMPATIBLE_VERSION, EFI_INCOMPATIBLE_VERSION, "Incompatible version") \
D(ERR_SECURITY_VIOLATION,   EFI_SECURITY_VIOLATION,   "Security violation")   \
D(ERR_CRC_ERROR,            EFI_CRC_ERROR,            "CRC error")            \
D(ERR_END_OF_MEDIA,         EFI_END_OF_MEDIA,         "End of media")         \
D(ERR_END_OF_FILE,          EFI_END_OF_FILE,          "End of file")          \
D(ERR_INVALID_LANGUAGE,     EFI_INVALID_LANGUAGE,     "Invalid language")     \
D(ERR_SYNTAX,               EFI_UNDEFINED_ERROR,      "Syntax")               \
D(ERR_INCONSISTENT_DATA,    EFI_UNDEFINED_ERROR,      "Inconsistent data")    \
D(ERR_UNEXPECTED_EOF,       EFI_UNDEFINED_ERROR,      "Unexpected EOF")       \
D(ERR_BAD_ARCH,             EFI_UNDEFINED_ERROR,      "Bad arch")             \
D(ERR_BAD_TYPE,             EFI_UNDEFINED_ERROR,      "Bad type")             \
D(ERR_BAD_HEADER,           EFI_UNDEFINED_ERROR,      "Bad header")           \
D(ERR_NOT_EXECUTABLE,       EFI_UNDEFINED_ERROR,      "Not executable")       \
D(ERR_INSECURE,             EFI_UNDEFINED_ERROR,      "Secure boot failed")   \
D(ERR_COMPROMISED_DATA,     EFI_COMPROMISED_DATA,     "Compromised data")     \
D(ERR_HTTP_ERROR,           EFI_HTTP_ERROR,           "HTTP Error")           \
D(ERR_NETWORK_UNREACHABLE,  EFI_NETWORK_UNREACHABLE,  "Network unreachable")  \
D(ERR_HOST_UNREACHABLE,     EFI_HOST_UNREACHABLE,     "Host unreachable")     \
D(ERR_PROTOCOL_UNREACHABLE, EFI_PROTOCOL_UNREACHABLE, "Protocol unreachable") \
D(ERR_PORT_UNREACHABLE,     EFI_PORT_UNREACHABLE,     "Port unreachable")     \
D(ERR_CONNECTION_FIN,       EFI_CONNECTION_FIN,       "Connection closed")    \
D(ERR_CONNECTION_RESET,     EFI_CONNECTION_RESET,     "Connection reset")     \
D(ERR_CONNECTION_REFUSED,   EFI_CONNECTION_REFUSED,   "Connection refused")   \

#define D(symbol, efi_symbol, string) symbol,
enum {
   ERROR_TABLE
   ERROR_NUMBER
};
#undef D

#define WARNING(_status_)     ((_status_) | 0x80000000)
#define IS_WARNING(_status_)  ((_status_) & 0x80000000)

#endif /* ERROR_H_ */
