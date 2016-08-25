package com.hhbgk.tftp.api;

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

    /**
     * Call from native
     */
    private void onError(String msg){
        Log.e(tag, "Error:" + msg);
    }

    public TftpClient(String ip){
        nativeInit(ip);
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
