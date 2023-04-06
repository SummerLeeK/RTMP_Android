package cn.aisaka.rtmp_hardcodec

import android.media.Image
import java.nio.ByteBuffer


object YuvHelper {
    private var mYuvFrame: YuvFrame? = null
    private var y: ByteBuffer? = null
    private var u: ByteBuffer? = null
    private var v: ByteBuffer? = null

    init {
        System.loadLibrary("RTMP")
    }
    fun createYuvFrame(width: Int, height: Int): YuvFrame {
        if (null == mYuvFrame || mYuvFrame!!.width != width || mYuvFrame!!.height != height) {
            mYuvFrame = YuvFrame()
            v = null
            u = v
            y = u
        }
        val ySize = width * height
        val uvSize = width * height / 4
        if (null == y) {
            y = ByteBuffer.allocateDirect(ySize)
        } else {
            y!!.clear()
        }
        if (null == u) {
            u = ByteBuffer.allocateDirect(uvSize)
        } else {
            u!!.clear()
        }
        if (null == v) {
            v = ByteBuffer.allocateDirect(uvSize)
        } else {
            v!!.clear()
        }
        val extra = if (width % 2 == 0) 0 else 1
        mYuvFrame!!.fill(y, u, v, width, width / 2 + extra, width / 2 + extra, width, height)
        return mYuvFrame!!
    }

    fun createYuvFrame(width: Int, height: Int, rotationMode: Int): YuvFrame {
        val outWidth = if (rotationMode == 90 || rotationMode == 270) height else width
        val outHeight = if (rotationMode == 90 || rotationMode == 270) width else height
        return createYuvFrame(outWidth, outHeight)
    }

    fun rotate(image: Image, rotationMode: Int): YuvFrame {
        assert(rotationMode == 0 || rotationMode == 90 || rotationMode == 180 || rotationMode == 270)
        val outFrame = createYuvFrame(image.width, image.height, rotationMode)
        RTMPHandle.rotate(
            image.planes[0].buffer,
            image.planes[1].buffer,
            image.planes[2].buffer,
            image.planes[0].rowStride,
            image.planes[1].rowStride,
            image.planes[2].rowStride,
            outFrame!!.y,
            outFrame.u,
            outFrame.v,
            outFrame.getyStride(),
            outFrame.getuStride(),
            outFrame.getvStride(),
            image.width,
            image.height,
            rotationMode
        )
        return outFrame
    }

    fun rotate(yuvFrame: YuvFrame, rotationMode: Int): YuvFrame {
        assert(rotationMode == 0 || rotationMode == 90 || rotationMode == 180 || rotationMode == 270)
        val outFrame = createYuvFrame(yuvFrame.width, yuvFrame.height, rotationMode)
        RTMPHandle.rotate(
            yuvFrame.y,
            yuvFrame.u,
            yuvFrame.v,
            yuvFrame.getyStride(),
            yuvFrame.getuStride(),
            yuvFrame.getvStride(),
            outFrame!!.y,
            outFrame.u,
            outFrame.v,
            outFrame.getyStride(),
            outFrame.getuStride(),
            outFrame.getvStride(),
            yuvFrame.width,
            yuvFrame.height,
            rotationMode
        )
        return outFrame
    }

    fun convertToI420(image: Image): YuvFrame {
        val outFrame = createYuvFrame(image.width, image.height)
        RTMPHandle.convertToI420(
            image.planes[0].buffer,
            image.planes[1].buffer,
            image.planes[2].buffer,
            image.planes[0].rowStride,
            image.planes[1].rowStride,
            image.planes[2].rowStride,
            image.planes[2].pixelStride,
            outFrame!!.y,
            outFrame.u,
            outFrame.v,
            outFrame.getyStride(),
            outFrame.getuStride(),
            outFrame.getvStride(),
            image.width,
            image.height
        )
        return outFrame
    }

    fun convertToI420(yuvFrame: YuvFrame, uvPixelStride: Int): YuvFrame {
        val outFrame = createYuvFrame(yuvFrame.width, yuvFrame.height)
        RTMPHandle.convertToI420(
            yuvFrame.y,
            yuvFrame.u,
            yuvFrame.v,
            yuvFrame.getyStride(),
            yuvFrame.getuStride(),
            yuvFrame.getvStride(),
            uvPixelStride,
            outFrame.y,
            outFrame.u,
            outFrame.v,
            outFrame.getyStride(),
            outFrame.getuStride(),
            outFrame.getvStride(),
            yuvFrame.width,
            yuvFrame.height
        )
        return outFrame
    }

    fun convertToI420WithDegree(image: Image,degree:Int): YuvFrame {
        val outFrame = createYuvFrame(image.width,image.height,degree)
        RTMPHandle.convertToI420WithDegree(
            image.planes[0].buffer,
            image.planes[1].buffer,
            image.planes[2].buffer,
            image.planes[0].rowStride,
            image.planes[1].rowStride,
            image.planes[2].rowStride,
            image.planes[2].pixelStride,
            outFrame.y,
            outFrame.u,
            outFrame.v,
            outFrame.getyStride(),
            outFrame.getuStride(),
            outFrame.getvStride(),
            image.width,image.height,degree
        )
        return outFrame
    }

    fun I420ToNV12(i420Src: ByteArray?, width: Int, height: Int, nv12Dst: ByteArray){
        RTMPHandle.I420ToNV12(
            i420Src, width, height, nv12Dst
        )
    }

}