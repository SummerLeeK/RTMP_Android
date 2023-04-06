package cn.aisaka.androidffmpeg

import android.hardware.camera2.CaptureRequest
import android.os.Bundle
import android.util.Log
import android.util.Range
import android.util.Size
import androidx.appcompat.app.AppCompatActivity
import androidx.camera.camera2.interop.Camera2Interop
import androidx.camera.core.CameraSelector
import androidx.camera.core.ImageAnalysis
import androidx.camera.core.Preview
import androidx.camera.lifecycle.ProcessCameraProvider
import androidx.core.content.ContextCompat
import cn.aisaka.androidffmpeg.databinding.ActivityMainBinding
import cn.aisaka.ffmpeg.test.FFmpegNative
import cn.aisaka.rtmp_hardcodec.YuvFrame
import java.io.File
import java.io.FileOutputStream
import java.io.OutputStream
import java.util.concurrent.Executors


class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)
        FFmpegNative.getVersion()
        initCamera()
    }

    lateinit var output: OutputStream
    lateinit var file: File
    var offset = 0


    fun initCamera() {

        file = File(
            getExternalFilesDir(null),
            "frame.yuv"
        )
        output = FileOutputStream(file)

        FFmpegNative.initVideoParameters(720, 1280, 25, 4000000, "libx264")

//rtmp://192.168.2.179/live/livestream
//        rtmp://172.16.103.71/live/livestream
        FFmpegNative.initPushUrl("rtmp://172.16.103.71/live/livestream", 0)
        val cameraProviderFuture = ProcessCameraProvider.getInstance(this)
        cameraProviderFuture.addListener(Runnable {

            val cameraProvider = cameraProviderFuture.get()

            val preview = Preview.Builder().setTargetResolution(Size(720, 1280))
                .build()

            preview.also { it.setSurfaceProvider(binding.textureView.surfaceProvider) }
            val cameraSelector = CameraSelector.DEFAULT_BACK_CAMERA
            var builder = ImageAnalysis.Builder()
            val ext= Camera2Interop.Extender(builder)
            ext.setCaptureRequestOption(
                CaptureRequest.CONTROL_AE_TARGET_FPS_RANGE,
                Range(30, 30)
            )
            val imageAnalysis = builder
                .setTargetResolution(Size(720, 1280))
                .setOutputImageFormat(ImageAnalysis.OUTPUT_IMAGE_FORMAT_YUV_420_888)
                .build()

            var count = 0
            imageAnalysis.setAnalyzer(
                Executors.newSingleThreadExecutor(),
                ImageAnalysis.Analyzer { image ->
                    val startTime = System.currentTimeMillis()
                    count++
//                    val yuvFrame = YuvHelper.convertToI420WithDegree(
//                        image.image!!,
//                        image.imageInfo.rotationDegrees
//                    )
//
                    FFmpegNative.writeVideo2(
                        image.planes[0].buffer,
                        image.planes[1].buffer,
                        image.planes[2].buffer,
                        image.planes[0].rowStride,
                        image.planes[1].rowStride,
                        image.planes[2].rowStride,
                        image.planes[2].pixelStride,
                        image.width,
                        image.height,
                        image.imageInfo.rotationDegrees, file.absolutePath
                    )

                    val time = System.currentTimeMillis() - startTime
                    Log.e("ImageAnalysis", "耗时 ${System.currentTimeMillis() - startTime}")

                    Thread.sleep(30-time)
                    image.close()

                })
//            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
//                val cameraManager = getSystemService(Context.CAMERA_SERVICE) as CameraManager
//                cameraManager.cameraIdList.forEach {
//                    var supportedUtilSize: List<Size?>? = ArrayList()
//                    val characteristics: CameraCharacteristics =
//                        cameraManager.getCameraCharacteristics(it) // 0和1
//
//                    val configs = characteristics.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP)
//
//                    supportedUtilSize = Arrays.asList(*configs!!.getOutputSizes(ImageFormat.YUV_420_888))
//                    Log.e("HHH","${it}\t${JSON.toJSONString(supportedUtilSize)}")
//                    Log.e("HHH","fps\t${it}\t${JSON.toJSONString(configs!!.highSpeedVideoFpsRanges)}")
//
//                }
//            }
            try {
                cameraProvider.unbindAll()
                cameraProvider.bindToLifecycle(this, cameraSelector, preview, imageAnalysis)
            } catch (e: Exception) {
                Log.e("HHH", e.toString())
            }

        }, ContextCompat.getMainExecutor(this))

    }

    fun saveYUVFile(i420Frame: YuvFrame) {
        val length = i420Frame.y!!.remaining()
        output.write(i420Frame.y!!.array(), 0, length)
        offset += length
    }
}