#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <Windows.h>
#define __STDC_CONSTANT_MACROS

typedef void(*VideoCallback)(unsigned char* data_y, unsigned char* data_u, unsigned char* data_v,
	int line1, int line2, int line3, int width, int height, long pts, long);

#ifdef __cplusplus
extern "C" {
#endif

#include "libavcodec/avcodec.h"
#include "libavutil/imgutils.h" 
#include "libavutil/opt.h"
#include "libswscale/swscale.h"

#define INBUF_SIZE 166264 + 8

	typedef enum ErrorCode {
		kErrorCode_Success = 0,
		kErrorCode_Invalid_Param,
		kErrorCode_Invalid_State,
		kErrorCode_Invalid_Data,
		kErrorCode_Invalid_Format,
		kErrorCode_NULL_Pointer,
		kErrorCode_Open_File_Error,
		kErrorCode_Eof,
		kErrorCode_FFmpeg_Error
	}ErrorCode;

	typedef enum LogLevel {
		kLogLevel_None, //Not logging.
		kLogLevel_Core, //Only logging core module(without ffmpeg).
		kLogLevel_All   //Logging all, with ffmpeg.
	}LogLevel;

	typedef enum DecoderType {
		kDecoderType_H264,
		kDecoderType_H265
	}DecoderType;

	typedef unsigned int UINT32;

	typedef struct WebDecoder {
		unsigned char* dest_data;
		AVFrame* frame_dest;
		struct SwsContext* img_convert_ctx;
		int current_width;
		int current_height;
		int out_width;
		int out_height;
		int init_flag;
		VideoCallback videoCallback;
		unsigned char* yuv_buffer;
		int video_size;
		int is_init;
		const AVCodec* codec;
		AVCodecParserContext* parser;
		AVCodecContext* codec_ctx;
		AVPacket* pkt;
		AVFrame* frame;
		AVFrame* out_frame;
		long ptslist[10];

		LogLevel log_level;
		DecoderType decoder_type;
	} WebDecoder;


	WebDecoder* g_decoder = NULL;
	char* temp_data = NULL;
	int temp_size = 0;

	void simpleLog(const char* format, ...) {
		if (g_decoder->log_level == kLogLevel_None) {
			return;
		}

		char szBuffer[1024] = { 0 };
		char szTime[32] = { 0 };
		char* p = NULL;
		int prefixLength = 0;
		const char* tag = "Core";

		prefixLength = sprintf(szBuffer, "[%s][%s][DT] ", szTime, tag);
		p = szBuffer + prefixLength;

		if (1) {
			va_list ap;
			va_start(ap, format);
			vsnprintf(p, 1024 - prefixLength, format, ap);
			va_end(ap);
		}

		printf("%s\n", szBuffer);
	}

	void ffmpegLogCallback(void* ptr, int level, const char* fmt, va_list vl) {
		static int printPrefix = 1;
		static int count = 0;
		static char prev[1024] = { 0 };
		char line[1024] = { 0 };
		static int is_atty;
		AVClass* avc = ptr ? *(AVClass**)ptr : NULL;
		if (level > AV_LOG_DEBUG) {
			return;
		}

		line[0] = 0;

		if (printPrefix && avc) {
			if (avc->parent_log_context_offset) {
				AVClass** parent = *(AVClass***)(((uint8_t*)ptr) + avc->parent_log_context_offset);
				if (parent && *parent) {
					snprintf(line, sizeof(line), "[%s @ %p] ", (*parent)->item_name(parent), parent);
				}
			}
			snprintf(line + strlen(line), sizeof(line) - strlen(line), "[%s @ %p] ", avc->item_name(ptr), ptr);
		}

		vsnprintf(line + strlen(line), sizeof(line) - strlen(line), fmt, vl);
		line[strlen(line) + 1] = 0;
		simpleLog("%s", line);
	}

	void initSws(enum AVPixelFormat nSrcImage, int nSrcWidth, int nSrcHeight, enum AVPixelFormat nDstImage, int nDstWidth, int nDestHeight)
	{
		if (g_decoder->init_flag)
			return;

		g_decoder->current_width = nSrcWidth;
		g_decoder->current_height = nSrcHeight;
		g_decoder->out_width = nDstWidth;
		g_decoder->out_height = nDestHeight;
		int numBytes2 = av_image_get_buffer_size((enum AVPixelFormat)nDstImage, nDstWidth, nDestHeight, 1);
		g_decoder->frame_dest = av_frame_alloc();
		g_decoder->dest_data = (unsigned char*)malloc(numBytes2);
		av_image_fill_arrays(g_decoder->frame_dest->data, g_decoder->frame_dest->linesize,
			g_decoder->dest_data, nDstImage, g_decoder->out_width, g_decoder->out_height, 1);

		g_decoder->img_convert_ctx = sws_getContext(nSrcWidth, nSrcHeight,  //长、宽（原始的）
			nSrcImage,                //原像素格式
			nDstWidth, nDestHeight,    //长、宽 (目标的)
			(enum AVPixelFormat)nDstImage, //目标像素
			4, NULL, NULL, NULL);//SWS_BICUBIC

		g_decoder->frame_dest->width = nDstWidth;
		g_decoder->frame_dest->height = nDestHeight;
		g_decoder->frame_dest->format = AV_PIX_FMT_YUV420P;
		g_decoder->init_flag = 1;
	}

	void aVFrameSWS(AVFrame* inframe) {

		if (g_decoder->init_flag && g_decoder->img_convert_ctx != NULL) {
			sws_scale(g_decoder->img_convert_ctx, inframe->data, inframe->linesize, 0,
				g_decoder->current_height, g_decoder->frame_dest->data, g_decoder->frame_dest->linesize);
		}

	}

	ErrorCode copyFrameData(AVFrame* src, AVFrame* dst, long ptslist[]) {
		ErrorCode ret = kErrorCode_Success;
		memcpy(dst->data, src->data, sizeof(src->data));
		dst->linesize[0] = src->linesize[0];
		dst->linesize[1] = src->linesize[1];
		dst->linesize[2] = src->linesize[2];
		dst->width = src->width;
		dst->height = src->height;
		long pts = LONG_MAX;
		int index = -1;
		for (int i = 0; i < 10; i++) {
			if (ptslist[i] < pts) {
				pts = ptslist[i];
				index = i;
			}
		}
		if (index > -1) {
			ptslist[index] = LONG_MAX;
		}
		dst->pts = pts;
		return ret;
	}

	void initBuffer(int width, int height) {
		g_decoder->video_size = av_image_get_buffer_size(AV_PIX_FMT_YUV420P, width, height, 1);
		int bufferSize = 3 * g_decoder->video_size;
		g_decoder->yuv_buffer = (unsigned char*)av_mallocz(bufferSize);
	}

	int alltime = 0;
	int allframe = 0;

	static ErrorCode decode(AVCodecContext* dec_ctx, AVFrame* frame, AVPacket* pkt, AVFrame* outFrame, long ptslist[])
	{
		ErrorCode res = kErrorCode_Success;
		int ret;
		clock_t start_time = clock();
		ret = avcodec_send_packet(dec_ctx, pkt);
		if (ret < 0) {
			simpleLog("*** Error sending a packet for decoding %d    %d \n", pkt->size, ret);
			res = kErrorCode_FFmpeg_Error;
		} else {
			while (ret >= 0) {
				ret = avcodec_receive_frame(dec_ctx, frame);
				if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
					break;
				} else if (ret < 0) {
					simpleLog("Error during decoding\n");
					res = kErrorCode_FFmpeg_Error;
					break;
				}
				//initSws((enum AVPixelFormat)frame->format, 1920, 1080, AV_PIX_FMT_YUV420P, 352, 288);
				//aVFrameSWS(frame);
				res = copyFrameData(frame, outFrame, ptslist);
				if (res != kErrorCode_Success) {
					break;
				}
				clock_t end_time = clock();
				int cost_time = (int)(end_time - start_time) / 1000;
				alltime += cost_time;
				allframe++;
				if (g_decoder->videoCallback) {
					g_decoder->videoCallback(outFrame->data[0], outFrame->data[1], outFrame->data[2], outFrame->linesize[0], outFrame->linesize[1], outFrame->linesize[2],
						outFrame->width, outFrame->height, outFrame->pts, cost_time);
				} else {
					simpleLog("videoCallback is nullptr");
				}

			}
		}
		return res;
	}

	/*
		导出函数
	*/

	ErrorCode openDecoder(int codecType, VideoCallback callback, int logLv) {
		g_decoder = (WebDecoder*)malloc(sizeof(WebDecoder));
		memset(g_decoder, 0, sizeof(WebDecoder));

		ErrorCode ret = kErrorCode_Success;
		do {
			simpleLog("Initialize decoder");

			if (g_decoder->is_init != 0) {
				break;
			}

			g_decoder->decoder_type = (DecoderType)codecType;
			g_decoder->log_level = (LogLevel)logLv;

			if (g_decoder->log_level == kLogLevel_All) {
				av_log_set_callback(ffmpegLogCallback);
			}

			/* find the video decoder */
			if (g_decoder->decoder_type == kDecoderType_H264) {
				g_decoder->codec = avcodec_find_decoder(AV_CODEC_ID_H264);
			} else {
				g_decoder->codec = avcodec_find_decoder(AV_CODEC_ID_H265);
			}

			if (!g_decoder->codec) {
				simpleLog("Codec not found\n");
				ret = kErrorCode_FFmpeg_Error;
				break;
			}

			g_decoder->parser = av_parser_init(g_decoder->codec->id);
			if (!g_decoder->parser) {
				simpleLog("parser not found id:%d \n", g_decoder->codec->id);
				ret = kErrorCode_FFmpeg_Error;
				break;
			}

			g_decoder->codec_ctx = avcodec_alloc_context3(g_decoder->codec);

			if (!g_decoder->codec_ctx) {
				simpleLog("Could not allocate video codec context\n");
				ret = kErrorCode_FFmpeg_Error;
				break;
			}
			AVDictionary* para = NULL;
			av_dict_set(&para, "x265-params", "dp=20", 0);
			av_dict_set(&para, "tune", "zerolatency", 0);
			av_dict_set(&para, "preset", "ultrafast", 0);
			//av_dict_set(&para, "rtsp_transport", "tcp", 0);
			if (avcodec_open2(g_decoder->codec_ctx, g_decoder->codec, &para) < 0) {
				simpleLog("Could not open codec\n");
				ret = kErrorCode_FFmpeg_Error;
				break;
			}

			g_decoder->frame = av_frame_alloc();
			if (!g_decoder->frame) {
				simpleLog("Could not allocate video frame\n");
				ret = kErrorCode_FFmpeg_Error;
				break;
			}

			g_decoder->out_frame = av_frame_alloc();
			if (!g_decoder->out_frame) {
				simpleLog("Could not allocate video frame\n");
				ret = kErrorCode_FFmpeg_Error;
				break;
			}

			g_decoder->pkt = av_packet_alloc();
			if (!g_decoder->pkt) {
				simpleLog("Could not allocate video packet\n");
				ret = kErrorCode_FFmpeg_Error;
				break;
			}

			for (int i = 0; i < 10; i++) {
				g_decoder->ptslist[i] = LONG_MAX;
			}

			g_decoder->videoCallback = (VideoCallback)callback;

		} while (0);
		simpleLog("Decoder initialized %d.", ret);
		return ret;
	}
	clock_t begin_time = 0;

	ErrorCode decodeData(unsigned char* data, size_t data_size, long pts, long time) {
		if (begin_time == 0)
			begin_time = clock();
		simpleLog("data_size:%d time:%d  begin_time:%d", data_size, time, begin_time);
		ErrorCode ret = kErrorCode_Success;

		for (int i = 0; i < 10; i++) {
			if (g_decoder->ptslist[i] == LONG_MAX) {
				g_decoder->ptslist[i] = pts;
				break;
			}
		}

		while (data_size > 0) {

			int size = av_parser_parse2(g_decoder->parser, g_decoder->codec_ctx, &g_decoder->pkt->data, &g_decoder->pkt->size,
				data, (int)data_size, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);

			if (size < 0) {
				simpleLog("Error while parsing\n");
				ret = kErrorCode_FFmpeg_Error;
				break;
			}

			data += size;
			data_size -= size;

			//if (size == 1) {
			//	simpleLog("*** %c %c",data[0], g_decoder->pkt->data[g_decoder->pkt->size - 1]);
			//}

			if (g_decoder->pkt->size) {
				simpleLog("--- pkt->size:%d  size:%d ", g_decoder->pkt->size, size);
				clock_t parser_time = clock();
				ret = decode(g_decoder->codec_ctx, g_decoder->frame, g_decoder->pkt, g_decoder->out_frame, g_decoder->ptslist);
				clock_t deocder_time = clock();
				simpleLog(" *** parser_time %d  - begin_time :%d = %d", parser_time, begin_time, (parser_time - begin_time) / 1000);
				simpleLog(" *** deocder_time %d  - parser_time :%d = %d", deocder_time, parser_time, (deocder_time - parser_time) / 1000);
				begin_time = 0;
				if (ret != kErrorCode_Success) {
					break;
				}


			}
		}
		return ret;
	}

	ErrorCode flushDecoder() {
		/* flush the decoder */
		return decode(g_decoder->codec_ctx, g_decoder->frame, NULL, g_decoder->out_frame, g_decoder->ptslist);
	}

	ErrorCode closeDecoder() {
		ErrorCode ret = kErrorCode_Success;

		do {
			if (g_decoder->parser != NULL) {
				av_parser_close(g_decoder->parser);
				simpleLog("Video codec context closed.");
			}
			if (g_decoder->codec_ctx != NULL) {
				avcodec_free_context(&g_decoder->codec_ctx);
				simpleLog("Video codec context closed.");
			}
			if (g_decoder->frame != NULL) {
				av_frame_free(&g_decoder->frame);
			}
			if (g_decoder->pkt != NULL) {
				av_packet_free(&g_decoder->pkt);
			}
			if (g_decoder->yuv_buffer != NULL) {
				av_freep(&g_decoder->yuv_buffer);
			}
			if (g_decoder->out_frame != NULL) {
				av_frame_free(&g_decoder->out_frame);
			}
			simpleLog("All buffer released.%d frames %d ms", allframe, alltime);
		} while (0);
		if (g_decoder)
			free(g_decoder);
		return ret;
	}

	int callbackIndex = 0;
	FILE* dst = NULL;
	void vcb_frame(unsigned char* data_y, unsigned char* data_u, unsigned char* data_v,
		int line1, int line2, int line3, int width, int height, long pts, long cost_time)
	{
		//simpleLog("[%d]In video call back, size = %d * %d, pts = %ld  cost_time:%d ms",
		//	++callbackIndex, width, height, pts, cost_time);

		int i = 0;
		unsigned char* src = NULL;
		for (i = 0; i < height; i++) {
			src = data_y + i * line1;
			fwrite(src, width, 1, dst);
		}

		for (i = 0; i < height / 2; i++) {
			src = data_u + i * line2;
			fwrite(src, width / 2, 1, dst);
		}

		for (i = 0; i < height / 2; i++) {
			src = data_v + i * line3;
			fwrite(src, width / 2, 1, dst);
		}

	}

	int main(int argc, char** argv)
	{
		//return 0;
		dst = fopen("d:/vcb.yuv", "wb+");
		openDecoder(1, vcb_frame, kLogLevel_Core);

		//const char* filename = "Forrest_Gump_IMAX.h264";
		//const char* filename = "FourPeople_1280x720_60_1M.265";
		const char* filename = "d:/15s.h265";

		FILE* f;

		//   uint8_t *data;
		size_t   data_size;

		//uint8_t inbuf[INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];
		///* set end of buffer to 0 (this ensures that no overreading happens for damaged MPEG streams) */
		//memset(inbuf + INBUF_SIZE, 0, AV_INPUT_BUFFER_PADDING_SIZE);

		unsigned char* buffer = (unsigned char*)malloc(sizeof(char) * (INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE));
		if (buffer == NULL) {
			simpleLog("Memory error");
			exit(2);
		}

		f = fopen(filename, "rb");
		if (!f) {
			simpleLog("Could not open %s\n", filename);
			exit(1);
		}

		while (!feof(f)) {
			/* read raw data from the input file */
			//data_size = fread(inbuf, 1, INBUF_SIZE, f);
			data_size = fread(buffer, 1, INBUF_SIZE, f);

			if (!data_size)
				break;

			/* use the parser to split the data into frames */
			//data = inbuf;
			static int time = 1;
			if (time > 1000)
				time = 0;
			decodeData(buffer, data_size, 0, time++);
		}
		fclose(f);
		free(buffer);

		printf("cost all time :%d ms frame:%d \n ", alltime, callbackIndex);
		flushDecoder();
		closeDecoder();
		fclose(dst);

		return 0;
	}


#ifdef __cplusplus
}
#endif
