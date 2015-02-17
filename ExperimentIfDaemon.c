/*************************************************************************\
*                  Copyright (C) Michael Kerrisk, 2014.                   *
*                                                                         *
* This program is free software. You may use, modify, and redistribute it *
* under the terms of the GNU Affero General Public License as published   *
* by the Free Software Foundation, either version 3 or (at your option)   *
* any later version. This program is distributed without any warranty.    *
* See the file COPYING.agpl-v3 for details.                               *
\*************************************************************************/

/* Listing 63-5 */

/* epoll_input.c

   Example of the use of the Linux epoll API.

   Usage: epoll_input file...

   This program opens all of the files named in its command-line arguments
   and monitors the resulting file descriptors for input events.

   This program is Linux (2.6 and later) specific.
*/
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mount.h>
#include <ctype.h>
#include <fcntl.h>
#include <fcntl.h>
#include <termios.h>
#include <time.h>
#include <mqueue.h>
#include <signal.h>
#include <semaphore.h>
#include <syslog.h>
#include <mqueue.h>
#include "typedef.h"
#include "become_daemon.h"
#include "ExperimentIfDaemon.h"
#include "SerialPacket.h"
#include "SerialMsgUtils.h"

#include "tlpi_hdr.h"

#define MAX_BUF     1000        /* Maximum bytes fetched by a single read() */
#define MAX_EVENTS     5        /* Maximum number of events to be returned from
                                   a single epoll_wait() call */

#define DEBUG_LEVEL 30


/* enable log messages when entering and returning from EPOLL_WAIT() */
#define EPOLL_WAIT_RETURN_DISP  1
#define EVENT_LIST_LOG			1

/* Display Log Message when Messages receive from or to EXP1 */
#define Experiment1_TX_RECEIVE   	1
#define Experiment1_RX_RECEIVE   	1

#define DISPLAY_MQ_RECEIVE		1

/* Log the Error Message */
void
LogErrno(){

char UsrMsg[50];
char *ErrMsg;
ErrMsg=strerror(errno);
sprintf(UsrMsg, "ERRNO = %i , %s", errno, ErrMsg);
syslog(LOG_INFO, "%s", UsrMsg);

}


/* Clear Message queue by grabbing the oldest message first,
 * up to the number of times specified by NumberToClear */
int ClearMessageQueue(mqd_t mqd , int NumberToClear){

	struct mq_attr attr;
	/*ARM_char_t *ASCII_Buff = (ARM_char_t*) malloc(attr.mq_msgsize);*/
	/*uint32_t Length = malloc(attr.mq_msgsize);*/
	ARM_char_t ASCII_Buff[2*MAX_MSG_SIZE + MSG_HEADER_LENGTH];
	char UsrMsg[100];
	int32_t  numRead = 1, numCleared = 0;
	uint32_t prio;

	    /* Get message attributes for allocating size */
	    if(mq_getattr(mqd, &attr) == -1)
	    {
	    		syslog(LOG_INFO, "SerialDaemon Clear Message QUEUE:: Failed to get message attributes");
	    		LogErrno();

	      return -1;
	    }

	    /* Read Message as long as specified by user, which gives them
	     * the ability to clear as many messages as they want, bail out if we stop
	     * receiving bytes as well */
	    while(NumberToClear > 0 && numRead > 0){

	    	numRead = mq_receive(mqd, (char *)&ASCII_Buff, attr.mq_msgsize, &prio);
	    	mq_getattr(mqd, &attr);

	    	sprintf(UsrMsg, "Serial Daemon clear Message Queue: Number of messages = %u \n", attr.mq_curmsgs);
	    	syslog(LOG_INFO, "%s", UsrMsg);

	    	NumberToClear--;
	    	numCleared++;
	    }

	 return numCleared;
}



int
SerialConfigure(const char *SerialFd, uint16_t BaudRate){

	int32_t ttyFd, flags = 0;
	char UsrMsg[100];

	struct termios OrigTermios, ModifiedTermios;

	/* Open Options Described in: https://www.cmrr.umn.edu/~strupp/serial.html
		The O_NOCTTY flag tells UNIX that this program doesn't want to
	    be the "controlling terminal" for that port. If you don't specify this then any
	    input (such as keyboard abort signals and so forth) will affect your process. */
	 ttyFd= open(SerialFd, O_RDWR | O_NOCTTY | O_NDELAY );

	 if (ttyFd == -1)
	 {
	    syslog(LOG_INFO, "Failed to open Serial FD");
	    LogErrno();
	    return OPEN_FAIL;

	 }
	 else{
		 syslog(LOG_INFO, "Serial Interface Opened Sucessfully");
	 }

	    /* Acquire Current Serial Terminal settings so that we can go ahead and
	      change and later restore them */
	 if(tcgetattr(ttyFd, &OrigTermios)==-1)
	 {
		syslog(LOG_INFO, "tcgetattr failure");
	    LogErrno();
		return TCGETATTR_FAIL;
	 }

	 ModifiedTermios = OrigTermios;

	 //Prevent Line clear conversion to new line character
	 ModifiedTermios.c_iflag &= ~ICRNL;

	 /*Prevent Echoing of Input characters (Received characters
	 * retransmitted on TX line) */
	 ModifiedTermios.c_lflag &= ~ECHO;

	 /* Set Baud Rate */
	 if(cfsetospeed(&ModifiedTermios, BaudRate) == -1)
	 {
		syslog(LOG_INFO, "Failed to Set BaudRate");
	    LogErrno();
		return BAUDRATE_FAIL;
	 }
	 /*  Put in Cbreak mode where break signals lead to interrupts */
	 if(tcsetattr(ttyFd, TCSAFLUSH, &ModifiedTermios)==-1)
	 {
		syslog(LOG_INFO, "Failed to modify terminal settings");
	    LogErrno();
		return TCSETATTR_FAIL;
	 }

	 /* enable "I/O Possible" signalling and make I/O nonblocking for FD
	  * O_ASYNC flag causes signal to be routed to the owner process, set
	  * above *//* Log the Error Message */
	 flags = fcntl(ttyFd, F_GETFL );
	 if (fcntl(ttyFd, F_SETFL, flags |  O_NONBLOCK) == -1 )
	 {
		syslog(LOG_INFO, "ERROR: FCTNL mode");
		LogErrno();
		return FCNTL_MODE;
	 }
	 else
	 {
		sprintf(UsrMsg, "Serial Interface %s Sucessfully Configured", SerialFd);
		syslog(LOG_INFO, "%s", UsrMsg);
	 }

	return ttyFd;
}

int MsgQueueMount(){

	unsigned long mountflags = 0;
	/* Mount message on Filesystem queue per pp.1084 of TLPI
	 * check to make sure it doesn't exist first
	 *
	 * This equivalent to typing the following at the command
	 * line
	 *
	 * mkdir /dev/mqueue
	 * mount -t mqueue none /dev/mqueue*/

	if(mkdir("/dev/mqueue", DIR_PERMS) == -1)
	{
		syslog(LOG_INFO, "MsgQueueMount: ERROR at mkdir");
		LogErrno();
		/* return MKDIR_FAIL; */
	}

	if(mount("none", "/dev/mqueue", "mqueue", mountflags, NULL)==-1)
	{
		syslog(LOG_INFO, "MsgQueueMount: ERROR at mount");
		LogErrno();
		return MOUNT_FAIL;
	}


	return 1;
}



int MsgQueueRxInit(const char * MsgQueuePath){

    int flags, opt, mqFd;
    mode_t perms;
    mqd_t mqd;
    struct mq_attr *attrp;
    char *UsrMsg;

    attrp = NULL;
    flags = O_RDWR;

    flags |= O_CREAT;
    perms = (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);

    /* Parse Message Queue string, grab the last slash here  */
    mqd = mq_open(MsgQueuePath, flags, perms, attrp);

    if (mqd == (mqd_t) -1)
    {
        syslog(LOG_INFO, "Failed to Open Receive Message Queue");
    	sprintf(UsrMsg, "Message Queue Path is %s", MsgQueuePath);
    	syslog(LOG_INFO,  "%s", UsrMsg);
    	LogErrno();
    	return MQOPEN_FAIL;

    }

    return 1;

}

int EXPMsgTx(const char * MsgQueueDescriptor, int txFd){

	int32_t flags = 0, TotalTxBytes = 0;
	mqd_t mqd;
	uint32_t prio;
	struct mq_attr attr;
	ssize_t numRead;
	char UsrMsg[100];

	flags = O_RDWR | O_NONBLOCK;

	/* Open the receive side message queue */
	/*NOTE: In test mode, this is a loopback interface,
	 * with the TX queue, hence the macro*/
	mqd = mq_open(MsgQueueDescriptor, flags);

	if(mqd < (mqd_t) 0)
	{
		syslog(LOG_INFO, "ExperimentIfTx: Failed to open receive queue");
		LogErrno();
		return -1;
	}

	#if DEBUG_LEVEL > 15
		syslog(LOG_INFO, "ExperimentIfTx: Cleared the Open ");
	#endif

	/* Get message attributes for allocating size */
	if(mq_getattr(mqd, &attr) == -1)
	{
		syslog(LOG_INFO,"ExperimentIfTx: Failed to get message attributes");
		LogErrno();
		return -1;
	}

	#if DEBUG_LEVEL > 15
		syslog(LOG_INFO, "ExperimentIfTx: Cleared the Attributes ");
	#endif

	ARM_char_t *ASCII_Buff = (ARM_char_t*) malloc(attr.mq_msgsize);

	if ( ASCII_Buff == NULL){
		syslog(LOG_INFO, "ExperimentIfTx: Failed to allocate buffer ");
		return -1;
	}

	numRead = mq_receive(mqd, ASCII_Buff, attr.mq_msgsize, &prio);

	if(numRead == -1){

		syslog(LOG_INFO, "ExperimentIfTx: Failed to read messages from Rx Queue ");
		LogErrno();
		return -1;
	}

	#if DEBUG_LEVEL > 15
		syslog(LOG_INFO, "ExperimentIfTx: Cleared the mqreceive  ");
	#endif

	if(numRead > 0)
	{


	#if DISPLAY_MQ_RECEIVE == 1
			int32_t i = 0;
			sprintf(UsrMsg, "mq_receieve: numRead = %i ", numRead );
			syslog(LOG_INFO, "%s", UsrMsg);
			/* Print the raw hex values, for debugging */
				for(i=0; i<numRead; i++)
				{
					sprintf(UsrMsg, " %x ",ASCII_Buff[i]);
					syslog(LOG_INFO, "%s", UsrMsg);
				}

	#endif // DISPLAY_MQ_RECEIVE == 1
	}

	TotalTxBytes=write(txFd, ASCII_Buff, numRead);

	#if DEBUG_LEVEL > 20
		syslog(LOG_INFO, "ExperimentIfTx: Write Cleared");
	#endif

	if(TotalTxBytes < 0)
	{
		sprintf(UsrMsg, "Error Transmitting Message from %s", MsgQueueDescriptor);
		syslog(LOG_INFO, "%s", UsrMsg);
		LogErrno();
		return -1;
	}

	/* Close message queue before returning */
	if(mq_close(mqd) < (int32_t)0){
			syslog(LOG_INFO, "ExperimentIfTx: Close Failed");
			LogErrno();
	}

	#if DEBUG_LEVEL > 20
		syslog(LOG_INFO, "ExperimentIfTx: Cleared the mq_close");
	#endif

	return numRead;

}


/* This function receives serial data from the Experiment, and places
 * it an outgoing Message Queue */
int
EXPMsgRx( int ttyFdEXPn, char * RxMsgQueue ){

	int TotalRxBytes = 0, AccumulatedRxByteCount = 0 , SndMsgRtn = 0;
	int flags = 0;
	int ClearReturn =0;
	mqd_t mqd;
	mode_t perms;
	struct mq_attr *attrp;

	char RxBuffer[MAX_RX_BUFF_SIZE];
	char ReadBuff[MAX_READ_BYTES];
	char * CurrentArrayPosition;
	struct timeval CurrentTime;
	Boolean done = 0;
	char *CurrentTimeString;
	char UsrMsg[100];


	SerialPacket * CurrentSerialPacket=malloc(sizeof(SerialPacket));

	done=0;
	TotalRxBytes=0;
	AccumulatedRxByteCount = 0;



	memset(ReadBuff, 0, sizeof(ReadBuff));
	memset(RxBuffer, 0, sizeof(RxBuffer));

     /* Read buffered Serial data using the file descriptor until we
       don't receive anymore (signaled by done flag) */
	while ( !done)  {
		TotalRxBytes=read(ttyFdEXPn, ReadBuff, MAX_READ_BYTES);

		/*Terminate Loop if we see 0 bytes returned, or an ERROR */
		if(TotalRxBytes <= 0)
		{
			done=1;
			continue;
		}
		else
		{

			//Copy bytes received this loop iteration into the RxBuff
			CurrentArrayPosition=&RxBuffer[AccumulatedRxByteCount];
			strncpy(CurrentArrayPosition, ReadBuff, TotalRxBytes);
			memset(ReadBuff, 0, sizeof(ReadBuff));

			#if DEBUG_LEVEL > 10
				sprintf(UsrMsg,"RxBuffer %s ",RxBuffer);
				syslog(LOG_INFO, "%s", UsrMsg);
			#endif

			AccumulatedRxByteCount+=TotalRxBytes;


		}
	}

	/* Outside the while loop. Process our received data */

	/* Get Current System Time, copy it to our serial packet header */
		if (gettimeofday(&CurrentTime, NULL) == -1 )
		{
				syslog(LOG_INFO, "Couldn't get current time ");
				memset(&CurrentTime, 0x0, sizeof(CurrentTime));
		}
		else
		{
				CurrentTimeString = ctime(&CurrentTime.tv_sec);
				strcpy(CurrentSerialPacket->TimeReceived,CurrentTimeString);
		}

	#if DEBUG_LEVEL > 15
		syslog(LOG_INFO, "New Complete Message Received");
	#endif

		/* Blast this message out on the MSG QUEUE */
		/* Open for Write, Create if not open, Open non-blocking-rcv and send will
		 * fail unless they can complete immediately. */
		flags = O_RDWR | O_NONBLOCK | O_CREAT;

		/* Set file permissions everyone can read and write */
		perms = (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);

		/* Must be null, otherwise it chokes */
		attrp=NULL;


		mqd=mq_open( RxMsgQueue , flags, perms, attrp);

		/* check for fail condition, if we failed asssume we need to open a
		 * message queue for the first time, then go ahead and do so
		 * the function below creates a message queue if not already open*/
		if(mqd == (mqd_t) -1)
		{
			syslog(LOG_INFO, "ERROR : EXPMsgRx: Message Queue Open Fail");
			LogErrno();
			return -1;

		}

		/* Send the Message  */
		#if DEBUG_LEVEL > 150
		/* Print the raw hex values, for debugging */
			syslog(LOG_INFO, "RxBuffer ");
				int i=0;
				for(i=0; i<AccumulatedRxByteCount; i++)
				{
					sprintf(UsrMsg[i], "%x",RxBuffer[i]);
				}

				syslog(LOG_INFO, "%s", UsrMsg);

				struct mq_attr attr;
				mq_getattr(mqd, &attr);

				sprintf(UsrMsg, "SerialRx MqAttr = %u ", attr.mq_msgsize);
				syslog(LOG_INFO, "%s", UsrMsg);
		#endif

		SndMsgRtn = mq_send(mqd, RxBuffer, (size_t)(AccumulatedRxByteCount), 0);
		if(SndMsgRtn < 0)
		{

			/* EAGAIN occurs when we have a full message queue,
			 * we need to clear space to send new messages */
			if (errno==EAGAIN){
				/* This function will clear the message queue one message
				 * at a time, oldest message first, freing up space for our
				 * newest message */

				ClearReturn = ClearMessageQueue(mqd , 1);


					sprintf(UsrMsg,"SerialRx:EAGAIN encountered, cleared %i Messages ", ClearReturn);
					syslog(LOG_INFO, "%s", UsrMsg);

				/* Reattampt our send */

				SndMsgRtn = mq_send(mqd, RxBuffer, (size_t)(AccumulatedRxByteCount), 0);


				/* If it fails this time, simply return */
				if(SndMsgRtn < 0)
				{
					#if DEBUG_LEVEL > 5
					syslog(LOG_INFO, "SerialDaemonRx: Msg RX Fails after attempting to clear queue");
					LogErrno();
					errMsg("SerialDaemonRx: Msg RX Fails, after attempting to clear queue");
					#endif
					return -1;
				}
			}
			else
			{
				#if DEBUG_LEVEL > 5
					syslog(LOG_INFO, "SerialDaemonRx: Msg RX Fails");
					errMsg("SerialDaemonRx: Msg Send Fails");
				#endif
			return -1;
			}


		memset(RxBuffer, 0 ,MAX_RX_BUFF_SIZE);
		free(CurrentSerialPacket);


		if(mq_close(mqd) < (int)0)
		{
			syslog(LOG_INFO, "SerialDaemonRx: Close Failed");
			LogErrno();
			errMsg("SerialDaemonRx: Close Failed");
		}

		/* If we haven't bailed out anywhere else up here, go ahead and send a positive
		 * int as a return, meaning success */
			return 1;
}
}


int
main()
{
    int32_t epfd, ready, fd, s, j, numOpenFds;
    int32_t ttyFdEXP1 = 0, ttyFdEXP2 = 0, ttyFdEXP3 = 0,  ttyFdEXP4 = 0;
    int32_t MqFd1 = 0, MqFd2 = 0, MqFd3 = 0,  MqFd4 = 0;
    int32_t	returnVal=0;
    struct epoll_event ev;
    struct epoll_event evlist[MAX_EVENTS];
    char UsrMsg[300];

	#ifndef FOREGROUND_RUN
		openlog(EXP_DAEMON_LOG_NAME, LOG_CONS | LOG_NDELAY | LOG_PERROR | LOG_PID, LOG_USER );
		syslog(LOG_INFO, "Starting Daemon");

		/* Start the process as a daemon,
		 * (forks and creates a child without controlling terminal, parent exits */
		if(becomeDaemon(BD_NO_CHDIR | BD_NO_CLOSE_FILES ) < 0 )
		{
			syslog(LOG_INFO, "Failed to Become Daemon");
			closelog();
			exit(EXIT_FAILURE);
		}

		openlog(EXP_DAEMON_LOG_NAME, LOG_CONS | LOG_NDELAY | LOG_PERROR | LOG_PID, LOG_USER );
		syslog(LOG_INFO, "Executing as Daemon");
	#else
		openlog(EXP_DAEMON_LOG_NAME, LOG_CONS | LOG_NDELAY | LOG_PERROR | LOG_PID, LOG_USER );
		syslog(LOG_INFO, "Executing Daemon in Foreground");
	#endif


    epfd = epoll_create(FILE_DESCRIPTOR_CNT);
    if (epfd == -1)
    {
    	syslog(LOG_INFO, "ERROR: EPOLL_CREATE failed" );
    	LogErrno();
    	closelog();
        errExit("epoll_create");
    }

    /* Mount our Message Queue in the file system for the first time  */
    returnVal = MsgQueueMount();

    if(returnVal < 0)
    {
     	syslog(LOG_INFO, "ERROR: MsgQueueMount" );
     	closelog();
    }

    /* Create Message Queues */
    MsgQueueRxInit(MQTXEXP1);
    /* MsgQueueRxInit(MQRTEXP2); */
    /* MsgQueueRxInit(MQRTEXP3); */
    /* MsgQueueRxInit(MQRtEXP4); */

    /* Open the file descriptor version of the message queue */
    MqFd1=open(PATHMQTXEXP1 ,O_RDONLY);

    if(MqFd1== -1)
    {
    	syslog(LOG_INFO, "ERROR: Experiment 1 Outgoing Message Queue Open (File Descriptor)" );
    	LogErrno();
    }

    /* MqFd2=open(PATHMQTXEXP2 ,O_RDONLY);

    if(MqFd2== -1)
    {
    	syslog(LOG_INFO, "ERROR: Experiment 2 Outgoing Message Queue Open (File Descriptor)" );
    	LogErrno();
    }

    MqFd3=open(PATHMQTXEXP3 ,O_RDONLY);

    if(MqFd3== -1)
    {
    	syslog(LOG_INFO, "ERROR: Experiment 3 Outgoing Message Queue Open (File Descriptor)" );
    	LogErrno();
    }

    MqFd4=open(PATHMQTXEXP4 ,O_RDONLY);

    if(MqFd4== -1)
    {
    	syslog(LOG_INFO, "ERROR: Experiment 4 Outgoing Message Queue Open (File Descriptor)" );
    	LogErrno();
    }


     Open each file Message Queue and Serial Driver, and add it to the "interest
       list" for the epoll instance */






    ev.events = EPOLLIN;            /* Only interested in input events */
    ev.data.fd = MqFd1;

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, MqFd1, &ev) == -1)
    {
        syslog(LOG_INFO, "ERROR:Message Queue 1 File DescriEXPMsgRx( ttyFdEXP1, MQRXEXP1 );ptor EPOLL ADD Failed");
        LogErrno();
    }

    /* Open the Serial Drivers   */
    ttyFdEXP1=SerialConfigure(TTYEXP1, TTYUSB0BAUDRATE);

    if(ttyFdEXP1== -1)
    {
    	syslog(LOG_INFO, "ERROR: Experiment 1 Serial File Port Open Failed" );
    }

    ev.events = EPOLLIN;            /* Only interested in input events */
    ev.data.fd = ttyFdEXP1;

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, ttyFdEXP1, &ev) == -1)
    {
        syslog(LOG_INFO, "ERROR: Experiment 1 File Descriptor Epoll add failed");
        LogErrno();
    }

     /*
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, MqFd2, &ev) == -1)
    {
        syslog(LOG_INFO, "ERROR: Message Queue 2 File Descriptor EPOLL ADD Failed");
        LogErrno();
    }

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, MqFd3, &ev) == -1)
    {
        syslog(LOG_INFO, "ERROR:Message Queue 3 File Descriptor EPOLL ADD Failed");
        LogErrno();
    }

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, MqFd4, &ev) == -1)
    {
        syslog(LOG_INFO, "ERROR:Message Queue 4 File Descriptor EPOLL ADD Failed");
        LogErrno();
    }

    */

    while (1) {

        /* Fetch up to MAX_EVENTS items from the ready list of the
           epoll instance */

		#if EPOLL_WAIT_RETURN_DISP == 1
    	    	syslog(LOG_INFO, "About to epoll_wait()");
		#endif

        ready = epoll_wait(epfd, evlist, MAX_EVENTS, -1);

		#if EPOLL_WAIT_RETURN_DISP == 1
        	syslog(LOG_INFO, "Returned from epoll_wait()");
		#endif

        if (ready == -1)
        {
            if (errno == EINTR)
      	            	continue;               /* Restart if interrupted by signal */
            else
                errExit("epoll_wait");
        }else{
       /* 	EXPMsgTx(MQTXEXP1, TTYEXP1); */

		#if EPOLL_WAIT_RETURN_DISP == 1
        	sprintf(UsrMsg,"epoll_wait() Ready: %d", ready  );
        	syslog(LOG_INFO, "%s", UsrMsg );
		#endif

        /* Deal with returned list of events */
        for (j = 0; j < ready; j++) {

			#if EVENT_LIST_LOG == 1
        	    sprintf(UsrMsg, "  fd=%d; events: %s%s%s\n", evlist[j].data.fd,
                    (evlist[j].events & EPOLLIN)  ? "EPOLLIN "  : "",
                    (evlist[j].events & EPOLLHUP) ? "EPOLLHUP " : "",
                    (evlist[j].events & EPOLLERR) ? "EPOLLERR " : "");
            	syslog(LOG_INFO, "%s", UsrMsg);
			#endif

            if (evlist[j].events & EPOLLIN) {
            	/* If a Tx Message to the Experiment (via Message Queue on File System)
            	 * transmit this message to the serial driver. */
            	if(evlist[j].data.fd == MqFd1 )
            	{
					#if Experiment1_TX_RECEIVE == 1
            			syslog(LOG_INFO, "Message Received for EXP1");
					#endif

            		returnVal=EXPMsgTx(MQTXEXP1, ttyFdEXP1 );

            		if(returnVal < 0)
            		{
            			syslog(LOG_INFO, "ERROR: Failed to transmit message to Experiment 1");
            			continue;
            		}
            	}

            	/* If a Rx Message from the Experiment (via ttyUSBn)
            	 * place this message in queue. */
            	if(evlist[j].data.fd == ttyFdEXP1 )
            	{
					#if Experiment1_RX_RECEIVE == 1
            			syslog(LOG_INFO, "Message Received from EXP1");
					#endif

            		returnVal=EXPMsgRx( ttyFdEXP1, MQRXEXP1 );

            		if(returnVal < 0)
            		{
            			syslog(LOG_INFO, "ERROR: Failed to receive message from Experiment 1");
            			continue;
            		}
            	}

            } else if (evlist[j].events & (EPOLLHUP | EPOLLERR)) {

                /* After the epoll_wait(), EPOLLIN and EPOLLHUP may both have
                   been set. But we'll only get here, and thus close the file
                   descriptor, if EPOLLIN was not set. This ensures that all
                   outstanding input (possibly more than MAX_BUF bytes) is
                   consumed (by further loop iterations) before the file
                   descriptor is closed. */

                sprintf(UsrMsg, "    closing fd %d\n", evlist[j].data.fd);
                syslog(LOG_INFO, "%s", UsrMsg);
                if (close(evlist[j].data.fd) == -1)
                {
                	syslog(LOG_INFO, "ERROR: Closing File Descriptor");
                	LogErrno();
                }
                numOpenFds--;
            }
        }
        } /* added */
    }

    syslog(LOG_INFO, "All file descriptors closed; bye");
    exit(EXIT_SUCCESS);

}
