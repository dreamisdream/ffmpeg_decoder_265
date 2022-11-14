#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>

#ifdef _WIN64
#include <Windows.h>
#else

#endif // __WIN64

typedef struct QueueNode
{
    char val[200 * 1000];
    int size;
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

void queue_destory(Queue* pq)
{
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
}

void queue_push(Queue* pq, char* data, int size)
{
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
    newNode->next = NULL;

    if (pq->tail == NULL) {
        pq->head = pq->tail = newNode;
    } else {
        pq->tail->next = newNode;
        pq->tail = newNode;
    }
    //printf("增加节点%d   size:%d   %d \n", queue_size(pq), size, strlen(newNode->val));
}

void queue_pop(Queue* pq)
{
    if (pq == NULL) {
        printf("queue_pop pq is null\n");
        return ;
    }
    if (pq->head==NULL && pq->tail == NULL) {
        printf("queue_push pq is null\n");
    }

    if (pq->head->next == NULL) {
        free(pq->head);
        pq->head = pq->tail = NULL;
    } else {
        QueueNode* next = pq->head->next;
        free(pq->head);
        pq->head = next;
    }

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
    if (pq == NULL || pq->head == NULL) {
        printf("queue_front pq or head is null\n");
        return 0;
    }

    if (size <= 0)
        return 0;
    *data = pq->head->val;
    *size = pq->head->size;

    return 1;
}

int queue_size(Queue* pq)
{
    if (pq == NULL) {
        printf("queue_size pq is null\n");
        return 0;
    }

    QueueNode* cur = pq->head;
    int count = 0;
    while (cur) {
        cur = cur->next;
        count++;
    }
    return count;
}

typedef void(*OnBuffer)(unsigned char* data, int size, int w, int h, int pts, int cost_time);

AVCodec* codec = NULL;
AVCodecContext* dec_ctx = NULL;
AVCodecParserContext* parser_ctx = NULL;
AVPacket* pkt = NULL;
AVFrame* frame = NULL;
OnBuffer decoder_callback = NULL;
Queue* g_queue;
clock_t start_time = 0;
clock_t end_time = 0;

void output_yuv_buffer(AVFrame* frame) {
    int width, height, frame_size;
    uint8_t* yuv_buffer = NULL;
    width = frame->width;
    height = frame->height;
    // 根据格式，获取buffer大小
    frame_size = av_image_get_buffer_size(frame->format, width, height, 1);
    // 分配内存
    yuv_buffer = (uint8_t*)av_mallocz(frame_size * sizeof(uint8_t));
    // 将frame数据按照yuv的格式依次填充到bufferr中。下面的步骤可以用工具函数av_image_copy_to_buffer代替。
    int i, j, k;
    // Y
    for (i = 0; i < height; i++) {
        memcpy(yuv_buffer + width * i,
            frame->data[0] + frame->linesize[0] * i,
            width);
    }
    for (j = 0; j < height / 2; j++) {
        memcpy(yuv_buffer + width * i + width / 2 * j,
            frame->data[1] + frame->linesize[1] * j,
            width / 2);
    }
    for (k = 0; k < height / 2; k++) {
        memcpy(yuv_buffer + width * i + width / 2 * j + width / 2 * k,
            frame->data[2] + frame->linesize[2] * k,
            width / 2);
    }
    // 通过之前传入的回调函数发给js
#ifdef _WIN64
    int cost_time = (end_time - start_time);
#else
    int cost_time = (end_time - start_time) / 1000;
#endif // _WIN64

    decoder_callback(yuv_buffer, frame_size, frame->width, frame->height, frame->pts, cost_time);
    av_free(yuv_buffer);
}

int decode_packet(AVCodecContext* ctx, AVFrame* frame, AVPacket* pkt)
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
        output_yuv_buffer(frame);
    }
    return ret;
}

// export interface
#ifdef _WIN64
void init_decoder(OnBuffer callback)
#else
void init_decoder(long callback)
#endif // __WIN64
{
    // 找到hevc解码器
    codec = avcodec_find_decoder(AV_CODEC_ID_HEVC);
    if (codec == NULL) {
        printf("avcodec_find_decoder error\n");
        return ;
    }
    // 初始化对应的解析器
    parser_ctx = av_parser_init(codec->id);
    if (parser_ctx == NULL) {
        printf("av_parser_init error\n");
        return;
    }
    // 初始化上下文
    dec_ctx = avcodec_alloc_context3(codec);
    if (parser_ctx == NULL) {
        printf("avcodec_alloc_context3 error\n");
        return;
    }
    // 打开decoder
    int res = avcodec_open2(dec_ctx, codec, NULL);
    if (res) {
        printf("avcodec_open2  %d  error\n", res);
        return;
    }
    // 分配一个frame内存，并指明yuv 420p格式
    frame = av_frame_alloc();
    frame->format = AV_PIX_FMT_YUV420P;
    // 分配一个pkt内存
    pkt = av_packet_alloc();
    // 暂存回调
    decoder_callback = (OnBuffer)callback;
    g_queue = (Queue*)malloc(sizeof(Queue));
    queue_init(g_queue);
}

void decode_buffer(unsigned char* buffer, int data_size) { // 入参是js传入的uint8array数据以及数据长度
    while (data_size > 0) {
        // 从buffer中解析出packet
        int size = av_parser_parse2(parser_ctx, dec_ctx, &pkt->data, &pkt->size,
            (char*)buffer, data_size, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
        if (size < 0) {
            break;
        }
        buffer += size;
        data_size -= size;
        if (pkt->size) {
            // 解码packet
            // 送进队列
            queue_push(g_queue, pkt->data, pkt->size);
          //  decode_packet(dec_ctx, frame, pkt);
        }
    }
}

void close_decoder() {

    if (parser_ctx) {
        av_parser_close(parser_ctx);
    }
    if (dec_ctx) {
        avcodec_free_context(&codec);
    }
    if (frame) {
        av_frame_free(&frame);
    }
    if (pkt) {
        av_packet_free(&pkt);
    }
    decoder_callback = NULL;

    queue_destory(g_queue);
}

int decode_one_packet()
{
    AVPacket *pkt = av_malloc(sizeof(AVPacket));
    av_init_packet(pkt);
    memset(pkt, 0, sizeof(pkt));
    if (queue_size(g_queue) == 0)
        return 0;

    if (queue_front(g_queue, &pkt->data, &pkt->size)) {
        // 解码packet
        decode_packet(dec_ctx, frame, pkt);

        queue_pop(g_queue);

        printf("剩余缓存帧数:%d \n", queue_size(g_queue));
    }
    return 1;
}

FILE* output = NULL;
void deal_buffer(unsigned char* data, int size, int w, int h, int pts, int cost_time) {

    printf("%d*%d size:%d   pts:%d  cost_time:%d  \n", w, h, size, pts, cost_time);

    int res = fwrite(data, size, 1, output);
    if (res < size) {
        printf("write file error\n");
    }
    return;

}

int main(int argc, char* argv[])
{
    return 0;
#ifdef _WIN64
    init_decoder(deal_buffer);
#else
    return 0;

#endif // __WIN64

    output = fopen("d:/test.yuv", "wb+");
    if (output == NULL) {
        printf("open output error \n");
        return 0;
    }

    const int data_chunk = 4096;
    FILE* input = fopen("d:/15s.h265", "rb");
    if (!input) {
        printf("openfile error\n");
    }

    size_t data_size = 0;
    char* buffer = (char*)malloc(sizeof(char) * (data_chunk + AV_INPUT_BUFFER_PADDING_SIZE));

    while (!feof(input)) {
        /* read raw data from the input file */
        if (buffer)
            data_size = fread(buffer, 1, data_chunk, input);

        if (!data_size) {
            printf("文件读取结束\n");
            break;
        }

        /* use the parser to split the data into frames */
        //data = inbuf;
        decode_buffer(buffer, data_size);
    }

    while (1) {
        int res = decode_one_packet();
        if (!res)
            break;
        //Sleep(40);

    }

    fclose(input);
    fclose(output);

    close_decoder();

    return 0;
}