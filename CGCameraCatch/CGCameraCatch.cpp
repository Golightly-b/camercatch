// CGCameraCatch.cpp : 定义控制台应用程序的入口点。
//
/* Copyright [c] 2018-2028 By www.chungen90.com Allrights Reserved 
   This file give a simple example of getting camera video form 
   PC.   Any questions, you can join QQ group for
   help, QQ   Group number:127903734 or 766718184.
*/
#include "stdafx.h"
#include <string>
#include <memory>
#include <thread>
#include <iostream>
#include <iostream>
using namespace std;
#include "pch.h"

AVFormatContext *inputContext = nullptr;
AVFormatContext * outputContext;
int64_t lastReadPacktTime ;
int64_t packetCount = 0;
static int interrupt_cb(void *ctx)
{
	int  timeout  = 3;
	if(av_gettime() - lastReadPacktTime > timeout *1000 *1000)
	{
		return -1;
	}
	return 0;
}

int OpenInput(string inputUrl)
{
	inputContext = avformat_alloc_context();	
	lastReadPacktTime = av_gettime();
	inputContext->interrupt_callback.callback = interrupt_cb;
	 AVInputFormat *ifmt = av_find_input_format("dshow");
	 AVDictionary *format_opts =  nullptr;
	 av_dict_set_int(&format_opts, "rtbufsize", 18432000  , 0);

	int ret = avformat_open_input(&inputContext, inputUrl.c_str(), ifmt,&format_opts);
	if(ret < 0)
	{
		av_log(NULL, AV_LOG_ERROR, "Input file open input failed\n");
		return  ret;
	}
	ret = avformat_find_stream_info(inputContext,nullptr);
	if(ret < 0)
	{
		av_log(NULL, AV_LOG_ERROR, "Find input file stream inform failed\n");
	}
	else
	{
		av_log(NULL, AV_LOG_FATAL, "Open input file  %s success\n",inputUrl.c_str());
	}
	return ret;
}


shared_ptr<AVPacket> ReadPacketFromSource()
{
	shared_ptr<AVPacket> packet(static_cast<AVPacket*>(av_malloc(sizeof(AVPacket))), [&](AVPacket *p) { av_packet_free(&p); av_freep(&p);});
	av_init_packet(packet.get());
	lastReadPacktTime = av_gettime();
	int ret = av_read_frame(inputContext, packet.get());
	if(ret >= 0)
	{
		return packet;
	}
	else
	{
		return nullptr;
	}
}

int OpenOutput(string outUrl,AVCodecContext *encodeCodec)
{

	int ret  = avformat_alloc_output_context2(&outputContext, nullptr, "flv", outUrl.c_str());
	if(ret < 0)
	{
		av_log(NULL, AV_LOG_ERROR, "open output context failed\n");
		goto Error;
	}

	ret = avio_open2(&outputContext->pb, outUrl.c_str(), AVIO_FLAG_WRITE,nullptr, nullptr);	
	if(ret < 0)
	{
		av_log(NULL, AV_LOG_ERROR, "open avio failed");
		goto Error;
	}

	for(int i = 0; i < inputContext->nb_streams; i++)
	{
		if(inputContext->streams[i]->codec->codec_type == AVMediaType::AVMEDIA_TYPE_AUDIO)
		{
			continue;
		}
		AVStream * stream = avformat_new_stream(outputContext, encodeCodec->codec);				
		ret = avcodec_copy_context(stream->codec, encodeCodec);	
		if(ret < 0)
		{
			av_log(NULL, AV_LOG_ERROR, "copy coddec context failed");
			goto Error;
		}
	}

	ret = avformat_write_header(outputContext, nullptr);
	if(ret < 0)
	{
		av_log(NULL, AV_LOG_ERROR, "format write header failed");
		goto Error;
	}

	av_log(NULL, AV_LOG_FATAL, " Open output file success %s\n",outUrl.c_str());			
	return ret ;
Error:
	if(outputContext)
	{
		for(int i = 0; i < outputContext->nb_streams; i++)
		{
			avcodec_close(outputContext->streams[i]->codec);
		}
		avformat_close_input(&outputContext);
	}
	return ret ;
}

void Init()
{
	av_register_all();
	avfilter_register_all();
	avformat_network_init();
	avdevice_register_all();
	av_log_set_level(AV_LOG_ERROR);
}

void CloseInput()
{
	if(inputContext != nullptr)
	{
		avformat_close_input(&inputContext);
	}
}

void CloseOutput()
{
	if(outputContext != nullptr)
	{
		int ret = av_write_trailer(outputContext);
		for(int i = 0 ; i < outputContext->nb_streams; i++)
		{
			AVCodecContext *codecContext = outputContext->streams[i]->codec;
			avcodec_close(codecContext);
		}
		avformat_close_input(&outputContext);
	}
}

int WritePacket(shared_ptr<AVPacket> packet)
{
	auto inputStream = inputContext->streams[packet->stream_index];
	auto outputStream = outputContext->streams[packet->stream_index];				
	return av_interleaved_write_frame(outputContext, packet.get());
}

int InitDecodeContext(AVStream *inputStream)
{	
	auto codecId = inputStream->codec->codec_id;
	auto codec = avcodec_find_decoder(codecId);
	if (!codec)
	{
		return -1;
	}

	int ret = avcodec_open2(inputStream->codec, codec, NULL);
	return ret;

}

int initEncoderCodec(AVStream* inputStream,AVCodecContext **encodeContext)
	{
		AVCodec *  picCodec;
		
		picCodec = avcodec_find_encoder(AV_CODEC_ID_H264);		
		(*encodeContext) = avcodec_alloc_context3(picCodec);
	
		(*encodeContext)->codec_id = picCodec->id;
		(*encodeContext)->time_base.num = inputStream->codec->time_base.num;
		(*encodeContext)->time_base.den = inputStream->codec->time_base.den;
		(*encodeContext)->pix_fmt =  *picCodec->pix_fmts;
		(*encodeContext)->width = inputStream->codec->width;
		(*encodeContext)->height =inputStream->codec->height;
		(*encodeContext)->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
		int ret = avcodec_open2((*encodeContext), picCodec, nullptr);
		if (ret < 0)
		{
			std::cout<<"open video codec failed"<<endl;
			return  ret;
		}
			return 1;
	}

bool Decode(AVStream* inputStream,AVPacket* packet, AVFrame *frame)
{
	int gotFrame = 0;
	auto hr = avcodec_decode_video2(inputStream->codec, frame, &gotFrame, packet);
	if (hr >= 0 && gotFrame != 0)
	{
		return true;
	}
	return false;
}


std::shared_ptr<AVPacket> Encode(AVCodecContext *encodeContext,AVFrame * frame)
{
	int gotOutput = 0;
	std::shared_ptr<AVPacket> pkt(static_cast<AVPacket*>(av_malloc(sizeof(AVPacket))), [&](AVPacket *p) { av_packet_free(&p); av_freep(&p); });
	av_init_packet(pkt.get());
	pkt->data = NULL;
	pkt->size = 0;
	frame->pts = frame->pkt_dts = frame->pkt_pts = 40 * packetCount;
	int ret = avcodec_encode_video2(encodeContext, pkt.get(), frame, &gotOutput);
	if (ret >= 0 && gotOutput)
	{
		return pkt;
	}
	else
	{
		return nullptr;
	}
}


int _tmain(int argc, _TCHAR* argv[])
{
	Init();
	int ret = OpenInput("video=Integrated Webcam");
	
	if(ret <0) goto Error;

	AVCodecContext *encodeContext = nullptr;
	InitDecodeContext(inputContext->streams[0]);
	AVFrame *videoFrame = av_frame_alloc();
	initEncoderCodec(inputContext->streams[0],&encodeContext);

	if(ret >= 0)
	{
		ret = OpenOutput("rtmp://192.168.1.117/live/stream0",encodeContext); 
	}
	if(ret <0) goto Error;
	 while(true)
	 {
		auto packet = ReadPacketFromSource();
		if(packet && packet->stream_index == 0)
		{
			if(Decode(inputContext->streams[0],packet.get(),videoFrame))
			{
				auto packetEncode = Encode(encodeContext,videoFrame);
				if(packetEncode)
				{
					ret = WritePacket(packetEncode);
					cout <<"ret:" << ret<<endl;
				}

			}
						
		}
		
	 }
	 cout <<"Get Picture End "<<endl;
	 av_frame_free(&videoFrame);
	 avcodec_close(encodeContext);
	Error:
	CloseInput();
	CloseOutput();
	
	while(true)
	{
		this_thread::sleep_for(chrono::seconds(100));
	}
	return 0;
}


