package cn.aisaka.rtmp_hardcodec

import android.media.Image
import java.nio.ByteBuffer

class YuvFrame {
    var y: ByteBuffer? = null
        private set
    var u: ByteBuffer? = null
        private set
    var v: ByteBuffer? = null
        private set
    private var yStride = 0
    private var uStride = 0
    private var vStride = 0
    var width = 0
        private set
    var height = 0
        private set

    fun fill(
        y: ByteBuffer?,
        u: ByteBuffer?,
        v: ByteBuffer?,
        yStride: Int,
        uStride: Int,
        vStride: Int,
        width: Int,
        height: Int
    ) {
        this.y = y
        this.u = u
        this.v = v
        this.yStride = yStride
        this.uStride = uStride
        this.vStride = vStride
        this.width = width
        this.height = height
    }

    fun fill(image: Image) {
        val planes = image.planes
        val yPlane = planes[0]
        y = yPlane.buffer
        yStride = yPlane.rowStride
        val uPlane = planes[1]
        u = uPlane.buffer
        uStride = uPlane.rowStride
        val vPlane = planes[2]
        v = vPlane.buffer
        vStride = vPlane.rowStride
        width = image.width
        height = image.height
    }

    fun getyStride(): Int {
        return yStride
    }

    fun getuStride(): Int {
        return uStride
    }

    fun getvStride(): Int {
        return vStride
    }

    fun asArray(): ByteArray {
        var array: ByteArray
        val yPos = y!!.position()
        val uPos = u!!.position()
        val vPos = v!!.position()
        try {
            array =
                ByteBuffer.allocate(y!!.capacity() + u!!.capacity() + v!!.capacity()).put(y).put(u)
                    .put(v).array()
            y!!.position(yPos)
            u!!.position(uPos)
            v!!.position(vPos)
        } catch (e: Exception) {
            array = ByteArray(size())
            y!![array, 0, y!!.remaining()]
            y!!.position(yPos)
            u!![array, y!!.remaining(), u!!.remaining()]
            u!!.position(uPos)
            v!![array, y!!.remaining() + u!!.remaining(), v!!.remaining()]
            v!!.position(vPos)
        }
        return array
    }

    fun size(): Int {
        return y!!.remaining() + u!!.remaining() + v!!.remaining()
    }

    fun free() {
        y = ByteBuffer.allocate(1)
        u = ByteBuffer.allocate(1)
        v = ByteBuffer.allocate(1)
        yStride = 0
        uStride = 0
        vStride = 0
        width = 0
        height = 0
    }
}