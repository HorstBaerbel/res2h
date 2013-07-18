#pragma once

// These numbers and strings need to be manually updated for a new version.
// Do this version number update as the very last commit for the new release version.
#define PROGRAM_VERSION_MAJOR       0
#define PROGRAM_VERSION_MINOR        1
#define PROGRAM_VERSION_MAINTENANCE  0
#define PROGRAM_VERSION_REVISION     0
#define PROGRAM_VERSION_STRING "0.1.0.0 - built " __DATE__ " - " __TIME__
#define RESOURCE_VERSION_STRING "0,1,0,0\0"

#define RESOURCE_VERSION PROGRAM_VERSION_MAJOR,PROGRAM_VERSION_MINOR,PROGRAM_VERSION_MAINTENANCE,PROGRAM_VERSION_REVISION
