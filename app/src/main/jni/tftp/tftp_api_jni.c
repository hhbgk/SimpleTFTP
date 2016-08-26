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
static jmethodID on_error_method_id;

static void error_message_handler(const char *msg){
	logd("%s:%s", __func__, msg);
	JNIEnv *env = NULL;
	(*g_jvm)->GetEnv(g_jvm, (void**)&env, JNI_VERSION_1_4);
	jstring jmsg = (*env)->NewStringUTF(env, msg);
	(*env)->CallVoidMethod(env, g_obj, on_error_method_id, jmsg);
	(*env)->ReleaseStringUTFChars(env, jmsg, msg);
}
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
		logi("send request %i time", i);
		ret = poll(fds, 1, TIMEOUT*1000);
		if( ret > 0 && (fds[0].revents & POLLIN )){
			break;       //有数据收到
		}
		else if( 3==i ){
			loge("%s:time out\n", __func__);
			return -1;
		}
	}

	return 0;
}

void *tftp_request_runnable(void *arg){
	logd("%s", __func__);
	req_pkg_t *req_pkg = (req_pkg_t *)arg;
	int i,ret;
	int fd,sockfd;
	client_t req;//用于发送读写请求
	client_t ack;
	int reqlen = 0;
	struct sockaddr_in server = {0}; //服务器端地址
	struct sockaddr_in sender = {0};
	int addrlen = sizeof(sender);

	//logi("ip=%s, op=%d, c_remote_path=%s, c_local_path=%s", client.dst_ip, req_pkg->op, req_pkg->remote, req_pkg->local);
	// Detach ourself. That way the main thread does not have to wait for us with pthread_join.
	pthread_detach(pthread_self());

	//设置服务器地址
	server.sin_family = AF_INET;
	server.sin_port = htons(SERVER_PORT);
	server.sin_addr.s_addr = inet_addr(client.dst_ip);
	if( INADDR_NONE == server.sin_addr.s_addr) {
		error_message_handler("IP addr invalid");
		goto eixt_thread;
	}
	//创建套接字
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if( sockfd < 0 ) {
		loge("%s:%s",__func__, strerror("socket"));
		error_message_handler("Create socket fail");
		goto eixt_thread;
	}

	if( TFTP_OP_GET == req_pkg->op ){  //下载文件，覆盖现有文件
		fd = open(req_pkg->local, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if( fd < 0 ) {
			loge("%s:%s",__func__, strerror("open"));
			error_message_handler("Open fail");
			goto ERROR_OUT;
		}
		req.pl_size = OP_GET_BLKSIZE;
		reqlen = fill_request( &req, RRQ, req_pkg->remote, "octet" );

		ret = send_request(sockfd, &server, &req, reqlen); //发送请求
		if( ret < 0 ){
			error_message_handler("Send request timeout");
			goto ERROR_OUT;
		}

		ret = recv_file(sockfd, fd, &server, req.pl_size); //接收文件
		if( 0 != ret ) {
			error_message_handler("Receive request timeout");
			goto ERROR_OUT;
		}
	}
	else if( TFTP_OP_PUT == req_pkg->op ) {//上传文件
		fd = open(req_pkg->local, O_RDONLY );
		if( fd < 0 ) {
			loge("%s:%s",__func__, strerror("open"));
			error_message_handler("Open fail");
			goto ERROR_OUT;
		}
		req.pl_size = OP_PUT_BLKSIZE;
		ack.pl_size = OP_PUT_BLKSIZE;
		reqlen=fill_request( &req, WRQ, req_pkg->remote, "octet");

		ret = send_request(sockfd, &server, &req, reqlen); //发送请求
		if( ret < 0 ){
			error_message_handler("Send request timeout");
			goto ERROR_OUT;
		}

		addrlen = sizeof(sender);
		recvfrom(sockfd, &ack.tftp, sizeof(ack.tftp),0,(SAP)&sender,&addrlen);
		if( sender.sin_addr.s_addr == server.sin_addr.s_addr ){
			if(( ntohs(ack.tftp.opcode) == ACK  &&  0 == ntohs(ack.tftp.be.block)) || ntohs(ack.tftp.opcode) == OACK ){
				logi("ntohs(ack.tftp.opcode)=%d", ntohs(ack.tftp.opcode));
				//break;   //收到0号确认，服务器启用新端口，新地址放在sender
			}
			if( ntohs(ack.tftp.opcode) == ERROR ) {
				loge("ERROR code %d: %s\n", ntohs(ack.tftp.be.error), ack.tftp.data);
				goto ERROR_OUT;
			}
		}
		ret = send_file(sockfd, fd, &sender, req.pl_size);
		if( 0 != ret ){
			loge("%s: send_file =%d\n",__func__, ret);
			error_message_handler("Send file timeout");
			goto ERROR_OUT;
		}
	} else{
		error_message_handler("No the operation exist");
		goto eixt_thread;
	}

ERROR_OUT:
	if( sockfd > 0 )
		close(sockfd);
	if ( fd > 0 )
		close(fd);
	// Free memory.
	if(req_pkg->local){
		free(req_pkg->local);
		loge("Free local");
	}
	if(req_pkg->remote){
		free(req_pkg->remote);
	}
eixt_thread:
	logd("Server thread exiting");
	pthread_exit(NULL);
}

static jboolean jni_tftp_request(JNIEnv *env, jobject thiz, jint op, jstring remote_file_path, jstring local_file_path){
	logd("%s", __func__);
	req_pkg_t *req_pkg;
	pthread_t tid;
	req_pkg = (req_pkg_t *)malloc(sizeof(req_pkg_t));
	if((req_pkg = (req_pkg_t *)malloc(sizeof(req_pkg_t))) == NULL){
		loge("%s: %d: Memory allocation failed", __FILE__, __LINE__);
		return JNI_FALSE;
	}
	if((req_pkg->local = malloc(128)) == NULL){
		loge("%s: %d: Memory allocation failed", __FILE__, __LINE__);
		return JNI_FALSE;
	}
	if((req_pkg->remote = malloc(128)) == NULL){
		loge("%s: %d: Memory allocation failed", __FILE__, __LINE__);
		return JNI_FALSE;
	}

   req_pkg->op = op;
   req_pkg->remote = (*env)->GetStringUTFChars(env, remote_file_path, NULL);
   req_pkg->local = (*env)->GetStringUTFChars(env, local_file_path, NULL);
   /* Start a new server thread. */
   if (pthread_create(&tid, NULL, tftp_request_runnable, (void *)req_pkg) != 0) {
	   loge("Failed to start new thread");
	   free(req_pkg);
	   return JNI_FALSE;
	}
   //logi("ip=%s, op=%d, c_remote_path=%s, c_local_path=%s, tid=%d", client.dst_ip, op, req_pkg->remote, req_pkg->local, tid);
   (*env)->DeleteLocalRef(env, remote_file_path);
   (*env)->DeleteLocalRef(env, local_file_path);
	return JNI_TRUE;
}

static void jni_native_init(JNIEnv *env, jobject thiz, jstring ip){
    logd("%s", __func__);
    //保存全局JVM以便在子线程中使用
    (*env)->GetJavaVM(env,&g_jvm);
    //不能直接赋值(g_obj = thiz)
    g_obj = (*env)->NewGlobalRef(env, thiz);

    const char *c_ip = (*env)->GetStringUTFChars(env, ip, NULL);
    sprintf(client.dst_ip, "%s", c_ip);
    logw("dst ip=%s, c_ip=%s", client.dst_ip, c_ip);
    (*env)->ReleaseStringUTFChars(env, ip, c_ip);

    jclass clazz = (*env)->GetObjectClass(env, thiz);
    if(clazz == NULL){
    	(*env)->ThrowNew(env, "java/lang/NullPointerException", "Unable to find exception class");
    }
    on_error_method_id = (*env)->GetMethodID(env, clazz, "onError", "(Ljava/lang/String;)V");
    if(!on_error_method_id){
    	loge("The calling class does not implement all necessary interface methods");
    }
}

static jboolean jni_tftp_destroy(JNIEnv *env, jobject thiz){
	logd("%s", __func__);

	return JNI_TRUE;
}

static JNINativeMethod g_methods[] = {
    { "nativeInit",         "(Ljava/lang/String;)V",                                               (void *) jni_native_init },
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
