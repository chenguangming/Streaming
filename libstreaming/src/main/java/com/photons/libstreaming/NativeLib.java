package com.photons.libstreaming;

public class NativeLib {

    // Used to load the 'libstreaming' library on application startup.
    static {
        System.loadLibrary("libstreaming");
    }

    /**
     * A native method that is implemented by the 'libstreaming' native library,
     * which is packaged with this application.
     */
    public native String stringFromJNI();
}