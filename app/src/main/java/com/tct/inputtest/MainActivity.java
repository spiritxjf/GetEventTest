package com.tct.inputtest;

import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

public class MainActivity extends AppCompatActivity {
    private boolean mMonitor = false;
    private int mX = 0;
    private int mY = 0;
    private int EV_ABS = 0x03;
    private int EV_KEY = 0x01;
    private int ABS_MT_POSITION_X = 0x35;
    private int ABS_MT_POSITION_Y = 0x36;
    private int ABS_MT_TRACKING_ID = 0x39;
    private int BTN_TOUCH = 0x14a;
    private int BTN_TOUCH_DOWN = 0x1;
    private int BTN_TOUCH_UP = 0x0;

    // Used to load the 'native-lib' library on application startup.
    static {
        System.loadLibrary("native-lib");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        Button start = findViewById(R.id.start);
        Button stop = findViewById(R.id.stop);

        stop.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                mMonitor = false;
            }
        });

        start.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                Runnable runnable = new Runnable() {
                    @Override
                    public void run() {
                        if (0 != openDevice())
                        {
                            return;
                        }
                        mMonitor = true;
                        int value = 0;
                        int eventcode;
                        int eventtype;
                        int eventvalue;

                        while (true){
                            value = readDevice();

                            eventtype = (value & 0xf0000000) >> 28;
                            eventcode = (value & 0x0fff0000) >> 16;
                            eventvalue = value & 0x0000ffff;

                            //Log.d("93367", "(type, code, value) = " + "(" + eventtype + ", " + eventcode + ", " +  eventvalue+ ") ");

                            if (eventtype == EV_ABS && eventcode == ABS_MT_POSITION_X)
                            {
                                mX = eventvalue;
                            }
                            if (eventtype == EV_ABS && eventcode == ABS_MT_POSITION_Y)
                            {
                                mY = eventvalue;
                            }
                            if (eventtype == EV_KEY && eventcode == BTN_TOUCH)
                            {
                                //滑动的话，down和up的x y不一样，中间会多次上报x y信息，如果有需要可以抓取
                                if (eventvalue == BTN_TOUCH_UP)
                                {
                                    Log.d("93367", "Motion Event(" + mX + ", " + mY + ") UP");
                                }else if (eventvalue == BTN_TOUCH_DOWN)
                                {
                                    Log.d("93367", "Motion Event(" + mX + ", " + mY + ") DOWN");
                                }
                            }

                            if (false == mMonitor)
                            {
                                break;
                            }
                        }
                        closeDevice();
                    }
                };

                Thread thread = new Thread(runnable);
                thread.start();
            }
        });



        //tv.setText(stringFromJNI());
    }

    @Override
    protected void onDestroy() {
        closeDevice();
        super.onDestroy();
    }

    /**
     * A native method that is implemented by the 'native-lib' native library,
     * which is packaged with this application.
     */
    public native int openDevice();
    public native int readDevice();
    public native int closeDevice();
}
