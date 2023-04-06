package cn.aisaka.ffmpeg.test

import java.nio.ByteBuffer

object FFmpegNative {
    init {
        System.loadLibrary("FFmpeg")
    }

    external fun getVersion(): String

    external fun initPushUrl(url: String, codecType: Int): Int

    external fun initVideoParameters(
        width: Int,
        height: Int,
        frameRate: Int,
        bitrate: Int,
        codecName: String
    ): Int

    external fun initAudioParameters(channelLayout: Int, sampleRate: Int, bitrate: Int): Int

    //
    external fun writeVideo(
        y: ByteBuffer, yRowStride: Int,
        u: ByteBuffer, uRowStride: Int, v: ByteBuffer, vRowStride: Int

    ): Int

    external fun writeVideo2(
        y: ByteBuffer?,
        u: ByteBuffer?,
        v: ByteBuffer?,
        yStride: Int,
        uStride: Int,
        vStride: Int,
        srcPixelStrideUv: Int,width:Int,height:Int,degree:Int,path:String
    ): Int
//
//    external fun writeAudio():Int
//
//    external fun pause():Int
//
//    external fun resume():Int
//
//    external fun stop():Int
//
//    external fun release()
}