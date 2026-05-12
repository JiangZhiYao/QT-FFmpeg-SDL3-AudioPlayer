#include <QApplication>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QLabel>
#include <QTimer>
#include <QHBoxLayout>

//#include "AudioPlayer.h"
#include "MusicPlayerWidget.h"

static QString fmtTime(double sec)
{
    int s = (int)sec;
    return QString("%1:%2")
        .arg(s / 60, 2, 10, QLatin1Char('0'))
        .arg(s % 60, 2, 10, QLatin1Char('0'));
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    MusicPlayerWidget w;
    w.show();

    //QWidget w;
    //w.setWindowTitle("FFmpeg 7.1 + SDL3 Audio Player (Persistent Decode Thread)");

    //AudioPlayer* player = new AudioPlayer(&w);
    //static bool playing = false;
    //QPushButton* openBtn = new QPushButton(QStringLiteral("打开"));
    //QPushButton* playBtn = new QPushButton(QStringLiteral("播放"));
    //QSlider* slider = new QSlider(Qt::Horizontal);
    //slider->setRange(0, 1000);

    //QSlider* volSlider = new QSlider(Qt::Horizontal);
    //volSlider->setRange(0, 200); // 0.0 ~ 2.0
    //volSlider->setValue(100);

    //QLabel* label = new QLabel("00:00 / 00:00");
    //QLabel* volLabel = new QLabel(QStringLiteral("音量: 1.00"));

    //QVBoxLayout* lay = new QVBoxLayout(&w);
    //lay->addWidget(openBtn);
    //lay->addWidget(playBtn);
    //lay->addWidget(slider);
    //lay->addWidget(label);

    //QHBoxLayout* volLay = new QHBoxLayout();
    //volLay->addWidget(new QLabel(QStringLiteral("音量")));
    //volLay->addWidget(volSlider);
    //volLay->addWidget(volLabel);
    //lay->addLayout(volLay);

    //bool sliderDragging = false;

    //QObject::connect(openBtn, &QPushButton::clicked, [&]() {
    //    QString f = QFileDialog::getOpenFileName(&w, "选择音频");
    //    if (!f.isEmpty()) {
    //        if (player->openFile(f)) {
    //            playing = false;
    //            playBtn->setText(QStringLiteral("播放"));
    //            slider->setValue(0);
    //            label->setText("00:00 / " + fmtTime(player->duration()));
    //            playBtn->clicked();
    //        }
    //    }
    //    });

    //QObject::connect(playBtn, &QPushButton::clicked, [&]() {
    //    playing = !playing;
    //    if (playing) {
    //        player->play();
    //        playBtn->setText(QStringLiteral("暂停"));
    //    }
    //    else {
    //        player->pause();
    //        playBtn->setText(QStringLiteral("播放"));
    //    }
    //    });

    //QObject::connect(volSlider, &QSlider::valueChanged, [&](int v) {
    //    float vol = v / 100.0f; // 0.0 ~ 2.0
    //    player->setVolume(vol);
    //    volLabel->setText(QStringLiteral("音量: %1").arg(vol, 0, 'f', 2));
    //    });

    //QTimer* timer = new QTimer(&w);
    //timer->setInterval(100);
    //QObject::connect(timer, &QTimer::timeout, [&]() {
    //    if (sliderDragging)
    //        return;

    //    double dur = player->duration();
    //    if (dur <= 0.0) return;
    //    double cur = player->currentTime();
    //    if (cur < 0.0) cur = 0.0;
    //    if (cur > dur) cur = dur;

    //    slider->setValue(static_cast<int>(cur / dur * 1000));
    //    label->setText(fmtTime(cur) + " / " + fmtTime(dur));
    //    });
    //timer->start();

    //// 拖动时只更新 UI，不调用 seek
    //QObject::connect(slider, &QSlider::sliderPressed, [&]() {
    //    sliderDragging = true;
    //    });

    //QObject::connect(slider, &QSlider::sliderMoved, [&](int v) {
    //    double dur = player->duration();
    //    if (dur > 0.0) {
    //        double pos = v / 1000.0;
    //        double cur = pos * dur;
    //        label->setText(fmtTime(cur) + " / " + fmtTime(dur));
    //    }
    //    });

    //// 松开后再真正 seek
    //QObject::connect(slider, &QSlider::sliderReleased, [&]() {
    //    sliderDragging = false;

    //    double dur = player->duration();
    //    if (dur > 0.0) {
    //        double pos = slider->value() / 1000.0;
    //        player->seek(pos);
    //    }
    //    });

    //w.resize(520, 220);
    //w.show();
    return app.exec();
}
