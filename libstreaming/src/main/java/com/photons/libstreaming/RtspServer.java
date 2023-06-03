package com.photons.libstreaming;

import android.util.Log;

import java.util.concurrent.ArrayBlockingQueue;

public class RtspServer {
    static {
        System.loadLibrary("libstreaming");
    }

    private static final String TAG = "Rtsp-Java";
    public ArrayBlockingQueue<byte[]> queue = new ArrayBlockingQueue<>(50);

    public interface Callback {
        void onSubsessionStateChanged(boolean isOpened);
    }

    private volatile boolean isSubsessionOpened;
    private Callback callback;

    public void start(Callback callback) {
        this.callback = callback;

        new Thread(() -> {
            unicast();

            Log.e(TAG, "server stopped");
        }).start();
    }

    public void putFrame(byte[] frame) {
        if (!isSubsessionOpened) {
            return;
        }

        try {
            queue.put(frame);
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
    }

    public byte[] getFrame() {
        byte[] take = new byte[1];
        try {
            take = queue.take();
        } catch (Exception e) {
            e.printStackTrace();
        }
        return take;
    }

    public void onSubsessionStateChanged(boolean open) {
        isSubsessionOpened = open;
        queue.clear();
        if (callback != null) {
            callback.onSubsessionStateChanged(open);
        }
    }

    // 多播，参考了live/testProgs/testH264VideoStreamer.cpp的写法
    // 当前有一个问题，客户端随时接入后，要等到下一个I帧才能正常播放
    private native void multicast();

    // 单播，参考了live/testProgs/testOnDemandRTSPServer.cpp的写法
    // 在客户端连接时才启动摄像头进行编码
    private native void unicast();
}