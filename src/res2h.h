#pragma once

// These numbers and strings need to be manually updated for a new version.
// Do this version number update as the very last commit for the new release version.
#define RES2H_VERSION_MAJOR       0
#define RES2H_VERSION_MINOR        4
#define RES2H_VERSION_MAINTENANCE  0
#define RES2H_VERSION_REVISION     0
#define RES2H_VERSION_STRING "0.4.0.0 - built " __DATE__ " - " __TIME__
#define RES2H_RESOURCE_VERSION_STRING "0,4,0,0\0"

#define RES2H_RESOURCE_VERSION RES2H_VERSION_MAJOR,RES2H_VERSION_MINOR,RES2H_VERSION_MAINTENANCE,RES2H_VERSION_REVISION

#define RES2H_ARCHIVE_VERSION 1
#define RES2H_MAGIC_BYTES "res2hbin"

#define RES2H_OFFSET_MAGIC_BYTES  0
#define RES2H_OFFSET_FILE_VERSION (sizeof(RES2H_MAGIC_BYTES)-1)
#define RES2H_OFFSET_FILE_FLAGS   (sizeof(RES2H_MAGIC_BYTES)-1 + 4)
#define RES2H_OFFSET_ARCHIVE_SIZE (sizeof(RES2H_MAGIC_BYTES)-1 + 8)
#define RES2H_OFFSET_NO_OF_FILES  (sizeof(RES2H_MAGIC_BYTES)-1 + 12)
#define RES2H_OFFSET_DIR_START    (sizeof(RES2H_MAGIC_BYTES)-1 + 16)
