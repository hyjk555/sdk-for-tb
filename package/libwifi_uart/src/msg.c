
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <syslog.h>
#include <stdarg.h>
#include <netdb.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <time.h>

#include "msg.h"
#include "wifi_uart.h"

/******************************************************************************
 *                               FUNCTIONS                                    *
 ******************************************************************************/

/*******************************************************************************
 * Function:
 *    static MsgRet DM_WaitDataAvailable(SINT32 iFd, UINT32 nTimeOut)
 * Description:
 *    Wait for available data until timeout
 * Parameters:
 *    iFd      (IN) socket file descriptor 
 *    nTimeOut (IN) Time for timeout
 * Returns:
 *    0:success;others,error
 *******************************************************************************/
static MsgRet DM_UART_WaitDataAvailable(SINT32 iFd, UINT32 nTimeOut)
{
    struct timeval stTv;
    fd_set fsReadFds;
    SINT32 iRc;

    FD_ZERO(&fsReadFds);
    FD_SET(iFd, &fsReadFds);

	if(nTimeOut > 0)
	{
		stTv.tv_sec = nTimeOut / MSECS_IN_SEC;
		stTv.tv_usec = (nTimeOut % MSECS_IN_SEC) * USECS_IN_MSEC;
		iRc = select(iFd+1, &fsReadFds, NULL, NULL, &stTv);
	}else{
		iRc = select(iFd+1, &fsReadFds, NULL, NULL, NULL);
	}
   
    if ((iRc == 1) && (FD_ISSET(iFd, &fsReadFds)))
    {
        return MSGRET_SUCCESS;
    }
    else
    {
        return MSGRET_TIMED_OUT;
    }
}

/*******************************************************************************
 * Function:
 *    SINT32 DM_InetServerInit(SINT32 iDomain, SINT32 iPort, SINT32 iType, SINT32 iBackLog)
 * Description:
 *    This function creates and initializes a TCP or UDP listening socket
 *    for an application.
 * Parameters:
 *    iDomain  (IN) Specifies whether it is a client-side socket or
 *                  server-side socket.
 *    iPort    (IN) The application TCP or UDP port.
 *    iType    (IN) The socket type, either SOCK_STREAM or SOCK_DGRAM.
 *    iBackLog (IN) Number of connections to queue.
 * Returns:
 *    The socket file descriptor
 *******************************************************************************/
SINT32 DM_UART_InetServerInit(SINT32 iDomain, SINT32 iPort, SINT32 iType, SINT32 iBackLog)
{
    SINT32 iFd;
    SINT32 iOptVal;
    
    /* Create a TCP or UDP based socket */
    if ((iFd = socket(iDomain, iType, 0)) < 0)
    {
        debug("socket errno=%d port=%d reason:%s\n", errno, iPort, strerror(errno));
        return MSG_INVALID_FD;
    }
    
    /* Set socket options */
    iOptVal = 1;
    if (setsockopt(iFd, SOL_SOCKET, SO_REUSEADDR, &iOptVal, sizeof(iOptVal)) < 0)
    {
        debug("setsockopt errno=%d port=%d fd=%d reason:%s\n", errno, iPort, iFd, strerror(errno));
        close(iFd);
        return MSG_INVALID_FD;
    }
    
    /* Set up the local address */
    if (iDomain == AF_INET)
    {
        struct sockaddr_in stServerAddr;
        
        if (iType == SOCK_DGRAM)
        {
            /* set option for getting the to ip address. */
            if (setsockopt(iFd, IPPROTO_IP, IP_PKTINFO, &iOptVal, sizeof(iOptVal)) < 0)
            {
                debug("setsockopt errno=%d port=%d fd=%d reason:%s\n",errno, iPort, iFd, strerror(errno));
                close(iFd);
                return MSG_INVALID_FD;
            }
        }
        
        bzero(&stServerAddr, sizeof(stServerAddr));
        stServerAddr.sin_family = AF_INET;
        stServerAddr.sin_port   = htons(iPort);
        stServerAddr.sin_addr.s_addr  = htonl(INADDR_ANY);
        
        /* Bind socket to local address */
        if (bind(iFd, (struct sockaddr *)&stServerAddr, sizeof(stServerAddr)) < 0)
        {
            debug( "bind errno=%d port=%d fd=%d reason:%s\n",errno, iPort, iFd, strerror(errno));
            close(iFd);
            return MSG_INVALID_FD;
        }
    }
    else
    {
        struct sockaddr_in6 stServerAddr;
        
        if (iType == SOCK_DGRAM)
        {
            /* set option for getting the to ip address. */
#ifdef IPV6_RECVPKTINFO
            if (setsockopt(iFd, IPPROTO_IPV6, IPV6_RECVPKTINFO, &iOptVal, sizeof(iOptVal)) < 0)
#else
            if (setsockopt(iFd, IPPROTO_IPV6, IPV6_PKTINFO, &iOptVal, sizeof(iOptVal)) < 0)
#endif
            {
                debug("setsockopt errno=%d port=%d fd=%d reason:%s\n",errno, iPort, iFd, strerror(errno));
                close(iFd);
                return MSG_INVALID_FD;
            }
        }
        
        bzero(&stServerAddr, sizeof(stServerAddr));
        stServerAddr.sin6_family = AF_INET6;
        stServerAddr.sin6_port   = htons(iPort);
        stServerAddr.sin6_addr   = in6addr_any;
        
        /* Bind socket to local address */
        if (bind(iFd, (struct sockaddr *)&stServerAddr, sizeof(stServerAddr)) < 0)
        {
            debug("bind errno=%d port=%d fd=%d reason:%s\n",errno, iPort, iFd, strerror(errno));
            close(iFd);
            return MSG_INVALID_FD;
        }
    }
    
    if (iType == SOCK_STREAM)
    {
        /* Enable connection to SOCK_STREAM socket */
        if (listen(iFd, iBackLog) < 0)
        {
            debug( "listen errno=%d port=%d fd=%d reason:%s\n",errno, iPort, iFd, strerror(errno));
            close(iFd);
            return MSG_INVALID_FD;
        }
    }
    
    return (iFd);
}

/*******************************************************************************
 * Function:
 *    void DM_InetServerDeinit(SINT32 iFd)
 * Description:
 *    This function deinit the socket created for internet server
 * Parameters:
 *    iFd    (IN) The socket file descriptor
 * Returns:
 *    void
 *******************************************************************************/
void DM_UART_InetServerDeinit(SINT32 iFd)
{
    if (MSG_INVALID_FD != iFd)
    {
        close(iFd);
    }
}

/*******************************************************************************
 * Function:
 *    SINT32 DM_ServerAcceptClient(SINT32 iListenFd)
 * Description:
 *    Before server recieve or send message,server should accept a client
 * Parameters:
 *    iFd    (IN) The listening socket file descriptor 
 * Returns:
 *    The accept socket file descriptor
 *******************************************************************************/
SINT32 DM_UART_ServerAcceptClient(SINT32 iListenFd)
{
    struct sockaddr_un stClientAddr;
    UINT32 nSockAddrSize;
    SINT32 iAcceptFd;

    nSockAddrSize = sizeof(stClientAddr);
    if ((iAcceptFd = accept(iListenFd, (struct sockaddr *)&stClientAddr, &nSockAddrSize)) < 0)
    {
        debug("accept IPC connection failed. errno=%d reason:%s\n", errno, strerror(errno));
        return -1;
    }

    return iAcceptFd;
}

/*******************************************************************************
 * Function:
 *    void DM_ServerAcceptClientClose(SINT32 iFd)
 * Description:
 *    This function deinit the socket created for internet server
 * Parameters:
 *    iFd    (IN) The socket file descriptor
 * Returns:
 *    void
 *******************************************************************************/
void DM_UART_ServerAcceptClientClose(SINT32 iFd)
{
    if (MSG_INVALID_FD != iFd)
    {
        close(iFd);
    }
}


SINT32 DM_UART_InetClientInit(SINT32 iDomain, SINT32 iPort, SINT32 iType,SINT8 *ipaddr)
{
    SINT32 iFd, iRc;
    struct sockaddr_in stServerAddr;
    /*
     * Create a unix domain socket.
     */
    iFd = socket(iDomain, iType, 0);
    if (iFd < 0)
    {
        debug("Could not create socket\n");
        return -1;
    }
    /*
     * Set close-on-exec, even though all apps should close their
     * fd's before fork and exec.
     */
    if ((iRc = fcntl(iFd, F_SETFD, FD_CLOEXEC)) != 0)
    {
        debug( "set close-on-exec failed, rc=%d errno=%d reason:%s\n",iRc, errno, strerror(errno));
        close(iFd);
        return -1;
    }

     
    bzero(&stServerAddr, sizeof(stServerAddr));
    stServerAddr.sin_family = AF_INET;
    stServerAddr.sin_port = htons(iPort);
    bzero(&(stServerAddr.sin_zero), 8);
    if (inet_aton(ipaddr, (struct in_addr*)&stServerAddr.sin_addr.s_addr) == 0)
    {
        perror(ipaddr);
        return -1;
    }
    iRc = connect(iFd, (struct sockaddr *) &stServerAddr, sizeof(stServerAddr));
    if (iRc != 0)
    {
        close(iFd);
        return -1;
    }
    
    return iFd;
}


/*******************************************************************************
 * Function:
 *    void DM_InetClientDeinit(SINT32 iFd)
 * Description:
 *    This function deinit the socket created for internet server
 * Parameters:
 *    iFd    (IN) The socket file descriptor
 * Returns:
 *    void
 *******************************************************************************/
void DM_UART_InetClientDeinit(SINT32 iFd)
{
    if (MSG_INVALID_FD != iFd)
    {
        close(iFd);
    }
}


/*******************************************************************************
 * Function:
 *    SINT32 DM_MsgSend(SINT32 iFd, const SINT8 *pBuf, UINT32 nLen)
 * Description:
 *    Use this function,client send message to server,
 *    or server send response message to client
 * Parameters:
 *    iFd    (IN) The socket file descriptor used to send message
 *    pBuf   (IN) Message for sending
 *    nLen   (IN) Message length
 * Returns:
 *    The sending result
 *    0:success;others,error
 *******************************************************************************/
SINT32 DM_UART_MsgSend(SINT32 iFd, const SINT8 *pBuf, UINT32 nLen)
{
    UINT32 nTotalLen;
    SINT32 iRc;
    MsgRet enRet = MSGRET_SUCCESS;
    MsgHeader *pstMsg;
    
    if (NULL == pBuf)
    {
        debug("Send buffer is null!\n");
        return MSGRET_INVALID_ARGUMENTS;
    }
    
    pstMsg = (MsgHeader *)malloc(sizeof(MsgHeader));
    if (pstMsg == NULL)
    {
        debug("malloc of msg header failed\n");
        return MSGRET_RESOURCE_EXCEEDED;
    }
    bzero(pstMsg, sizeof(MsgHeader));
    pstMsg->dataLength = nLen;
    
    pstMsg = (MsgHeader *)realloc(pstMsg, sizeof(MsgHeader) + nLen);
    if (pstMsg == NULL)
    {
        debug("realloc to %ld bytes failed\n", sizeof(MsgHeader) + nLen);
        free(pstMsg);
        return MSGRET_RESOURCE_EXCEEDED;
    }
    memcpy((SINT8 *)(pstMsg+1), pBuf, nLen);
    
    nTotalLen = sizeof(MsgHeader) + nLen;
    iRc = write(iFd, (SINT8 *)pstMsg, nTotalLen);
    if (iRc < 0)
    {
        if (errno == EPIPE)
        {
            /*
             * This could happen when tries to write to an app that
             * has exited.  Don't print out a scary error message.
             * Just return an error code and let upper layer app handle it.
             */
            debug("got EPIPE, dest app is dead, errno:%d reason:%s\n",errno, strerror(errno));
            free(pstMsg);
            return MSGRET_DISCONNECTED;
        }
        else
        {
            debug("write failed, errno=%d reason:%s\n",errno, strerror(errno));
            free(pstMsg);
            enRet = MSGRET_INTERNAL_ERROR;
        }
    }
    else if (iRc != (SINT32) nTotalLen)
    {
        debug("unexpected rc %d, expected %u\n",iRc, nTotalLen);
        free(pstMsg);
        enRet = MSGRET_INTERNAL_ERROR;
    }
    
    free(pstMsg);
    return enRet;
}


/*******************************************************************************
 * Function:
 *    SINT32 DM_MsgReceive(SINT32 iFd, SINT8 **pBuf, UINT32 pnTimeOut)
 * Description:
 *    Use this function,server recieve message from client,
 *    or client recieve response message from server
 * Parameters:
 *    iFd         (IN)  The socket file descriptor used to recieve message
 *    pBuf        (OUT) The recieve message buf
 *    pnTimeOut   (IN)  Timeout for recieving
 * Returns:
 *    When > 0,the recieved message bytes;or not,recieve error
 *******************************************************************************/
SINT32 DM_UART_MsgReceive(SINT32 iFd, SINT8 **pBuf, UINT32 pnTimeOut)
{
    MsgHeader *pstMsg;
    SINT32 iRc;
    MsgRet enRet;
    UINT32 nTotalRemaining;
    UINT32 nTotalReadSoFar = 0;
    SINT8 *pInBuf = NULL;
    if(pBuf == NULL)
    {
        debug("buf is NULL!\n");
        return MSGRET_INVALID_ARGUMENTS;
    }
    else
    {
        *pBuf = NULL;
    }
    
    if (pnTimeOut)
    {
        if ((enRet = DM_UART_WaitDataAvailable(iFd, pnTimeOut)) != MSGRET_SUCCESS)
        {
            return enRet;
        }
    }
    /*
     * Read just the header in the first read.
     * Do not try to read more because we might get part of
     * another message in the TCP socket.
     */
    pstMsg = (MsgHeader *)malloc(sizeof(MsgHeader));
    if (pstMsg == NULL)
    {
        debug("alloc of msg header failed\n");
        return MSGRET_RESOURCE_EXCEEDED;
    }
    bzero(pstMsg, sizeof(MsgHeader));
    iRc = read(iFd, pstMsg, sizeof(MsgHeader));
    if ((iRc == 0) ||
        ((iRc == -1) && (errno == 131)))  /* new 2.6.21 kernel seems to give us this before rc==0 */
    {
        /* broken connection */
        free(pstMsg);
        return MSGRET_DISCONNECTED;
    }
    else if (iRc < 0 || iRc != sizeof(MsgHeader))
    {
        debug("bad read, rc=%d errno=%d reason:%s\n",iRc, errno, strerror(errno));
        free(pstMsg);
        return MSGRET_INTERNAL_ERROR;
    }
    /* get additional data size and free msg head */
    nTotalRemaining = pstMsg->dataLength +1;
    if (nTotalRemaining > 0)
    {
        /* there is additional data in the message */
        pInBuf = (SINT8 *)calloc(1, nTotalRemaining);
        if (pInBuf == NULL)
        {
            debug("malloc to %d bytes failed\n",nTotalRemaining);
            free(pstMsg);
            free(pInBuf);
            return MSGRET_RESOURCE_EXCEEDED;
        }
        while (nTotalReadSoFar < nTotalRemaining)
        {
            //debug("reading segment: soFar=%d total=%d\n", 
                //nTotalReadSoFar, nTotalRemaining);
            if (pnTimeOut)
            {
                if ((enRet = DM_UART_WaitDataAvailable(iFd, pnTimeOut)) != MSGRET_SUCCESS)
                {
                    free(pstMsg);
                    free(pInBuf-nTotalReadSoFar);
                    return enRet;
                }
            }
            iRc = read(iFd, pInBuf, nTotalRemaining);
            if (iRc <= 0)
            {
                debug("bad data read, rc=%d errno=%d readSoFar=%d remaining=%d reason:%s\n", iRc, errno, nTotalReadSoFar, nTotalRemaining, strerror(errno));
                free(pstMsg);
                free(pInBuf);
                return MSGRET_INTERNAL_ERROR;
            }
            else
            {
                pInBuf += iRc;
                nTotalReadSoFar += iRc;
                nTotalRemaining -= iRc;
            }
        }
    }
	//debug("pstMsg->dataLength = %d\n",pstMsg->dataLength);
    *pBuf = pInBuf - nTotalReadSoFar;
	free(pstMsg);
    return nTotalReadSoFar;
}



