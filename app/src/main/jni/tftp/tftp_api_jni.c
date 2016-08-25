#include "tftp_api_jni.h"

#define JNI_CLASS_IJKPLAYER     "com/hhbgk/tftp/api/TftpClient"
#define NELEM(x) ((int) (sizeof(x) / sizeof((x)[0])))

#ifdef __GNUC__
    #define UNUSED_PARAM __attribute__ ((unused))
#else /* not a GCC */
    #define UNUSED_PARAM
#endif /* GCC */

static JavaVM* g_jvm = NULL;
static jobject g_obj = NULL;

static client_t client;

/*
 * 填充tftp_t结构体，以便发送读写请求
 * 返回:操作码＋文件名＋模式的长度
 */
static int fill_request(client_t * req, int opcode, char * filename, char * mode){
	logd("%s", __func__);
	int len = 0; //统计一共应发送出去多少字节
	char * p = (char*)&(req->tftp.be);
	req->tftp.opcode = htons(opcode) ; //操作码
	len += 2;

	strcpy(p, filename);   //文件名
	len += strlen(p)+1;

	while( *p++ ); //end of filename, after '\0'
	strcpy( p, mode);	   //模式(netascii或octet)
	len += strlen(p)+1;

	while( *p++ ); //end of mode(netascii或octet)
	strcpy( p, KEY_BLOCK_SIZE);
	len += strlen(p)+1;

	while( *p++ ); //end of block size
	char str[32];
	sprintf(str, "%d", req->pl_size);
	strcpy( p, str);
	len += strlen(p)+1;

	return len;
}

//发送读写请求,有数据发过来即返回0
static int send_request( int sockfd,  struct sockaddr_in * dst, client_t * req, int reqlen ){
	logd("%s", __func__);
	int i,ret;
	struct pollfd fds[1] = {sockfd, POLLIN, 0};

	for(i=1; i<=3; i++)	{
		sendto(sockfd, &req->tftp, reqlen, 0, (SAP)dst, sizeof(*dst));
		ret = poll(fds, 1, TIMEOUT*1000);
		if( ret > 0 && fds[0].revents & POLLIN ){
			break;       //有数据收到
		}
		else if( 3==i ){
			loge("%s:time out\n", __func__);
			return -1;
		}
	}

	return 0;
}

static jboolean jni_tftp_download(JNIEnv *env, jobject thiz, jstring remote_file_path, jstring save_file_path){
	logd("%s", __func__);
	client_t req;//用于发送读写请求
	req.pl_size = 512;
	int ret, is_success = 1;
	int reqlen = 0;
	int sockfd = 0;
	int downloadfd = 0;
	struct sockaddr_in server = {0}; //服务器端地址
	const char *c_remote_path = (*env)->GetStringUTFChars(env, remote_file_path, NULL);
	const char *c_save_path = (*env)->GetStringUTFChars(env, save_file_path, NULL);
	logi("c_file_path=%s, c_save_path=%s", c_remote_path, c_save_path);
	loge("dst ip=%s", client.dst_ip);

	//设置服务器地址
	server.sin_family = AF_INET;
	server.sin_port = htons(SERVER_PORT);
	server.sin_addr.s_addr = inet_addr(client.dst_ip);
	if( INADDR_NONE == server.sin_addr.s_addr)	{
		loge("IP addr invalid");
		goto error_out;
	}

	//创建套接字
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if( sockfd < 0 )	{
		loge("%s:%s",__func__, strerror("socket"));
		goto error_out;
	}

	downloadfd = open(c_save_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	logi("downloadfd=%d", downloadfd);
	if( downloadfd < 0 )	{
		loge("%s:%s", __func__, strerror("open"));
		perror("open");
		is_success = 0;
		goto error_out;
	}
	reqlen = fill_request( &req, RRQ, c_remote_path, "octet" );

	ret = send_request(sockfd, &server, &req, reqlen); //发送请求
	if( ret < 0 ){
		is_success = 0;
		goto error_out;
	}

	ret = recv_file(sockfd, downloadfd, &server, req.pl_size); //接收文件
	if( 0 != ret ){
		is_success = 0;
		goto error_out;
	}

error_out:
	if(downloadfd > 0){
		logi("close downloadfd");
		close(downloadfd);
	}
	if(sockfd > 0){
		close(sockfd);
		logi("close sockfd");
	}
	(*env)->ReleaseStringUTFChars(env, remote_file_path, c_remote_path);
	(*env)->ReleaseStringUTFChars(env, save_file_path, c_save_path);
	if(is_success){
		return JNI_TRUE;
	} else{
		return JNI_FALSE;
	}
}

static jboolean jni_tftp_request(JNIEnv *env, jobject thiz, jint op, jstring remote_file_path, jstring local_file_path){
	logd("%s", __func__);
	int i,ret;
	int fd,sockfd;
	client_t req;//用于发送读写请求
	client_t ack;

	int reqlen = 0;
	struct sockaddr_in server = {0}; //服务器端地址
	struct sockaddr_in sender = {0};
	int addrlen = sizeof(sender);
	const char *c_remote_path = (*env)->GetStringUTFChars(env, remote_file_path, NULL);
	const char *c_local_path = (*env)->GetStringUTFChars(env, local_file_path, NULL);
	logi("op=%d, c_remote_path=%s, c_local_path=%s", op, c_remote_path, c_local_path);
	loge("dst ip=%s", client.dst_ip);

	//设置服务器地址
	server.sin_family = AF_INET;
	server.sin_port = htons(SERVER_PORT);
	server.sin_addr.s_addr = inet_addr(client.dst_ip);
	if( INADDR_NONE == server.sin_addr.s_addr) {
		loge("IP addr invalid");
		return JNI_FALSE;
	}
	//创建套接字
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if( sockfd < 0 ) {
		loge("%s:%s",__func__, strerror("socket"));
		perror("socket");
		return JNI_FALSE;
	}

	if( TFTP_OP_GET == op ){  //下载文件，覆盖现有文件
		fd = open(c_local_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if( fd < 0 ) {
			loge("%s:%s",__func__, strerror("open"));
			perror("open");
			goto ERROR_OUT;
		}
		req.pl_size = OP_GET_BLKSIZE;
		reqlen = fill_request( &req, RRQ, c_remote_path, "octet" );

		ret = send_request(sockfd, &server, &req, reqlen); //发送请求
		if( ret < 0 )
			goto ERROR_OUT;

		ret = recv_file(sockfd, fd, &server, req.pl_size); //接收文件
		if( 0 != ret )
			goto ERROR_OUT;
	}
	else if( TFTP_OP_PUT == op ) {//上传文件
		fd = open(c_local_path, O_RDONLY );
		if( fd < 0 ) {
			loge("%s:%s",__func__, strerror("open"));
			perror("open");
			goto ERROR_OUT;
		}
		req.pl_size = OP_PUT_BLKSIZE;
		ack.pl_size = OP_PUT_BLKSIZE;
		reqlen=fill_request( &req, WRQ, c_remote_path, "octet");

		for(i=1;i<=3; i++ ) {
			ret = send_request(sockfd, &server, &req, reqlen); //发送请求
			if( ret < 0 ){
				loge("%s: send_request =%d\n",__func__, ret);
				goto ERROR_OUT;
			}

			addrlen = sizeof(sender);
			recvfrom(sockfd, &ack.tftp, sizeof(ack.tftp),0,(SAP)&sender,&addrlen);
			if( sender.sin_addr.s_addr == server.sin_addr.s_addr ){
				if(( ntohs(ack.tftp.opcode) == ACK  &&  0 == ntohs(ack.tftp.be.block)) || ntohs(ack.tftp.opcode) == OACK )
					break;   //收到0号确认，服务器启用新端口，新地址放在sender

				if( ntohs(ack.tftp.opcode) == ERROR ) {
					loge("ERROR code %d: %s\n", ntohs(ack.tftp.be.error), ack.tftp.data);
					goto ERROR_OUT;
				}
			}
			if( 3 == i ){
				loge("%s: i =%d\n",__func__, i);
				goto ERROR_OUT;
			}
		}

		ret = send_file(sockfd, fd, &sender, req.pl_size);
		if( 0 != ret ){
			loge("%s: send_file =%d\n",__func__, ret);
			goto ERROR_OUT;
		}
	} else{
		loge("usage: client <server IP> <get|put> <filename>\n");
		return JNI_FALSE;
	}

	close(fd);
	close(sockfd);
	(*env)->ReleaseStringUTFChars(env, remote_file_path, c_remote_path);
	(*env)->ReleaseStringUTFChars(env, local_file_path, c_local_path);
	return JNI_TRUE;

ERROR_OUT:
	if( sockfd > 0 )
		close(sockfd);
	if ( fd > 0 )
		close(fd);
	(*env)->ReleaseStringUTFChars(env, remote_file_path, c_remote_path);
	(*env)->ReleaseStringUTFChars(env, local_file_path, c_local_path);
	loge("Error: exit!\n");
	return JNI_FALSE;
}

static void native_init(JNIEnv *env, jobject thiz, jstring ip){
    logd("%s", __func__);
    //保存全局JVM以便在子线程中使用
    (*env)->GetJavaVM(env,&g_jvm);
    //不能直接赋值(g_obj = thiz)
    g_obj = (*env)->NewGlobalRef(env, thiz);

    const char *c_ip = (*env)->GetStringUTFChars(env, ip, NULL);
    logw("000 dst ip=%s, c_ip=%s", client.dst_ip, c_ip);
    sprintf(client.dst_ip, "%s", c_ip);
    logw("dst ip=%s, c_ip=%s", client.dst_ip, c_ip);
    (*env)->ReleaseStringUTFChars(env, ip, c_ip);
}

static jboolean jni_tftp_destroy(JNIEnv *env, jobject thiz){
	logd("%s", __func__);

	return JNI_TRUE;
}

static JNINativeMethod g_methods[] = {
    { "nativeInit",         "(Ljava/lang/String;)V",                                               (void *) native_init },
    //{ "_setup",         "(Ljava/lang/String;)Z",                                               (void *) jni_tftp_setup },
    { "_download",         "(Ljava/lang/String;Ljava/lang/String;)Z",                                               (void *) jni_tftp_download },
    { "_request",         "(ILjava/lang/String;Ljava/lang/String;)Z",                                               (void *) jni_tftp_request },
    { "_destroy",         "()Z",                                               (void *) jni_tftp_destroy},
 };

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    JNIEnv* env = NULL;

    if ((*vm)->GetEnv(vm, (void**) &env, JNI_VERSION_1_4) != JNI_OK) {
        return -1;
    }
    assert(env != NULL);

    // FindClass returns LocalReference
    jclass klass = (*env)->FindClass (env, JNI_CLASS_IJKPLAYER);
    if (klass == NULL) {
      //LOGE ("Native registration unable to find class '%s'", JNI_CLASS_IJKPLAYER);
      return JNI_ERR;
    }
    (*env)->RegisterNatives(env, klass, g_methods, NELEM(g_methods) );

    return JNI_VERSION_1_4;
}
