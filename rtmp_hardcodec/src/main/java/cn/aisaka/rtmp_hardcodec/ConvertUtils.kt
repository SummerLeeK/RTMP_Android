package cn.aisaka.rtmp_hardcodec

import android.annotation.SuppressLint
import androidx.camera.core.ImageProxy

import java.io.File
import java.io.FileNotFoundException
import java.io.FileOutputStream
import java.io.IOException
import java.nio.ByteBuffer


object ConvertUtils {
    private const val TAG = "ConvertUtils"
    private var mPortraitYUV: ByteArray?=null
    private var mLandscapeYUV: ByteArray?=null
    fun YUV_420_888toNV12(image: ImageProxy, rotation: Int): ByteArray? {
        return if (0 == rotation) {
            YUV_420_888toPortraitNV12(image, rotation)
        } else {
            YUV_420_888toLandscapeNV12(image, rotation)
        }
    }

    /**
     * YUV_420_888 to Landscape NV21
     *
     * @param image CameraX ImageProxy
     * @return nv12 byte array
     */
    @SuppressLint("UnsafeOptInUsageError")
    fun YUV_420_888toLandscapeNV12(image: ImageProxy, rotation: Int): ByteArray? {
        val bytes: ByteArray = YuvHelper.convertToI420(image.getImage()!!).asArray()
        if (null == mLandscapeYUV || mLandscapeYUV!!.size != bytes.size) {
            mLandscapeYUV = ByteArray(bytes.size)
        }
        RTMPHandle.I420ToNV12(bytes, image.getWidth(), image.getHeight(), mLandscapeYUV)
        return mLandscapeYUV
    }

    /**
     * YUV_420_888 to Portrait NV12
     * @param image CameraX ImageProxy
     * @param rotation display rotation
     * @return nv12 byte array
     */
    @SuppressLint("UnsafeOptInUsageError")
    fun YUV_420_888toPortraitNV12(image: ImageProxy, rotation: Int): ByteArray? {
        val yuvFrame: YuvFrame = YuvHelper.convertToI420(image.getImage()!!)
        //TODO optimization of vertical screen libyuv rotation
        val bytes: ByteArray =
            YuvHelper.rotate(yuvFrame, image.getImageInfo().getRotationDegrees()).asArray()
        if (null == mPortraitYUV || mPortraitYUV!!.size != bytes.size) {
            mPortraitYUV = ByteArray(bytes.size)
        }
        RTMPHandle.I420ToNV12(bytes, image.getWidth(), image.getHeight(), mPortraitYUV)
        return mPortraitYUV
    }

    private fun writeFile(mImage: ImageProxy, path: String) {
        val file = File(path)
        var output: FileOutputStream? = null
        var buffer: ByteBuffer
        var bytes: ByteArray
        val prebuffer = ByteBuffer.allocate(16)
        prebuffer.putInt(mImage.getWidth())
            .putInt(mImage.getHeight())
            .putInt(mImage.getPlanes().get(1).getPixelStride())
            .putInt(mImage.getPlanes().get(1).getRowStride())
        try {
            output = FileOutputStream(file)
            output.write(prebuffer.array()) // write meta information to file
            // Now write the actual planes.
            for (i in 0..2) {
                buffer = mImage.getPlanes().get(i).getBuffer()
                bytes = ByteArray(buffer.remaining()) // makes byte array large enough to hold image
                buffer[bytes] // copies image from buffer to byte array
                output.write(bytes) // write the byte array to file
            }
        } catch (e: FileNotFoundException) {
            e.printStackTrace()
        } catch (e: IOException) {
            e.printStackTrace()
        } finally {
//            mImage.close(); // close this to free up buffer for other images
            if (null != output) {
                try {
                    output.close()
                } catch (e: IOException) {
                    e.printStackTrace()
                }
            }
        }
    }
}