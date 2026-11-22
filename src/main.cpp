#include <QApplication>
#include <QVector>
#include <QMainWindow>
#include <QWidget>
#include <QSplitter>
#include <QListWidget>
#include <QListWidgetItem>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QVideoWidget>
#include <QSlider>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QDateTime>
#include <QStyle>
#include <QStatusBar>
#include <QMenuBar>
#include <QAction>
#include <QUrl>
#include <QRegularExpression>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <algorithm>

struct Clip {
    QString path;
    double inPoint = 0.0;
    double outPoint = -1.0;
    double timelineStart = 0.0;
    double duration() const { return qMax(0.0, outPoint < 0 ? 0.0 : outPoint - inPoint); }
};

class StudioCutWindow final : public QMainWindow {
    Q_OBJECT
public:
    StudioCutWindow() {
        setWindowTitle("StudioCut Pro");
        resize(1500, 900);
        setMinimumSize(1100, 700);
        setAcceptDrops(true);
        buildUi();
        applyTheme();
        statusBar()->showMessage("Ready — import media to begin");
        connect(player, &QMediaPlayer::positionChanged, this, [this](qint64 ms) {
            if (!seeking) playhead->setValue(int(ms / 1000));
            timeLabel->setText(QString("%1 / %2").arg(timecode(ms/1000)).arg(timecode(player->duration()/1000)));
        });
        connect(player, &QMediaPlayer::durationChanged, this, [this](qint64 ms) {
            playhead->setMaximum(qMax(1, int(ms / 1000)));
            timeLabel->setText(QString("00:00:00 / %1").arg(timecode(ms/1000)));
        });
        connect(player, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus s) {
            if (s == QMediaPlayer::InvalidMedia) statusBar()->showMessage("Preview could not open this media. Check codec support.");
        });
    }

protected:
    void dragEnterEvent(QDragEnterEvent *event) override {
        if (event->mimeData()->hasUrls()) event->acceptProposedAction();
        else event->ignore();
    }
    void dropEvent(QDropEvent *event) override {
        if (!mediaBin || !event->mimeData()->hasUrls()) { event->ignore(); return; }
        int added = 0;
        for (const QUrl &url : event->mimeData()->urls()) {
            if (!url.isLocalFile()) continue;
            const QString path = url.toLocalFile();
            const QString ext = QFileInfo(path).suffix().toLower();
            static const QStringList supported = {"mp4","mov","mkv","avi","webm","mp3","wav","aac","m4a","flac","ogg","png","jpg","jpeg","bmp","webp"};
            if (!supported.contains(ext)) continue;
            bool exists = false;
            for (int i=0; i<mediaBin->count(); ++i)
                if (mediaBin->item(i)->data(Qt::UserRole).toString() == path) { exists=true; break; }
            if (exists) continue;
            auto *item = new QListWidgetItem(QFileInfo(path).fileName());
            item->setToolTip(path);
            item->setData(Qt::UserRole,path);
            mediaBin->addItem(item);
            ++added;
        }
        statusBar()->showMessage(QString("Added %1 media file(s) by drag and drop").arg(added));
        event->acceptProposedAction();
    }

private:
    QListWidget *mediaBin{}, *timeline{};
    QVideoWidget *video{};
    QMediaPlayer *player{};
    QAudioOutput *audio{};
    QSlider *playhead{};
    QLabel *timeLabel{}, *inspectorName{}, *inspectorPath{};
    QDoubleSpinBox *inSpin{}, *outSpin{}, *timelineStartSpin{};
    QPushButton *playButton{};
    QVector<Clip> clips;
    QVector<Clip> audioClips;
    QVector<Clip> overlayClips;
    QListWidget *audioTimeline{}, *overlayTimeline{};
    QString projectPath;
    bool seeking = false;
    bool inspectorAudio = false;
    bool inspectorOverlay = false;

    static QString timecode(qint64 seconds) {
        seconds = qMax<qint64>(0, seconds);
        return QString("%1:%2:%3").arg(seconds/3600,2,10,QChar('0')).arg((seconds/60)%60,2,10,QChar('0')).arg(seconds%60,2,10,QChar('0'));
    }
    QWidget* panel(const QString &title, QWidget *content) {
        auto *w = new QWidget; auto *l = new QVBoxLayout(w); l->setContentsMargins(8,8,8,8); l->setSpacing(7);
        auto *h = new QLabel(title); h->setObjectName("panelTitle"); l->addWidget(h); l->addWidget(content,1); return w;
    }
    QPushButton* action(const QString &label) { auto *b = new QPushButton(label); b->setCursor(Qt::PointingHandCursor); return b; }

    void buildUi() {
        auto *file = menuBar()->addMenu("&File");
        auto *newProject = file->addAction("New Project"); auto *openProject = file->addAction("Open Project…");
        auto *saveProject = file->addAction("Save Project"); auto *saveAs = file->addAction("Save Project As…");
        file->addSeparator(); auto *quit = file->addAction("Exit");
        connect(newProject,&QAction::triggered,this,[this]{ clips.clear(); audioClips.clear(); overlayClips.clear(); refreshTimeline(); projectPath.clear(); player->stop(); statusBar()->showMessage("New project");});
        connect(openProject,&QAction::triggered,this,&StudioCutWindow::openProject);
        connect(saveProject,&QAction::triggered,this,&StudioCutWindow::saveProject);
        connect(saveAs,&QAction::triggered,this,[this]{ projectPath.clear(); this->saveProject();});
        connect(quit,&QAction::triggered,this,&QWidget::close);

        auto *root = new QWidget; auto *outer = new QVBoxLayout(root); outer->setContentsMargins(8,8,8,8); outer->setSpacing(8);
        auto *toolbar = new QHBoxLayout;
        auto *brand = new QLabel("STUDIOCUT  /  PRO"); brand->setObjectName("brand"); toolbar->addWidget(brand);
        toolbar->addStretch();
        auto *importBtn=action("＋ Import Media"); auto *addBtn=action("Add Video"); auto *addOverlayBtn=action("Add Overlay"); auto *addAudioBtn=action("Add Audio"); auto *splitBtn=action("Split"); auto *exportBtn=action("Export");
        toolbar->addWidget(importBtn); toolbar->addWidget(addBtn); toolbar->addWidget(addOverlayBtn); toolbar->addWidget(addAudioBtn); toolbar->addWidget(splitBtn); toolbar->addWidget(exportBtn);
        outer->addLayout(toolbar);

        auto *upper = new QSplitter(Qt::Horizontal);
        mediaBin = new QListWidget; mediaBin->setSelectionMode(QAbstractItemView::ExtendedSelection);
        auto *mediaPanel = panel("MEDIA LIBRARY", mediaBin);
        auto *center = new QWidget; auto *centerL = new QVBoxLayout(center); centerL->setContentsMargins(0,0,0,0);
        video = new QVideoWidget; video->setMinimumSize(420,260); video->setStyleSheet("background:#090b10;");
        player = new QMediaPlayer(this); audio = new QAudioOutput(this); player->setAudioOutput(audio); player->setVideoOutput(video);
        centerL->addWidget(video,1);
        auto *transport = new QHBoxLayout; playButton=action("▶ Play"); auto *stopBtn=action("■ Stop"); timeLabel=new QLabel("00:00:00 / 00:00:00");
        transport->addWidget(playButton); transport->addWidget(stopBtn); transport->addStretch(); transport->addWidget(timeLabel); centerL->addLayout(transport);
        auto *rightContent = new QWidget; auto *rightL = new QVBoxLayout(rightContent);
        inspectorName = new QLabel("No clip selected"); inspectorName->setWordWrap(true); inspectorPath = new QLabel("Select a timeline clip to inspect."); inspectorPath->setWordWrap(true);
        inSpin = new QDoubleSpinBox; outSpin = new QDoubleSpinBox; timelineStartSpin = new QDoubleSpinBox;
        inSpin->setRange(0,999999); outSpin->setRange(0,999999); timelineStartSpin->setRange(0,999999);
        inSpin->setDecimals(3); outSpin->setDecimals(3); timelineStartSpin->setDecimals(3);
        inSpin->setSuffix(" s"); outSpin->setSuffix(" s"); timelineStartSpin->setSuffix(" s");
        auto *grid = new QGridLayout; grid->addWidget(new QLabel("Source In"),0,0); grid->addWidget(inSpin,0,1); grid->addWidget(new QLabel("Source Out"),1,0); grid->addWidget(outSpin,1,1); grid->addWidget(new QLabel("Timeline Start"),2,0); grid->addWidget(timelineStartSpin,2,1);
        auto *applyTrim=action("Apply Clip Settings"); rightL->addWidget(inspectorName); rightL->addWidget(inspectorPath); rightL->addLayout(grid); rightL->addWidget(applyTrim); rightL->addStretch();
        auto *inspectorPanel=panel("INSPECTOR",rightContent);
        upper->addWidget(mediaPanel); upper->addWidget(center); upper->addWidget(inspectorPanel); upper->setSizes({280,760,300}); upper->setStretchFactor(1,1);
        outer->addWidget(upper,3);

        auto *timelineArea = new QWidget; auto *tl = new QVBoxLayout(timelineArea); tl->setContentsMargins(0,0,0,0);
        auto *tlHead = new QHBoxLayout; auto *tlTitle=new QLabel("TIMELINE"); tlTitle->setObjectName("panelTitle"); tlHead->addWidget(tlTitle); tlHead->addStretch();
        auto *up=action("Move Up"); auto *down=action("Move Down"); auto *remove=action("Remove Clip"); tlHead->addWidget(up); tlHead->addWidget(down); tlHead->addWidget(remove); tl->addLayout(tlHead);
        timeline = new QListWidget; timeline->setSelectionMode(QAbstractItemView::SingleSelection); timeline->setMinimumHeight(105); tl->addWidget(new QLabel("VIDEO TRACK 1")); tl->addWidget(timeline);
        overlayTimeline = new QListWidget; overlayTimeline->setSelectionMode(QAbstractItemView::SingleSelection); overlayTimeline->setMinimumHeight(72); tl->addWidget(new QLabel("VIDEO TRACK 2 — OVERLAY")); tl->addWidget(overlayTimeline);
        audioTimeline = new QListWidget; audioTimeline->setSelectionMode(QAbstractItemView::SingleSelection); audioTimeline->setMinimumHeight(72); tl->addWidget(new QLabel("AUDIO TRACK 1")); tl->addWidget(audioTimeline);
        auto *seekRow = new QHBoxLayout; playhead = new QSlider(Qt::Horizontal); playhead->setRange(0,1); seekRow->addWidget(new QLabel("PLAYHEAD")); seekRow->addWidget(playhead,1); tl->addLayout(seekRow);
        outer->addWidget(timelineArea,2);
        setCentralWidget(root);

        connect(importBtn,&QPushButton::clicked,this,&StudioCutWindow::importMedia);
        connect(addBtn,&QPushButton::clicked,this,&StudioCutWindow::addSelectedToTimeline);
        connect(addOverlayBtn,&QPushButton::clicked,this,&StudioCutWindow::addSelectedToOverlayTrack);
        connect(addAudioBtn,&QPushButton::clicked,this,&StudioCutWindow::addSelectedToAudioTrack);
        connect(splitBtn,&QPushButton::clicked,this,&StudioCutWindow::splitAtPlayhead);
        connect(exportBtn,&QPushButton::clicked,this,&StudioCutWindow::exportVideo);
        connect(playButton,&QPushButton::clicked,this,[this]{ if(player->playbackState()==QMediaPlayer::PlayingState){player->pause();playButton->setText("▶ Play");}else{player->play();playButton->setText("Ⅱ Pause");}});
        connect(stopBtn,&QPushButton::clicked,this,[this]{player->stop();playButton->setText("▶ Play");});
        connect(playhead,&QSlider::sliderPressed,this,[this]{seeking=true;});
        connect(playhead,&QSlider::sliderReleased,this,[this]{player->setPosition(qint64(playhead->value())*1000);seeking=false;});
        connect(timeline,&QListWidget::currentRowChanged,this,&StudioCutWindow::showInspector);
        connect(audioTimeline,&QListWidget::itemDoubleClicked,this,[this](QListWidgetItem *it){int i=audioTimeline->row(it);if(i>=0&&i<audioClips.size())player->setSource(QUrl::fromLocalFile(audioClips[i].path));});
        connect(audioTimeline,&QListWidget::currentRowChanged,this,&StudioCutWindow::showAudioInspector);
        connect(overlayTimeline,&QListWidget::itemDoubleClicked,this,[this](QListWidgetItem *it){int i=overlayTimeline->row(it);if(i>=0&&i<overlayClips.size())player->setSource(QUrl::fromLocalFile(overlayClips[i].path));});
        connect(overlayTimeline,&QListWidget::currentRowChanged,this,&StudioCutWindow::showOverlayInspector);
        connect(applyTrim,&QPushButton::clicked,this,&StudioCutWindow::applyTrim);
        connect(remove,&QPushButton::clicked,this,&StudioCutWindow::removeClip);
        connect(up,&QPushButton::clicked,this,[this]{moveClip(-1);}); connect(down,&QPushButton::clicked,this,[this]{moveClip(1);});
        connect(mediaBin,&QListWidget::itemDoubleClicked,this,[this](QListWidgetItem *it){player->setSource(QUrl::fromLocalFile(it->data(Qt::UserRole).toString()));});
        connect(timeline,&QListWidget::itemDoubleClicked,this,[this](QListWidgetItem *it){auto i=timeline->row(it);if(i>=0&&i<clips.size())player->setSource(QUrl::fromLocalFile(clips[i].path));});
    }
    void applyTheme() {
        setStyleSheet(R"(
          QMainWindow,QWidget { background:#171a21;color:#e6e9ef;font-family:'Segoe UI';font-size:10pt; }
          QLabel#brand { color:#72d5c5;font-size:15pt;font-weight:800;letter-spacing:2px; }
          QLabel#panelTitle { color:#8e9aab;font-size:9pt;font-weight:700; }
          QListWidget { background:#10131a;border:1px solid #303744;border-radius:5px;padding:5px; }
          QListWidget::item { padding:9px;border-bottom:1px solid #242a34; }
          QListWidget::item:selected { background:#244b55;color:white; }
          QPushButton { background:#29313d;border:1px solid #3c4655;border-radius:5px;padding:8px 12px;color:#f4f6fa; }
          QPushButton:hover { background:#354455;border-color:#62cbbb; }
          QPushButton:pressed { background:#1d6b66; }
          QDoubleSpinBox,QLineEdit { background:#10131a;border:1px solid #3c4655;border-radius:4px;padding:6px; }
          QSlider::groove:horizontal { height:5px;background:#343c49;border-radius:2px; }
          QSlider::handle:horizontal { background:#69d0bf;width:13px;margin:-5px 0;border-radius:6px; }
          QMenuBar { background:#11141b; } QMenuBar::item:selected,QMenu::item:selected { background:#245a5c; }
          QStatusBar { background:#11141b;color:#93a0b1; }
        )");
    }
    void importMedia() {
        const auto paths=QFileDialog::getOpenFileNames(this,"Import Media",QString(),"Media Files (*.mp4 *.mov *.mkv *.avi *.webm *.mp3 *.wav *.aac *.m4a *.flac *.ogg *.png *.jpg *.jpeg *.bmp *.webp);;All Files (*)");
        for(const auto &p:paths){ bool exists=false; for(int i=0;i<mediaBin->count();++i)if(mediaBin->item(i)->data(Qt::UserRole).toString()==p)exists=true;
            if(!exists){auto *it=new QListWidgetItem(QFileInfo(p).fileName());it->setToolTip(p);it->setData(Qt::UserRole,p);mediaBin->addItem(it);} }
        statusBar()->showMessage(QString("%1 media item(s) in library").arg(mediaBin->count()));
    }
    void addSelectedToTimeline() {
        auto selected=mediaBin->selectedItems(); if(selected.isEmpty()){QMessageBox::information(this,"StudioCut Pro","Select one or more video or image files in the Media Library first.");return;}
        int added=0;
        for(auto *it:selected){QString path=it->data(Qt::UserRole).toString();QString ext=QFileInfo(path).suffix().toLower();
            if(QStringList{"mp3","wav","aac","m4a","flac","ogg"}.contains(ext))continue;
            Clip c; c.path=path; c.inPoint=0; c.outPoint=-1; clips.push_back(c);++added;}
        refreshTimeline(); statusBar()->showMessage(QString("Added %1 clip(s) to Video Track 1").arg(added));
    }
    void addSelectedToOverlayTrack() {
        auto selected=mediaBin->selectedItems(); if(selected.isEmpty()){QMessageBox::information(this,"StudioCut Pro","Select video or image files for Video Track 2.");return;}
        int added=0;
        for(auto *it:selected){QString path=it->data(Qt::UserRole).toString();QString ext=QFileInfo(path).suffix().toLower();
            if(QStringList{"mp3","wav","aac","m4a","flac","ogg"}.contains(ext))continue;
            Clip c; c.path=path; c.inPoint=0; c.outPoint=-1; overlayClips.push_back(c);++added;}
        refreshTimeline(); statusBar()->showMessage(QString("Added %1 clip(s) to Video Track 2").arg(added));
    }
    void addSelectedToAudioTrack() {
        auto selected=mediaBin->selectedItems(); if(selected.isEmpty()){QMessageBox::information(this,"StudioCut Pro","Select audio files in the Media Library first.");return;}
        int added=0;
        for(auto *it:selected){QString path=it->data(Qt::UserRole).toString();QString ext=QFileInfo(path).suffix().toLower();
            if(!QStringList{"mp3","wav","aac","m4a","flac","ogg"}.contains(ext))continue;
            Clip c; c.path=path; c.inPoint=0; c.outPoint=-1; audioClips.push_back(c);++added;}
        refreshTimeline(); statusBar()->showMessage(QString("Added %1 clip(s) to Audio Track 1").arg(added));
    }
    void refreshTimeline() {
        timeline->clear();
        for(int i=0;i<clips.size();++i){const auto &c=clips[i];QString end=c.outPoint<0?"Source end":timecode(qint64(c.outPoint));auto *it=new QListWidgetItem(QString("%1   |   START %2s   |   IN %3   OUT %4").arg(QFileInfo(c.path).fileName()).arg(c.timelineStart,0,'f',2).arg(timecode(qint64(c.inPoint)),end));it->setToolTip(c.path);timeline->addItem(it);}
        overlayTimeline->clear();
        for(int i=0;i<overlayClips.size();++i){const auto &c=overlayClips[i];QString end=c.outPoint<0?"Source end":timecode(qint64(c.outPoint));auto *it=new QListWidgetItem(QString("▧  %1   |   START %2s   |   IN %3   OUT %4").arg(QFileInfo(c.path).fileName()).arg(c.timelineStart,0,'f',2).arg(timecode(qint64(c.inPoint)),end));it->setToolTip(c.path);overlayTimeline->addItem(it);}
        audioTimeline->clear();
        for(int i=0;i<audioClips.size();++i){const auto &c=audioClips[i];QString end=c.outPoint<0?"Source end":timecode(qint64(c.outPoint));auto *it=new QListWidgetItem(QString("♫  %1   |   START %2s   |   IN %3   OUT %4").arg(QFileInfo(c.path).fileName()).arg(c.timelineStart,0,'f',2).arg(timecode(qint64(c.inPoint)),end));it->setToolTip(c.path);audioTimeline->addItem(it);}
    }
    void showInspector(int row) {
        inspectorAudio=false;inspectorOverlay=false;
        if(row<0||row>=clips.size()){inspectorName->setText("No clip selected");inspectorPath->setText("Select a video clip to inspect.");return;}
        const auto &c=clips[row];inspectorName->setText(QFileInfo(c.path).fileName());inspectorPath->setText("Video Track 1  •  "+c.path);inSpin->setValue(c.inPoint);outSpin->setValue(c.outPoint<0?0:c.outPoint);timelineStartSpin->setValue(c.timelineStart);
    }
    void showAudioInspector(int row) {
        inspectorAudio=true;inspectorOverlay=false;
        if(row<0||row>=audioClips.size()){inspectorName->setText("No audio clip selected");inspectorPath->setText("Select an audio clip to inspect.");return;}
        const auto &c=audioClips[row];inspectorName->setText(QFileInfo(c.path).fileName());inspectorPath->setText("Audio Track 1  •  "+c.path);inSpin->setValue(c.inPoint);outSpin->setValue(c.outPoint<0?0:c.outPoint);timelineStartSpin->setValue(c.timelineStart);
    }
    void showOverlayInspector(int row) {
        inspectorAudio=false;inspectorOverlay=true;
        if(row<0||row>=overlayClips.size()){inspectorName->setText("No overlay selected");inspectorPath->setText("Select a clip on Video Track 2.");return;}
        const auto &c=overlayClips[row];inspectorName->setText(QFileInfo(c.path).fileName());inspectorPath->setText("Video Track 2 — Overlay  •  "+c.path);inSpin->setValue(c.inPoint);outSpin->setValue(c.outPoint<0?0:c.outPoint);timelineStartSpin->setValue(c.timelineStart);
    }
    void applyTrim() {
        double a=inSpin->value(),b=outSpin->value(); if(b<=a){QMessageBox::warning(this,"Invalid trim","Out point must be greater than In point.");return;}
        if(inspectorOverlay){int row=overlayTimeline->currentRow();if(row<0||row>=overlayClips.size())return;overlayClips[row].inPoint=a;overlayClips[row].outPoint=b;overlayClips[row].timelineStart=timelineStartSpin->value();refreshTimeline();overlayTimeline->setCurrentRow(row);}
        else if(inspectorAudio){int row=audioTimeline->currentRow();if(row<0||row>=audioClips.size())return;audioClips[row].inPoint=a;audioClips[row].outPoint=b;audioClips[row].timelineStart=timelineStartSpin->value();refreshTimeline();audioTimeline->setCurrentRow(row);}
        else {int row=timeline->currentRow();if(row<0||row>=clips.size())return;clips[row].inPoint=a;clips[row].outPoint=b;clips[row].timelineStart=timelineStartSpin->value();refreshTimeline();timeline->setCurrentRow(row);}
    }
    void splitAtPlayhead() {
        const double cut=player->position()/1000.0;
        if(inspectorOverlay){
            int row=overlayTimeline->currentRow();if(row<0||row>=overlayClips.size()){QMessageBox::information(this,"Split","Select an overlay clip first.");return;}
            Clip original=overlayClips[row];double end=original.outPoint<0?player->duration()/1000.0:original.outPoint;
            if(cut<=original.inPoint||cut>=end){QMessageBox::warning(this,"Split","Move the playhead inside the selected overlay clip's source range.");return;}
            Clip left=original,right=original;left.outPoint=cut;right.inPoint=cut;overlayClips[row]=left;overlayClips.insert(row+1,right);refreshTimeline();overlayTimeline->setCurrentRow(row+1);return;
        }
        if(inspectorAudio){
            int row=audioTimeline->currentRow();if(row<0||row>=audioClips.size()){QMessageBox::information(this,"Split","Select an audio clip first.");return;}
            Clip original=audioClips[row];double end=original.outPoint<0?player->duration()/1000.0:original.outPoint;
            if(cut<=original.inPoint||cut>=end){QMessageBox::warning(this,"Split","Move the playhead inside the selected audio clip's source range.");return;}
            Clip left=original,right=original;left.outPoint=cut;right.inPoint=cut;audioClips[row]=left;audioClips.insert(row+1,right);refreshTimeline();audioTimeline->setCurrentRow(row+1);return;
        }
        int row=timeline->currentRow();if(row<0||row>=clips.size()){QMessageBox::information(this,"Split","Select a video clip first.");return;}
        Clip original=clips[row];double end=original.outPoint<0?player->duration()/1000.0:original.outPoint;
        if(cut<=original.inPoint||cut>=end){QMessageBox::warning(this,"Split","Move the playhead inside the selected video clip's source range.");return;}
        Clip left=original,right=original;left.outPoint=cut;right.inPoint=cut;clips[row]=left;clips.insert(row+1,right);refreshTimeline();timeline->setCurrentRow(row+1);
    }
    void removeClip(){if(inspectorOverlay){int row=overlayTimeline->currentRow();if(row<0||row>=overlayClips.size())return;overlayClips.removeAt(row);refreshTimeline();}else if(inspectorAudio){int row=audioTimeline->currentRow();if(row<0||row>=audioClips.size())return;audioClips.removeAt(row);refreshTimeline();}else{int row=timeline->currentRow();if(row<0||row>=clips.size())return;clips.removeAt(row);refreshTimeline();}}
    void moveClip(int delta){if(inspectorOverlay){int row=overlayTimeline->currentRow(),to=row+delta;if(row<0||to<0||to>=overlayClips.size())return;overlayClips.swapItemsAt(row,to);refreshTimeline();overlayTimeline->setCurrentRow(to);}else if(inspectorAudio){int row=audioTimeline->currentRow(),to=row+delta;if(row<0||to<0||to>=audioClips.size())return;audioClips.swapItemsAt(row,to);refreshTimeline();audioTimeline->setCurrentRow(to);}else{int row=timeline->currentRow(),to=row+delta;if(row<0||to<0||to>=clips.size())return;clips.swapItemsAt(row,to);refreshTimeline();timeline->setCurrentRow(to);}}
    void saveProject() {
        if(projectPath.isEmpty())projectPath=QFileDialog::getSaveFileName(this,"Save StudioCut Project",QDir::homePath()+"/Untitled.studiocut","StudioCut Project (*.studiocut)");
        if(projectPath.isEmpty())return;
        QJsonArray arr;for(const auto &c:clips){QJsonObject o;o["path"]=c.path;o["in"]=c.inPoint;o["out"]=c.outPoint;o["start"]=c.timelineStart;arr.append(o);}
        QJsonArray audioArr;for(const auto &c:audioClips){QJsonObject o;o["path"]=c.path;o["in"]=c.inPoint;o["out"]=c.outPoint;o["start"]=c.timelineStart;audioArr.append(o);}
        QJsonArray overlayArr;for(const auto &c:overlayClips){QJsonObject o;o["path"]=c.path;o["in"]=c.inPoint;o["out"]=c.outPoint;o["start"]=c.timelineStart;overlayArr.append(o);}
        QJsonObject root;root["format"]="StudioCutProject";root["version"]=4;root["clips"]=arr;root["audioClips"]=audioArr;root["overlayClips"]=overlayArr;
        QFile f(projectPath);if(!f.open(QIODevice::WriteOnly)){QMessageBox::critical(this,"Save failed",f.errorString());return;}
        f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));f.close();statusBar()->showMessage("Project saved: "+projectPath);
    }
    void openProject() {
        QString p=QFileDialog::getOpenFileName(this,"Open StudioCut Project",QDir::homePath(),"StudioCut Project (*.studiocut)");
        if(p.isEmpty())return;QFile f(p);if(!f.open(QIODevice::ReadOnly)){QMessageBox::critical(this,"Open failed",f.errorString());return;}
        auto doc=QJsonDocument::fromJson(f.readAll());if(!doc.isObject()||doc.object()["format"].toString()!="StudioCutProject"){QMessageBox::warning(this,"Invalid project","This file is not a valid StudioCut project.");return;}
        clips.clear();audioClips.clear();overlayClips.clear();auto arr=doc.object()["clips"].toArray();for(const auto &v:arr){auto o=v.toObject();Clip c;c.path=o["path"].toString();c.inPoint=o["in"].toDouble();c.outPoint=o["out"].toDouble(-1);c.timelineStart=o["start"].toDouble(0);clips.push_back(c);}
        auto audioArr=doc.object()["audioClips"].toArray();for(const auto &v:audioArr){auto o=v.toObject();Clip c;c.path=o["path"].toString();c.inPoint=o["in"].toDouble();c.outPoint=o["out"].toDouble(-1);c.timelineStart=o["start"].toDouble(0);audioClips.push_back(c);}
        auto overlayArr=doc.object()["overlayClips"].toArray();for(const auto &v:overlayArr){auto o=v.toObject();Clip c;c.path=o["path"].toString();c.inPoint=o["in"].toDouble();c.outPoint=o["out"].toDouble(-1);c.timelineStart=o["start"].toDouble(0);overlayClips.push_back(c);}
        projectPath=p;refreshTimeline();statusBar()->showMessage("Project opened: "+p);
    }
    void exportVideo() {
        if(clips.isEmpty()){QMessageBox::information(this,"Export","Add at least one video clip to Video Track 1 first.");return;}
        QDialog settings(this);
        settings.setWindowTitle("Export Settings");
        settings.setMinimumWidth(360);
        auto *form = new QFormLayout(&settings);
        auto *resolution = new QComboBox(&settings);
        resolution->addItem("1280 × 720 (HD)", "1280:720");
        resolution->addItem("1920 × 1080 (Full HD)", "1920:1080");
        resolution->addItem("2560 × 1440 (QHD)", "2560:1440");
        resolution->addItem("3840 × 2160 (4K)", "3840:2160");
        resolution->setCurrentIndex(1);
        auto *frameRate = new QComboBox(&settings);
        frameRate->addItems({"24 fps", "25 fps", "30 fps", "50 fps", "60 fps"});
        frameRate->setCurrentIndex(2);
        auto *codec = new QComboBox(&settings);
        codec->addItem("H.264 — widest compatibility", "libx264");
        codec->addItem("H.265 / HEVC — smaller files", "libx265");
        auto *quality = new QComboBox(&settings);
        quality->addItem("High quality", 18);
        quality->addItem("Balanced", 23);
        quality->addItem("Smaller file", 28);
        quality->setCurrentIndex(1);
        form->addRow("Resolution", resolution);
        form->addRow("Frame rate", frameRate);
        form->addRow("Video codec", codec);
        form->addRow("Quality", quality);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &settings);
        form->addRow(buttons);
        connect(buttons, &QDialogButtonBox::accepted, &settings, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &settings, &QDialog::reject);
        if (settings.exec() != QDialog::Accepted) return;
        const QString dimensions = resolution->currentData().toString();
        const QStringList sizeParts = dimensions.split(':');
        const QString width = sizeParts.value(0, "1920");
        const QString height = sizeParts.value(1, "1080");
        const QString fps = frameRate->currentText().split(' ').first();
        const QString encoder = codec->currentData().toString();
        const QString crf = quality->currentData().toString();
        QString out=QFileDialog::getSaveFileName(this,"Export Video",QDir::homePath()+"/StudioCut-Export.mp4","MP4 Video (*.mp4)");
        if(out.isEmpty())return;
#ifdef STUDIOCUT_FFMPEG_PATH
        QString ffmpeg=QString::fromUtf8(STUDIOCUT_FFMPEG_PATH);
#else
        QString ffmpeg="ffmpeg";
#endif
        QStringList args;args<<"-y";
        QStringList filters;QStringList videoInputs;QStringList audioInputs;
        const int videoCount=clips.size();
        for(int i=0;i<clips.size();++i){const auto &c=clips[i];const QString ext=QFileInfo(c.path).suffix().toLower();
            const bool stillImage=QStringList{"png","jpg","jpeg","bmp","webp"}.contains(ext);
            if(stillImage)args<<"-loop"<<"1"<<"-t"<<"5";
            args<<"-i"<<c.path;
            double duration=c.outPoint<0?(stillImage?5.0:-1.0):c.outPoint-c.inPoint;
            QString v=QString("[%1:v]trim=start=%2").arg(i).arg(c.inPoint,0,'f',3);
            if(duration>0)v+=QString(":duration=%1").arg(duration,0,'f',3);
            v+=QString(",setpts=PTS-STARTPTS,scale=%1:%2:force_original_aspect_ratio=decrease,pad=%1:%2:(ow-iw)/2:(oh-ih)/2,fps=%3,format=yuv420p[v%4]").arg(width, height, fps).arg(i);
            filters<<v;videoInputs<<QString("[v%1]").arg(i);
        }
        filters<<videoInputs.join("")+QString("concat=n=%1:v=1:a=0[vbase]").arg(clips.size());
        for(int i=0;i<overlayClips.size();++i){
            const auto &c=overlayClips[i];const int inputIndex=videoCount+i;const QString ext=QFileInfo(c.path).suffix().toLower();
            const bool stillImage=QStringList{"png","jpg","jpeg","bmp","webp"}.contains(ext);
            if(stillImage)args<<"-loop"<<"1"<<"-t"<<"5";
            args<<"-i"<<c.path;
            double duration=c.outPoint<0?(stillImage?5.0:-1.0):c.outPoint-c.inPoint;
            QString v=QString("[%1:v]trim=start=%2").arg(inputIndex).arg(c.inPoint,0,'f',3);
            if(duration>0)v+=QString(":duration=%1").arg(duration,0,'f',3);
            v+=QString(",setpts=PTS-STARTPTS,scale=%1:%2:force_original_aspect_ratio=decrease,pad=%1:%2:(ow-iw)/2:(oh-ih)/2,fps=%3,format=yuva420p[ov%4]").arg(width,height,fps).arg(i);
            filters<<v;videoInputs<<QString("[ov%1]").arg(i);
        }
        if(!overlayClips.isEmpty()){
            QStringList overlayInputs;for(int i=0;i<overlayClips.size();++i)overlayInputs<<QString("[ov%1]").arg(i);
            filters<<overlayInputs.join("")+QString("concat=n=%1:v=1:a=0[vtop]").arg(overlayClips.size());
            filters<<"[vbase][vtop]overlay=eof_action=pass:shortest=0,format=yuv420p[outv]";
        } else {
            filters<<"[vbase]null[outv]";
        }
        for(int i=0;i<audioClips.size();++i){
            const auto &c=audioClips[i];const int inputIndex=videoCount+overlayClips.size()+i;args<<"-i"<<c.path;
            QString a=QString("[%1:a]atrim=start=%2").arg(inputIndex).arg(c.inPoint,0,'f',3);
            if(c.outPoint>c.inPoint)a+=QString(":duration=%1").arg(c.outPoint-c.inPoint,0,'f',3);
            a+=",asetpts=PTS-STARTPTS[a%1]";a=a.arg(i);filters<<a;audioInputs<<QString("[a%1]").arg(i);
        }
        if(!audioInputs.isEmpty())filters<<audioInputs.join("")+QString("concat=n=%1:v=0:a=1[outa]").arg(audioInputs.size());
        args<<"-filter_complex"<<filters.join(";")<<"-map"<<"[outv]";
        if(!audioInputs.isEmpty())args<<"-map"<<"[outa]"<<"-c:a"<<"aac"<<"-b:a"<<"192k";
        else args<<"-an";
        args<<"-c:v"<<encoder<<"-preset"<<"medium"<<"-crf"<<crf<<"-movflags"<<"+faststart"<<out;
        auto *proc=new QProcess(this);proc->setProgram(ffmpeg);proc->setArguments(args);
        statusBar()->showMessage("Exporting video with FFmpeg…");exportVideoButtonEnabled(false);
        connect(proc,&QProcess::readyReadStandardError,this,[this,proc]{QString s=QString::fromLocal8Bit(proc->readAllStandardError());if(!s.isEmpty())statusBar()->showMessage(s.right(180));});
        connect(proc,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this,proc,out](int code,QProcess::ExitStatus){
            exportVideoButtonEnabled(true);if(code==0)QMessageBox::information(this,"Export complete","Video exported successfully:\n"+out);else QMessageBox::critical(this,"Export failed","FFmpeg returned an error. Check that all source files are available and FFmpeg is installed.");proc->deleteLater();
        });
        proc->start();if(!proc->waitForStarted(3000)){exportVideoButtonEnabled(true);QMessageBox::critical(this,"FFmpeg not found","Could not start FFmpeg. Install it or add ffmpeg.exe to PATH.");proc->deleteLater();}
    }
    void exportVideoButtonEnabled(bool enabled){for(auto *b:findChildren<QPushButton*>())if(b->text()=="Export")b->setEnabled(enabled);}
};

#include "main.moc"
int main(int argc,char **argv){QApplication app(argc,argv);QApplication::setApplicationName("StudioCut Pro");QApplication::setOrganizationName("StudioCut");StudioCutWindow w;w.show();return app.exec();}
