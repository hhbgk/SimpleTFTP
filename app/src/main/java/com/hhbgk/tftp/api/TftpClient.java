package com.hhbgk.tftp.api;

import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.util.Log;

public class TftpClient {
    private final String tag = getClass().getSimpleName();
    static {
        System.loadLibrary("jl_tftp");
    }

    private static final int TFTP_OP_GET = 1;
    private static final int TFTP_OP_PUT = 2;

    private native void nativeInit(String ip);
    private native boolean _request(int operation, String remoteFilePath, String localFilePath);
    private native boolean _destroy();

    private static final int MAIN_MSG_TFTP_ERROR = 1;
    private final Handler mMainThreadHandler = new Handler(Looper.getMainLooper(), new Handler.Callback() {
        @Override
        public boolean handleMessage(Message msg) {
            switch (msg.what){
                case MAIN_MSG_TFTP_ERROR:
                    if (mOnTftpClientListener != null){
                        mOnTftpClientListener.onError((String) msg.obj);
                    }
                    break;
            }
            return false;
        }
    });

    /**
     * Call from native
     */
    private void onError(String msg){
        //Log.e(tag, "Error:" + msg);
        Message message = Message.obtain();
        message.what = MAIN_MSG_TFTP_ERROR;
        message.obj = msg;
        mMainThreadHandler.sendMessage(message);
    }

    public TftpClient(String ip){
        nativeInit(ip);
    }

    public interface OnTftpClientListener{
        void onError(String message);
    }
    private OnTftpClientListener mOnTftpClientListener;

    public void setOnTftpClientListener(OnTftpClientListener listener){
        mOnTftpClientListener = listener;
    }
    public boolean download(String remoteFilePath, String localFilePath){
        return _request(TFTP_OP_GET, remoteFilePath, localFilePath);
    }

    public boolean upload(String remoteFilePath, String localFilePath){
        return _request(TFTP_OP_PUT, remoteFilePath, localFilePath);
    }

    public boolean release(){
        return _destroy();
    }
}
