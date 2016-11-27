//
// Created by bob on 16-8-24.
//

#ifndef SIMPLETFTP_TFTP_API_JNI_H
#define SIMPLETFTP_TFTP_API_JNI_H

#ifdef __ANDROID__
#include <android/log.h>
#include <jni.h>
#endif
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>

#include "tftp/common.h"

typedef struct{
	int pl_size;//payload size
	tftp_t tftp;
	int sockfd;
	char dst_ip[32];
}client_t;

typedef struct{
	int op;
	char *remote;
	char *local;
}req_pkg_t;

#endif //SIMPLETFTP_TFTP_API_JNI_H
