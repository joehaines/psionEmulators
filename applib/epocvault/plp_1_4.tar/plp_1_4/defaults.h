#ifndef _defaults_h
#define _defaults_h

/////////////////////////////////////////////////////
// The following are compiled in default values
// NB. most of these can be changed at run time by command line options
/////////////////////////////////////////////////////

// This is the default tcp/ip socket
#define DEFAULT_SOCKET 7501

// This is the default baud rate for the psion serial link
#define DEFAULT_BAUD_RATE 115200

// This is the default device driver for the serial link
#define DEFAULT_SERIAL_DEVICE "/dev/cua1"

// This is the default drive to use on the psion
#define DEFAULT_DRIVE "C:\\"

// This is the default base directory to use on the psion
// - if no leading "/" is used, then this directory is taken as
//   the "home directory"
#define DEFAULT_BASE_DIRECTORY "Documents\\"

// Default unix starting directory for ftp - with trailing separator
#define DEFAULT_FTP_DIR "./"

//////////////////////////////////////////////////////
// Debugging
//////////////////////////////////////////////////////

#define PACKET_LAYER_DIAGNOSTICS false
#define LINK_LAYER_DIAGNOSTICS false
// #define SOCKET_DIAGNOSTICS

//////////////////////////////////////////////////////
// Do not change the following lines
//////////////////////////////////////////////////////

#define VERSION "1.4"
#define DIR_COMMAND "dir"
#define READ_COMMAND "read"
#define WRITE_COMMAND "write"
#define MKDIR_COMMAND "mkdir"
#define DEL_COMMAND "del"
#define FTP_COMMAND "ftp"

#define NCP_SENDLEN 250
#define RFSV_SENDLEN 230

#endif
