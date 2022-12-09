#define _CRT_SECURE_NO_WARNINGS

#define IFRAME 1 
#define AUDIO_FRAME 10
#define QUEUE_MAX_NUM 25
//#define WIN_TEST

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>

#ifdef  __linux__
#include <pthread.h>
pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif // __linux__

typedef struct QueueNode
{
    char val[200 * 1000];
    int size;
    int flag;
    struct QueueNode* next;
}QueueNode;

typedef	struct Queue
{
    QueueNode* head;
    QueueNode* tail;
}Queue;

void queue_init(Queue* pq)
{
    pq->head = pq->tail = NULL;
}

int queue_size(Queue* pq)
{
#ifdef __linux__
    pthread_mutex_lock(&g_mutex);
#endif
    if (pq == NULL) {
        printf("queue_size pq is null\n");
#ifdef __linux__
        pthread_mutex_unlock(&g_mutex);
#endif
        return 0;
    }

    QueueNode* cur = pq->head;
    int count = 0;
    while (cur) {
        cur = cur->next;
        count++;
    }
#ifdef __linux__
    pthread_mutex_unlock(&g_mutex);
#endif
    return count;
}

void queue_pop(Queue* pq)
{
#ifdef __linux__
    pthread_mutex_lock(&g_mutex);
#endif
    if (pq == NULL) {
        printf("queue_pop pq is null\n"); 
#ifdef __linux__
            pthread_mutex_unlock(&g_mutex);
#endif
        return;
    }
    if (pq->head == NULL && pq->tail == NULL) {
        printf("queue_push pq is null\n");
#ifdef __linux__
        pthread_mutex_unlock(&g_mutex);
#endif

    }

    if (pq->head->next == NULL) {
        free(pq->head);
        pq->head = pq->tail = NULL;
    } else {
        QueueNode* next = pq->head->next;
        free(pq->head);
        pq->head = next;
    }
#ifdef __linux__
    pthread_mutex_unlock(&g_mutex);
#endif

}

void queue_destory(Queue* pq)
{
#ifdef __linux__
    pthread_mutex_lock(&g_mutex);
#endif
    if (pq == NULL) {
        printf("queue_destory pq is null\n");
    }
    QueueNode* cur = pq->head;
    while (cur) {
        QueueNode* next = cur->next;
        free(cur);
        cur = next;
    }
    pq->tail = pq->head = NULL;
#ifdef __linux__
    pthread_mutex_unlock(&g_mutex);
#endif
}

void queue_push(Queue* pq, char* data, int size,int flag)
{
#ifdef __linux__
    pthread_mutex_lock(&g_mutex);
#endif
    if (pq == NULL) {
        printf("queue_push pq is null\n");
    }

    QueueNode* newNode = (QueueNode*)malloc(sizeof(QueueNode));
    if (NULL == newNode) {
        printf("malloc error\n");
        return ;
    }
    memcpy(newNode->val, data, size);
    newNode->size = size;
    newNode->flag = flag;
    newNode->next = NULL;

    if (pq->tail == NULL) {
        pq->head = pq->tail = newNode;
    } else {
        pq->tail->next = newNode;
        pq->tail = newNode;
    }

#ifdef __linux__
    pthread_mutex_unlock(&g_mutex);
#endif
}

int queue_empty(Queue* pq)
{
    if (pq == NULL) {
        printf("queue_empty pq is null\n");
        return 0;
    }

    return pq->head == NULL;
}

int queue_front(Queue* pq, char** data, int* size)
{
#ifdef __linux__
    pthread_mutex_lock(&g_mutex);
#endif
    if (pq == NULL || pq->head == NULL) {
        printf("queue_front pq or head is null\n");
#ifdef __linux__
        pthread_mutex_unlock(&g_mutex);
#endif
        return 0;
    }

    while (queue_size(pq) > QUEUE_MAX_NUM && pq->head->flag != AUDIO_FRAME) {
        while (pq->head->flag >= AV_PICTURE_TYPE_P && pq->head->flag <= AV_PICTURE_TYPE_S) {
            queue_pop(pq);
        }
    }

    if (size <= 0)
        return 0;
    *data = pq->head->val;
    *size = pq->head->size;
#ifdef __linux__
    pthread_mutex_unlock(&g_mutex);
#endif
    return 1;
}

#if (defined _WIN32 || defined _WIN64)
typedef void(*VideoCallback)(unsigned char* data, int size, int w, int h, int pts, int cost_time);
typedef void(*AudioCallback)(unsigned char* buff, int size, int timestamp, int cost_time);
#else
typedef void(*VideoCallback)(long data, int size, int w, int h, int pts, int cost_time);
typedef void(*AudioCallback)(unsigned char* buff, int size, int timestamp, int cost_time);
#endif

AVCodec* codec = NULL;
AVCodec* audio_codec = NULL;
AVCodecContext* dec_ctx = NULL;
AVCodecContext* audio_dec_ctx = NULL;
AVCodecParserContext* parser_ctx = NULL;
AVCodecParserContext* audio_parser_ctx = NULL;
AVPacket* pkt = NULL;
AVPacket* audio_pkt = NULL;
AVFrame* frame = NULL;
AVFrame* audio_frame = NULL;
AVPacket* pkt_cache = NULL;
AVPacket* audio_pkt_cache = NULL;
VideoCallback decoder_callback = NULL;
AudioCallback decoder_audio_callback = NULL;
Queue* g_queue;
Queue* g_audio_queue;
clock_t start_time = 0;
clock_t end_time = 0;
uint8_t* yuv_buffer = NULL;
int frame_size = 0;
unsigned char* pcmBuffer;
int currentPcmBufferSize;
int video_width, video_height;

int roundUp(int numToRound, int multiple) {
    return (numToRound + multiple - 1) & -multiple;
}

int output_audio_frame(AVFrame* frame) {
    int ret = 0;
    int sampleSize = 0;
    int audioDataSize = 0;
    int targetSize = 0;
    int offset = 0;
    int i = 0;
    int ch = 0;
    double timestamp = 0.0f;
    do {
        if (frame == NULL) {
            ret = -1;
            break;
        }

        sampleSize = av_get_bytes_per_sample(audio_dec_ctx->sample_fmt);
        if (sampleSize < 0) {
            printf("Failed to calculate data size.");
            ret = -1;
            break;
        }

        audioDataSize = frame->nb_samples * audio_dec_ctx->channels * sampleSize;
        if (currentPcmBufferSize < audioDataSize) {
            targetSize = roundUp(audioDataSize, 4);
            printf("Current PCM buffer size %d not sufficient for data size %d, round up to target %d.",
                currentPcmBufferSize,
                audioDataSize,
                targetSize);
            currentPcmBufferSize = targetSize;
            av_free(pcmBuffer);
            pcmBuffer = (unsigned char*)av_mallocz(currentPcmBufferSize);
        }

        for (i = 0; i < frame->nb_samples; i++) {
            for (ch = 0; ch < audio_dec_ctx->channels; ch++) {
                memcpy(pcmBuffer + offset, frame->data[ch] + sampleSize * i, sampleSize);
                offset += sampleSize;
            }
        }

        timestamp = (double)frame->pts * av_q2d(audio_dec_ctx->time_base);

#if (defined _WIN32 || defined _WIN64)
        int cost_time = (end_time - start_time);
#else
        int cost_time = (end_time - start_time) / 1000;
#endif // _WIN64

        if (decoder_audio_callback != NULL) {
            decoder_audio_callback(pcmBuffer, audioDataSize, timestamp, cost_time);
        }
    } while (0);
    return ret;
}

void output_yuv_buffer(AVFrame* frame) {
    int width, height;
    width = frame->width;
    height = frame->height;

    // 将frame数据按照yuv的格式依次填充到bufferr中。下面的步骤可以用工具函数av_image_copy_to_buffer代替。
    int i, j, k;
    // Y
    for (i = 0; i < height; i++) {
        memcpy(yuv_buffer + width * i, frame->data[0] + frame->linesize[0] * i, width);
    }
    for (j = 0; j < height / 2; j++) {
        memcpy(yuv_buffer + width * i + width / 2 * j, frame->data[1] + frame->linesize[1] * j, width / 2);
    }
    for (k = 0; k < height / 2; k++) {
        memcpy(yuv_buffer + width * i + width / 2 * j + width / 2 * k, frame->data[2] + frame->linesize[2] * k, width / 2);
    }
    // 通过之前传入的回调函数发给js
#if (defined _WIN32 || defined _WIN64)
    int cost_time = (end_time - start_time);
    decoder_callback(yuv_buffer, frame_size, frame->width, frame->height, frame->pts, cost_time);
#else
    int cost_time = (end_time - start_time) / 1000;
    decoder_callback((long)yuv_buffer, frame_size, frame->width, frame->height, frame->pts, cost_time);
#endif // _WIN64

}

int decode_packet(AVCodecContext* ctx, AVFrame* frame, AVPacket* pkt,int is_video)
{
    start_time = clock();
    int ret = 0;
    // 发送packet到解码器
    ret = avcodec_send_packet(ctx, pkt);
    if (ret < 0) {
        return ret;
    }
    // 从解码器接收frame
    while (ret >= 0) {
        ret = avcodec_receive_frame(ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            // handle error
            break;
        }
        // 输出yuv buffer数据
        end_time = clock();
        if (is_video) {
            output_yuv_buffer(frame);
        } else {
            output_audio_frame(frame);
        }

    }
    return ret;
}

void init_buffer(int width, int height) {
    if (video_width != width && video_height != height) {
        video_width = width;
        video_height = height;
    } else {
        return;
    }
    pkt_cache = av_packet_alloc();
    frame_size = av_image_get_buffer_size(AV_PIX_FMT_YUV420P, video_width, video_height, 1);
    // 分配内存
    yuv_buffer = (uint8_t*)av_mallocz(frame_size);
}

void close_video_decoder()
{
    if (parser_ctx) {
        av_parser_close(parser_ctx);
    }
    if (dec_ctx) {
        avcodec_free_context(&dec_ctx);
    }

    if (pkt_cache) {
        av_packet_free(&pkt_cache);
    }

    if (frame) {
        av_frame_free(&frame);
    }
    if (pkt) {
        av_packet_free(&pkt);
    }

    av_free(yuv_buffer);
    decoder_callback = NULL;

    queue_destory(g_queue);
    free(g_queue);
}

void close_audio_decoder()
{
    if (audio_parser_ctx) {
        av_parser_close(audio_parser_ctx);
    }
    if (audio_dec_ctx) {
        avcodec_free_context(&audio_dec_ctx);
    }

    if (audio_frame) {
        av_frame_free(&audio_frame);
    }
    if (audio_pkt) {
        av_packet_free(&audio_pkt);
    }

    queue_destory(g_audio_queue);
    free(g_audio_queue);
}

/*
    export interface
*/ 

#if (defined _WIN32 || defined _WIN64)
void init_decoder(VideoCallback callback, AudioCallback audio_callback)
#else
void init_decoder(long callback, long audio_callback)
#endif // __WIN64
{
    if (callback) {
        // 找到hevc解码器
        codec = avcodec_find_decoder(AV_CODEC_ID_HEVC);
        if (codec == NULL) {
            printf("avcodec_find_decoder error\n");
            return;
        }

        // 初始化对应的解析器
        parser_ctx = av_parser_init(codec->id);
        if (parser_ctx == NULL) {
            printf("av_parser_init error\n");
            return;
        }
        //dec_ctx->thread_count = 4;
        // 初始化上下文
        dec_ctx = avcodec_alloc_context3(codec);
        if (dec_ctx == NULL) {
            printf("avcodec_alloc_context3 error\n");
            return;
        }

        AVDictionary* para = NULL;
        av_dict_set(&para, "preset", "ultrafast", 0);
        av_dict_set(&para, "tune", "zerolatency", 0);
        av_dict_set(&para, "framerate", "20", 0);
        // 打开decoder
        int res = avcodec_open2(dec_ctx, codec, &para);
        if (res) {
            printf("avcodec_open2  %d  error\n", res);
            return;
        }
        // 分配一个frame内存，并指明yuv 420p格式
        frame = av_frame_alloc();
        frame->format = AV_PIX_FMT_YUV420P;
        // 分配一个pkt内存
        pkt = av_packet_alloc();

        decoder_callback = (VideoCallback)callback;
        video_width = 0;
        video_height = 0;
        g_queue = (Queue*)malloc(sizeof(Queue));
        queue_init(g_queue);
    }
    
    if (audio_callback) {
        // 找到hevc解码器
        audio_codec = avcodec_find_decoder(AV_CODEC_ID_AAC);
        if (audio_codec == NULL) {
            printf("avcodec_find_decoder error\n");
            return;
        }

        // 初始化对应的解析器
        audio_parser_ctx = av_parser_init(audio_codec->id);
        if (audio_parser_ctx == NULL) {
            printf("av_parser_init error\n");
            return;
        }
        //dec_ctx->thread_count = 4;
        // 初始化上下文
        audio_dec_ctx = avcodec_alloc_context3(audio_codec);
        if (audio_dec_ctx == NULL) {
            printf("avcodec_alloc_context3 error\n");
            return;
        }

        // 打开decoder
        int res = avcodec_open2(audio_dec_ctx, audio_codec, NULL);
        if (res) {
            printf("avcodec_open2  %d  error\n", res);
            return;
        }
        audio_frame = av_frame_alloc();
        audio_pkt = av_packet_alloc();
        audio_pkt_cache = av_packet_alloc();

        if (pcmBuffer == NULL) {
            pcmBuffer = (unsigned char*)av_mallocz(128 * 1024);
            currentPcmBufferSize = 128 * 1024;
            printf("Initial PCM buffer size %d.\n", currentPcmBufferSize);
        }
        g_audio_queue = (Queue*)malloc(sizeof(Queue));
        queue_init(g_audio_queue);
        // 暂存回调
        decoder_audio_callback = (AudioCallback)audio_callback;
    }
   
}

void decode_buffer(unsigned char* buffer, int data_size, int is_video) { // 入参是js传入的uint8array数据以及数据长度
    
    if (is_video) {
        while (data_size > 0) {
            // 从buffer中解析出packet
            int size = av_parser_parse2(parser_ctx, dec_ctx, &pkt->data, &pkt->size,
                (uint8_t*)buffer, data_size, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
            if (size < 0) {
                break;
            }
            buffer += size;
            data_size -= size;

            if (pkt->size) {
                init_buffer(parser_ctx->width, parser_ctx->height);
                // 送进队列  AV_PICTURE_TYPE_I
                queue_push(g_queue, (char*)pkt->data, pkt->size, parser_ctx->pict_type);
#ifdef WIN_TEST
                decode_one_packet(1);
#endif
            }
        }
    } else {
        while (data_size > 0) {
            // 从buffer中解析出packet
            int size = av_parser_parse2(audio_parser_ctx, audio_dec_ctx, &audio_pkt->data, &audio_pkt->size,
                (uint8_t*)buffer, data_size, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
            if (size < 0) {
                break;
            }
            buffer += size;
            data_size -= size;

            if (audio_pkt->size) {
                audio_pkt->flags = 10; // 音频
                // 送进队列
                printf("---pict type :%d \n", (enum AVSampleFormat)parser_ctx->format);
                queue_push(g_audio_queue, (char*)audio_pkt->data, audio_pkt->size, audio_pkt->flags);
#ifdef WIN_TEST
                decode_one_packet(0);
#endif
            }
        }
        
    }
    
}

void close_decoder() 
{
    close_video_decoder();
    close_audio_decoder();
 
}

int decode_one_packet(int is_video)
{
    if (is_video) {
        av_init_packet(pkt_cache);
        if (queue_size(g_queue) == 0)
            return 0;

        if (queue_front(g_queue, (char**)&pkt_cache->data, &pkt_cache->size)) {
            // 解码packet
            decode_packet(dec_ctx, frame, pkt_cache, 1);

            queue_pop(g_queue);

            // printf("剩余缓存帧数:%d \n", queue_size(g_queue));
        }
    } else {
        av_init_packet(audio_pkt_cache);
        if (queue_size(g_audio_queue) == 0)
            return 0;

        if (queue_front(g_audio_queue, (char**)&audio_pkt_cache->data, &audio_pkt_cache->size)) {
            // 解码packet
            decode_packet(audio_dec_ctx, audio_frame, audio_pkt_cache, 0);

            queue_pop(g_audio_queue);

            // printf("剩余缓存帧数:%d \n", queue_size(g_queue));
        }
    }
   
    return 1;
}

/*  
   * window test program 
*/
FILE* output = NULL;

void deal_buffer(unsigned char* data, int size, int w, int h, int pts, int cost_time) {
    static int count = 0;
    printf("[%d] %d*%d size:%d   pts:%d  cost_time:%d  \n", ++count, w, h, size, pts, cost_time);

    fwrite(data, size, 1, output);
    fflush(output);

    return;

}

void deal_audio_buffer(unsigned char* data, int size, int pts, int cost_time) {
    static int count = 0;
    printf("deal_audio_buffer  [%d] size:%d   pts:%d  cost_time:%d  \n",  ++count, size, pts, cost_time);
    fwrite(data, size, 1, output);
    fflush(output);
    return;
}

int main(int argc, char* argv[])
{
#if (defined _WIN32 || defined _WIN64)
    init_decoder(deal_buffer, deal_audio_buffer);
#else
    return 0;
#endif // __WIN64
    // test.yuv   test.pcm
    output = fopen("d:/test.yuv", "wb+");
    if (output == NULL) {
        printf("open output error \n");
        return 0;
    }

    const int data_chunk = 4096;
    FILE* input = fopen("d:/test.aac", "rb");
    if (!input) {
        printf("openfile error\n");
    }

    size_t data_size = 0;
    unsigned char* buffer = (unsigned char*)malloc(sizeof(char) * (data_chunk + AV_INPUT_BUFFER_PADDING_SIZE));
    
    while (!feof(input)) {
        /* read raw data from the input file */
        if (buffer)
            data_size = fread(buffer, 1, data_chunk, input);

        if (data_size < 4096) {
            printf("文件读取结束\n");
            break;
        }

        /* use the parser to split the data into frames */
        //data = inbuf;
        decode_buffer(buffer, data_size, 0);
    }

    fclose(input);
    fclose(output);

    close_decoder();

    return 0;
}