#include "SpectrumWidget.h"
#include <QPainter>
#include <cmath>
#include <algorithm>

SpectrumWidget::SpectrumWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(150);

    const int bands = 16;
    m_bars.resize(bands);
    m_bars.fill(0.0f);

    // KissFFT 缓冲区（避免 C6262）
    constexpr int N = 1024;
    m_fftIn.resize(N);
    m_fftOut.resize(N);

    // FFT 配置（只创建一次）
    m_fftCfg = kiss_fft_alloc(N, 0, nullptr, nullptr);
}

void SpectrumWidget::onPcmFrame(QVector<float> pcm)
{
    if (!pcm.isEmpty()) {
        computeFFT(pcm.data(), pcm.size());
        update();   // 立即刷新，减少延迟
    }
}

void SpectrumWidget::computeFFT(const float* pcm, int samples)
{
    constexpr int N = 1024;
    kiss_fft_cpx* in = m_fftIn.data();
    kiss_fft_cpx* out = m_fftOut.data();

    // Hann 窗 + 自动补齐
    int copy = std::min(samples, N);
    constexpr float PI = 3.14159265358979323846f;

    for (int i = 0; i < copy; ++i) {
        float w = 0.5f * (1.0f - std::cos(2.0f * PI * i / (N - 1)));
        in[i].r = pcm[i] * w;
        in[i].i = 0.0f;
    }
    for (int i = copy; i < N; ++i) {
        in[i].r = 0.0f;
        in[i].i = 0.0f;
    }

    // 执行 FFT（零延迟）
    kiss_fft(m_fftCfg, in, out);

    // 16 频段
    const int bands = m_bars.size();
    int bandSize = (N / 2) / bands;

    for (int b = 0; b < bands; ++b) {
        int start = b * bandSize;
        int end = (b + 1) * bandSize;

        float sumSq = 0.0f;
        for (int i = start; i < end; ++i) {
            float re = out[i].r;
            float im = out[i].i;
            float mag = std::sqrt(re * re + im * im);
            sumSq += mag * mag;
        }

        // 平均幅值（比 RMS 更快响应）
        float avg = std::sqrt(sumSq / std::max(1, end - start));

        // dB 映射：-60 ~ 0 dB → 0 ~ 1
        float db = 20.0f * std::log10(avg + 1e-6f);
        float norm = (db + 60.0f) / 60.0f;
        norm = std::clamp(norm, 0.0f, 1.0f);

        // 快速响应高音：高音立刻弹起
        float cur = m_bars[b];
        if (norm > cur) {
            m_bars[b] = norm;
        }
        else {
            // 慢慢衰减（视觉更自然）
            m_bars[b] = 0.80f * cur + 0.20f * norm;
        }
    }
}

void SpectrumWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), QColor("#FFFF00"));

    int barCount = m_bars.size();
    if (barCount <= 0)
        return;

    int w = width() / barCount;
    int h = height();

    for (int i = 0; i < barCount; ++i) {
        float v = std::clamp(m_bars[i], 0.0f, 1.0f);
        int barH = int(v * h);

        QRect r(i * w + 2, h - barH, w - 4, barH);

        QLinearGradient g(r.topLeft(), r.bottomLeft());
        g.setColorAt(0, QColor("#00FFAA"));
        g.setColorAt(1, QColor("#006644"));

        p.fillRect(r, g);
    }
}
