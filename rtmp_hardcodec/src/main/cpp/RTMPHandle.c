//
// Created by lhk12 on 2023-02-16.
//
#include <string.h>
#include "jni.h"
#include "rtmp.h"
#include "libyuv.h"
#include "android/log.h"
#include "pthread.h"
#include "amf.h"
#include "log.h"

#define  LOG_TAG    "RTMP"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define  LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#define FLV_TAG_HEAD_LEN 11
#define FLV_PRE_TAG_LEN 4
static const AVal av_onMetaData = AVC("onMetaData");
static const AVal av_duration = AVC("duration");
static const AVal av_width = AVC("width");
static const AVal av_height = AVC("height");
static const AVal av_videocodecid = AVC("videocodecid");
static const AVal av_avcprofile = AVC("avcprofile");
static const AVal av_avclevel = AVC("avclevel");
static const AVal av_videoframerate = AVC("framerate");
static const AVal av_videodatarate = AVC("videodatarate");
static const AVal av_audiodatarate = AVC("audiodatarate");
static const AVal av_audiocodecid = AVC("audiocodecid");
static const AVal av_audiosamplerate = AVC("audiosamplerate");
static const AVal av_audiochannels = AVC("audiochannels");
static const AVal av_avc1 = AVC("avc1");
static const AVal av_mp4a = AVC("mp4a");
static const AVal av_onPrivateData = AVC("onPrivateData");
static const AVal av_record = AVC("record");
#define QUEUE_SIZE 60
typedef struct Frame {
    uint8_t *data;
    uint32_t length;
    uint64_t pts;           /* presentation timestamp for the frame */
    uint8_t type; //0 sps pps 1 IDR-frame 2 other-frame
    double duration;
} Frame;

typedef struct FrameList {
    Frame **frameList;
    int start;
    int end;
    int count;
} FrameList;


RTMP *rtmp;
jobject mGlobalLifecycleListener;
JavaVM *mGlobalJavaVm;

pthread_t pushThread;
pthread_mutex_t mutex;
pthread_cond_t cond;

FrameList frameList;

static uint32_t find_start_code(uint8_t *buf, uint32_t zeros_in_startcode) {
    uint32_t info;
    uint32_t i;

    info = 1;
    if ((info = (buf[zeros_in_startcode] != 1) ? 0 : 1) == 0)
        return 0;

    for (i = 0; i < zeros_in_startcode; i++)
        if (buf[i] != 0) {
            info = 0;
            break;
        };

    return info;
}

static uint8_t *get_nal(uint32_t *len, uint8_t **offset, uint8_t *start, uint32_t total) {
    uint32_t info;
    uint8_t *q;
    uint8_t *p = *offset;
    *len = 0;

    while (1) {
        //p=offset
        // p - start >= total means reach of the end of the packet
        // HINT "-3": Do not access not allowed memory
        if ((p - start) >= total - 3)
            return NULL;

        info = find_start_code(p, 3);
        //if info equals to 1, it means it find the start code
        if (info == 1)
            break;
        p++;
    }
    q = p + 4; // add 4 for first bytes 0 0 0 1
    p = q;
    // find a second start code in the data, there may be second code in data or there may not
    while (1) {
        // HINT "-3": Do not access not allowed memory
        if ((p - start) >= total - 3) {
            p = start + total;
            break;
        }

        info = find_start_code(p, 3);

        if (info == 1)
            break;
        p++;
    }

    // length of the nal unit
    *len = (p - q);
    //offset is the second nal unit start or the end of the data
    *offset = p;
    //return the first nal unit pointer
    return q;
}

void notifyJava(char *methodName) {
    JNIEnv *env = NULL;
    if ((*mGlobalJavaVm)->AttachCurrentThread(mGlobalJavaVm, &env, NULL) !=
        JNI_OK) { //从JavaVM获取JNIEnv，一般使用1.4的版本
        return;
    }

    jclass jclass1 = (*env)->GetObjectClass(env, mGlobalLifecycleListener);
    jmethodID jmethodId = (*env)->GetMethodID(env, jclass1, methodName, "()V");
    (*env)->CallVoidMethod(env, mGlobalLifecycleListener, jmethodId);

    (*mGlobalJavaVm)->DetachCurrentThread(mGlobalJavaVm);
}

void sendOnMetaData(int videoCodecId, int width, int height, int frameRate, int audioCodecid) {
    char buffer[512];
    char *start, *end;
    start = buffer;
    end = buffer + sizeof(buffer);
    char fmt = 0;
    int csId = 0;

    int amf0_header_length = 12;
    int arrayLength = 5;
    // 生成RTMP Body
    start = AMF_EncodeString(start, end, &av_onMetaData);
    *start++ = AMF_ECMA_ARRAY;
    start = AMF_EncodeInt32(start, end, arrayLength);
    start = AMF_EncodeNamedNumber(start, end, &av_width, width);
    start = AMF_EncodeNamedNumber(start, end, &av_height, height);
    start = AMF_EncodeNamedNumber(start, end, &av_duration, 0.0);
    start = AMF_EncodeNamedNumber(start, end, &av_videocodecid, videoCodecId);
    start = AMF_EncodeNamedNumber(start, end, &av_videoframerate, frameRate);
//    start = AMF_EncodeNamedNumber(start, end, &av_audiocodecid, audioCodecid);
    start = AMF_EncodeInt24(start, end, AMF_OBJECT_END);


    int rtmpBodyLength = start - buffer;
    int bodyLength = rtmpBodyLength + 11 + 4;
    char onMetaDataPkt[512];
    int offset = 0;

    uint64_t timestamp = 0;

    onMetaDataPkt[offset++] = 0x12;  //message type id

    onMetaDataPkt[offset++] = (uint8_t) (rtmpBodyLength >> 16);
    onMetaDataPkt[offset++] = (uint8_t) (rtmpBodyLength >> 8);
    onMetaDataPkt[offset++] = (uint8_t) (rtmpBodyLength);

    onMetaDataPkt[offset++] = (uint8_t) (timestamp >> 16);
    onMetaDataPkt[offset++] = (uint8_t) (timestamp >> 8);
    onMetaDataPkt[offset++] = (uint8_t) (timestamp);

    onMetaDataPkt[offset++] = 0x00;  //message stream id
    onMetaDataPkt[offset++] = 0x00;  //message stream id
    onMetaDataPkt[offset++] = 0x00;  //message stream id
    onMetaDataPkt[offset++] = 0x00;  //message stream id

    memcpy(onMetaDataPkt + offset, buffer, rtmpBodyLength);

    int result = RTMP_Write(rtmp, onMetaDataPkt, bodyLength);

    LOGE("send onMetaData result = %d", result);
}

int send_key_frame(Frame *frame) {
    int result = 0;
    int nal_len = 0;
    uint8_t *buf = frame->data;
    buf = get_nal(&nal_len, &buf, frame->data, frame->length);
    int body_len = nal_len + 5 + 4; // flv videotag header + NALU length
    int output_len = body_len + FLV_TAG_HEAD_LEN + FLV_PRE_TAG_LEN;
    char *data = malloc(output_len);

    int offset = 0;
    data[offset++] = 0x09;
    data[offset++] = (uint8_t) (body_len >> 16);
    data[offset++] = (uint8_t) (body_len >> 8);
    data[offset++] = (uint8_t) (body_len);

    uint32_t timeStamp = frame->pts;

    data[offset++] = (uint8_t) (timeStamp >> 16);
    data[offset++] = (uint8_t) (timeStamp >> 8);
    data[offset++] = (uint8_t) (timeStamp);
    data[offset++] = (uint8_t) (timeStamp >> 24);

    int streamID = 0;
    data[offset++] = (uint8_t) (streamID);
    data[offset++] = (uint8_t) (streamID);
    data[offset++] = (uint8_t) (streamID);

    //flv videotag header
    data[offset++] = 0x17;
    data[offset++] = 0x01; //avc NALU unit
    data[offset++] = 0x00; //composit time ??????????
    data[offset++] = 0x00; // composit time
    data[offset++] = 0x00; //composit time

    data[offset++] = (uint8_t) (nal_len >> 24); //nal length
    data[offset++] = (uint8_t) (nal_len >> 16); //nal length
    data[offset++] = (uint8_t) (nal_len >> 8); //nal length
    data[offset++] = (uint8_t) (nal_len); //nal length
    memcpy(data + offset, buf, nal_len);

    offset += nal_len;
    uint32_t fff = body_len + FLV_TAG_HEAD_LEN;
    data[offset++] = (uint8_t) (fff >> 24); //data len
    data[offset++] = (uint8_t) (fff >> 16); //data len
    data[offset++] = (uint8_t) (fff >> 8); //data len
    data[offset++] = (uint8_t) (fff); //data len

    result = RTMP_Write(rtmp, data, output_len);


    LOGD("rtmp send msglength =%d ,write = %d ", output_len,
         result);
    free(data);

    return result;
}


int send_p_frame(Frame *frame) {
    int result = 0;
    int nal_len = 0;
    uint8_t *buf=frame->data;
    buf = get_nal(&nal_len, &buf, frame->data, frame->length);

    int body_len = nal_len + 5 + 4; // flv videotag header + NALU length
    int output_len = body_len + FLV_TAG_HEAD_LEN + FLV_PRE_TAG_LEN;
    char *data = malloc(output_len);

    int offset = 0;
    data[offset++] = 0x09;
    data[offset++] = (uint8_t) (body_len >> 16);
    data[offset++] = (uint8_t) (body_len >> 8);
    data[offset++] = (uint8_t) (body_len);

    uint32_t timeStamp = frame->pts;

    data[offset++] = (uint8_t) (timeStamp >> 16);
    data[offset++] = (uint8_t) (timeStamp >> 8);
    data[offset++] = (uint8_t) (timeStamp);
    data[offset++] = (uint8_t) (timeStamp >> 24);

    int streamID = 0;
    data[offset++] = (uint8_t) (streamID);
    data[offset++] = (uint8_t) (streamID);
    data[offset++] = (uint8_t) (streamID);

    //flv videotag header
    data[offset++] = 0x27;
    data[offset++] = 0x01; //avc NALU unit
    data[offset++] = 0x00; //composit time ??????????
    data[offset++] = 0x00; // composit time
    data[offset++] = 0x00; //composit time

    data[offset++] = (uint8_t) (nal_len >> 24); //nal length
    data[offset++] = (uint8_t) (nal_len >> 16); //nal length
    data[offset++] = (uint8_t) (nal_len >> 8); //nal length
    data[offset++] = (uint8_t) (nal_len); //nal length
    memcpy(data + offset, buf, nal_len);

    offset += nal_len;
    uint32_t fff = body_len + FLV_TAG_HEAD_LEN;
    data[offset++] = (uint8_t) (fff >> 24); //data len
    data[offset++] = (uint8_t) (fff >> 16); //data len
    data[offset++] = (uint8_t) (fff >> 8); //data len
    data[offset++] = (uint8_t) (fff); //data len

    result = RTMP_Write(rtmp, data, output_len);

    LOGD("rtmp send  msglength =%d ,write = %d ", output_len,
         result);
    free(data);

    return result;
}


int send_sps_pps(Frame *frame) {
    uint8_t *data = frame->data;
    uint8_t *buf_offset = frame->data;

    int len = frame->length;
    uint32_t spslen = 0;
    uint32_t ppslen = 0;
    uint8_t *sps = NULL;
    uint8_t *pps = NULL;


    sps = get_nal(&spslen, &buf_offset, data, len);

    if ((sps[0] & 0x1f) != 0x07) {
        LOGE("%s", "cant parse sps");
        return -1;
    }

    pps = get_nal(&ppslen, &buf_offset, data, len);

    if (pps == NULL) {
        LOGE("%s", "No Nal after SPS\n");
        return -1;
    }

    int result = 0;
    int bodySize = spslen + ppslen + 16;
    int output_len = bodySize + FLV_TAG_HEAD_LEN + FLV_PRE_TAG_LEN;
    uint8_t *output = malloc(output_len);

    uint64_t timestamp = 0;
    int i = 0;

    output[i++] = 0x09;  //message type id

    output[i++] = (uint8_t) (bodySize >> 16);
    output[i++] = (uint8_t) (bodySize >> 8);
    output[i++] = (uint8_t) (bodySize);

    output[i++] = (uint8_t) (timestamp >> 16);
    output[i++] = (uint8_t) (timestamp >> 8);
    output[i++] = (uint8_t) (timestamp);
    output[i++] = (uint8_t) (timestamp >> 24);

    output[i++] = 0x00;  //message stream id
    output[i++] = 0x00;  //message stream id
    output[i++] = 0x00;  //message stream id

    //固定头
    output[i++] = 0x17;
    //类型
    output[i++] = 0x00;
    //composition time 0x000000
    output[i++] = 0x00;
    output[i++] = 0x00;
    output[i++] = 0x00;

    output[i++] = 0x01;
    output[i++] = sps[1];
    output[i++] = sps[2];
    output[i++] = sps[3];

    output[i++] = 0xff; //reserved + lengthsizeminusone
    output[i++] = 0xe1; //numofsequenceset


    output[i++] = (spslen >> 8); //reserved + lengthsizeminusone
    output[i++] = spslen; //numofsequenceset

    memcpy(output + i, sps, spslen);

    i += spslen;

    output[i++] = 0x01;
    output[i++] = ((ppslen >> 8) & 0xFF);
    output[i++] = (ppslen & 0xFF);

    memcpy(output + i, pps, ppslen);

    i += ppslen;


    uint32_t fff = bodySize + FLV_TAG_HEAD_LEN;
    output[i++] = (uint8_t) (fff >> 24); //data len
    output[i++] = (uint8_t) (fff >> 16); //data len
    output[i++] = (uint8_t) (fff >> 8); //data len
    output[i++] = (uint8_t) (fff); //data len

    result = RTMP_Write(rtmp, output, output_len);

    LOGD("rtmp send  msglength =%d ,write = %d ", bodySize,
         result);

    free(output);
    return result;
}

void *push(void *obj) {
    const char *url = obj;
    rtmp = RTMP_Alloc();
    RTMP_Init(rtmp);

    int result = RTMP_SetupURL(rtmp, url);

    RTMP_EnableWrite(rtmp);


    result = RTMP_Connect(rtmp, NULL);

    result = RTMP_ConnectStream(rtmp, 0);

    if (mGlobalLifecycleListener != NULL) {
        notifyJava("onConnect");
    }

    int first = 0;
    while (RTMP_IsConnected(rtmp)) {
        if (first == 0) {
            sendOnMetaData(7, 720, 1280, 25, 10);
            first = 1;
        }

        pthread_mutex_lock(&mutex);
        if (frameList.count <= 0) {
            pthread_cond_wait(&cond, &mutex);
        }

        Frame *frame = frameList.frameList[frameList.start];
        frameList.frameList[frameList.start] = NULL;

        switch (frame->type) {
            case 0:
                send_sps_pps(frame);
                break;
            case 1:
                send_key_frame(frame);
                break;
            case 2:
                send_p_frame(frame);
                break;
        }
        LOGD("rtmp send frame , pop index = %d,count = %d", frameList.start,frameList.count);
        frameList.start = (frameList.start + 1) % QUEUE_SIZE;
        frameList.count -= 1;
        free(frame->data);
        free(frame);
        frame = NULL;
        pthread_cond_signal(&cond);
        pthread_mutex_unlock(&mutex);
    }
    if (mGlobalLifecycleListener != NULL) {
        notifyJava("onDisconnect");
    }
}

void rtmp_log_default(int level, const char *format, va_list vl) {
    LOGD(format, vl);
}

int initThread(char *url) {
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&cond, NULL);
    int result = pthread_create(&pushThread, NULL, &push, url);
    frameList.frameList = malloc(sizeof(Frame) * QUEUE_SIZE);
    frameList.start = 0;
    frameList.end = 0;
    frameList.count = 0;

    RTMP_LogSetCallback(&rtmp_log_default);
    return 0;
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    LOGI("JNI_OnLoad called");
    mGlobalJavaVm = vm;
    JNIEnv *env = NULL; //注册时在JNIEnv中实现的，所以必须首先获取它
    if ((*vm)->GetEnv(vm, (void **) &env, JNI_VERSION_1_4) != JNI_OK) { //从JavaVM获取JNIEnv，一般使用1.4的版本
        return -1;
    }
    return JNI_VERSION_1_4; //这里很重要，必须返回版本，否则加载会失败。
}

JNIEXPORT void JNI_OnUnload(JavaVM *vm, void *reserved) {
    LOGI("JNI_OnUnload called");

}


JNIEXPORT  jint JNICALL
Java_cn_aisaka_rtmp_1hardcodec_RTMPHandle_getVersion(JNIEnv *env, jobject thiz) {
    int version = RTMP_LibVersion();
    return version;
}

JNIEXPORT jint JNICALL
Java_cn_aisaka_rtmp_1hardcodec_RTMPHandle_initPush(JNIEnv *env, jobject thiz, jstring url) {
    int result = 0;
    char *pushUrl = (*env)->GetStringUTFChars(env, url, NULL);
    initThread(pushUrl);
    return result;
}

//JNIEXPORT jint JNICALL
//Java_cn_aisaka_rtmp_1hardcodec_RTMPHandle_writeSPSPPSData(JNIEnv *env, jobject thiz,
//                                                          jbyteArray spsData, jbyteArray ppsData,
//                                                          jlong pts, jint type) {
//
//    pthread_mutex_lock(&mutex);
//
//    if (frameList.count >= QUEUE_SIZE) {
//        pthread_cond_wait(&cond, &mutex);
//    }
//
//    uint8_t *spsdata = (uint8_t *) (*env)->GetByteArrayElements(env, spsData, NULL);
//    uint8_t *ppsdata = (uint8_t *) (*env)->GetByteArrayElements(env, ppsData, NULL);
//
//    uint32_t spslength = (*env)->GetArrayLength(env, spsData);
//    uint32_t ppslength = (*env)->GetArrayLength(env, ppsData);
//
//    Frame *frame = malloc(sizeof(Frame));
//    uint8_t *copyData = malloc(spslength);
//    uint8_t *copyData2 = malloc(ppslength);
//
//    memcpy(copyData, spsdata, spslength);
//    memcpy(copyData2, ppsdata, ppslength);
//
//    frame->data = copyData;
//    frame->data2 = copyData2;
//    frame->pts = pts;
//    frame->length = spslength;
//    frame->length2 = ppslength;
//    frame->duration = 40;
//    frame->type = type;
//
//    frameList.frameList[frameList.end] = frame;
//    frameList.count += 1;
//    frameList.end = (frameList.end + 1) % QUEUE_SIZE;
//
//    (*env)->ReleaseByteArrayElements(env, spsData, spsdata, NULL);
//    (*env)->ReleaseByteArrayElements(env, ppsData, ppsdata, NULL);
//
//    LOGD("rtmp push sps pps dataLength=%d, start=%d,end=%d,count=%d", (spslength + ppslength),
//         frameList.start,
//         frameList.end, frameList.count);
//
//    pthread_cond_signal(&cond);
//    pthread_mutex_unlock(&mutex);
//
//    return 0;
//}

JNIEXPORT jint JNICALL
Java_cn_aisaka_rtmp_1hardcodec_RTMPHandle_writeVideoData(JNIEnv *env, jobject thiz,
                                                         jbyteArray videoData, jlong pts,
                                                         jint type) {

    pthread_mutex_lock(&mutex);

    if (frameList.count >= QUEUE_SIZE) {
        pthread_cond_wait(&cond, &mutex);
    }

    uint8_t *data = (uint8_t *) (*env)->GetByteArrayElements(env, videoData, NULL);
    uint32_t length = (*env)->GetArrayLength(env, videoData);
    Frame *frame = malloc(sizeof(Frame));
    uint8_t *copyData = malloc(length);
    memcpy(copyData, data, length);

    (*env)->ReleaseByteArrayElements(env, videoData, data, NULL);

    frame->data = copyData;
    frame->pts = pts;
    frame->length = length;
    frame->duration = 40;
    frame->type = type;

    LOGD("rtmp push key frame dataLength=%d,push index=%d,count=%d", length,
         frameList.end, frameList.count);

    frameList.frameList[frameList.end] = frame;
    frameList.count += 1;
    frameList.end = (frameList.end + 1) % QUEUE_SIZE;
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex);

    return 0;
}


JNIEXPORT void JNICALL
Java_cn_aisaka_rtmp_1hardcodec_RTMPHandle_rotate(JNIEnv *env, jobject thiz, jobject y, jobject u,
                                                 jobject v, jint y_stride, jint u_stride,
                                                 jint v_stride, jobject y_out, jobject u_out,
                                                 jobject v_out, jint y_out_stride,
                                                 jint u_out_stride, jint v_out_stride, jint width,
                                                 jint height, jint rotation_mode) {
    uint8_t *yNative = (uint8_t *) (*env)->GetDirectBufferAddress(env, y);
    uint8_t *uNative = (uint8_t *) (*env)->GetDirectBufferAddress(env, u);
    uint8_t *vNative = (uint8_t *) (*env)->GetDirectBufferAddress(env, v);

    uint8_t *yOutNative = (uint8_t *) (*env)->GetDirectBufferAddress(env, y_out);
    uint8_t *uOutNative = (uint8_t *) (*env)->GetDirectBufferAddress(env, u_out);
    uint8_t *vOutNative = (uint8_t *) (*env)->GetDirectBufferAddress(env, v_out);
    RotationModeEnum mode = kRotate0;
    switch (rotation_mode) {
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

    I420Rotate(yNative, y_stride,
               uNative, u_stride,
               vNative, v_stride,
               yOutNative, y_out_stride,
               uOutNative, u_out_stride,
               vOutNative, v_out_stride,
               width, height, mode);
}

JNIEXPORT void JNICALL
Java_cn_aisaka_rtmp_1hardcodec_RTMPHandle_convertToI420(JNIEnv *env, jobject thiz, jobject y,
                                                        jobject u, jobject v, jint y_stride,
                                                        jint u_stride, jint v_stride,
                                                        jint src_pixel_stride_uv, jobject y_out,
                                                        jobject u_out, jobject v_out,
                                                        jint y_out_stride, jint u_out_stride,
                                                        jint v_out_stride, jint width,
                                                        jint height) {

    uint8_t *yNative = (uint8_t *) (*env)->GetDirectBufferAddress(env, y);
    uint8_t *uNative = (uint8_t *) (*env)->GetDirectBufferAddress(env, u);
    uint8_t *vNative = (uint8_t *) (*env)->GetDirectBufferAddress(env, v);

    uint8_t *yOutNative = (uint8_t *) (*env)->GetDirectBufferAddress(env, y_out);
    uint8_t *uOutNative = (uint8_t *) (*env)->GetDirectBufferAddress(env, u_out);
    uint8_t *vOutNative = (uint8_t *) (*env)->GetDirectBufferAddress(env, v_out);

    Android420ToI420(yNative, y_stride,
                     uNative, u_stride,
                     vNative, v_stride,
                     src_pixel_stride_uv,
                     yOutNative, y_out_stride,
                     uOutNative, u_out_stride,
                     vOutNative, v_out_stride,
                     width, height);
}


JNIEXPORT void JNICALL
Java_cn_aisaka_rtmp_1hardcodec_RTMPHandle_convertToI420WithDegree(JNIEnv *env, jobject thiz,
                                                                  jobject y,
                                                                  jobject u, jobject v,
                                                                  jint y_stride,
                                                                  jint u_stride, jint v_stride,
                                                                  jint src_pixel_stride_uv,
                                                                  jobject y_out,
                                                                  jobject u_out, jobject v_out,
                                                                  jint y_out_stride,
                                                                  jint u_out_stride,
                                                                  jint v_out_stride, jint width,
                                                                  jint height, jint degree) {

    uint8_t *yNative = (uint8_t *) (*env)->GetDirectBufferAddress(env, y);
    uint8_t *uNative = (uint8_t *) (*env)->GetDirectBufferAddress(env, u);
    uint8_t *vNative = (uint8_t *) (*env)->GetDirectBufferAddress(env, v);

    uint8_t *yOutNative = (uint8_t *) (*env)->GetDirectBufferAddress(env, y_out);
    uint8_t *uOutNative = (uint8_t *) (*env)->GetDirectBufferAddress(env, u_out);
    uint8_t *vOutNative = (uint8_t *) (*env)->GetDirectBufferAddress(env, v_out);
    RotationModeEnum mode = kRotate0;
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
    Android420ToI420Rotate(yNative, y_stride,
                           uNative, u_stride,
                           vNative, v_stride,
                           src_pixel_stride_uv,
                           yOutNative, y_out_stride,
                           uOutNative, u_out_stride,
                           vOutNative, v_out_stride,
                           width, height, mode);

}

JNIEXPORT void JNICALL
Java_cn_aisaka_rtmp_1hardcodec_RTMPHandle_I420ToNV12(JNIEnv *env, jobject thiz, jbyteArray i420_src,
                                                     jint width, jint height, jbyteArray nv12_dst) {
    jbyte *src_i420_data = (*env)->GetByteArrayElements(env, i420_src, NULL);
    jbyte *dst_nv12_data = (*env)->GetByteArrayElements(env, nv12_dst, NULL);
    jint src_y_size = width * height;
    jint src_u_size = (width >> 1) * (height >> 1);

    jbyte *src_nv12_y_data = dst_nv12_data;
    jbyte *src_nv12_uv_data = dst_nv12_data + src_y_size;

    jbyte *src_i420_y_data = src_i420_data;
    jbyte *src_i420_u_data = src_i420_data + src_y_size;
    jbyte *src_i420_v_data = src_i420_data + src_y_size + src_u_size;

    I420ToNV12(
            (const uint8_t *) src_i420_y_data, width,
            (const uint8_t *) src_i420_u_data, width >> 1,
            (const uint8_t *) src_i420_v_data, width >> 1,
            (uint8_t *) src_nv12_y_data, width,
            (uint8_t *) src_nv12_uv_data, width,
            width, height);


    (*env)->ReleaseByteArrayElements(env, i420_src, src_i420_data, 0);
    (*env)->ReleaseByteArrayElements(env, nv12_dst, dst_nv12_data, 0);

}

JNIEXPORT void JNICALL
Java_cn_aisaka_rtmp_1hardcodec_RTMPHandle_setListener(JNIEnv *env, jobject thiz, jobject listener) {
    mGlobalLifecycleListener = (*env)->NewGlobalRef(env, listener);
}


JNIEXPORT void JNICALL
Java_cn_aisaka_rtmp_1hardcodec_RTMPHandle_convertToNV21WithDegree(JNIEnv *env, jobject thiz,
                                                                  jobject y, jobject u, jobject v,
                                                                  jint y_stride, jint u_stride,
                                                                  jint v_stride,
                                                                  jint src_pixel_stride_uv,
                                                                  jobject y_out, jobject u_out,
                                                                  jobject v_out, jint y_out_stride,
                                                                  jint u_out_stride,
                                                                  jint v_out_stride, jint width,
                                                                  jint height, jint degree) {
    uint8_t *yNative = (uint8_t *) (*env)->GetDirectBufferAddress(env, y);
    uint8_t *uNative = (uint8_t *) (*env)->GetDirectBufferAddress(env, u);
    uint8_t *vNative = (uint8_t *) (*env)->GetDirectBufferAddress(env, v);

    uint8_t *yOutNative = (uint8_t *) (*env)->GetDirectBufferAddress(env, y_out);
    uint8_t *uOutNative = (uint8_t *) (*env)->GetDirectBufferAddress(env, u_out);
    uint8_t *vOutNative = (uint8_t *) (*env)->GetDirectBufferAddress(env, v_out);
    RotationModeEnum mode = kRotate0;
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
    Android420ToI420Rotate(yNative, y_stride,
                           uNative, u_stride,
                           vNative, v_stride,
                           src_pixel_stride_uv,
                           yOutNative, y_out_stride,
                           uOutNative, u_out_stride,
                           vOutNative, v_out_stride,
                           width, height, mode);
}