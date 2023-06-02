package com.photons.streaming

import android.media.MediaCodec
import android.media.MediaCodecInfo.CodecCapabilities
import android.media.MediaCodecInfo.CodecProfileLevel
import android.media.MediaFormat
import android.os.Build
import android.os.Handler
import android.os.HandlerThread
import android.util.Log
import android.view.Surface


class VideoEncoder(private val callback: EncoderCallback, width: Int, height: Int, bitrate: Int,
                   frameRate: Int, codecProfile: Int = -1) : MediaCodec.Callback() {
    private val TAG = "encoder"
    private val codec: MediaCodec = MediaCodec.createEncoderByType("video/avc")
    private val thread = HandlerThread(TAG)

    interface EncoderCallback {
        fun onFrameReady(data: ByteArray, sps: Boolean)
    }

    init {
        Log.e(TAG, "init encoder $width X $height, bitrate: $bitrate, frameRate: $frameRate")
        val mediaFormat = MediaFormat.createVideoFormat(
            "video/avc",
            height,
            width
        ).apply {
            setInteger(MediaFormat.KEY_BIT_RATE, bitrate)
            setInteger(MediaFormat.KEY_FRAME_RATE, frameRate)
            setInteger(MediaFormat.KEY_COLOR_FORMAT, CodecCapabilities.COLOR_FormatSurface)
            setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, 2)

            if (codecProfile != -1) {
                setInteger(MediaFormat.KEY_PROFILE, codecProfile)
                setInteger(MediaFormat.KEY_COLOR_STANDARD, MediaFormat.COLOR_STANDARD_BT2020)
                setInteger(MediaFormat.KEY_COLOR_RANGE, MediaFormat.COLOR_RANGE_FULL)
                setInteger(MediaFormat.KEY_COLOR_TRANSFER, getTransferFunction(codecProfile))
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                    setFeatureEnabled(CodecCapabilities.FEATURE_HdrEditing, true)
                }
            }
        }

        thread.start()

        codec.configure(mediaFormat, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE)
        codec.setCallback(this, Handler(thread.looper))
    }

    private fun getTransferFunction(codecProfile: Int) = when (codecProfile) {
        CodecProfileLevel.HEVCProfileMain10 -> MediaFormat.COLOR_TRANSFER_HLG
        CodecProfileLevel.HEVCProfileMain10HDR10 ->
            MediaFormat.COLOR_TRANSFER_ST2084
        CodecProfileLevel.HEVCProfileMain10HDR10Plus ->
            MediaFormat.COLOR_TRANSFER_ST2084
        else -> MediaFormat.COLOR_TRANSFER_SDR_VIDEO
    }

    fun start() {
        codec.start()
    }

    fun stop() {
        thread.quitSafely()
        codec.stop()
        codec.release()
    }

    fun getInputSurface(): Surface {
        return codec.createInputSurface()
    }

    override fun onInputBufferAvailable(codec: MediaCodec, index: Int) {
        Log.d(TAG, "onInputBufferAvailable $index")
    }

    override fun onOutputBufferAvailable(
        codec: MediaCodec,
        index: Int,
        info: MediaCodec.BufferInfo
    ) {
        codec.getOutputBuffer(index)?.let {
            if (info.size > 0) {
                // adjust the ByteBuffer values to match BufferInfo (not needed?)
                it.position(info.offset)
                it.limit(info.offset + info.size)

                ByteArray(info.size).let {data ->
                    it.get(data)
                    callback.onFrameReady(data, (info.flags and MediaCodec.BUFFER_FLAG_CODEC_CONFIG != 0))
                }
            }
        }

        codec.releaseOutputBuffer(index, false)
    }

    override fun onError(codec: MediaCodec, e: MediaCodec.CodecException) {
        Log.d(TAG, "onOutputBufferAvailable ${e.errorCode}")
    }

    override fun onOutputFormatChanged(codec: MediaCodec, format: MediaFormat) {
        Log.d(TAG, "onOutputFormatChanged $format")
    }
}