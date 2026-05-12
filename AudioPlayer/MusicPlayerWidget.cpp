#include "MusicPlayerWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QMimeData>
#include <QUrl>
#include <QRandomGenerator>
#include <QDragEnterEvent>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include "SpectrumWidget.h"



#pragma execution_character_set("utf-8")
MusicPlayerWidget::MusicPlayerWidget(QWidget* parent)
    : QWidget(parent)
{
    setAcceptDrops(true);

    m_player = new AudioPlayer(this);

    m_uiTimer = new QTimer(this);
    m_uiTimer->setInterval(500);
    setupUI();
    setupConnections();
    updateModeIcon();
    loadPlaylist();
    m_uiTimer->start();
}

MusicPlayerWidget::~MusicPlayerWidget()
{
    savePlaylist();
}

void MusicPlayerWidget::setupUI()
{
    setAutoFillBackground(true);
    QPalette pal;
    pal.setColor(QPalette::Window, QColor("#E8F5E9"));
    setPalette(pal);

    m_prevBtn = new QPushButton();
    m_playBtn = new QPushButton();
    m_nextBtn = new QPushButton();
    m_modeBtn = new QPushButton();

    QSize iconSize(48, 48);   // 图标大小
    QSize btnSize(64, 64);    // 按钮整体大小

    m_prevBtn->setIcon(QIcon(":/icons/Playprevious.svg"));
    m_playBtn->setIcon(QIcon(":/icons/Playerplay.svg"));
    m_nextBtn->setIcon(QIcon(":/icons/Playnext.svg"));
    m_modeBtn->setIcon(QIcon(":/icons/RepeatAll.svg"));   // 默认列表循环

    m_prevBtn->setIconSize(iconSize);
    m_playBtn->setIconSize(iconSize);
    m_nextBtn->setIconSize(iconSize);
    m_modeBtn->setIconSize(iconSize);

    m_prevBtn->setFixedSize(btnSize);
    m_playBtn->setFixedSize(btnSize);
    m_nextBtn->setFixedSize(btnSize);
    m_modeBtn->setFixedSize(btnSize);


    for (auto btn : { m_prevBtn, m_playBtn, m_nextBtn, m_modeBtn })
        btn->setStyleSheet("background-color:#4CAF50; color:white; font-size:20px;");

    QHBoxLayout* controlLayout = new QHBoxLayout();
    controlLayout->addStretch();
    controlLayout->addWidget(m_prevBtn);
    controlLayout->addWidget(m_playBtn);
    controlLayout->addWidget(m_nextBtn);
    controlLayout->addWidget(m_modeBtn);
    controlLayout->addStretch();

    m_currentTime = new QLabel("0:00");
    m_totalTime = new QLabel("0:00");

    m_progressSlider = new QSlider(Qt::Horizontal);
    m_progressSlider->setRange(0, 1000);
    m_progressSlider->setStyleSheet(
        "QSlider::groove:horizontal {"
        "    height: 10px;"
        "    background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
        "                                stop:0 #A5D6A7, stop:1 #81C784);"
        "    border-radius: 5px;"
        "}"

        "QSlider::sub-page:horizontal {"
        "    background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
        "                                stop:0 #66BB6A, stop:1 #43A047);"
        "    border-radius: 5px;"
        "}"

        "QSlider::handle:horizontal {"
        "    background: qradialgradient(cx:0.5, cy:0.5, radius:0.6,"
        "                                fx:0.5, fy:0.5,"
        "                                stop:0 #C8E6C9, stop:1 #2E7D32);"
        "    width: 20px;"
        "    height: 20px;"
        "    margin: -6px 0;"
        "    border-radius: 10px;"
        "    border: 2px solid #1B5E20;"
        "    transition: all 150ms;"
        "}"

        "QSlider::handle:horizontal:hover {"
        "    width: 24px;"
        "    height: 24px;"
        "    margin: -8px 0;"
        "    background: qradialgradient(cx:0.5, cy:0.5, radius:0.6,"
        "                                fx:0.5, fy:0.5,"
        "                                stop:0 #E8F5E9, stop:1 #388E3C);"
        "}"

        "QSlider::handle:horizontal:pressed {"
        "    background: qradialgradient(cx:0.5, cy:0.5, radius:0.6,"
        "                                fx:0.5, fy:0.5,"
        "                                stop:0 #FFFFFF, stop:1 #1B5E20);"
        "    width: 22px;"
        "    height: 22px;"
        "    margin: -7px 0;"
        "}"
    );


    QHBoxLayout* progressLayout = new QHBoxLayout();
    progressLayout->addWidget(m_currentTime);
    progressLayout->addWidget(m_progressSlider);
    progressLayout->addWidget(m_totalTime);

    m_volSlider = new QSlider(Qt::Horizontal);
    m_volSlider->setRange(0, 200);
    m_volSlider->setValue(100);
    m_volLabel = new QLabel("音量: 1.00");
    m_spectrumWidget = new SpectrumWidget(this);
    QHBoxLayout* volLayout = new QHBoxLayout();
    volLayout->addWidget(new QLabel("音量"));
    volLayout->addWidget(m_volSlider);
    volLayout->addWidget(m_volLabel);

    m_listWidget = new QListWidget();
    m_listWidget->setStyleSheet("background-color:#F1F8E9; selection-background-color:#81C784;");

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(controlLayout);
    mainLayout->addLayout(progressLayout);
    mainLayout->addLayout(volLayout);
    mainLayout->addWidget(m_spectrumWidget);
    mainLayout->addWidget(m_listWidget);
}

void MusicPlayerWidget::setupConnections()
{
    connect(m_playBtn, &QPushButton::clicked, [&]() {
        static bool playing = false;

        // ⭐ 如果还没有打开任何文件，自动播放当前选中项或第一项
        if (!playing && m_player->totalClock() == 0.0) {
            int row = m_listWidget->currentRow();
            if (row < 0 && m_listWidget->count() > 0)
                row = 0;
            if (row >= 0)
                playTrack(row);
        }

        playing = !playing;
        if (playing) {
            m_player->play();
            m_playBtn->setIcon(QIcon(":/icons/Playerpause.svg"));
        }
        else {
            m_player->pause();
            m_playBtn->setIcon(QIcon(":/icons/Playerplay.svg"));
        }
        });


    connect(m_prevBtn, &QPushButton::clicked, [&]() {
        int row = m_listWidget->currentRow();
        if (row > 0) playTrack(row - 1);
        });

    connect(m_nextBtn, &QPushButton::clicked, [&]() {
        playNextTrack();
        });

    connect(m_modeBtn, &QPushButton::clicked, [&]() {
        m_mode = static_cast<PlayMode>((m_mode + 1) % 3);
        updateModeIcon();
        });

    connect(m_volSlider, &QSlider::valueChanged, [&](int v) {
        float vol = v / 100.0f;
        m_volLabel->setText(QString("音量: %1").arg(vol, 0, 'f', 2));
        m_player->setVolume(vol);
        });

    connect(m_listWidget, &QListWidget::itemDoubleClicked, [&](QListWidgetItem* item) {
        playTrack(m_listWidget->row(item));
        });

    connect(m_uiTimer, &QTimer::timeout, [&]() {
        if (m_sliderDragging) return;

        double dur = m_player->totalClock();
        double cur = m_player->audioClock();
        if (dur <= 0.0) return;


        m_progressSlider->setValue(cur / dur * 1000);

        m_currentTime->setText(QString("%1:%2")
            .arg(int(cur) / 60, 2, 10, QLatin1Char('0'))
            .arg(int(cur) % 60, 2, 10, QLatin1Char('0')));

        m_totalTime->setText(QString("%1:%2")
            .arg(int(dur) / 60, 2, 10, QLatin1Char('0'))
            .arg(int(dur) % 60, 2, 10, QLatin1Char('0')));

        });

    connect(m_progressSlider, &QSlider::sliderPressed, [&]() {
        m_sliderDragging = true;
        });

    connect(m_progressSlider, &QSlider::sliderReleased, [&]() {
        m_sliderDragging = false;
        double pos = m_progressSlider->value() / 1000.0;
        m_player->seek(pos);
        });

    connect(m_player, &AudioPlayer::finished, this, [&]() {
        playNextTrack();
        });

    connect(m_player, &AudioPlayer::pcmFrameReady,
        m_spectrumWidget, &SpectrumWidget::onPcmFrame,
        Qt::QueuedConnection);

}

void MusicPlayerWidget::updateModeIcon()
{
    switch (m_mode) {
    case SingleLoop:
        m_modeBtn->setIcon(QIcon(":/icons/RepeatOne.svg"));
        break;
    case ListLoop:
        m_modeBtn->setIcon(QIcon(":/icons/RepeatAll.svg"));
        break;
    case Shuffle:
        m_modeBtn->setIcon(QIcon(":/icons/Shuffle.svg"));
        break;
    }
}


void MusicPlayerWidget::playTrack(int index)
{
    if (index < 0 || index >= m_listWidget->count()) return;
    //m_listWidget->setStyleSheet("background-color:#F1F8E9; selection-background-color:#81C784;");
    // 清除所有项的样式
    for (int i = 0; i < m_listWidget->count(); ++i) {
        QListWidgetItem* it = m_listWidget->item(i);
        it->setForeground(Qt::black);
        it->setFont(QFont());
        it->setBackground(QColor("#F1F8E9"));
        it->setIcon(QIcon());
    }

    m_listWidget->setCurrentRow(index);

    QString file = m_listWidget->item(index)->data(Qt::UserRole).toString();
    if (!file.isEmpty()) {
        m_player->openFile(file);
        m_player->play();
        m_playBtn->setIcon(QIcon(":/icons/Playerpause.svg"));
    }

    // ⭐ 当前播放项增强效果
    QListWidgetItem* item = m_listWidget->item(index);

    item->setForeground(QColor("#1B5E20"));      // 深绿色字体
    QFont boldFont;
    boldFont.setBold(true);
    item->setFont(boldFont);

    item->setBackground(QColor("#C8E6C9"));      // 浅绿色背景
    item->setIcon(QIcon(":/icons/playing.png")); // 播放图标
}


void MusicPlayerWidget::playNextTrack()
{
    int count = m_listWidget->count();
    if (count == 0) return;

    int cur = m_listWidget->currentRow();
    int next = cur;

    switch (m_mode) {
    case SingleLoop:
        next = cur;
        break;
    case ListLoop:
        next = (cur + 1) % count;
        break;
    case Shuffle:
        next = QRandomGenerator::global()->bounded(count);
        break;
    }

    playTrack(next);
}


void MusicPlayerWidget::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void MusicPlayerWidget::dropEvent(QDropEvent* event)
{
    const QList<QUrl> urls = event->mimeData()->urls();
    if (urls.isEmpty())
        return;

    for (const QUrl& url : urls) {
        QString file = url.toLocalFile();

        // 只接受常见音频格式
        if (!(file.endsWith(".mp3", Qt::CaseInsensitive) ||
            file.endsWith(".flac", Qt::CaseInsensitive) ||
            file.endsWith(".wav", Qt::CaseInsensitive) ||
            file.endsWith(".aac", Qt::CaseInsensitive) ||
            file.endsWith(".m4a", Qt::CaseInsensitive) ||
            file.endsWith(".ogg", Qt::CaseInsensitive)))
            continue;

        // ⭐ 重复过滤逻辑
        bool exists = false;
        for (int i = 0; i < m_listWidget->count(); ++i) {
            QString existing = m_listWidget->item(i)->data(Qt::UserRole).toString();
            if (existing == file) {
                exists = true;
                break;
            }
        }
        if (exists)
            continue;

        // 添加到列表
        QListWidgetItem* item = new QListWidgetItem(QFileInfo(file).fileName());
        item->setData(Qt::UserRole, file);
        m_listWidget->addItem(item);
    }
    savePlaylist();

}


void MusicPlayerWidget::loadPlaylist()
{
    QString path = QDir::currentPath() + "/playlist.txt";
    QFile file(path);

    if (!file.exists())
        return;

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty())
            continue;

        QFileInfo info(line);
        if (!info.exists())
            continue;

        QListWidgetItem* item = new QListWidgetItem(info.fileName());
        item->setData(Qt::UserRole, line);
        m_listWidget->addItem(item);
    }

    file.close();
}


void MusicPlayerWidget::savePlaylist()
{
    QString path = QDir::currentPath() + "/playlist.txt";
    QFile file(path);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return;

    QTextStream out(&file);

    for (int i = 0; i < m_listWidget->count(); ++i) {
        QString filePath = m_listWidget->item(i)->data(Qt::UserRole).toString();
        out << filePath << "\n";
    }

    file.close();
}
