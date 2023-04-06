package cn.aisaka.androidffmpeg

import android.media.MediaCodec
import android.media.MediaCodec.*
import android.media.MediaCodecInfo
import android.media.MediaCodecList
import android.media.MediaFormat
import android.os.Bundle
import android.os.Handler
import android.os.Message
import android.os.SystemClock
import android.util.Log
import android.util.Size
import androidx.appcompat.app.AppCompatActivity
import androidx.camera.core.CameraSelector
import androidx.camera.core.ImageAnalysis
import androidx.camera.core.Preview
import androidx.camera.lifecycle.ProcessCameraProvider
import androidx.core.content.ContextCompat
import cn.aisaka.androidffmpeg.databinding.ActivityMainBinding
import cn.aisaka.rtmp_hardcodec.*
import java.io.File
import java.io.FileOutputStream
import java.io.OutputStream
import java.util.*
import java.util.concurrent.Executors

class RTMPActivity : AppCompatActivity() {
    private val CSD0 = "csd-0"
    private val CSD1 = "csd-1"
    val TAG = "RTMPActivity"
    val TIMEOUT_USEC = 10000L
    private lateinit var binding: ActivityMainBinding

    var sps: ByteArray? = null
    var pps: ByteArray? = null

    val width = 1080
    val height = 1920
    private lateinit var mediacodc: MediaCodec
    private var prevOutputPTSUs = 0L
    lateinit var output: OutputStream
    lateinit var file: File
    var offset = 0
    var handler = object : Handler() {
        override fun handleMessage(msg: Message) {
            super.handleMessage(msg)
            val avgFps = msg.obj.toString()
            binding.tvStartPush.setText("fps : ${avgFps}")
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)
        file = File(
            getExternalFilesDir(null),
            "frame22.yuv"
        )
        output = FileOutputStream(file)
        initMediaCodec()
        initCamera()
        Log.d(TAG, "${Thread.currentThread().toString()}")

        RTMPHandle.setListener(object : OnRTMPStateChangeListener {
            override fun onCreate() {
                Log.d(TAG, "RTMPHandle onCreate")
            }

            override fun onConnect() {
                Log.d(TAG, "RTMPHandle onConnect ${Thread.currentThread().toString()}")
            }

            override fun onDisconnect() {
                Log.d(TAG, "RTMPHandle onDisconnect")
            }


            override fun onMessageReceive() {
                Log.d(TAG, "RTMPHandle onMessageReceive")
            }

            override fun onPause() {
                Log.d(TAG, "RTMPHandle onPause")

            }

            override fun onResume() {
                Log.d(TAG, "RTMPHandle onResume")

            }

            override fun onStop() {
                Log.d(TAG, "RTMPHandle onStop")

            }

            override fun onRelease() {
                Log.d(TAG, "RTMPHandle onRelease")

            }
        })
        //rtmp://172.16.103.71/live/livestream
        RTMPHandle.initPush("rtmp://172.16.103.71/live/livestream")

        Log.e("RTMPActivity", "rtmp version = ${RTMPHandle.getVersion()}")
    }


    fun initCamera() {
        val cameraProviderFuture = ProcessCameraProvider.getInstance(this)
        cameraProviderFuture.addListener(Runnable {

            val cameraProvider = cameraProviderFuture.get()

            val preview = Preview.Builder().setTargetResolution(Size(width, height))
                .build()

            preview.also { it.setSurfaceProvider(binding.textureView.surfaceProvider) }
            val cameraSelector = CameraSelector.DEFAULT_BACK_CAMERA
            val imageAnalysis = ImageAnalysis.Builder()
                .setTargetResolution(Size(width, height))
                .setOutputImageFormat(ImageAnalysis.OUTPUT_IMAGE_FORMAT_YUV_420_888).build()

            var count = 0
            var inputFrameCount = 0
            var outputFrameCount = 0
            imageAnalysis.setAnalyzer(
                Executors.newSingleThreadExecutor()
            ) { image ->
                val startTime = System.currentTimeMillis()
                val array =
                    YuvHelper.convertToI420WithDegree(image.image!!, image.imageInfo.rotationDegrees)
                val nv12Array=ByteArray(array.size())

                YuvHelper.I420ToNV12(array.asArray(),width,height,nv12Array)

                if (count == 0) {
                    saveYUVFile(nv12Array)
                }

                val inputBufId = mediacodc.dequeueInputBuffer(TIMEOUT_USEC)

                if (inputBufId >= 0) {
                    val inByteBuf = mediacodc.getInputBuffer(inputBufId)

                    inByteBuf!!.put(
                        nv12Array
                    )

                    mediacodc.queueInputBuffer(
                        inputBufId,
                        0,
                        image.width * image.height * 3 / 2,
                        computePresentationTime(inputFrameCount),
                        BUFFER_FLAG_CODEC_CONFIG
                    )
                    inputFrameCount++
                }


                val bufferInfo = BufferInfo()
                var outBufId = mediacodc.dequeueOutputBuffer(bufferInfo, TIMEOUT_USEC)
                while (outBufId >= 0) {


                    val outputBuffer = mediacodc.getOutputBuffer(outBufId)
                    val outData = ByteArray(bufferInfo.size)
                    outputBuffer!!.get(outData)
                    bufferInfo.presentationTimeUs = computePresentationTime(outputFrameCount)
                    // flags 利用位操作，定义的 flag 都是 2 的倍数
                    if (bufferInfo.flags and BUFFER_FLAG_CODEC_CONFIG != 0) { // 配置相关的内容，也就是 SPS，PPS
                        Log.e("ImageAnalysis", "SPS，PPS ${bufferInfo.presentationTimeUs}  ${outBufId}")

                        // sps pps
                        // sps pps
//                        val outputFormat: MediaFormat = mediacodc.getOutputFormat()
//                        val csd0 = outputFormat.getByteBuffer("csd-0")
//                        val csd1 = outputFormat.getByteBuffer("csd-1")
//                        if (csd1 != null) {
//                            sps = ByteArray(csd0!!.remaining())
//                            pps = ByteArray(csd1.remaining())
//                            csd0[sps!!, 0, csd0.remaining()]
//                            csd1[pps!!, 0, csd1.remaining()]
//                        }

//                        RTMPHandle.writeSPSPPSData(sps!!,pps!!,bufferInfo.presentationTimeUs/1000,0)
                        RTMPHandle.writeVideoData(outData, bufferInfo.presentationTimeUs/1000, 0)
                    } else if (bufferInfo.flags and BUFFER_FLAG_KEY_FRAME != 0) { // 关键帧
                        Log.e("ImageAnalysis", "关键帧 ${bufferInfo.flags} ${bufferInfo.presentationTimeUs}  ${outBufId}")
                        RTMPHandle.writeVideoData(outData, bufferInfo.presentationTimeUs/1000, 1)
                        count++
                        outputFrameCount++
                    } else {
                        // 非关键帧和SPS、PPS,直接写入文件，可能是B帧或者P帧
                        Log.e(
                            "ImageAnalysis",
                            "非关键帧和SPS、PPS,直接写入文件，可能是B帧或者P帧 ${bufferInfo.presentationTimeUs}  ${outBufId}"
                        )
                        RTMPHandle.writeVideoData(outData, bufferInfo.presentationTimeUs/1000, 2)
                        count++
                        outputFrameCount++
                    }
                    val pts = calcPTS()
                    if (pts > 0) {
                        val avgFps = count * 1.0 / pts * 1000
                        var msg = handler.obtainMessage()
                        msg.what = 0
                        msg.obj = avgFps
                        handler.sendMessage(msg)
                    }

                    mediacodc.releaseOutputBuffer(outBufId, false)
                    outBufId =
                        mediacodc.dequeueOutputBuffer(bufferInfo, TIMEOUT_USEC)
                }
                image.close()
                Log.e("ImageAnalysis", "耗时 ${System.currentTimeMillis() - startTime}")
            }
            try {
                cameraProvider.unbindAll()
                cameraProvider.bindToLifecycle(this, cameraSelector, preview, imageAnalysis)
            } catch (e: Exception) {
                Log.e("HHH", e.toString())
            }

        }, ContextCompat.getMainExecutor(this))
    }

    fun initMediaCodec() {
        val mime = MediaFormat.MIMETYPE_VIDEO_AVC
        mediacodc = MediaCodec.createEncoderByType(mime)
        val mediaCodecInfo = getOptimalFormat(mime)
        val mediaFormat = MediaFormat.createVideoFormat(mime, width, height)

        // 像素格式不同也会导致画面模糊的问题？
        mediaFormat.setInteger(
            MediaFormat.KEY_COLOR_FORMAT,
            MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420SemiPlanar
        )

        mediaFormat.setInteger(MediaFormat.KEY_BIT_RATE, 2000000)

//        mediaCodecInfo?.let {
//            mediaFormat.setInteger(
//                MediaFormat.KEY_QUALITY,
//                it.encoderCapabilities.qualityRange.upper
//            )
//            if (it.encoderCapabilities.isBitrateModeSupported(MediaCodecInfo.EncoderCapabilities.BITRATE_MODE_CQ)) {
//                // 调整码率的控流模式
//                mediaFormat.setInteger(
//                    MediaFormat.KEY_BITRATE_MODE,
//                    MediaCodecInfo.EncoderCapabilities.BITRATE_MODE_CQ
//                )
//            } else if (it.encoderCapabilities.isBitrateModeSupported(MediaCodecInfo.EncoderCapabilities.BITRATE_MODE_CBR)) {
//                mediaFormat.setInteger(
//                    MediaFormat.KEY_BITRATE_MODE,
//                    MediaCodecInfo.EncoderCapabilities.BITRATE_MODE_CBR
//                )
//            }
//        }
        val codecProfileLevels: Array<MediaCodecInfo.CodecProfileLevel> = mediaCodecInfo!!.profileLevels
        var leve = -1
        var profile = -1
        if (null != codecProfileLevels) {
            val highProfileLevelList =
                Collections.synchronizedList(LinkedList<MediaCodecInfo.CodecProfileLevel>())
            for (profileLevel in codecProfileLevels) {
                if (MediaCodecInfo.CodecProfileLevel.AVCProfileHigh == profileLevel.profile) {
                    highProfileLevelList.add(profileLevel)
                }
            }
            if (!highProfileLevelList.isEmpty()) {
                Collections.sort(
                    highProfileLevelList
                ) { o1: MediaCodecInfo.CodecProfileLevel, o2: MediaCodecInfo.CodecProfileLevel -> o2.profile - o1.profile + o2.level - o1.level }
                leve = highProfileLevelList[0].level
                profile = highProfileLevelList[0].profile
            }
        }
        /**There are improvements here, but there are no qualitative changes**/
        /**There are improvements here, but there are no qualitative changes */
        if (-1 != leve) {
            mediaFormat.setInteger(MediaFormat.KEY_LEVEL, leve)
        }
        if (-1 != profile) {
            mediaFormat.setInteger(MediaFormat.KEY_PROFILE, profile)
        }


        mediaFormat.setInteger(MediaFormat.KEY_FRAME_RATE, 25)
        mediaFormat.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, 15)


        mediacodc.configure(mediaFormat, null, null, CONFIGURE_FLAG_ENCODE)
        mediacodc.start()
    }

    private fun getOptimalFormat(mime: String): MediaCodecInfo.CodecCapabilities? {
        val list = MediaCodecList(MediaCodecList.REGULAR_CODECS)
        val infos = list.codecInfos
        var cap: MediaCodecInfo.CodecCapabilities? = null
        run {
            var i = 0
            val count = infos.size
            while (i < count) {
                val info = infos[i]
                if (!info.isEncoder) {
                    i++
                    continue
                }
                val types = info.supportedTypes
                var j = 0
                val jCount = types.size
                while (j < jCount) {
                    val type = types[j]
                    if (type == mime) {
                        cap = info.getCapabilitiesForType(mime)
                        return cap!!
                    }
                    j++
                }
                i++
            }
        }
        return null
    }

    fun calcPTS(): Long {
        if (prevOutputPTSUs <= 0) {
            prevOutputPTSUs = SystemClock.uptimeMillis()
        }
        val pts = (SystemClock.uptimeMillis() - prevOutputPTSUs)
        Log.e("HHH", "${pts}")
        return pts
    }

    /**
     * 时间戳计算十分重要 如果顺序不连贯，那么编码会有问题，比如画面模糊，马赛克
     * MediaCodec.queueInputBuffer 是输入数据的顺序 每一帧都是累加差值且单位是微秒
     *
     * MediaCodec.dequeueOutputBuffer 是输出数据的顺序 每一帧都是累加差值且单位是微秒
     */
    private fun computePresentationTime(frameIndex: Int): Long {
        return 132 + frameIndex * 1000000 / 25L
    }

    fun saveYUVFile(i420Frame: ByteArray) {
        val length = i420Frame.size
        output.write(i420Frame, 0, length)
        offset += length
    }
}