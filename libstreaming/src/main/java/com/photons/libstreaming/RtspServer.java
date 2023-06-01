package com.photons.libstreaming;

import java.util.concurrent.ArrayBlockingQueue;

public class RtspServer {
    static {
        System.loadLibrary("libstreaming");
    }

    public native void loop(String fileName, String addr);
    public native void unicast(String fileName);

    public static ArrayBlockingQueue<byte[]> queue;

    public static void setQueue(ArrayBlockingQueue<byte[]> queue) {
        RtspServer.queue = queue;
    }

    public static byte[] getFrame() {
        byte[] take = new byte[1];
//        Log.e("RtspServer", "getFrame: E " + RtspServer.queue.size());
        try {
            take = queue.take();

        } catch (Exception e) {
            e.printStackTrace();
        }
//        Log.e("RtspServer", "getFrame: X" + take.length);
        return take;
    }
}