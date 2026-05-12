#include "AudioPlayer.h"
#include "SpectrumWidget.h"  // 如果你单独文件名不同，改这里
#include <QDebug>
#include <QThread>

namespace {
    static constexpr SDL_AudioFormat kAudioFormatF32 = SDL_AUDIO_F32;
}

// ===================== SDL 回调 =====================
static void SDLCALL AudioStreamCB(void* userdata,
    SDL_AudioStream* stream,
    int additional_amount,
    int /*total_amount*/)
{
    auto* thisPlayer = static_cast<AudioPlayer*>(userdata);

    int remaining = additional_amount;
    std::vector<uint8_t> outbuf(additional_amount);
    uint8_t* out = outbuf.data();

    std::vector<float> visualMono;
    visualMono.reserve(additional_amount / 4);

    while (remaining > 0) {

        if (thisPlayer->offsetBytes >= thisPlayer->currentFrame.data.size()) {
            AudioFrame newFrame;
            if (!thisPlayer->audioQueue.try_pop(newFrame)) {

                if (thisPlayer->audioQueue.empty() && thisPlayer->currentFrame.data.empty()) {

                    if (!thisPlayer->m_startedDecoding) {
                        SDL_PutAudioStreamData(stream, nullptr, 0);
                        return;
                    }

                    if (!thisPlayer->m_finishedEmitted.exchange(true)) {
                        emit thisPlayer->finished();
                    }

                    SDL_PutAudioStreamData(stream, nullptr, 0);
                    return;
                }
            }

            thisPlayer->currentFrame = std::move(newFrame);
            thisPlayer->offsetBytes = 0;
            thisPlayer->bytesPerSample = av_get_bytes_per_sample(thisPlayer->currentFrame.format);
        }

        size_t frame_left = thisPlayer->currentFrame.data.size() - thisPlayer->offsetBytes;
        size_t to_copy = std::min(static_cast<size_t>(remaining), frame_left);

        memcpy(out,
            thisPlayer->currentFrame.data.data() + thisPlayer->offsetBytes,
            to_copy);

        int channels = thisPlayer->currentFrame.channels > 0 ?
            thisPlayer->currentFrame.channels : thisPlayer->m_srcChannels;
        int bps = thisPlayer->bytesPerSample;

        if (channels > 0 && bps == 4) {
            int samples = static_cast<int>(to_copy) / (bps * channels);
            const float* frameF32 = reinterpret_cast<const float*>(
                thisPlayer->currentFrame.data.data() + thisPlayer->offsetBytes
                );
            for (int i = 0; i < samples; ++i) {
                float sum = 0.0f;
                for (int ch = 0; ch < channels; ++ch) {
                    sum += frameF32[i * channels + ch];
                }
                visualMono.push_back(sum / channels);
            }
        }

        out += to_copy;
        remaining -= static_cast<int>(to_copy);
        thisPlayer->offsetBytes += to_copy;
    }

    // ⭐ 安全跨线程传输（复制到 QVector）
    if (!visualMono.empty()) {
        QVector<float> pcm(visualMono.size());
        memcpy(pcm.data(), visualMono.data(), visualMono.size() * sizeof(float));
        emit thisPlayer->pcmFrameReady(pcm);
    }

    SDL_PutAudioStreamData(stream, outbuf.data(), additional_amount);

    int channels = thisPlayer->currentFrame.channels > 0 ? thisPlayer->currentFrame.channels : thisPlayer->m_srcChannels;
    int sample_rate = thisPlayer->currentFrame.sample_rate > 0 ? thisPlayer->currentFrame.sample_rate : thisPlayer->m_srcSampleRate;
    int bps = thisPlayer->bytesPerSample;

    if (channels > 0 && sample_rate > 0 && bps > 0) {
        int samples = additional_amount / (bps * channels);
        double seconds = static_cast<double>(samples) / sample_rate;
        thisPlayer->m_currentTime += seconds;
    }
}


// ===================== AudioPlayer 实现 =====================

AudioPlayer::AudioPlayer(QObject* parent)
    : QObject(parent)
{
    connect(this, &AudioPlayer::playsignal, this, &AudioPlayer::onPlay, Qt::QueuedConnection);
    connect(this, &AudioPlayer::pausesignal, this, &AudioPlayer::onPause, Qt::QueuedConnection);
    connect(this, &AudioPlayer::stopsignal, this, &AudioPlayer::onStop, Qt::QueuedConnection);

    av_log_set_level(AV_LOG_ERROR);
    startDecodeThread();
}

AudioPlayer::~AudioPlayer()
{
    m_quit = true;
    {
        std::lock_guard<std::mutex> lock(m_taskMutex);
        m_newFileRequested = true;
    }
    m_taskCV.notify_one();

    if (m_threadStarted && m_decodeThread.joinable())
        m_decodeThread.join();

    cleanupFFmpegContext();
    closeSDL();
    SDL_Quit();
}

double AudioPlayer::audioClock() const
{
    return m_currentTime.load();
}

double AudioPlayer::totalClock() const
{
    return m_duration;
}



bool AudioPlayer::probeFile(const QString& path, int& outChannels, int& outSampleRate, double& outDuration)
{
    QByteArray ba = path.toUtf8();
    const char* filename = ba.constData();

    AVFormatContext* fmt = nullptr;
    if (avformat_open_input(&fmt, filename, nullptr, nullptr) < 0) {
        qWarning() << "probe: open input failed";
        return false;
    }

    if (avformat_find_stream_info(fmt, nullptr) < 0) {
        qWarning() << "probe: find stream info failed";
        avformat_close_input(&fmt);
        return false;
    }

    int audio_stream = av_find_best_stream(fmt, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (audio_stream < 0) {
        qWarning() << "probe: no audio stream";
        avformat_close_input(&fmt);
        return false;
    }

    AVStream* st = fmt->streams[audio_stream];
    AVCodecParameters* par = st->codecpar;

    outDuration = fmt->duration > 0 ? static_cast<double>(fmt->duration) / AV_TIME_BASE : 0.0;
    outSampleRate = par->sample_rate > 0 ? par->sample_rate : 48000;

    if (par->ch_layout.nb_channels > 0)
        outChannels = par->ch_layout.nb_channels;
    else
        outChannels = 2;

    avformat_close_input(&fmt);
    return true;
}

bool AudioPlayer::openFile(const QString& path)
{
    m_filePath = path;

    int ch = 2;
    int sr = 48000;
    double dur = 0.0;
    if (!probeFile(path, ch, sr, dur))
        return false;

    m_srcChannels = ch;
    m_srcSampleRate = sr;
    m_duration = dur;
    m_currentTime = 0.0;

    if (!initSDL(m_srcChannels, m_srcSampleRate))
        return false;

    audioQueue.clear();
    offsetBytes = 0;
    currentFrame.data.clear();

    {
        std::lock_guard<std::mutex> lock(m_taskMutex);
        m_newFileRequested = true;
        m_seekRequested = false;
    }
    m_taskCV.notify_one();

    m_finishedEmitted = false;
    m_startedDecoding = false;
    return true;
}

bool AudioPlayer::initSDL(int channels, int sampleRate)
{
    closeSDL();

    if (SDL_WasInit(SDL_INIT_AUDIO) == 0) {
        if (!SDL_Init(SDL_INIT_AUDIO)) {
            SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
            return false;
        }
    }

    SDL_AudioSpec spec{};
    spec.format = kAudioFormatF32;
    spec.channels = static_cast<Uint8>(channels);
    spec.freq = sampleRate;

    m_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
        &spec,
        AudioStreamCB,
        this);
    if (!m_stream) {
        SDL_Log("Couldn't open audio device: %s", SDL_GetError());
        return false;
    }

    m_spec = spec;
    SDL_SetAudioStreamGain(m_stream, m_volume);
    SDL_PauseAudioStreamDevice(m_stream); // 初始暂停，play() 时恢复
    return true;
}

void AudioPlayer::closeSDL()
{
    if (m_stream) {
        SDL_DestroyAudioStream(m_stream);
        m_stream = nullptr;
    }
}

void AudioPlayer::cleanupFFmpegContext()
{
    if (m_swr) {
        swr_free(&m_swr);
        m_swr = nullptr;
    }
    if (m_ctx) {
        avcodec_free_context(&m_ctx);
        m_ctx = nullptr;
    }
    if (m_fmt) {
        avformat_close_input(&m_fmt);
        m_fmt = nullptr;
    }
    m_audioStreamIndex = -1;
}

void AudioPlayer::startDecodeThread()
{
    if (m_threadStarted)
        return;

    m_threadStarted = true;
    m_decodeThread = std::thread(&AudioPlayer::decodeThreadFunc, this);
}

void AudioPlayer::decodeThreadFunc()
{
    AVPacket* pkt = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();

    while (!m_quit) {

        {
            std::unique_lock<std::mutex> lock(m_taskMutex);
            m_taskCV.wait(lock, [&] {
                return m_quit || m_newFileRequested || m_seekRequested;
                });
        }

        if (m_quit)
            break;

        if (m_newFileRequested) {
            m_newFileRequested = false;
            cleanupFFmpegContext();

            if (m_filePath.isEmpty()) {
                continue;
            }

            QByteArray ba = m_filePath.toUtf8();
            const char* filename = ba.constData();

            if (avformat_open_input(&m_fmt, filename, nullptr, nullptr) < 0) {
                qWarning() << "decode: open input failed";
                cleanupFFmpegContext();
                continue;
            }

            if (avformat_find_stream_info(m_fmt, nullptr) < 0) {
                qWarning() << "decode: find stream info failed";
                cleanupFFmpegContext();
                continue;
            }

            m_audioStreamIndex = av_find_best_stream(m_fmt, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
            if (m_audioStreamIndex < 0) {
                qWarning() << "decode: no audio stream";
                cleanupFFmpegContext();
                continue;
            }

            AVStream* st = m_fmt->streams[m_audioStreamIndex];
            AVCodecParameters* par = st->codecpar;

            const AVCodec* codec = avcodec_find_decoder(par->codec_id);
            if (!codec) {
                qWarning() << "decode: decoder not found";
                cleanupFFmpegContext();
                continue;
            }

            m_ctx = avcodec_alloc_context3(codec);
            avcodec_parameters_to_context(m_ctx, par);

            if (avcodec_open2(m_ctx, codec, nullptr) < 0) {
                qWarning() << "decode: open codec failed";
                cleanupFFmpegContext();
                continue;
            }

            AVChannelLayout out_ch_layout;
            av_channel_layout_default(&out_ch_layout, m_srcChannels);

            m_swr = swr_alloc();
            swr_alloc_set_opts2(
                &m_swr,
                &out_ch_layout, AV_SAMPLE_FMT_FLT, m_srcSampleRate,
                &m_ctx->ch_layout, m_ctx->sample_fmt, m_ctx->sample_rate,
                0, nullptr
            );
            swr_init(m_swr);

            m_seekRequested = false;
        }

        if (m_seekRequested && m_fmt && m_ctx) {
            double target = m_seekTargetSec.load();
            int64_t ts = static_cast<int64_t>(target * AV_TIME_BASE);
            if (av_seek_frame(m_fmt, -1, ts, AVSEEK_FLAG_BACKWARD) >= 0) {
                avcodec_flush_buffers(m_ctx);
            }
            m_seekRequested = false;
        }

        if (!m_fmt || !m_ctx || !m_swr) {
            continue;
        }

        // 可以在这里加“队列限流”，但你已经有 SafeQueue，可选
        while (!m_quit && !m_newFileRequested && !m_seekRequested) {

            int ret = av_read_frame(m_fmt, pkt);
            if (ret < 0) {
                break;
            }

            if (pkt->stream_index != m_audioStreamIndex) {
                av_packet_unref(pkt);
                continue;
            }

            if (avcodec_send_packet(m_ctx, pkt) == 0) {
                while ((ret = avcodec_receive_frame(m_ctx, frame)) == 0) {

                    int out_samples = swr_get_out_samples(m_swr, frame->nb_samples);
                    if (out_samples <= 0)
                        continue;

                    std::vector<float> out_buffer(out_samples * m_srcChannels);
                    uint8_t* out[] = { reinterpret_cast<uint8_t*>(out_buffer.data()) };

                    int converted = swr_convert(
                        m_swr,
                        out, out_samples,
                        (const uint8_t**)frame->extended_data,
                        frame->nb_samples
                    );

                    if (converted <= 0)
                        continue;

                    AudioFrame af;
                    af.nb_samples = converted;
                    af.channels = m_srcChannels;
                    af.sample_rate = m_srcSampleRate;
                    af.format = AV_SAMPLE_FMT_FLT;

                    af.data.resize(static_cast<size_t>(converted) * m_srcChannels * sizeof(float));
                    memcpy(af.data.data(), out_buffer.data(), af.data.size());
                    audioQueue.push(af);
                    m_startedDecoding = true;
                    if (m_newFileRequested || m_seekRequested || m_quit)
                        break;
                }
            }

            av_packet_unref(pkt);
        }
    }

    av_frame_free(&frame);
    av_packet_free(&pkt);

    cleanupFFmpegContext();
}

void AudioPlayer::play()
{
    emit playsignal();
}

void AudioPlayer::pause()
{
    emit pausesignal();
}

void AudioPlayer::stop()
{
    emit stopsignal();
}

void AudioPlayer::onPlay()
{
    if (m_stream) {
        SDL_ResumeAudioStreamDevice(m_stream);
    }
}

void AudioPlayer::onPause()
{
    if (m_stream) {
        SDL_PauseAudioStreamDevice(m_stream);
    }
}

void AudioPlayer::onStop()
{
    if (m_stream) {
        SDL_PauseAudioStreamDevice(m_stream);
        SDL_FlushAudioStream(m_stream);
    }
    audioQueue.clear();
    offsetBytes = 0;
    currentFrame.data.clear();
    m_currentTime = 0.0;
}

void AudioPlayer::seek(double pos)
{
    m_startedDecoding = false;
    m_finishedEmitted = false;
    if (m_duration <= 0.0 || m_filePath.isEmpty())
        return;

    double targetSec = pos * m_duration;

    m_currentTime = targetSec;

    audioQueue.clear();
    offsetBytes = 0;
    currentFrame.data.clear();

    if (m_stream) {
        SDL_FlushAudioStream(m_stream);
    }

    m_seekTargetSec = targetSec;
    {
        std::lock_guard<std::mutex> lock(m_taskMutex);
        m_seekRequested = true;
    }
    m_taskCV.notify_one();
}

void AudioPlayer::setVolume(float v)
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    if (v < 0.0f) v = 0.0f;
    if (v > 2.0f) v = 2.0f;
    m_volume = v;
    if (m_stream) {
        SDL_SetAudioStreamGain(m_stream, m_volume);
    }
}
