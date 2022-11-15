
#include "common.h"

#define MIN(a,b) (a<b)?a:b

typedef struct _BufferData
{
    uint8_t* ptr; // ָ��buffer������ "��û��io���������ĵ�λ��"
    uint8_t* ori_ptr; // Ҳ��ָ��buffer���ݵ�ָ��,֮���Զ���ori_ptr,�������Զ���seek������
    size_t size; // ��Ƶbuffer��û�����Ĳ��ֵĴ�С,���Ų�������,Խ��ԽС
    size_t file_size; //ԭʼ��Ƶbuffer�Ĵ�С,Ҳ�������Զ���seek������
} BufferData;

/* �������ļ�������ȫ������ȥ�ڴ� */
uint8_t* readFile(char* path, size_t* length)
{
    FILE* pfile;
    uint8_t* data;

    pfile = fopen(path, "rb");
    if (pfile == NULL)
        return NULL;
    fseek(pfile, 0, SEEK_END);
    *length = ftell(pfile);
    data = (uint8_t*)malloc((*length) * sizeof(uint8_t));
    rewind(pfile);
    *length = fread(data, 1, *length, pfile);
    fclose(pfile);
    return data;
}

/* ��ȡ AVPacket �ص����� */
static int read_packet(void* opaque, uint8_t* buf, int buf_size)
{
    BufferData* bd = (BufferData*)opaque;
    buf_size = MIN((int)bd->size, buf_size);

    printf("buf is %p \n", buf);
    if (!buf_size) {
        printf("no buf_size pass to read_packet,%d,%zu\n", buf_size, bd->size);
        return -1;
    }
    //printf("ptr in file:%p io.buffer ptr:%p, size:%zu,buf_size:%d\n", bd->ptr, buf, bd->size, buf_size);
    memcpy(buf, bd->ptr, buf_size);
    bd->ptr += buf_size;
    bd->size -= buf_size; // left size in buffer
    return buf_size;
}

/* seek �ص����� */
static int64_t seek_in_buffer(void* opaque, int64_t offset, int whence)
{
    BufferData* bd = (BufferData*)opaque;
    int64_t ret = -1;

    printf("whence=%d , offset=%lld , file_size=%zu\n", whence, offset, bd->file_size);
    switch (whence) {
    case AVSEEK_SIZE:
        ret = bd->file_size;
        break;
    case SEEK_SET:
        bd->ptr = bd->ori_ptr + offset;
        bd->size = bd->file_size - offset;
        ret = (int64_t)bd->ptr;
        break;
    }
    return ret;
}

int main()
{
    int ret = 0; int err;
    uint8_t* input;
    AVFormatContext* fmt_ctx = NULL;
    AVIOContext* avio_ctx = NULL;
    uint8_t* avio_ctx_buffer = NULL;
    int avio_ctx_buffer_size = 4096;
    size_t file_len;
    BufferData bd = { 0 };

    char filename[] = "juren-30s.mp4";
    input = readFile(filename, &file_len);
    bd.ptr = input;
    bd.ori_ptr = input;
    bd.size = file_len;
    bd.file_size = file_len;

    //�������ļ�
    fmt_ctx = avformat_alloc_context();
    if (!fmt_ctx) {
        printf("error code %d \n", AVERROR(ENOMEM));
        return ENOMEM;
    }

    avio_ctx_buffer = (uint8_t*)av_malloc(avio_ctx_buffer_size);
    printf("avio_ctx_buffer is %p \n", avio_ctx_buffer);
    if (!avio_ctx_buffer) {
        printf("error code %d \n", AVERROR(ENOMEM));
        return ENOMEM;
    }
    avio_ctx = avio_alloc_context(avio_ctx_buffer, avio_ctx_buffer_size,
        0, &bd, &read_packet, NULL, &seek_in_buffer);
    if (!avio_ctx) {
        printf("error code %d \n", AVERROR(ENOMEM));
        return ENOMEM;
    }
    fmt_ctx->pb = avio_ctx;

    if ((err = avformat_open_input(&fmt_ctx, NULL, NULL, NULL)) < 0) {
        printf("can not open file %d \n", err);
        return err;
    }

    ret = avformat_find_stream_info(fmt_ctx, NULL);
    if (ret < 0) {
        printf("avformat_find_stream_info file %d \n", ret);
        return ret;
    }

    //�򿪽�����
    AVCodecContext* avctx = avcodec_alloc_context3(NULL);
    ret = avcodec_parameters_to_context(avctx, fmt_ctx->streams[0]->codecpar);
    if (ret < 0) {
        printf("error code %d \n", ret);
        return ret;
    }
    AVCodec* codec = avcodec_find_decoder(avctx->codec_id);
    if ((ret = avcodec_open2(avctx, codec, NULL)) < 0) {
        printf("open codec faile %d \n", ret);
        return ret;
    }

    //������ļ�����
    char filename_out[] = "juren-30s-5.mp4";
    AVFormatContext* fmt_ctx_out = NULL;
    err = avformat_alloc_output_context2(&fmt_ctx_out, NULL, NULL, filename_out);
    if (!fmt_ctx_out) {
        printf("error code %d \n", AVERROR(ENOMEM));
        return ENOMEM;
    }
    //���һ·��������������
    AVStream* st = avformat_new_stream(fmt_ctx_out, NULL);
    st->time_base = fmt_ctx->streams[0]->time_base;

    AVCodecContext* enc_ctx = NULL;

    AVPacket* pkt = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    AVPacket* pkt_out = av_packet_alloc();

    int read_end = 0;
    for (;;) {
        if (1 == read_end) {
            break;
        }

        ret = av_read_frame(fmt_ctx, pkt);
        //������������Ƶ��
        if (1 == pkt->stream_index) {
            av_packet_unref(pkt);
            continue;
        }

        if (AVERROR_EOF == ret) {
            //��ȡ���ļ�����ʱ�� pkt �� data �� size Ӧ���� null
            avcodec_send_packet(avctx, NULL);
        } else {
            if (0 != ret) {
                printf("read error code %d \n", ret);
                return ENOMEM;
            } else {
            retry:
                if (avcodec_send_packet(avctx, pkt) == AVERROR(EAGAIN)) {
                    printf("Receive_frame and send_packet both returned EAGAIN, which is an API violation.\n");
                    //������Կ������� 0.1 �룬���� EAGAIN ͨ���� ffmpeg ���ڲ� api ��bug
                    goto retry;
                }
                //�ͷ� pkt ����ı�������
                av_packet_unref(pkt);
            }

        }

        //ѭ�����ϴӽ����������ݣ�ֱ��û�����ݿɶ���
        for (;;) {
            //��ȡ AVFrame
            ret = avcodec_receive_frame(avctx, frame);
            /* �ͷ� frame �����YUV���ݣ�
             * ���� avcodec_receive_frame ������������ av_frame_unref����������Ĵ������ע�͡�
             * �������ǲ���Ҫ �ֶ� unref ��� AVFrame
             * */
             //av_frame_unref(frame);

            if (AVERROR(EAGAIN) == ret) {
                //��ʾ EAGAIN ���� ������ ��Ҫ ����� AVPacket
                //���� ��һ�� for���� �������õ������ AVPacket
                break;
            } else if (AVERROR_EOF == ret) {
                /* ��ʾ AVERROR_EOF ����֮ǰ�Ѿ��� ������������һ�� data �� size ���� NULL �� AVPacket
                 * ���� NULL �� AVPacket ����ʾ�����������еĻ���֡ȫ��ˢ������
                 * ͨ��ֻ���� ���������ļ��Żᷢ�� NULL �� AVPacket��������Ҫ�����еĽ�����������һ������Ƶ���Ż���ô�ɡ�
                 *
                 * */

                 /* ������������ null �� AVFrame���ñ�������ʣ�µ�����ˢ������
                  * */
                ret = avcodec_send_frame(enc_ctx, NULL);
                for (;;) {
                    ret = avcodec_receive_packet(enc_ctx, pkt_out);
                    //���ﲻ���ܷ��� EAGAIN�������ֱ���˳���
                    if (ret == AVERROR(EAGAIN)) {
                        printf("avcodec_receive_packet error code %d \n", ret);
                        return ret;
                    }
                    if (AVERROR_EOF == ret) {
                        break;
                    }
                    //����� AVPacket ���ȴ�ӡһЩ��Ϣ��Ȼ�����д���ļ���
                    //printf("pkt_out size : %d \n",pkt_out->size);
                    //���� AVPacket �� stream_index ��������֪�����ĸ����ġ�
                    pkt_out->stream_index = st->index;
                    //ת�� AVPacket ��ʱ���Ϊ �������ʱ�����
                    pkt_out->pts = av_rescale_q_rnd(pkt_out->pts, fmt_ctx->streams[0]->time_base, st->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
                    pkt_out->dts = av_rescale_q_rnd(pkt_out->dts, fmt_ctx->streams[0]->time_base, st->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
                    pkt_out->duration = av_rescale_q_rnd(pkt_out->duration, fmt_ctx->streams[0]->time_base, st->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));

                    ret = av_interleaved_write_frame(fmt_ctx_out, pkt_out);
                    if (ret < 0) {
                        printf("av_interleaved_write_frame faile %d \n", ret);
                        return ret;
                    }
                    av_packet_unref(pkt_out);
                }
                av_write_trailer(fmt_ctx_out);
                //���� �ڶ��� for���ļ��Ѿ�������ϡ�
                read_end = 1;
                break;
            } else if (ret >= 0) {
                //ֻ�н������һ��֡���ſ��Կ�ʼ��ʼ����������
                if (NULL == enc_ctx) {
                    //�򿪱��������������� ������Ϣ��
                    AVCodec* encode = avcodec_find_encoder(AV_CODEC_ID_H264);
                    enc_ctx = avcodec_alloc_context3(encode);
                    enc_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
                    enc_ctx->bit_rate = 400000;
                    enc_ctx->framerate = avctx->framerate;
                    enc_ctx->gop_size = 30;
                    enc_ctx->max_b_frames = 10;
                    enc_ctx->profile = FF_PROFILE_H264_MAIN;
                    /*
                     * ��ʵ������Щ��Ϣ����������Ҳ�У�Ҳ����һ��ʼֱ������������򿪱�����
                     * �Ҵ� AVFrame ������Щ��������������Ϊ�������Ĳ�һ���������յġ�
                     * ��Ϊ���������� AVFrame ���ܻᾭ�� filter �˾��������˾�֮����Ϣ�ͻ�任�����Ǳ���û��ʹ���˾���
                     */
                     //��������ʱ���Ҫȡ AVFrame ��ʱ�������Ϊ AVFrame �����롣AVFrame ��ʱ������� ����ʱ�����
                    enc_ctx->time_base = fmt_ctx->streams[0]->time_base;
                    enc_ctx->width = fmt_ctx->streams[0]->codecpar->width;
                    enc_ctx->height = fmt_ctx->streams[0]->codecpar->height;
                    enc_ctx->sample_aspect_ratio = st->sample_aspect_ratio = frame->sample_aspect_ratio;
                    enc_ctx->pix_fmt = (AVPixelFormat)frame->format;
                    enc_ctx->color_range = frame->color_range;
                    enc_ctx->color_primaries = frame->color_primaries;
                    enc_ctx->color_trc = frame->color_trc;
                    enc_ctx->colorspace = frame->colorspace;
                    enc_ctx->chroma_sample_location = frame->chroma_location;

                    /* ע�⣬��� field_order ��ͬ����Ƶ��ֵ�ǲ�һ���ģ�������д���ˡ�
                     * ��Ϊ ���ĵ���Ƶ���� AV_FIELD_PROGRESSIVE
                     * ��������Ҫ�Բ�ͬ����Ƶ�������
                     */
                    enc_ctx->field_order = AV_FIELD_PROGRESSIVE;

                    /* ����������Ҫ�� �������������Ƹ����������ʱ���� ������ֵ��������������
                     * ����Ҫ��������
                     * */
                    ret = avcodec_parameters_from_context(st->codecpar, enc_ctx);
                    if (ret < 0) {
                        printf("error code %d \n", ret);
                        return ret;
                    }
                    if ((ret = avcodec_open2(enc_ctx, encode, NULL)) < 0) {
                        printf("open codec faile %d \n", ret);
                        return ret;
                    }
                    //��ʽ������ļ�
                    if ((ret = avio_open2(&fmt_ctx_out->pb, filename_out, AVIO_FLAG_WRITE, &fmt_ctx_out->interrupt_callback, NULL)) < 0) {
                        printf("avio_open2 fail %d \n", ret);
                        return ret;
                    }
                    //Ҫ��д���ļ�ͷ����
                    ret = avformat_write_header(fmt_ctx_out, NULL);
                    if (ret < 0) {
                        printf("avformat_write_header fail %d \n", ret);
                        return ret;
                    }
                }

                //������������ AVFrame��Ȼ�󲻶϶�ȡ AVPacket
                ret = avcodec_send_frame(enc_ctx, frame);
                if (ret < 0) {
                    printf("avcodec_send_frame fail %d \n", ret);
                    return ret;
                }
                for (;;) {
                    ret = avcodec_receive_packet(enc_ctx, pkt_out);
                    if (ret == AVERROR(EAGAIN)) {
                        break;
                    }
                    if (ret < 0) {
                        printf("avcodec_receive_packet fail %d \n", ret);
                        return ret;
                    }
                    //����� AVPacket ���ȴ�ӡһЩ��Ϣ��Ȼ�����д���ļ���
                    //printf("pkt_out size : %d \n",pkt_out->size);

                    //���� AVPacket �� stream_index ��������֪�����ĸ����ġ�
                    pkt_out->stream_index = st->index;
                    //ת�� AVPacket ��ʱ���Ϊ �������ʱ�����
                    pkt_out->pts = av_rescale_q_rnd(pkt_out->pts, fmt_ctx->streams[0]->time_base, st->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
                    pkt_out->dts = av_rescale_q_rnd(pkt_out->dts, fmt_ctx->streams[0]->time_base, st->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
                    pkt_out->duration = av_rescale_q_rnd(pkt_out->duration, fmt_ctx->streams[0]->time_base, st->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));

                    ret = av_interleaved_write_frame(fmt_ctx_out, pkt_out);
                    if (ret < 0) {
                        printf("av_interleaved_write_frame faile %d \n", ret);
                        return ret;
                    }
                    av_packet_unref(pkt_out);
                }
            } else {
                printf("other fail \n");
                return ret;
            }
        }
    }


    av_free(&avio_ctx_buffer);
    avio_context_free(&avio_ctx);
    av_frame_free(&frame);
    av_packet_free(&pkt);
    av_packet_free(&pkt_out);

    //�رձ���������������
    avcodec_close(avctx);
    avcodec_close(enc_ctx);

    //�ͷ������ڴ档
    avformat_free_context(fmt_ctx);
    avformat_free_context(fmt_ctx_out);
    printf("done \n");

    return 0;
}
