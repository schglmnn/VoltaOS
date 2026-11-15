#include "CarVideoDecoder.h"

#include <QDebug>
#include <QImage>
#include <QMetaObject>
#include <QVideoFrame>
#include <QVideoFrameFormat>
#include <cstring>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
}

CarVideoDecoder::CarVideoDecoder() = default;
CarVideoDecoder::~CarVideoDecoder() { close(); }

void CarVideoDecoder::setSink(QVideoSink *sink)
{
    QMutexLocker l(&m_sinkLock);
    m_sink = sink;
}

bool CarVideoDecoder::open()
{
    const AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec) { qWarning() << "[AA] H.264 decoder not available in FFmpeg"; return false; }
    m_ctx = avcodec_alloc_context3(codec);
    // Low latency: no frames held back for reordering, one thread per slice
    m_ctx->flags |= AV_CODEC_FLAG_LOW_DELAY;
    m_ctx->flags2 |= AV_CODEC_FLAG2_FAST;
    m_ctx->thread_count = 0;
    m_ctx->thread_type = FF_THREAD_SLICE;
    if (avcodec_open2(m_ctx, codec, nullptr) < 0) {
        qWarning() << "[AA] opening the H.264 decoder failed";
        avcodec_free_context(&m_ctx);
        return false;
    }
    m_frame = av_frame_alloc();
    m_pkt = av_packet_alloc();
    return true;
}

void CarVideoDecoder::close()
{
    if (m_pkt) av_packet_free(&m_pkt);
    if (m_frame) av_frame_free(&m_frame);
    if (m_ctx) avcodec_free_context(&m_ctx);
}

void CarVideoDecoder::reset()
{
    close();
    m_frames = 0;
    m_waitingForKeyframe = true;
    m_sps.clear();
    m_pps.clear();
}

void CarVideoDecoder::decode(const QByteArray &h264)
{
    // A running adapter may resume with P-frames when the host restarts.
    // Cache Annex-B parameter sets, including when sent as separate packets,
    // and wait for an IDR before submitting dependent slices to FFmpeg.
    bool idr = false, hasSlice = false;
    auto startCode = [&h264](int from, int *prefix) {
        for (int i = from; i + 2 < h264.size(); ++i) {
            if (h264[i] != '\0' || h264[i + 1] != '\0') continue;
            if (h264[i + 2] == '\1') { *prefix = 3; return i; }
            if (i + 3 < h264.size() && h264[i + 2] == '\0' && h264[i + 3] == '\1') {
                *prefix = 4; return i;
            }
        }
        return -1;
    };
    int prefix = 0, start = startCode(0, &prefix);
    while (start >= 0) {
        const int header = start + prefix;
        int nextPrefix = 0;
        const int next = startCode(header + 1, &nextPrefix);
        if (header < h264.size()) {
            const int type = uchar(h264[header]) & 31;
            const QByteArray nal = h264.mid(start, (next < 0 ? int(h264.size()) : next) - start);
            if (type == 7) { if (m_sps != nal) m_waitingForKeyframe = true; m_sps = nal; }
            if (type == 8) { if (m_pps != nal) m_waitingForKeyframe = true; m_pps = nal; }
            if (type == 5) idr = true;
            if (type >= 1 && type <= 5) hasSlice = true;
        }
        start = next;
        prefix = nextPrefix;
    }
    if (!hasSlice) return;
    if (m_waitingForKeyframe && (!idr || m_sps.isEmpty() || m_pps.isEmpty())) return;
    if (!m_ctx && !open()) return;
    // libavcodec wants some zero padding at the end of the buffer
    QByteArray buf = (m_waitingForKeyframe || idr) ? m_sps + m_pps + h264 : h264;
    const int size = int(buf.size());
    buf.append(QByteArray(AV_INPUT_BUFFER_PADDING_SIZE, '\0'));
    m_pkt->data = reinterpret_cast<uint8_t *>(buf.data());
    m_pkt->size = size;
    auto recover = [this] {
        avcodec_flush_buffers(m_ctx);
        m_waitingForKeyframe = true;
    };
    if (avcodec_send_packet(m_ctx, m_pkt) < 0) { recover(); return; }
    m_waitingForKeyframe = false;
    int result;
    while ((result = avcodec_receive_frame(m_ctx, m_frame)) == 0) {
        deliver(m_frame);
        av_frame_unref(m_frame);
    }
    if (result != AVERROR(EAGAIN) && result != AVERROR_EOF) recover();
}

void CarVideoDecoder::deliver(AVFrame *f)
{
    QPointer<QVideoSink> sink;
    {
        QMutexLocker l(&m_sinkLock);
        sink = m_sink;
    }
    ++m_frames;
    if (!sink) return;
    if (f->format != AV_PIX_FMT_YUV420P && f->format != AV_PIX_FMT_YUVJ420P) {
        if (m_frames == 1) qWarning() << "[AA] formato video inatteso" << f->format;
        return;
    }

    QVideoFrameFormat fmt(QSize(f->width, f->height), QVideoFrameFormat::Format_YUV420P);
    fmt.setColorRange(f->format == AV_PIX_FMT_YUVJ420P || f->color_range == AVCOL_RANGE_JPEG
                      ? QVideoFrameFormat::ColorRange_Full : QVideoFrameFormat::ColorRange_Video);
    fmt.setColorSpace(QVideoFrameFormat::ColorSpace_BT709);
    QVideoFrame frame(fmt);
    if (!frame.map(QVideoFrame::WriteOnly)) return;
    for (int p = 0; p < 3; ++p) {
        const int h = p == 0 ? f->height : (f->height + 1) / 2;
        const int w = p == 0 ? f->width  : (f->width + 1) / 2;
        uint8_t *dst = frame.bits(p);
        const int dstStride = frame.bytesPerLine(p);
        const uint8_t *src = f->data[p];
        for (int y = 0; y < h; ++y)
            std::memcpy(dst + y * dstStride, src + y * f->linesize[p], size_t(w));
    }
    frame.unmap();
    static const QString dumpPath = qEnvironmentVariable("VOLTA_CARLINK_DUMP");
    if (!dumpPath.isEmpty() && m_frames == 90) frame.toImage().save(dumpPath);
    QMetaObject::invokeMethod(sink, [sink, frame]() {
        if (sink) sink->setVideoFrame(frame);
    }, Qt::QueuedConnection);
}
