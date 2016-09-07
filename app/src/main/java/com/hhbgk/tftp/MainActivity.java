package com.hhbgk.tftp;

import android.app.Activity;
import android.os.Bundle;
import android.os.Environment;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.Toast;

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
        mGetEdit.setText("aa.txt");
        mPutEdit = (EditText) findViewById(R.id.put_file_ed);
        mPutEdit.setText("test.mp4");

        mGetBtn = (Button) findViewById(R.id.get_file_bt);
        mPutBtn = (Button) findViewById(R.id.put_file_bt);

        if (mTftpClient == null){
            mTftpClient = new TftpClient("192.168.8.174");
        }

        mTftpClient.setOnTftpClientListener(new TftpClient.OnTftpClientListener() {
            @Override
            public void onDownloadCompleted() {

            }

            @Override
            public void onUploadCompleted() {

            }

            @Override
            public void onError(String message) {
                Log.e(tag, "Error:"+message);
                Toast.makeText(MainActivity.this, message, Toast.LENGTH_SHORT).show();
            }
        });

        mGetBtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                getFileFromServer();

            }
        });

        mPutBtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                putFileToServer();
            }
        });
    }

    private void getFileFromServer(){
        String getFile = mGetEdit.getText().toString().trim();
        String saveFile = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS).getPath() + "/" + getFile;
        mTftpClient.download(getFile, saveFile);
    }

    private void putFileToServer(){
        String putFile = mPutEdit.getText().toString().trim();
        String localFile = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS).getPath() + "/" + putFile;
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
