
/* Copyright [c] 2017-2027 By Gang.Wang Allrights Reserved
This file give a simple example of how to get camera video form
PC . Any questions, you can join QQ group for help, QQ
Group number:127903734.
*/
#include "stdafx.h"
#include "pch.h"
#include <string>
#include <iostream>
#include <thread>
#include <memory>
using namespace std;
AVFormatContext * context = nullptr;
AVFormatContext* outputContext;
int64_t  lastPts = 0;
int64_t  lastDts = 0;
int64_t lastFrameRealtime = 0;

int64_t firstPts = AV_NOPTS_VALUE;
int64_t startTime = 0;

AVCodecContext*	pOutPutEncContext = NULL;
#define SrcWidth 1920
#define SrcHeight 1080
#define DstWidth 640
#define DstHeight 480
int interrupt_cb(void *ctx)
{
	return 0;
}

void Init()
{
	av_register_all();
	avfilter_register_all();
	avformat_network_init();
	avdevice_register_all();
	av_log_set_level(AV_LOG_ERROR);
}

int OpenInput(char *fileName)
{
	context = avformat_alloc_context();
	context->interrupt_callback.callback = interrupt_cb;
	AVInputFormat *ifmt = av_find_input_format("dshow");
	AVDictionary *format_opts = nullptr;
	av_dict_set_int(&format_opts, "rtbufsize", 73728000, 0);
	int ret = avformat_open_input(&context, fileName, ifmt, &format_opts);
	if (ret < 0)
	{
		return  ret;
	}
	ret = avformat_find_stream_info(context, nullptr);
	av_dump_format(context, 0, fileName, 0);
	if (ret >= 0)
	{
		cout << "open input stream successfully" << endl;
	}
	return ret;
}

std::shared_ptr<AVPacket> ReadPacketFromSource()
{
	std::shared_ptr<AVPacket> packet(static_cast<AVPacket*>(av_malloc(sizeof(AVPacket))), [&](AVPacket *p) { av_packet_free(&p); av_freep(&p); });
	av_init_packet(packet.get());
	lastFrameRealtime = av_gettime();
	int ret = av_read_frame(context, packet.get());
	if (ret >= 0)
	{
		return packet;
	}
	else
	{
		return nullptr;
	}
}
void CloseInput()
{
	if (context != nullptr)
	{
		for (int i = 0; i < context->nb_streams; i++)
		{
			AVCodecContext *codecContext = context->streams[i]->codec;
			avcodec_close(codecContext);
		}
		avformat_close_input(&context);
	}
}

int OpenOutput(char *fileName)
{
	int ret = 0;
	ret = avformat_alloc_output_context2(&outputContext, nullptr, "flv", fileName);
	if (ret < 0)
	{
		goto Error;
	}
	ret = avio_open2(&outputContext->pb, fileName, AVIO_FLAG_READ_WRITE, nullptr, nullptr);
	if (ret < 0)
	{
		goto Error;
	}

	for (int i = 0; i < context->nb_streams; i++)
	{
		AVStream * stream = avformat_new_stream(outputContext, pOutPutEncContext->codec);
		stream->codec = pOutPutEncContext;
		if (ret < 0)
		{
			goto Error;
		}
	}
	av_dump_format(outputContext, 0, fileName, 1);
	ret = avformat_write_header(outputContext, nullptr);
	if (ret < 0)
	{
		goto Error;
	}
	if (ret >= 0)
		cout << "open output stream successfully" << endl;
	return ret;
Error:
	if (outputContext)
	{
		for (int i = 0; i < outputContext->nb_streams; i++)
		{
			avcodec_close(outputContext->streams[i]->codec);
		}
		avformat_close_input(&outputContext);
	}
	return ret;
}

void CloseOutput()
{
	if (outputContext != nullptr)
	{
		for (int i = 0; i < outputContext->nb_streams; i++)
		{
			AVCodecContext *codecContext = outputContext->streams[i]->codec;
			avcodec_close(codecContext);
		}
		avformat_close_input(&outputContext);
	}
}

int InitOutputCodec(AVCodecContext** pOutPutEncContext, int iWidth, int iHeight)
{
	AVCodec *  pH264Codec = avcodec_find_encoder(AV_CODEC_ID_H264);
	if (NULL == pH264Codec)
	{
		printf("%s", "avcodec_find_encoder failed");
		return  0;
	}
	*pOutPutEncContext = avcodec_alloc_context3(pH264Codec);
	(*pOutPutEncContext)->codec_id = pH264Codec->id;
	(*pOutPutEncContext)->time_base.num = 0;
	(*pOutPutEncContext)->time_base.den = 1;
	(*pOutPutEncContext)->pix_fmt = AV_PIX_FMT_YUV420P;
	(*pOutPutEncContext)->width = iWidth;
	(*pOutPutEncContext)->height = iHeight;
	(*pOutPutEncContext)->has_b_frames = 0;
	(*pOutPutEncContext)->max_b_frames = 0;

	AVDictionary *options = nullptr;
	(*pOutPutEncContext)->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	int ret = avcodec_open2(*pOutPutEncContext, pH264Codec, &options);
	if (ret < 0)
	{
		printf("%s", "open codec failed");
		return  ret;
	}
	return 1;
}

int YUV422To420(uint8_t *yuv422, uint8_t *yuv420, int width, int height)
{
	int s = width * height;
	int i, j, k = 0;
	for (i = 0; i < s; i++)
	{
		yuv420[i] = yuv422[i * 2];
	}

	for (i = 0; i < height; i++)
	{
		if (i % 2 != 0) continue;
		for (j = 0; j <(width / 2); j++)
		{
			if (4 * j + 1 > 2 * width) break;
			yuv420[s + k * 2 * width / 4 + j] = yuv422[i * 2 * width + 4 * j + 1];
		}
		k++;
	}

	k = 0;

	for (i = 0; i < height; i++)
	{
		if (i % 2 == 0) continue;
		for (j = 0; j < width / 2; j++)
		{
			if (4 * j + 3 > 2 * width) break;
			yuv420[s + s / 4 + k * 2 * width / 4 + j] = yuv422[i * 2 * width + 4 * j + 3];
		}
		k++;
	}
	return 1;
};
int _tmain(int argc, _TCHAR* argv[])
{
	string fileInput = "video=Integrated Webcam";
	string fileOutput = "rtmp://192.168.1.117/live/stream0";

	Init();
	if (OpenInput((char *)fileInput.c_str()) < 0)
	{
		cout << "Open file Input failed!" << endl;
		this_thread::sleep_for(chrono::seconds(10));
		return 0;
	}
	InitOutputCodec(&pOutPutEncContext, DstWidth, DstHeight);
	if (OpenOutput((char *)fileOutput.c_str()) < 0)
	{
		cout << "Open file Output failed!" << endl;
		this_thread::sleep_for(chrono::seconds(10));
		return 0;
	}
	auto timebase = av_q2d(context->streams[0]->time_base);
	int count = 0;
	auto in_stream = context->streams[0];
	auto out_stream = outputContext->streams[0];
	int iGetPic = 0;
	uint8_t *yuv420Buffer = (uint8_t *)malloc(DstWidth * DstHeight * 3 / 2);
	yuv420Buffer[DstWidth * DstHeight * 3 / 2 - 1] = 0;
	while (true)
	{
		auto packet = ReadPacketFromSource();
		if (packet)
		{
			auto pSwsFrame = av_frame_alloc();
			int numBytes = av_image_get_buffer_size(AV_PIX_FMT_YUYV422, DstWidth, DstHeight, 1);
			YUV422To420(packet->data, yuv420Buffer, DstWidth, DstHeight);
			av_image_fill_arrays((pSwsFrame)->data, (pSwsFrame)->linesize, yuv420Buffer, AV_PIX_FMT_YUV420P, DstWidth, DstHeight, 1);
			AVPacket *pTmpPkt = (AVPacket *)av_malloc(sizeof(AVPacket));
			av_init_packet(pTmpPkt);
			pTmpPkt->data = NULL;
			pTmpPkt->size = 0;

			int iRet = avcodec_encode_video2(pOutPutEncContext, pTmpPkt, pSwsFrame, &iGetPic);
			if (iRet >= 0 && iGetPic)
			{
				int ret = av_write_frame(outputContext, pTmpPkt);
			}
			av_frame_free(&pSwsFrame);
			av_packet_free(&pTmpPkt);
		}
	}
	CloseInput();
	CloseOutput();
	cout << "Transcode file end!" << endl;
	this_thread::sleep_for(chrono::hours(10));
	return 0;
}

