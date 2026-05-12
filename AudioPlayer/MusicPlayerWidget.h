#pragma once
#include <QWidget>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QListWidget>
#include <QTimer>
#include "AudioPlayer.h"
class SpectrumWidget;
class MusicPlayerWidget : public QWidget
{
    Q_OBJECT
public:
    enum PlayMode {
        SingleLoop,
        ListLoop,
        Shuffle
    };

    explicit MusicPlayerWidget(QWidget* parent = nullptr);
    ~MusicPlayerWidget() override;

private:
    void setupUI();
    void setupConnections();
    void updateModeIcon();
    void playTrack(int index);
    void playNextTrack();
    void loadPlaylist();
    void savePlaylist();

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    AudioPlayer* m_player = nullptr;
    SpectrumWidget* m_spectrumWidget = nullptr;
    QPushButton* m_prevBtn = nullptr;
    QPushButton* m_playBtn = nullptr;
    QPushButton* m_nextBtn = nullptr;
    QPushButton* m_modeBtn = nullptr;

    QLabel* m_currentTime = nullptr;
    QLabel* m_totalTime = nullptr;
    QSlider* m_progressSlider = nullptr;

    QSlider* m_volSlider = nullptr;
    QLabel* m_volLabel = nullptr;

    QListWidget* m_listWidget = nullptr;

    QTimer* m_uiTimer = nullptr;

    bool m_sliderDragging = false;
    PlayMode m_mode = ListLoop;
};
