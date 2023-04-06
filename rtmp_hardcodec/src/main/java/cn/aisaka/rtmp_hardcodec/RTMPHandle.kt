package cn.aisaka.rtmp_hardcodec

import java.nio.ByteBuffer

object RTMPHandle {
    init {
        System.loadLibrary("RTMP")
    }

    external fun getVersion(): Int

    external fun initPush(url: String): Int

    external fun setListener(listener: OnRTMPStateChangeListener)

    external fun writeVideoData(data: ByteArray,pts:Long,type:Int): Int
    external fun writeSPSPPSData(data: ByteArray,data2:ByteArray,pts:Long,type:Int): Int

    external fun rotate(
        y: ByteBuffer?,
        u: ByteBuffer?,
        v: ByteBuffer?,
        yStride: Int,
        uStride: Int,
        vStride: Int,
        yOut: ByteBuffer?,
        uOut: ByteBuffer?,
        vOut: ByteBuffer?,
        yOutStride: Int,
        uOutStride: Int,
        vOutStride: Int,
        width: Int,
        height: Int,
        rotationMode: Int
    )

    external fun convertToI420(
        y: ByteBuffer?,
        u: ByteBuffer?,
        v: ByteBuffer?,
        yStride: Int,
        uStride: Int,
        vStride: Int,
        srcPixelStrideUv: Int,
        yOut: ByteBuffer?,
        uOut: ByteBuffer?,
        vOut: ByteBuffer?,
        yOutStride: Int,
        uOutStride: Int,
        vOutStride: Int,
        width: Int,
        height: Int
    )


    external fun convertToI420WithDegree(
        y: ByteBuffer?,
        u: ByteBuffer?,
        v: ByteBuffer?,
        yStride: Int,
        uStride: Int,
        vStride: Int,
        srcPixelStrideUv: Int,
        yOut: ByteBuffer?,
        uOut: ByteBuffer?,
        vOut: ByteBuffer?,
        yOutStride: Int,
        uOutStride: Int,
        vOutStride: Int,
        width: Int,
        height: Int,
        degree: Int
    )
    external fun convertToNV21WithDegree(
        y: ByteBuffer?,
        u: ByteBuffer?,
        v: ByteBuffer?,
        yStride: Int,
        uStride: Int,
        vStride: Int,
        srcPixelStrideUv: Int,
        yOut: ByteBuffer?,
        uOut: ByteBuffer?,
        vOut: ByteBuffer?,
        yOutStride: Int,
        uOutStride: Int,
        vOutStride: Int,
        width: Int,
        height: Int,
        degree: Int
    )
    external fun I420ToNV12(i420Src: ByteArray?, width: Int, height: Int, nv12Dst: ByteArray?)
}