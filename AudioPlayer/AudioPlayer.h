#pragma once

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
}

#include <SDL3/SDL.h>

#include <QObject>
#include <QString>
#include <QVector>

#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>

struct AudioFrame {
    std::vector<uint8_t> data;
    int nb_samples = 0;
    int channels = 0;
    int sample_rate = 0;
    AVSampleFormat format = AV_SAMPLE_FMT_FLT;
};

template<typename T>
class SafeQueue {
public:
    void push(const T& v) {
        std::lock_guard<std::mutex> lock(mtx);
        q.push(v);
        cv.notify_one();
    }

    bool try_pop(T& out) {
        std::lock_guard<std::mutex> lock(mtx);
        if (q.empty()) return false;
        out = q.front();
        q.pop();
        return true;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mtx);
        std::queue<T> empty;
        q.swap(empty);
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mtx);
        return q.empty();
    }

private:
    mutable std::mutex mtx;
    std::condition_variable cv;
    std::queue<T> q;
};

class AudioPlayer : public QObject
{
    Q_OBJECT
public:
    explicit AudioPlayer(QObject* parent = nullptr);
    ~AudioPlayer() override;

    bool openFile(const QString& path);
    void play();
    void pause();
    void stop();
    void seek(double pos);

    double duration() const { return m_duration; }
    double audioClock() const;
    double totalClock() const;

    void setVolume(float v);

signals:
    void playsignal();
    void pausesignal();
    void stopsignal();
    void finished();

    // ⭐ 解耦后的 PCM 信号（安全跨线程）
    void pcmFrameReady(QVector<float> pcm);

protected slots:
    void onPlay();
    void onPause();
    void onStop();

public:
    SafeQueue<AudioFrame> audioQueue;
    AudioFrame currentFrame;
    size_t offsetBytes = 0;
    int bytesPerSample = 4;
    std::atomic<double> m_currentTime{ 0.0 };

private:
    bool initSDL(int channels, int sampleRate);
    void closeSDL();
    void startDecodeThread();
    void decodeThreadFunc();
    void cleanupFFmpegContext();
    bool probeFile(const QString& path, int& outChannels, int& outSampleRate, double& outDuration);

    friend void SDLCALL AudioStreamCB(void*, SDL_AudioStream*, int, int);

private:
    SDL_AudioStream* m_stream = nullptr;
    SDL_AudioSpec m_spec{};

    QString m_filePath;
    double m_duration = 0.0;
    int m_srcChannels = 2;
    int m_srcSampleRate = 48000;

    std::thread m_decodeThread;
    std::atomic<bool> m_threadStarted{ false };
    std::atomic<bool> m_quit{ false };

    std::mutex m_taskMutex;
    std::condition_variable m_taskCV;
    std::atomic<bool> m_startedDecoding{ false };
    std::atomic<bool> m_finishedEmitted{ false };
    std::atomic<bool> m_newFileRequested{ false };
    std::atomic<bool> m_seekRequested{ false };
    std::atomic<double> m_seekTargetSec{ 0.0 };

    AVFormatContext* m_fmt = nullptr;
    AVCodecContext* m_ctx = nullptr;
    SwrContext* m_swr = nullptr;
    int m_audioStreamIndex = -1;

    std::mutex m_stateMutex;
    float m_volume = 1.0f;
};
