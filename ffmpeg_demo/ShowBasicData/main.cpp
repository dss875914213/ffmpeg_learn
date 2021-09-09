#include <iostream>

extern "C"
{
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
#include "libswresample/swresample.h"
}

#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib,"avutil.lib")
#pragma comment(lib, "swscale.lib")
#pragma comment(lib, "swresample.lib")

AVFormatContext* m_pAvFormatCtx = nullptr;  // ��ý�����������
uint32_t		 m_nVideoStreamIndex = -1;	// ��Ƶ������ֵ
uint32_t		 m_nAudioStreamIndex = -1;	// ��Ƶ������ֵ
AVCodecContext* m_pVideoDecodeCtx = nullptr;// ��Ƶ������������
AVCodecContext* m_pAudioDecodeCtx = nullptr; // ��Ƶ������������

void Close();
int32_t Open(const char* pszFilePath);
void ReadInfo();
void ReadFrame();
int32_t DecodePktToFrame(AVCodecContext* pDecodeCtx, AVPacket* pInPacket, AVFrame** ppOutFrame);
void showPacketInfo(const AVFrame* avFrame, bool isVideo);
int32_t VideoConvert(const AVFrame* pInFrame, AVPixelFormat eOutFormat, int32_t nOutWidth, int32_t nOutHeight, AVFrame** ppOutFrame);
int32_t AudioConvert(const AVFrame* pInFrame, AVSampleFormat eOutSmplFmt, int32_t nOutChannels, int32_t nOutSmplRate, AVFrame** ppOutFrame);
int32_t VidEncoderOpen(
	AVPixelFormat	ePxlFormat,			// ����ͼ�����ظ�ʽ
	int32_t			nFrameWidth,		// ����ͼ����
	int32_t			nFrameHeight,		// ����ͼ��߶�
	int32_t			nFrameRate,			// ����֡��
	float			nBitRateFactor);	// ת�����ӣ�ͨ����Ϊ 0.8~8
void VidEncoderClose();
int32_t VidEncoderEncPacket(AVFrame* pInFrame, AVPacket** ppOutPacket);


int main()
{
	const char* videoPath = "test.mp4";
	if (0 == Open(videoPath))
	{
		// ReadInfo();
		ReadFrame();
	}
	int a;
	std::cin >> a;
}

int32_t Open(const char* pszFilePath)
{
	int res = 0;

	// �򿪶�ý���ļ�
	res = avformat_open_input(&m_pAvFormatCtx, pszFilePath, nullptr, nullptr);
	if (res != 0)
	{
		std::cout << "<Open> [ERROR] fail avformat_open_input(), res=" << res << std::endl;
		return res;
	}

	// ��������ý������Ϣ
	res = avformat_find_stream_info(m_pAvFormatCtx, nullptr);
	if (res != 0)
	{
		std::cout << "<Open> reached to file end" << std::endl;
	}

	AVCodec* pVideoDecoder = nullptr;
	AVCodec* pAudioDecoder = nullptr;
	// ��������ý������Ϣ
	for (unsigned int i = 0; i < m_pAvFormatCtx->nb_streams; i++)
	{
		AVStream* pAvStream = m_pAvFormatCtx->streams[i];
		if (pAvStream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			if ((pAvStream->codecpar->width <= 0) || (pAvStream->codecpar->height <= 0))
			{
				std::cout << "<Open> [ERROR] invalid resolution, streamIndex=" << i << std::endl;
				continue;
			}

			// ��Ƶ������
			pVideoDecoder = avcodec_find_decoder(pAvStream->codecpar->codec_id);
			if (pVideoDecoder == nullptr)
			{
				std::cout << "<Open> [ERROR] can not find video codec" << std::endl;
			}

			m_nVideoStreamIndex = (uint32_t)i;
			std::cout << "<Open> pxlFmt=" << (int)pAvStream->codecpar->format << ", frameSize=" <<
				pAvStream->codecpar->width << "*" << pAvStream->codecpar->height << std::endl;
		}
		else if (pAvStream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
		{
			if ((pAvStream->codecpar->channels <= 0) || (pAvStream->codecpar->sample_rate <= 0))
			{
				std::cout << "<Open> [ERROR] invalid resolution, streamIndex=" << i << std::endl;
				continue;
			}

			pAudioDecoder = avcodec_find_decoder(pAvStream->codecpar->codec_id);
			if (pAudioDecoder == nullptr)
			{
				std::cout << "<Open> [ERROR] can not find Audio codec" << std::endl;
			}
			m_nAudioStreamIndex = (uint32_t)i;
			std::cout << "<Open> sample_fmt=" << (int)pAvStream->codecpar->format << ", sampleRate=" <<
				pAvStream->codecpar->sample_rate << ", channels=" << pAvStream->codecpar->channels <<
				"chnl_layout=" << pAvStream->codecpar->channel_layout << std::endl;
		}

		if (pVideoDecoder == nullptr && pAudioDecoder == nullptr)
		{
			std::cout << "<Open> [ERROR] can not find video or audio stream" << std::endl;
			Close();
			return -1;
		}

		// seek �� ��0ms��ʼ��ȡ
		res = avformat_seek_file(m_pAvFormatCtx, -1, INT64_MIN, 0, INT64_MAX, 0);

		// ������Ƶ����������
		if (pVideoDecoder != nullptr)
		{
			m_pVideoDecodeCtx = avcodec_alloc_context3(pVideoDecoder);
			if (m_pVideoDecodeCtx == nullptr)
			{
				std::cout << "<Open> [ERROR] fail to video avcodec_alloc_context3()" << std::endl;
				Close();
				return -1;
			}
			res = avcodec_parameters_to_context(m_pVideoDecodeCtx, m_pAvFormatCtx->streams[m_nVideoStreamIndex]->codecpar);

			res = avcodec_open2(m_pVideoDecodeCtx, nullptr, nullptr);
			if (res != 0)
			{
				std::cout << "<Open> [ERROR] fail to video avcodec_open2(), res=" << res << std::endl;
				Close();
				return -1;
			}
		}

		// ������Ƶ����������
		if (pAudioDecoder != nullptr)
		{
			m_pAudioDecodeCtx = avcodec_alloc_context3(pAudioDecoder);
			if (m_pAudioDecodeCtx == nullptr)
			{
				std::cout << "<Open> [ERROR] fail to audio avcodec_alloc_context3()" << std::endl;
				Close();
				return -1;
			}
			res = avcodec_parameters_to_context(m_pAudioDecodeCtx, m_pAvFormatCtx->streams[m_nAudioStreamIndex]->codecpar);

			res = avcodec_open2(m_pAudioDecodeCtx, nullptr, nullptr);
			if (res != 0)
			{
				std::cout << "<Open> [ERROR] fail to audio avcodec_open2(), res=" << res << std::endl;
				Close();
				return -1;
			}
		}
	}
	return 0;
}

void Close()
{
	// �ر�ý���ļ�����
	if (m_pAvFormatCtx != nullptr)
	{
		avformat_close_input(&m_pAvFormatCtx);
		m_pAvFormatCtx = nullptr;
	}

	// �ر���Ƶ������
	if (m_pVideoDecodeCtx != nullptr)
	{
		avcodec_close(m_pVideoDecodeCtx);
		avcodec_free_context(&m_pVideoDecodeCtx);
		m_pVideoDecodeCtx = nullptr;
	}

	// �ر���Ƶ������
	if (m_pAudioDecodeCtx != nullptr)
	{
		avcodec_close(m_pAudioDecodeCtx);
		avcodec_free_context(&m_pAudioDecodeCtx);
		m_pAudioDecodeCtx = nullptr;
	}
}

void ReadInfo()
{
	int res = 0;
	while (1)
	{
		AVPacket* pPacket = av_packet_alloc();
		res = av_read_frame(m_pAvFormatCtx, pPacket);
		if (res == AVERROR_EOF)
		{
			std::cout << "<ReadFrame> reached media file end" << std::endl;
			break;
		}
		else if (res < 0)
		{
			std::cout << "<ReadFrame> fail to av_read_frame, res=" << res << std::endl;
			break;
		}

		std::cout << "pts=" << pPacket->pts << std::endl
			<< "dts=" << pPacket->dts << std::endl
			<< "size=" << pPacket->size << std::endl
			<< "stream_index=" << pPacket->stream_index << std::endl
			<< "flags=" << pPacket->flags << std::endl
			<< "duration=" << pPacket->duration << std::endl
			<< "pos=" << pPacket->pos << std::endl << std::endl;
	}
}

// ѭ�����ϵĶ�ȡ����Ƶ���ݰ����н��봦��
void ReadFrame()
{
	int res = 0;
	bool isVideoOK = false;
	bool isAudioOK = false;
	AVFrame* pVideoFrame = nullptr;
	AVFrame* pAudioFrame = nullptr;

	for (;;)
	{
		AVPacket* pPacket = av_packet_alloc();

		// ���ζ�ȡ���ݰ�
		res = av_read_frame(m_pAvFormatCtx, pPacket);
		if (res == AVERROR_EOF)
		{
			std::cout << "<ReadFrame> reached media file end" << std::endl;
			if (!isVideoOK)
			{
				res = DecodePktToFrame(m_pVideoDecodeCtx, nullptr, &pVideoFrame);
				if (res == 0)
					showPacketInfo(pVideoFrame, true);
			}
			if (res == AVERROR_EOF)
			{
				isVideoOK = true;
			}

			if (!isAudioOK)
			{
				res = DecodePktToFrame(m_pAudioDecodeCtx, nullptr, &pAudioFrame);
				if (res == 0)
					showPacketInfo(pAudioFrame, false);
			}
			if (res == AVERROR_EOF)
			{
				isAudioOK = true;
			}
		}
		else if (res < 0)
		{
			std::cout << "<ReadFrame> fail to av_read_frame(), res=" << res << std::endl;
			break;
		}
		else
		{
			if (pPacket->stream_index == m_nVideoStreamIndex)	// ��Ƶ��
			{
				res = DecodePktToFrame(m_pVideoDecodeCtx, pPacket, &pVideoFrame);
				if (res == 0)
				{
					AVFrame* pOutVideoFrame;
					std::cout << "Video info" << std::endl;
					showPacketInfo(pVideoFrame, true);
					VideoConvert(pVideoFrame, AV_PIX_FMT_YUYV422, 1080, 720, &pOutVideoFrame);
					std::cout << "Converted Video info" << std::endl;
					showPacketInfo(pOutVideoFrame, true);
					std::cout << "-----------------------" << std::endl;
				}
				else
				{
					std::cout << "no video data" << std::endl;
				}
			}
			else if (pPacket->stream_index == m_nAudioStreamIndex)
			{
				res = DecodePktToFrame(m_pAudioDecodeCtx, pPacket, &pAudioFrame);
				if (res == 0)
				{
					AVFrame* pOutAudioFrame;
					std::cout << "Audio info" << std::endl;
					showPacketInfo(pAudioFrame, false);
					AudioConvert(pAudioFrame, AV_SAMPLE_FMT_U8, 1, 44100, &pOutAudioFrame);
					std::cout << "Converted Audio info" << std::endl;
					showPacketInfo(pOutAudioFrame, false);
					std::cout << "-----------------------" << std::endl;
				}
				else
				{
					std::cout << "no audio data" << std::endl;
				}
			}
		}

		av_packet_free(&pPacket); // ���ݰ������ͷ�
		if (isAudioOK && isVideoOK)
			break;
	}
}

void showPacketInfo(const AVFrame* avFrame, bool isVideo)
{
	if (isVideo)
	{
		std::cout <<
			"linesize : " << avFrame->linesize << std::endl <<
			"format : " << avFrame->format << std::endl <<
			"pts : " << avFrame->pts << std::endl <<
			"width : " << avFrame->width << std::endl <<
			"height : " << avFrame->height << std::endl <<
			"key_frame : " << avFrame->key_frame << std::endl;
	}
	else
	{
		std::cout <<
			"linesize : " << avFrame->linesize << std::endl <<
			"format : " << avFrame->format << std::endl <<
			"pts : " << avFrame->pts << std::endl <<
			"sample_rate : " << avFrame->sample_rate << std::endl <<
			"channel_layout : " << avFrame->channel_layout << std::endl <<
			"nb_samples : " << avFrame->nb_samples << std::endl;
	}
}



int32_t DecodePktToFrame(
	AVCodecContext* pDecodeCtx,		// ��������������Ϣ
	AVPacket* pInPacket,			// ��������
	AVFrame** ppOutFrame)			// ����������Ƶ֡
{
	AVFrame* pOutFrame = nullptr;
	int res = 0;

	// ����Ҫ��������ݰ�
	res = avcodec_send_packet(pDecodeCtx, pInPacket);
	if (res == AVERROR(EAGAIN))	// û���������룬���ǿ��Լ������ڲ���������ȡ����������Ƶ����
	{
		std::cout << "<DecodePktFrame> avcodec_send_packet() EAGAIN" << std::endl;
		//return EAGAIN;
	}
	else if (res == AVERROR_EOF) // �ڲ�������������ȫ��������ɣ������н�����֡�������
	{
		std::cout << "<DecodePktFrame> avocodec_send_packet() AVERROR_EOF" << std::endl;
		//return AVERROR_EOF;
	}
	else if (res < 0)
	{
		std::cout << "<DecodePktFrame> [ERROR] fail to avcodec_send_frame(), res=" << res << std::endl;
		return res;
	}

	// ��ȡ��������Ƶ������Ƶ֡
	pOutFrame = av_frame_alloc();
	res = avcodec_receive_frame(pDecodeCtx, pOutFrame);
	if (res == AVERROR(EAGAIN)) // ��ǰ���û�н����������Ƶ֡
	{
		std::cout << "<DecodePktFrame> no data output" << std::endl;
		av_frame_free(&pOutFrame);
		(*ppOutFrame) = nullptr;
		return EAGAIN;
	}
	else if (res == AVERROR_EOF) // ���뻺������ˢ�����
	{
		std::cout << "<DecodePktFrame> EOF" << std::endl;
		av_frame_free(&pOutFrame);
		(*ppOutFrame) = nullptr;
		return AVERROR_EOF;
	}
	else if (res < 0)
	{
		std::cout << "DecodePktFrame> [ERROR] fail to avcodec_receive_packet(), res=" << res << std::endl;
		av_frame_free(&pOutFrame);
		(*ppOutFrame) = nullptr;
		return res;
	}

	(*ppOutFrame) = pOutFrame;
	return 0;
}

int32_t VideoConvert(
	const AVFrame* pInFrame,	// ������Ƶ֡
	AVPixelFormat eOutFormat,	// �����Ƶ��ʽ
	int32_t		  nOutWidth,	// �����Ƶ���
	int32_t		  nOutHeight,	// �����Ƶ�߶�
	AVFrame** ppOutFrame)		// �����Ƶ֡
{
	struct SwsContext* pSwsCtx = nullptr;
	AVFrame* pOutFrame = nullptr;

	// ������ʽת������ָ�������㷨���������˾���Ч����
	pSwsCtx = sws_getContext(pInFrame->width, pInFrame->height,
		(AVPixelFormat)pInFrame->format, nOutWidth, nOutHeight, eOutFormat,
		SWS_BICUBIC, nullptr, nullptr, nullptr);

	if (pSwsCtx == nullptr)
	{
		std::cout << "<VideoConvert> [ERROR] fail to sws_getContext()" << std::endl;
		return -1;
	}

	// ���������Ƶ֡�����Լ�������Ӧ�Ļ�����
	uint8_t* data[4] = { nullptr };
	int linesize[4] = { 0 };
	int res = av_image_alloc(data, linesize, nOutWidth, nOutHeight, eOutFormat, 1);
	if (res < 0)
	{
		std::cout << "<VideoConvert> [ERROR] fail to av_image_alloc(), res=" << res << std::endl;
		sws_freeContext(pSwsCtx);
		return -2;
	}
	pOutFrame = av_frame_alloc();
	pOutFrame->format = eOutFormat;
	pOutFrame->width = nOutWidth;
	pOutFrame->height = nOutHeight;
	pOutFrame->data[0] = data[0];
	pOutFrame->data[1] = data[1];
	pOutFrame->data[2] = data[2];
	pOutFrame->data[3] = data[3];
	pOutFrame->linesize[0] = linesize[0];
	pOutFrame->linesize[1] = linesize[1];
	pOutFrame->linesize[2] = linesize[2];
	pOutFrame->linesize[3] = linesize[3];

	// ���и�ʽת��
	res = sws_scale(pSwsCtx,
		static_cast<const uint8_t* const*>(pInFrame->data),
		pInFrame->linesize,
		0,
		pInFrame->height,
		pOutFrame->data,
		pOutFrame->linesize);

	if (res < 0)
	{
		std::cout << "<VideoConvert> [ERROR] fail to sws_scale(), res=" << res << std::endl;
		sws_freeContext(pSwsCtx);
		av_frame_free(&pOutFrame);
		return -3;
	}

	pOutFrame->pts = pInFrame->pts;
	pOutFrame->key_frame = pInFrame->key_frame;

	(*ppOutFrame) = pOutFrame;
	sws_freeContext(pSwsCtx);	// �ͷ�ת����
	return 0;
}

// ��Ƶ��ʽת��
int32_t AudioConvert(
	const AVFrame* pInFrame,	// ������Ƶ֡
	AVSampleFormat eOutSmplFmt,  // �����Ƶ��ʽ
	int32_t		   nOutChannels,// �����Ƶͨ����
	int32_t		   nOutSmplRate,// �����Ƶ������
	AVFrame** ppOutFrame)	// �����Ƶ֡
{
	struct SwrContext* pSwrCtx = nullptr;
	AVFrame* pOutFrame = nullptr;
	// ������ʽת����
	int64_t nInChnlLayout = av_get_default_channel_layout(pInFrame->channels);
	int64_t nOutChnlLayout = (nOutChannels == 1) ? AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO;

	pSwrCtx = swr_alloc();
	if (pSwrCtx == nullptr)
	{
		std::cout << "AudioConvert> [ERROR] fail to swr_alloc" << std::endl;
		return -1;
	}

	swr_alloc_set_opts(pSwrCtx, nOutChnlLayout, eOutSmplFmt, nOutSmplRate,
		nInChnlLayout, (enum AVSampleFormat)(pInFrame->format), pInFrame->sample_rate, 0, nullptr);

	// �����ز���ת����������������Ӷ����仺������С
	int64_t nCvtBufSamples = av_rescale_rnd(pInFrame->nb_samples, nOutSmplRate, pInFrame->sample_rate, AV_ROUND_UP);

	// ���������Ƶ
	pOutFrame = av_frame_alloc();
	pOutFrame->format = eOutSmplFmt;
	pOutFrame->nb_samples = (int)nCvtBufSamples;
	pOutFrame->channel_layout = (uint64_t)nOutChnlLayout;
	int res = av_frame_get_buffer(pOutFrame, 0);
	if (res < 0)
	{
		std::cout << "<AudioConvert> [ERROR] fail to av_frame_get_buffer(), res=" << res << std::endl;
		swr_free(&pSwrCtx);
		av_frame_free(&pOutFrame);
		return -2;
	}
	swr_init(pSwrCtx);
	// �����ز���ת����������ת�������������
	int nCvtedSamples = swr_convert(pSwrCtx,
		const_cast<uint8_t**>(pOutFrame->data),
		(int)nCvtBufSamples,
		const_cast<const uint8_t**>(pInFrame->data),
		pInFrame->nb_samples);
	if (nCvtedSamples <= 0)
	{
		std::cout << "<AudioConvert> [ERROR] no data for swr_convert()" << std::endl;
		swr_free(&pSwrCtx);
		av_frame_free(&pOutFrame);
		return -3;
	}
	pOutFrame->nb_samples = nCvtedSamples;
	pOutFrame->pts = pInFrame->pts;
	pOutFrame->sample_rate = nOutSmplRate;

	(*ppOutFrame) = pOutFrame;
	swr_free(&pSwrCtx);
	return 0;
}

AVCodecContext* m_pVideoEncCtx = nullptr; // ��Ƶ������������

// ����ͼ���ʽ����Ϣ������������
int32_t VidEncoderOpen(
	AVPixelFormat	ePxlFormat,		// ����ͼ�����ظ�ʽ
	int32_t			nFrameWidth,	// ����ͼ����
	int32_t			nFrameHeight,	// ����ͼ��߶�
	int32_t			nFrameRate,		// ����֡��
	float			nBitRateFactor)	// ת�����ӣ�ͨ����Ϊ 0.8~8
{
	AVCodec* pVideoEncoder = nullptr;
	int res = 0;
	pVideoEncoder = avcodec_find_encoder(AV_CODEC_ID_H264);
	if (pVideoEncoder == nullptr)
	{
		std::cout << "<VidEncoderOpen> [ERROR] fail to find AV_CODEC_ID_H264" << std::endl;
		return -1;
	}

	m_pVideoEncCtx = avcodec_alloc_context3(pVideoEncoder);

	if (m_pVideoEncCtx == nullptr)
	{
		std::cout << "VideoEncoderOpen> [ERROR] fail to find avcodec_alloc_context3" << std::endl;
		return -2;
	}

	int64_t nBitRate = (((int64_t)nFrameWidth * nFrameHeight * 3 / 2) * nBitRateFactor);// ��������
	m_pVideoEncCtx->codec_id = AV_CODEC_ID_H264;
	m_pVideoEncCtx->pix_fmt = ePxlFormat;
	m_pVideoEncCtx->width = nFrameWidth;
	m_pVideoEncCtx->height = nFrameHeight;
	m_pVideoEncCtx->bit_rate = nBitRate;
	m_pVideoEncCtx->rc_buffer_size = static_cast<int>(nBitRate);
	m_pVideoEncCtx->framerate.num = nFrameRate;	// ֡��
	m_pVideoEncCtx->framerate.den = 1;
	m_pVideoEncCtx->gop_size = nFrameRate;	// ÿ��һ���ؼ�֡
	m_pVideoEncCtx->time_base.num = 1;
	m_pVideoEncCtx->time_base.den = nFrameRate * 1000;	// ʱ���
	m_pVideoEncCtx->has_b_frames = 0;
	m_pVideoEncCtx->max_b_frames = 0;
	m_pVideoEncCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

	res = avcodec_open2(m_pVideoEncCtx, pVideoEncoder, nullptr);
	if (res < 0)
	{
		std::cout << "<VidEncoderOpen> [ERROR] fail to find avcodec_open2(), res=" << res << std::endl;
		avcodec_free_context(&m_pVideoEncCtx);
		return -3;
	}
	return 0;
}

// �رձ�����
void VidEncoderClose()
{
	if (m_pVideoEncCtx != nullptr)
	{
		avcodec_free_context(&m_pVideoEncCtx);
		m_pVideoEncCtx = nullptr;
	}
}

// ������Ƶ֡����,ע�ⲻ��ÿ��������Ƶ֡����ͻ���������ݰ�
int32_t VidEncoderEncPacket(AVFrame* pInFrame, AVPacket** ppOutPacket)
{
	AVPacket* pOutPacket = nullptr;
	int res = 0;

	if (m_pVideoEncCtx == nullptr)
	{
		std::cout << "<VidEncoderEncPacket> [ERROR] bad status" << std::endl;
		return -1;
	}

	// ����Ҫ�������Ƶ֡
	res = avcodec_send_frame(m_pVideoEncCtx, pInFrame);
	if (res == AVERROR(EAGAIN))
	{
		std::cout << "<VidEncoderEncPacket> avcodec_send_frame() EAGAIN" << std::endl;
	}
	else if (res == AVERROR_EOF)
	{
		std::cout << "<VidEncoderEncPacket> avcoder_send_frame() AVERROR_EOF" << std::endl;
	}
	else if (res < 0)
	{
		std::cout << "<VidEncoderEncPacket> [ERROR] fail to avcodec_send_frame" << std::endl;
		return -2;
	}

	// ��ȡ���������ݰ�
	pOutPacket = av_packet_alloc();
	res = avcodec_receive_packet(m_pVideoEncCtx, pOutPacket);
	if (res == AVERROR(EAGAIN))	// ��ǰ���û��������������Ǻ������Լ�����ȡ
	{
		av_packet_free(&pOutPacket);
		return EAGAIN;
	}
	else if (res == AVERROR_EOF)
	{
		std::cout << "<VidEncoderEncPacket> avcodec_receive_packet() EOF" << std::endl;
		av_packet_free(&pOutPacket);
		return AVERROR_EOF;
	}
	else if (res < 0)
	{
		std::cout << "<VidEncoderEncPacket> avcodec_receive_packet(), res=" << res << std::endl;
		av_packet_free(&pOutPacket);
		return res;
	}

	(*ppOutPacket) = pOutPacket;
	return 0;
}



