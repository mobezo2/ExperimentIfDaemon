/*
 * PayloadIfDaemon.h
 *
 *  Created on: Dec 1, 2014
 *      Author: mbezold
 */

#ifndef PAYLOADIFDAEMON_H_
#define PAYLOADIFDAEMON_H_


#define PYLD_DAEMON_LOG_NAME  "PyldIfDaemon"

/* Error Return Codes for SerialConfigure() */
#define 		OPEN_FAIL			-1
#define 		TCGETATTR_FAIL		-2
#define			TCSETATTR_FAIL		-3
#define			FCNTL_MODE			-4
#define     	BAUDRATE_FAIL   	-5
#define 		MQOPEN_FAIL			-6
#define 		MKDIR_FAIL			-7
#define 		MOUNT_FAIL			-8

/* Used to create and open directory for mounting
 * Message Queues */

#define FILE_PERMS (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)
#define DIR_PERMS  (S_IRWXU | S_IXGRP | S_IWOTH | S_IXOTH)

/* Baud Rates of Serial Ports
 * Valid Baud Rates = B300, B2400, B9600, B38400
 * Don't forget the B in front!!! */
#define 	TTYUSB0BAUDRATE		B9600
#define 	TTYUSB1BAUDRATE		B9600
#define 	TTYUSB2BAUDRATE		B9600
#define 	TTYUSB3BAUDRATE		B9600


/* Path to Payload file descriptors */
#define TTYPYLD1			"/dev/ttyUSB0"
#define TTYPYLD2			"/dev/ttyUSB1"
#define TTYPYLD3			"/dev/ttyUSB2"
#define TTYPYLD4			"/dev/ttyUSB3"

/* Path to Payload TX Message Queues */
#define PATHMQTXPYLD1			"/dev/mqueue/MqTxPyld1"
#define PATHMQTXPYLD2			"/dev/mqueue/MqTxPyld2"
#define PATHMQTXPYLD3			"/dev/mqueue/MqTxPyld3"
#define PATHMQTXPYLD4			"/dev/mqueue/MqTxPyld4"

#define MQTXPYLD1	"/MqTxPyld1"
#define MQTXPYLD2	"/MqTxPyld2"
#define MQTXPYLD3	"/MqTxPyld3"
#define MQTXPYLD4	"/MqTxPyld4"

/* Message Queueus for Messages received from
 * payloads via ttyUSBn */
#define MQRXPYLD1	"/MqRxPyld1"
#define MQRXPYLD2	"/MqRxPyld2"
#define MQRXPYLD3	"/MqRxPyld3"
#define MQRXPYLD4	"/MqRxPyld4"


/* File descriptors for Epoll to Monitor
 * total count of ttyUSBn and /dev/mqueue/MqTxPyldn */
#define FILE_DESCRIPTOR_CNT 	8


/* Message queue receive definitions */
#define MAX_READ_BYTES 		255
#define MAX_RX_BUFF_SIZE 	1020


#endif /* PAYLOADIFDAEMON_H_ */
