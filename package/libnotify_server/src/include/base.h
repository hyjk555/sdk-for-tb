/*
 * =============================================================================
 *
 *       Filename:  base.h
 *
 *    Description:  base define and include
 *
 *        Version:  1.0
 *        Created:  2015/3/19 11:16
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Oliver (), 515296288jf@163.com
 *   Organization:  
 *
 * =============================================================================
 */

#ifndef _BASE_NC_H_
#define _BASE_NC_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <resolv.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <semaphore.h>
#include <pthread.h>
#include <time.h>
#include <dirent.h>
#include <stdint.h>
#include "my_json.h"


#ifdef __cplusplus
extern "C"{
#endif

#define THIS_MODULE         ""

typedef unsigned long long uint64_t;
typedef unsigned int uint32_t;
typedef int int32_t;
typedef unsigned short uint16_t;
typedef short int16_t;
// typedef char int8_t;
typedef unsigned char uint8_t;


#define ENTER_FUNC() do{\
    DMCLOG_D("\nEnter %s()\n", __FUNCTION__);\
    }while(0)

#define EXIT_FUNC() do{\
    DMCLOG_D("Exit %s()\n", __FUNCTION__);\
    }while(0)



#define safe_free(p) do{\
	if((p) != NULL)\
	{\
		free((p));\
		(p) = NULL;\
	}\
	}while(0)

#define safe_fclose(f) do{\
	if((f) != NULL)\
	{\
		fclose((f));\
		(f) = NULL;\
	}\
	}while(0)

#define safe_close(fd) do{\
	if((fd) > 0)\
	{\
		close((fd));\
		(fd) = -1;\
	}\
	}while(0)

#define MEMSET(o) memset(&(o), 0, sizeof(o))
#define MEMSET_P(p,s) memset(p, 0, s)

#define TCP_INIT_PORT 			13001
#define RECV_TIMEOUT            3000
#define LOCAL_ADDR				"127.0.0.1"

//cmd num
//#define FN_NOTIFY_DEL_IP 		5
#define FN_NOTIFY_RELEASE_DISK	7


typedef struct _ClientThreadInfo
{
	struct sockaddr_in clientAddr;
	struct sockaddr_in clientAddrSend;
    int client_fd;  // for tcp or cgi_forward udp
    int client_fd_send;  // for tcp or cgi_forward udp

    // for recv
    char *recv_buf;
    uint32_t session_id; // for session
    // for send response
    JObj *header_json;
    JObj *r_json;
    JObj *s_json;
	uint32_t cmd;
	uint32_t seq;
    uint16_t ver;
	uint16_t error;
    uint16_t device;
    uint16_t appid;
	uint16_t code;
	int acc_fd;
	char *retstr;
	uint32_t time_out;
	char ip[32];
	char client_ip[32];
	int release_flag;
	int status;
}ClientTheadInfo;

#define EXPIRE_TIME 5
#define EXPIRE_COUNT 30


#ifdef __cplusplus
}
#endif


#endif

