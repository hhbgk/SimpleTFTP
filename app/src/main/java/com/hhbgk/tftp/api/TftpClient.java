package com.hhbgk.tftp.api;

public class TftpClient {
    private final String tag = getClass().getSimpleName();
    static {
        System.loadLibrary("jl_tftp");
    }

    private static final int TFTP_OP_GET = 1;
    private static final int TFTP_OP_PUT = 2;

    private native void nativeInit(String ip);
    private native boolean _setup(String ip);
    private native boolean _download(String remoteFilePath, String savePath);
    private native boolean _request(int operation, String remoteFilePath, String localFilePath);
    private native boolean _destroy();

    public TftpClient(String ip){
        nativeInit(ip);
//        _setup(ip);

    }

    public boolean download(String remoteFilePath, String localFilePath){
        return _request(TFTP_OP_GET, remoteFilePath, localFilePath);
//        return _download(remoteFilePath, savePath);
    }

    public boolean upload(String remoteFilePath, String localFilePath){
        return _request(TFTP_OP_PUT, remoteFilePath, localFilePath);
    }

    public boolean release(){
        return _destroy();
    }
}
