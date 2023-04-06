//
// Created by lhk12 on 2023-01-13.
//

#include "libavutil/avutil.h"
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavutil/imgutils.h"
#include "android/log.h"
#include "jni.h"
#include "pthread.h"
#include "libavutil/opt.h"
#include "libyuv/rotate.h"

#define  LOG_TAG    "FFmpeg"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define  LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)


#define QUEUE_SIZE 50
typedef struct Packet {
    AVPacket *packet;
    double pts;           /* presentation timestamp for the frame */
    double duration;
} Packet;
typedef struct Frame {
    AVFrame *frame;
    double pts;           /* presentation timestamp for the frame */
    double duration;
} Frame;


typedef struct PacketList {
    Packet pktList[QUEUE_SIZE];
    int start;
    int end;
    int count;
} PacketList;
typedef struct FrameList {
    Frame frameList[QUEUE_SIZE];
    int start;
    int end;
    int count;
} FrameList;

AVFormatContext *format;
AVStream *videoStream;
AVCodec *videoCodec;
AVCodecContext *videoCodecContext;
AVCodecParameters *videoParameters;
AVPacket *tmpPacket;

uint8_t *data;
int pts = 0;
FrameList frameList1;
AVStream *audioStream;
AVCodec *audioCodec;
AVCodecContext *audioCodecContext;
AVCodecParameters *audioCodecParams;

pthread_cond_t cond;
pthread_mutex_t mutex;
pthread_t encodeThd;


FILE *outYuv;

int initDone = 0;

int openVideoCodec() {

    int result = 0;
    int frame_rate = videoParameters->frame_size;
    videoCodecContext = avcodec_alloc_context3(videoCodec);
    videoCodecContext->codec_id = videoCodec->id;
    videoCodecContext->framerate = (AVRational) {frame_rate, 1};
    videoCodecContext->time_base = (AVRational) {1, frame_rate};
    videoCodecContext->pix_fmt = AV_PIX_FMT_YUV420P;
    videoCodecContext->gop_size = 1;
    videoCodecContext->max_b_frames = 0;
    videoCodecContext->codec_type = AVMEDIA_TYPE_VIDEO;

    videoCodecContext->mb_lmin = 1;
    videoCodecContext->mb_lmax = 50;
    result = avcodec_parameters_to_context(videoCodecContext, videoParameters);
/* Some formats want stream headers to be separate. */
    if (format->oformat->flags & AVFMT_GLOBALHEADER)
        videoCodecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    if (result < 0) {
        LOGE("%s:%s", "avcodec_parameters_to_context",
             av_err2str(result));
        return result;
    }
    AVDictionary *param = 0;
    if (videoCodec->id == AV_CODEC_ID_H264) {
//        av_dict_set(&param, "preset", "slow", 0);
        /**
         * 这个非常重要，如果不设置延时非常的大
         * ultrafast,superfast, veryfast, faster, fast, medium
         * slow, slower, veryslow, placebo.　这是x264编码速度的选项
       */
//        av_opt_set(videoCodecContext->priv_data, "preset", "ultrafast", 0);
        av_dict_set(&param, "preset", "ultrafast", 0);
        av_dict_set(&param, "tune", "zerolatency", 0);
        av_dict_set(&param, "profile", "baseline", 0);


    }

//    if (videoCodec->id == AV_CODEC_ID_H264) {
//        av_opt_set(videoCodecContext->priv_data, "preset", "ultrafast", 0);
//        av_opt_set(videoCodecContext->priv_data, "tune", "zerolatency", 0);
//        av_opt_set(videoCodecContext->priv_data, "profile", "high", 0);
//    }

    result = avcodec_open2(videoCodecContext, videoCodec, &param);
    if (result < 0) {
        LOGE("%s:%s", "avcodec_open2",
             av_err2str(result));
        return result;
    }

    LOGI("%s:%s", "video avcodec_open2",
         "success");
    videoStream = avformat_new_stream(format, videoCodec);

    videoStream->codecpar->codec_tag = 0;
    LOGI("%s:%s:%d", "video avformat_new_stream",
         "success", format->nb_streams);
    avcodec_parameters_from_context(videoStream->codecpar, videoCodecContext);

    result = avio_open(&format->pb, format->url, AVIO_FLAG_READ_WRITE);

    if (result < 0) {
        LOGE("avio_open failed %s\n", format->url);
        return -1;
    }

    AVDictionary *options = NULL;
//    av_dict_set(&options, "rtmp_buffer", "0", 0);
    result = avformat_write_header(format, NULL);

    if (result < 0) {
        LOGE("avformat_write_header failed %s:%d\n", av_err2str(result), result);
        return -1;
    }
    if (!videoStream) {
        LOGE("%s:%s", "video avformat_new_stream ",
             "failed");
        return -1;
    }
    LOGI("%s:%s", "video avformat_new_stream",
         "success");


    return result;
}


int videoEncode() {
    Frame frame;
    pthread_mutex_lock(&mutex);

    if (frameList1.count <= 0) {
        pthread_cond_wait(&cond, &mutex);
    }

    frame = frameList1.frameList[frameList1.start];
    frameList1.start = (frameList1.start + 1) % QUEUE_SIZE;
    frameList1.count -= 1;
    LOGI("%s:%d:%d",
         "pop Video", frameList1.count, frameList1.start);
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex);
    tmpPacket = av_packet_alloc();

    int result = avcodec_send_frame(videoCodecContext, frame.frame);
    while (result >= 0) {

        result = avcodec_receive_packet(videoCodecContext, tmpPacket);
        if (tmpPacket->size <= 0) {
            av_frame_unref(frame.frame);
            av_packet_unref(tmpPacket);
            return -1;
        }
        if (result == 0) {
            tmpPacket->pos = -1;
            tmpPacket->stream_index = 0;
            tmpPacket->pts = tmpPacket->pts * format->streams[0]->time_base.den /
                             videoCodecContext->time_base.den;
            tmpPacket->dts = tmpPacket->pts;
            tmpPacket->duration =
                    format->streams[0]->time_base.den / videoCodecContext->time_base.den;
            time_t now = time(NULL);
            LOGD("%s:%ld:%ld:%ld time=%ld", "in av_interleaved_write_frame", tmpPacket->pts,
                 tmpPacket->dts,
                 tmpPacket->duration, now);
            result = av_interleaved_write_frame(format, tmpPacket);
            if (result != 0) {
                LOGE("%s:%s:%d",
                     "av_write_frame failure", av_err2str(result), result);
            } else {
                LOGE("%s:%s",
                     "av_write_frame ", "success");
            }
        } else if (result == AVERROR(EINVAL)) {
            LOGE("%s:%s",
                 "avcodec_receive_packet failure", "codec not opened, or it is an encoder");
        } else if (result == AVERROR(EAGAIN)) {
//            LOGE("%s:%s",
//                 "avcodec_receive_packet failure",
//                 "output is not available in the current state - user must try to send input");
        } else {
            LOGE("%s:%s",
                 "avcodec_receive_packet failure", av_err2str(result));
        }
    }
    av_frame_unref(frame.frame);
    av_packet_unref(tmpPacket);
    return 0;
}

void ffmpegLogCallback(void *avcl, int level, const char *fmt, va_list vl) {
    if (fmt != NULL) {
        // 将FFmpeg输出的日志转换为UTF-8编码
//        char utf8_line[1024];
//        av_log_format_line2(avcl, level, fmt, vl, utf8_line, sizeof(utf8_line),
//                            AV_LOG_SKIP_REPEATED);

        LOGD(fmt, vl);
    }
}

void *encodeThread(void *obj) {

    while (1) {
        videoEncode();
    }
}

void initThread() {

    frameList1.start = 0;
    frameList1.end = 0;
    frameList1.count = 0;
    int result = pthread_create(&encodeThd, NULL, &encodeThread, NULL);
    pthread_cond_init(&cond, NULL);
    pthread_mutex_init(&mutex, NULL);

}

int openAudioCodec() {
}


JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    LOGI("JNI_OnLoad called");
    JNIEnv *env = NULL; //注册时在JNIEnv中实现的，所以必须首先获取它
    if ((*vm)->GetEnv(vm, (void **) &env, JNI_VERSION_1_4) != JNI_OK) { //从JavaVM获取JNIEnv，一般使用1.4的版本
        return -1;
    }
    return JNI_VERSION_1_4; //这里很重要，必须返回版本，否则加载会失败。
}

JNIEXPORT void JNI_OnUnload(JavaVM *vm, void *reserved) {
    LOGI("JNI_OnUnload called");

}


JNIEXPORT jstring JNICALL
Java_cn_aisaka_ffmpeg_test_FFmpegNative_getVersion(JNIEnv *env, jobject thiz) {
    const char *versionInfo = av_version_info();
    void *iter = NULL;
    for (;;) {
        const AVCodec *cur = av_codec_iterate(&iter);
        if (!cur)
            break;
        if (av_codec_is_encoder(cur) && cur->type == AVMEDIA_TYPE_VIDEO) {
            LOGE("encoder:%s", cur->name);
        }
        if (av_codec_is_decoder(cur) && cur->type == AVMEDIA_TYPE_VIDEO) {
            LOGI("decoder:%s", cur->name);
        }
    }
    return (*env)->NewStringUTF(env, versionInfo);
}

JNIEXPORT jint JNICALL
Java_cn_aisaka_ffmpeg_test_FFmpegNative_initPushUrl(JNIEnv *env, jobject thiz, jstring url,
                                                    jint codec_type) {
    int result = 0;
    result = avformat_network_init();
    if (result < 0) {
        LOGE("%s:%s", "initPushUrl avformat_network_init failed", av_err2str(result));
        return result;
    }
    format = avformat_alloc_context();
    const char *fileName = (*env)->GetStringUTFChars(env, url, NULL);
    result = avformat_alloc_output_context2(&format, NULL, "flv", fileName);
    if (result < 0) {
        LOGE("%s:%s", "initPushUrl avformat_alloc_output_context2 failed", av_err2str(result));
        return result;
    }
    (*env)->ReleaseStringUTFChars(env, url, fileName);

    // 设置缓冲时间
    av_opt_set_int(format, "max_delay", 0, AV_OPT_SEARCH_CHILDREN);
    // 设置rtmp_buffer_size
    av_log_set_callback(ffmpegLogCallback);
    openVideoCodec();
    initThread();
    initDone = 1;
    return 0;
}


JNIEXPORT jint JNICALL
Java_cn_aisaka_ffmpeg_test_FFmpegNative_initVideoParameters(JNIEnv *env, jobject thiz, jint width,
                                                            jint height, jint frame_rate,
                                                            jint bitrate,
                                                            jstring specCodecName) {
    __android_log_print(ANDROID_LOG_DEBUG, "FFmpeg", "%d:%d:%d:%d:%s", width, height, frame_rate,
                        bitrate, (*env)->GetStringUTFChars(env, specCodecName, NULL));
    int result = 0;
    if (specCodecName == NULL) {
        videoCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
    } else {
        const char *codeName = (*env)->GetStringUTFChars(env, specCodecName, NULL);
        videoCodec = avcodec_find_encoder_by_name(codeName);
        if (videoCodec == NULL) {
            LOGE("%s:%s",
                 "avcodec_find_encoder_by_name not found ", codeName);
        } else {
            videoCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
        }
        (*env)->ReleaseStringUTFChars(env, specCodecName, codeName);
    }
    LOGI("%s:%s", "find video codec", videoCodec->name);

    videoParameters = avcodec_parameters_alloc();
    videoParameters->bit_rate = bitrate;
    videoParameters->width = width;
    videoParameters->height = height;
    videoParameters->format = AV_PIX_FMT_YUV420P;
    videoParameters->codec_type = AVMEDIA_TYPE_VIDEO;
    videoParameters->frame_size = frame_rate;
    return result;
}

JNIEXPORT jint JNICALL
Java_cn_aisaka_ffmpeg_test_FFmpegNative_initAudioParameters(JNIEnv *env, jobject thiz,
                                                            jint channel_layout, jint sample_rate,
                                                            jint bitrate) {

    return 0;
}

JNIEXPORT jint JNICALL
Java_cn_aisaka_ffmpeg_test_FFmpegNative_writeVideo(JNIEnv *env, jobject thiz, jobject ydata,
                                                   jint yRowStride, jobject udata, jint uRowStride,
                                                   jobject vdata, jint vRowStride) {
    if (!initDone)
        return 0;
    int result = 0;

    AVFrame *tmpFrame = av_frame_alloc();

    tmpFrame->format = AV_PIX_FMT_YUV420P;
    tmpFrame->width = videoCodecContext->width;
    tmpFrame->height = videoCodecContext->height;


    int yuvlength = videoCodecContext->width * videoCodecContext->height;
    int ulength = yuvlength / 4;
    int vlength = yuvlength / 4;
    av_frame_get_buffer(tmpFrame, 0);

    result = av_frame_make_writable(tmpFrame);

    if (result != 0) {
        LOGI("%s", "av_frame_make_writable failure");
    }

    uint8_t *yNative = (uint8_t *) (*env)->GetDirectBufferAddress(env, ydata);
    uint8_t *uNative = (uint8_t *) (*env)->GetDirectBufferAddress(env, udata);
    uint8_t *vNative = (uint8_t *) (*env)->GetDirectBufferAddress(env, vdata);

    memcpy(tmpFrame->data[0], (const uint8_t *) yNative, yuvlength);
    memcpy(tmpFrame->data[1], (const uint8_t *) uNative, ulength);
    memcpy(tmpFrame->data[2], (const uint8_t *) vNative, vlength);


    tmpFrame->pts = pts;
    pts++;
    pthread_mutex_lock(&mutex);

    if (frameList1.count >= QUEUE_SIZE) {
        pthread_cond_wait(&cond, &mutex);
    }
    Frame frame = {tmpFrame, pts, av_q2d(videoCodecContext->time_base)};
    int index = frameList1.end;
    frameList1.frameList[index] = frame;
    frameList1.end = (index += 1) % QUEUE_SIZE;
    frameList1.count += 1;
    LOGI("%s:%d:%d",
         "push Video", frameList1.count, frameList1.end);
    pthread_cond_signal(&cond);

    pthread_mutex_unlock(&mutex);


    return 0;
}

JNIEXPORT jint JNICALL
Java_cn_aisaka_ffmpeg_test_FFmpegNative_writeVideo2(JNIEnv *env, jobject thiz, jobject y, jobject u,
                                                    jobject v, jint y_stride, jint u_stride,
                                                    jint v_stride, jint src_pixel_stride_uv,
                                                    jint width, jint height,
                                                    jint degree, jstring path) {

    if (!initDone)
        return 0;
    if (outYuv == NULL) {
        const char *filePath = (*env)->GetStringUTFChars(env, path, NULL);

        outYuv = fopen(filePath, "w+");

        (*env)->ReleaseStringUTFChars(env, path, filePath);
    }

    int result = 0;

    AVFrame *tmpFrame = av_frame_alloc();

    tmpFrame->format = AV_PIX_FMT_YUV420P;
    tmpFrame->width = videoCodecContext->width;
    tmpFrame->height = videoCodecContext->height;


    int yuvlength = videoCodecContext->width * videoCodecContext->height;
    int ulength = yuvlength / 4;
    int vlength = yuvlength / 4;
    // align参数不对会有字节对齐问题  导致AVFrame.linesize的值初始化有问题
    av_frame_get_buffer(tmpFrame, 2);

    result = av_frame_make_writable(tmpFrame);

    if (result != 0) {
        LOGI("%s", "av_frame_make_writable failure");
    }


    uint8_t *yNative = (uint8_t *) (*env)->GetDirectBufferAddress(env, y);
    uint8_t *uNative = (uint8_t *) (*env)->GetDirectBufferAddress(env, u);
    uint8_t *vNative = (uint8_t *) (*env)->GetDirectBufferAddress(env, v);

    uint8_t *yOutNative = tmpFrame->data[0];
    uint8_t *uOutNative = tmpFrame->data[1];
    uint8_t *vOutNative = tmpFrame->data[2];
    RotationModeEnum mode = kRotate0;
    int y_out_stride, u_out_stride, v_out_stride;
    switch (degree) {
        case 0:
            mode = kRotate0;
            break;
        case 90:
            mode = kRotate90;
            break;
        case 180:
            mode = kRotate180;
            break;
        case 270:
            mode = kRotate270;
            break;
    }


    if (mode == kRotate0 || mode == kRotate180) {
        y_out_stride = width;
    } else {
        y_out_stride = height;
    }
    u_out_stride = y_out_stride / 2;
    v_out_stride = y_out_stride / 2;
    result = Android420ToI420Rotate(yNative, y_stride,
                                    uNative, u_stride,
                                    vNative, v_stride,
                                    src_pixel_stride_uv,
                                    yOutNative, y_out_stride,
                                    uOutNative, u_out_stride,
                                    vOutNative, v_out_stride,
                                    width, height, mode);


//    tmpFrame->linesize[0]=y_out_stride;
//    tmpFrame->linesize[1]=u_out_stride;
//    tmpFrame->linesize[2]=v_out_stride;
    fwrite(yOutNative, yuvlength, 1, outYuv);
    fwrite(uOutNative, ulength, 1, outYuv);
    fwrite(vOutNative, vlength, 1, outYuv);


    if (result < 0) {
        LOGE("libyuv %s", " Android420ToI420Rotate failure");
    }

    tmpFrame->pts = pts;
    pts++;
    pthread_mutex_lock(&mutex);

    if (frameList1.count >= QUEUE_SIZE) {
        pthread_cond_wait(&cond, &mutex);
    }
    Frame frame = {tmpFrame, pts, av_q2d(videoCodecContext->time_base)};
    int index = frameList1.end;
    frameList1.frameList[index] = frame;
    frameList1.end = (index += 1) % QUEUE_SIZE;
    frameList1.count += 1;
    time_t now = time(NULL);
    LOGI("%s:%d:%d time=%ld",
         "push Video", frameList1.count, frameList1.end, now);
    pthread_cond_signal(&cond);

    pthread_mutex_unlock(&mutex);


    return 0;
}