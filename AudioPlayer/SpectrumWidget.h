#pragma once
#include <QWidget>
#include <QVector>
extern "C" {
#include "kiss_fft.h"
}
class SpectrumWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SpectrumWidget(QWidget* parent = nullptr);

public slots:
    void onPcmFrame(QVector<float> pcm);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void computeFFT(const float* pcm, int samples);

private:
    QVector<float> m_bars;   // ÆµÆ×Öù¸ß¶È£¨0~1£©

    // KissFFT »º³åÇø£¨±ÜÃâ C6262 Õ»¾¯¸æ£©
    std::vector<kiss_fft_cpx> m_fftIn;
    std::vector<kiss_fft_cpx> m_fftOut;

    kiss_fft_cfg m_fftCfg = nullptr;
};
