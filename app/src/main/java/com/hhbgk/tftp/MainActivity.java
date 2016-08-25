package com.hhbgk.tftp;

import android.app.Activity;
import android.os.Bundle;
import android.os.Environment;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;

import com.hhbgk.tftp.api.TftpClient;

public class MainActivity extends Activity {
    private final String tag = getClass().getSimpleName();
    private EditText mGetEdit, mPutEdit;
    private Button mGetBtn, mPutBtn;
    private TftpClient mTftpClient;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        mGetEdit = (EditText) findViewById(R.id.get_file_ed);
        mGetEdit.setText("rec00001.avi");
        mPutEdit = (EditText) findViewById(R.id.put_file_ed);
        mPutEdit.setText("rec00001.avi");

        mGetBtn = (Button) findViewById(R.id.get_file_bt);
        mPutBtn = (Button) findViewById(R.id.put_file_bt);

        if (mTftpClient == null){
            mTftpClient = new TftpClient("192.168.9.181");
        }

        mGetBtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                new Thread(new Runnable() {
                    @Override
                    public void run() {

                        getFileFromServer();
                    }
                }).start();
            }
        });

        mPutBtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                new Thread(new Runnable() {
                    @Override
                    public void run() {

                        putFileToServer();
                    }
                }).start();
            }
        });
    }

    private void getFileFromServer(){
        String getFile = mGetEdit.getText().toString().trim();
        String saveFile = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS).getPath() + "/" + getFile;
        Log.i(tag, "download file="+ saveFile);
        mTftpClient.download(getFile, saveFile);
    }

    private void putFileToServer(){
        String putFile = mPutEdit.getText().toString().trim();
        String localFile = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS).getPath() + "/" + putFile;
        Log.i(tag, "upload file="+ localFile);
        mTftpClient.upload(putFile, localFile);
    }
    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (mTftpClient != null){
            mTftpClient.release();
        }
    }
}
