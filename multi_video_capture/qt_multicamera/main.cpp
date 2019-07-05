// https://stackoverflow.com/questions/21246766/how-to-efficiently-display-opencv-video-in-qt
// https://github.com/KubaO/stackoverflown/tree/master/questions/opencv-21246766
// https://stackoverflow.com/questions/11222813/2-usb-cameras-not-working-with-opencv


#include <QtWidgets>
#include <algorithm>
#include <opencv2/opencv.hpp>
#include "include-cpp-properties/PropertiesParser.h"
#include <QtMath>
#include <QGridLayout>
#include <QWidget>
#include <QString>
#include <QChar>
#include <time.h>
#include <QDateTime>
#include <QtConcurrent/QtConcurrent>
#include <QFile>
#include <QPushButton>
#include <QHBoxLayout>
#include <QIcon>
#include <QSize>
#include <QList>

#define MS_ONE_SECOND  1000
#define VIDEO_FILE_FRAMES_PER_SECOND 30
#define CAPTURED_IMAGES_DIRECTORY_PATH "captured/images"
#define JPEG_FILE_EXTENSION "JPEG"
#define MJPG_FILE_EXTENSION "MP4"
#define CAPTURED_VIDEO_DIRECTORY_PATH "captured/videos"
#define STANDARD_KB 1024
#define DISK_SPACE_STOP_RECORDING_LIMIT ((qint64)STANDARD_KB*STANDARD_KB*STANDARD_KB*2)


Q_DECLARE_METATYPE(cv::Mat)

struct AddressTracker {
   const void *address = {};
   int reallocs = 0;
   void track(const cv::Mat &m) { track(m.data); }
   void track(const QImage &img) { track(img.bits()); }
   void track(const void *data) {
      if (data && data != address) {
         address = data;
         reallocs ++;
      }
   }
};

class Capture : public QObject {
   Q_OBJECT
   Q_PROPERTY(cv::Mat frame READ frame NOTIFY frameReady USER true)
   cv::Mat m_frame;
   QBasicTimer m_captureTimer;
   QScopedPointer<cv::VideoCapture> m_videoCapture;
   QScopedPointer<cv::VideoWriter> m_videoWriter;
   int m_cap_api_preference = cv::CAP_ANY;
   AddressTracker m_track;
   int m_msFrameInterval = 0; // Blocking calls to camera mean this is irrelevant. however, for videos this can be too fast and need interval
   bool m_delayed_start = false;
   bool m_pausedRecording = false;
public:
   Capture(QObject *parent = {}) : QObject(parent) { }
   ~Capture() { qDebug() << __FUNCTION__ << "reallocations" << m_track.reallocs; }
   Q_SIGNAL void started();
   Q_SIGNAL void cameraNamed(QString);
   Q_SLOT void start(int cam = {}, QString camName = "NoName", bool recordVideo = false) {
//       qDebug() << "Camera " << cam << ".";
       m_captureName = QString::number(cam);
       m_cameraName = camName;
       m_recordVideo = recordVideo;
       m_cap_api_preference = cv::CAP_V4L2;
       m_msFrameInterval = 0;
       m_captureTimer.start(m_msFrameInterval, this);
       emit cameraNamed(m_cameraName);
   }
   Q_SLOT void start(QString camUrl, QString camName, bool recordVideo = false) {
       qDebug() << "video file " << camUrl << ".";
       m_captureName = camUrl;
       m_cameraName = camName;
       m_recordVideo = recordVideo;
       m_cap_api_preference = cv::CAP_ANY;
       m_msFrameInterval = MS_ONE_SECOND / VIDEO_FILE_FRAMES_PER_SECOND;
       m_captureTimer.start(m_msFrameInterval, this);
       emit cameraNamed(m_cameraName);
   }
   bool postponed_camera_start() {
       bool isWebcam = false;
       bool ok = false;
       int camnum = m_captureName.toInt(&isWebcam);
       if (!m_videoCapture)
       {
           if (isWebcam)
               m_videoCapture.reset(new cv::VideoCapture(camnum, cv::CAP_V4L2));
           else
               m_videoCapture.reset(new cv::VideoCapture(m_captureName.toStdString(), m_cap_api_preference));
        }
       if (m_videoCapture->isOpened()) {
          qDebug() << "Started playing video file " << m_captureName << ".";
          emit started();
          ok = true;
       } else {
         m_captureTimer.stop();
         ok = false;
         qDebug() << "Failed to start playing video file " << m_captureName << ".";
     }
     return ok;
   }
   Q_SLOT void stop() { stopRecording(); m_captureTimer.stop(); }

   Q_SLOT void snapshot() {
       QtConcurrent::run([this]() {
            // Code in this block will run in another thread. We detach the storing image
            // so as not to block ongoing video if its slow to store in the filesystem.

            QString path(CAPTURED_IMAGES_DIRECTORY_PATH);
            QFile file;
            openFileForCapture(file, path, fileNameSuggestion() + "." + JPEG_FILE_EXTENSION);
            if (!file.isOpen()) {
                qDebug() << "Filed to capture " << file.fileName();
                return;
            }

            Q_ASSERT(this->m_frame.type() == CV_8UC3);

            this->frameMutex.lock();
            cv::Mat capturedFrame = this->m_frame.clone();
            this->frameMutex.unlock();

            int w = capturedFrame.cols , h = capturedFrame.rows ;
            QImage image = QImage(w, h, QImage::Format_RGB888);
            cv::Mat mat(h, w, CV_8UC3, image.bits(), image.bytesPerLine());
            cv::resize(capturedFrame, mat, mat.size(), 0, 0, cv::INTER_AREA);
            cv::cvtColor(mat, mat, cv::COLOR_BGR2RGB);
            image.save(&file,JPEG_FILE_EXTENSION);
       });
   }

   Q_SLOT void startRecording() {
       // If we are recording video then nothing more to do
       if (!m_pausedRecording && !m_videoWriter.isNull() && m_videoWriter->isOpened()) return;

       QString path(CAPTURED_VIDEO_DIRECTORY_PATH);
       QFile file;
       openFileForCapture(file, path, fileNameSuggestion() + "." + MJPG_FILE_EXTENSION);
       if (!file.isOpen()) {
           qDebug() << "Filed to capture " << file.fileName();
           emit recordingStopped();
           return;
       }
       file.close();
       m_videoWriter.reset(new cv::VideoWriter(file.fileName().toStdString(),cv::VideoWriter::fourcc('M','J','P','G'),10, cv::Size(m_frame.cols,m_frame.rows)));
       emit recordingStarted();
   }

   Q_SLOT void stopRecording() {
       // Simply check if we are actually recording.
       if (m_videoWriter.isNull()) return;

       m_videoWriter->release(); m_videoWriter.reset(); emit recordingStopped();
   }

   Q_SLOT void pauseRecording() {m_pausedRecording = true;}
   Q_SLOT void continueRecording() {m_pausedRecording = false;}
   Q_SIGNAL void recordingStopped();
   Q_SIGNAL void recordingStarted();

   Q_SIGNAL void frameReady(const cv::Mat &);
   cv::Mat frame() const { return m_frame; }
private:
   void openFileForCapture(QFile & file, const QString & path, const QString & filename)
   {
       QDir dir; // Initialize to the desired dir if 'path' is relative
                 // By default the program's working directory "." is used.

       // We create the directory if needed
       if (!dir.exists(path))
           dir.mkpath(path); // You can check the success if needed

       file.setFileName(path + "/" + filename);
       file.open(QIODevice::WriteOnly); // Or QIODevice::ReadWrite
   }

   QString fileNameSuggestion() {
       return m_cameraName + " " + QDateTime::currentDateTime().toString("ddMMyyyy_HHmmss");
   }
   void timerEvent(QTimerEvent * ev) {
      if (ev->timerId() == m_captureTimer.timerId()) handle_capture();
   }

   void handle_capture() {
      if (!m_delayed_start) m_delayed_start = postponed_camera_start();
      frameMutex.lock();
      if (!m_videoCapture->read(m_frame)) { // Blocks until a new frame is ready
         m_captureTimer.stop();
         return;
      }
      frameMutex.unlock();

//      qDebug() << "Captured Image [cols,rows] = [" << m_frame.cols << ", " << m_frame.rows << "]";
      m_track.track(m_frame);

      // If we are recording video then do it...
      if (!m_pausedRecording && !m_videoWriter.isNull() && m_videoWriter->isOpened()) m_videoWriter->write(m_frame);

      emit frameReady(m_frame);
   }
   QMutex frameMutex; // To gaurd m_frame
   // URL and name of the camera
   QString m_captureName = "no name";
   QString m_cameraName = "no name";
   bool m_recordVideo = false;
};

class Converter : public QObject {
   Q_OBJECT
   Q_PROPERTY(QImage image READ image NOTIFY imageReady USER true)
   Q_PROPERTY(bool processAll READ processAll WRITE setProcessAll)
   QBasicTimer m_converterTimer;
   cv::Mat m_frame;
   QImage m_image;
   bool m_processAll = false;
   AddressTracker m_track;
   void queue(const cv::Mat &frame) {
      if (!m_frame.empty()) qDebug() << "Converter dropped frame!";
      m_frame = frame;
      if (! m_converterTimer.isActive()) m_converterTimer.start(0, this);
   }
   void process(const cv::Mat &frame) {
      Q_ASSERT(frame.type() == CV_8UC3);
//       int w = frame.cols / 3.0, h = frame.rows / 3.0; // This was found to be wrong for Colour supplied camera. Needs checking out further
      int w = frame.cols , h = frame.rows ;
      if (m_image.size() != QSize{w,h})
      {
         m_image = QImage(w, h, QImage::Format_RGB888);
//        qDebug() << "Converter frame Size [" << w << ", " << h << "] and Image Size ["<< m_image.size() << "]";
      }
      cv::Mat mat(h, w, CV_8UC3, m_image.bits(), m_image.bytesPerLine());
      cv::resize(frame, mat, mat.size(), 0, 0, cv::INTER_AREA);
      cv::cvtColor(mat, mat, cv::COLOR_BGR2RGB);
      emit imageReady(m_image);
   }
   void timerEvent(QTimerEvent *ev) {
      if (ev->timerId() != m_converterTimer.timerId()) return;
      process(m_frame);
      m_frame.release();
      m_track.track(m_frame);
      m_converterTimer.stop();
   }
public:
   explicit Converter(QObject * parent = nullptr) : QObject(parent) {}
   ~Converter() { qDebug() << __FUNCTION__ << "reallocations" << m_track.reallocs; }
   bool processAll() const { return m_processAll; }
   void setProcessAll(bool all) { m_processAll = all; }
   Q_SIGNAL void imageReady(const QImage &);
   QImage image() const { return m_image; }
   Q_SLOT void processFrame(const cv::Mat &frame) {
      if (m_processAll) process(frame); else queue(frame);
   }
};

class ImageViewer : public QWidget {
   Q_OBJECT
   Q_PROPERTY(QImage image READ image WRITE setImage USER true)
   bool painted = true;
   QImage m_img;
   AddressTracker m_track;
   QBasicTimer m_fpsTimer;
   QString m_cameraName = "Unknown";
   QWidget * m_toolbar = nullptr;
   void paintEvent(QPaintEvent *) {

       QPainter p(this);

      if (!m_img.isNull()) {
//         setMinimumSize(m_img.width(), m_img.height());
         setAttribute(Qt::WA_OpaquePaintEvent);
         QRectF targetSize(0,0,width(), height());
         QRect sourceSize(0,0,m_img.width(), m_img.height());
         p.drawImage(targetSize, m_img, sourceSize, Qt::DiffuseDither);
         painted = true;
      }
      else {
          // As standard draw a border as no image present.
          QPen pen(QColor(255,0,0,255));
          p.setPen(pen);
          p.drawRect(0,0,width()-1, height()-1);
          p.drawLine((QLine(0,0,width() -1, height()-1)));
          p.drawLine((QLine(width() -1,0,0, height()-1)));
      }

      QString fontType = "times";
      p.setFont(QFont(fontType,12));
      QFontMetrics fm(fontType);
      int pixelsHigh = fm.height() + 1;
      QPen tpen(QColor(255,255,255,255));
      p.setPen(tpen);
      p.drawText(10, height()-pixelsHigh * 2, m_cameraName);
      p.drawText(10, height()-pixelsHigh * 1, m_measuredFps);
   }
public:
   ImageViewer(QWidget * parent = nullptr) : QWidget(parent) {
       setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
       m_fpsTimer.start(MS_ONE_SECOND, this);

       showToolbar();
       setMinimumSize(m_toolbar->size() * 2);
    }

   ~ImageViewer() { qDebug() << __FUNCTION__ << "reallocations" << m_track.reallocs; }


   Q_SLOT void setCameraName(const QString camName) {
       m_cameraName = camName;
   }

   Q_SLOT void setImage(const QImage &img) {
      m_fps++;
      if (!painted) qDebug() << "Viewer dropped frame!";
      if (m_img.size() == img.size() && m_img.format() == img.format()
          && m_img.bytesPerLine() == img.bytesPerLine())
         std::copy_n(img.bits(), img.sizeInBytes(), m_img.bits());
      else
         m_img = img.copy();
      painted = false;
//      if (m_img.size() != size()) setFixedSize(m_img.size());
      m_track.track(m_img);
      update();
   }
   QImage image() const { return m_img; }

   Q_SIGNAL void startRecording();
   Q_SIGNAL void continueRecording();
   Q_SIGNAL void pauseRecording();
   Q_SIGNAL void takeSnapshotImage();
   Q_SIGNAL void stopRecording();

   Q_SIGNAL void buttonRecordingStarted();
   Q_SIGNAL void buttonRecordingStopped();

   Q_SLOT void recordingStarted() {
        emit buttonRecordingStarted();
   }
   Q_SLOT void recordingStopped() {
        emit buttonRecordingStopped();
   }

   // Record all toolbar or internal button can start recording or stop.
   Q_SLOT void recordPressed() { emit startRecording();}
   Q_SLOT void stopPressed() { emit stopRecording();}
private:
   // Button pressed slots we relay and use internally here...
   Q_SLOT void playPressed() { emit continueRecording();}
   Q_SLOT void pausePressed() { emit pauseRecording();}
   Q_SLOT void snapshotPressed() { emit takeSnapshotImage();}


   void resizeEvent(QResizeEvent *event)
   {
       QSize toolbarSize = m_toolbar->size();
       QSize windowSize = event->size();
       m_toolbar->move(windowSize.width() - toolbarSize.width(), windowSize.height() - toolbarSize.height());
   }

   void showToolbar()
   {
      m_toolbar = new QWidget(this);
      QPushButton * record = new QPushButton(QIcon(":/toolbar/icons/record.png"),"");
      record->setToolTip("Record video");

      QPushButton * snapshot = new QPushButton(QIcon(":/toolbar/icons/snapshot.png"),"");
      snapshot->setToolTip("Store the current video frame as a single image");

      QPushButton * pause = new QPushButton(QIcon(":/toolbar/icons/pause.png"),"");
      pause->setToolTip("Pause video");

      QPushButton * play = new QPushButton(QIcon(":/toolbar/icons/play.png"),"");
      play->setToolTip("Play video");

      QPushButton * stop = new QPushButton(QIcon(":/toolbar/icons/stop.png"),"");
      stop->setToolTip("Stop recording video");

      // Signals to be emitted to get the video capture component to control recording
      connect(record, SIGNAL(pressed()), SLOT(recordPressed()));
      connect(snapshot, SIGNAL(pressed()), SLOT(snapshotPressed()));
      connect(pause, SIGNAL(pressed()), SLOT(pausePressed()));
      connect(play, SIGNAL(pressed()), SLOT(playPressed()));
      connect(stop, SIGNAL(pressed()), SLOT(stopPressed()));

      // Initial state of the icon buttons
      // Not all icon buttons implemented yet so may stay hidden and not connected.
      record->show();
      pause->hide();
      stop->hide();
      play->hide();
      snapshot->show();

      // Signals to control state visibility of the toolbar in response to Capture class recording states
      QObject::connect(this, SIGNAL(buttonRecordingStarted()), record, SLOT(hide()));
      QObject::connect(this, SIGNAL(buttonRecordingStarted()), stop, SLOT(show()));
      QObject::connect(this, SIGNAL(buttonRecordingStopped()), record, SLOT(show()));
      QObject::connect(this, SIGNAL(buttonRecordingStopped()), stop, SLOT(hide()));

      QHBoxLayout * layout = new QHBoxLayout;
      layout->addWidget(record);
      layout->addWidget(pause);
      layout->addWidget(play);
      layout->addWidget(stop);
      layout->addWidget(snapshot);
      m_toolbar->setLayout(layout);
      m_toolbar->show();
   }

   void timerEvent(QTimerEvent * ev) {
      if (ev->timerId() == m_fpsTimer.timerId()) handleFPSInterval();
   }

   void handleFPSInterval() {
       static bool forceUpdate = false;

//       qDebug() << m_measuredFps << "- handleFPSInterval["<< m_cameraName << "], m_fps == " << m_fps;

       // If we have not been sent image display is frozen, so cause a refresh
       if (m_fps == 0) forceUpdate = true;
       else forceUpdate = false;

       m_measuredFps = "FPS[" + QString::number(m_fps) + "]";
       m_fps = 0;

       if (forceUpdate) update();
   }

   // FPS counting
   uint64_t m_fps = 0;
   QString m_measuredFps = "FPS[-]";
};

class Thread final : public QThread { public: ~Thread() { quit(); wait(); } };

class VideoStreamInstance {
public:
    VideoStreamInstance(QWidget * parent = nullptr):view(parent){}

    ImageViewer view;
    Capture capture;
    Converter converter;
    Thread captureThread, converterThread;
};

#define PROPKEY_CAMERAS_PROPERTY "cameras"
#define PROPKEY_FULLSCREEN "full_screen"
#define PROPKEY_RECORD_VIDEO_CAMERA_LIST_PROPERTY "recordVideoFromCamera"

int main(int argc, char *argv[])
{
    qDebug() << "------------------------------------------";
    qDebug().noquote() << "Qt Version:: " << qVersion();
    qDebug() << "------------------------------------------";
    qDebug().noquote() << QString::fromStdString(cv::getBuildInformation());
    qDebug() << "------------------------------------------";

    qRegisterMetaType<cv::Mat>();
   QApplication app(argc, argv);

   // Load the ini file that notifies what video streams to use.
   cppproperties::PropertiesParser propParser = cppproperties::PropertiesParser();
   cppproperties::Properties p = propParser.Read("../qt_multicamera/videoProperties.ini");

   QString cameraList = QString::fromStdString(p.GetProperty(PROPKEY_CAMERAS_PROPERTY));
   QStringList camera_list = cameraList.split((","));

   QString recordVideoRequest = QString::fromStdString(p.GetProperty(PROPKEY_RECORD_VIDEO_CAMERA_LIST_PROPERTY));
   QStringList recordVideoRequestCameraList = recordVideoRequest.split((","));

   // Calculate the size of the grid required to display evenly
   int numStreams = camera_list.size();
   float root = qSqrt(numStreams);
   int gridSizeY = numStreams / (int) root;
   int gridSizeX = numStreams / (int) gridSizeY;
   gridSizeX += (int) numStreams % (gridSizeX * gridSizeY);
//   qDebug() << "--------------------------------";
//   qDebug() << "gridX = " << gridSizeX << ", gridY = " << gridSizeY << ".";
//   qDebug() << "--------------------------------";

  // For now one window and display all video stream widgets within it
  QMainWindow viewingWindow;
  viewingWindow.setWindowTitle("Multiple Video Streaming Viewer");
  viewingWindow.setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);

  // Make sure we can put the video streams in a grid
  QGridLayout * viewingGrid = new QGridLayout();

  QWidget* widget = new QWidget(&viewingWindow);
  widget->setLayout(viewingGrid);
  viewingWindow.setCentralWidget(widget);
  viewingWindow.setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);

  viewingWindow.setVisible(true);
  viewingWindow.show();

   int row = 0, col = 0;
   QList<VideoStreamInstance *> streamList;
   QString camera;
   foreach (camera, camera_list)
   {
       camera = camera.trimmed();

       VideoStreamInstance * vStream = new VideoStreamInstance(widget);

       // Everything runs at the same priority as the gui, so it won't supply useless frames.
       vStream->converter.setProcessAll(false);
       vStream->captureThread.start();
       vStream->converterThread.start();
       vStream->capture.moveToThread(&vStream->captureThread);
       vStream->converter.moveToThread(&vStream->converterThread);

       // Set up basic relationship between capture -> converter -> imageViewer.
       QObject::connect(&vStream->capture, &Capture::frameReady, &vStream->converter, &Converter::processFrame);
       QObject::connect(&vStream->capture, &Capture::cameraNamed, &vStream->view, &ImageViewer::setCameraName);
       QObject::connect(&vStream->converter, &Converter::imageReady, &vStream->view, &ImageViewer::setImage);
       QObject::connect(&vStream->capture, &Capture::started, [](){ qDebug() << "Capture started."; });

       // Set up recording and snapshot relationship between capture -> imageViewer.
       QObject::connect(&vStream->view, &ImageViewer::startRecording, &vStream->capture, &Capture::startRecording);
       QObject::connect(&vStream->view, &ImageViewer::stopRecording, &vStream->capture, &Capture::stopRecording);
       QObject::connect(&vStream->view, &ImageViewer::takeSnapshotImage, &vStream->capture, &Capture::snapshot);
       QObject::connect(&vStream->capture, &Capture::recordingStarted, &vStream->view, &ImageViewer::recordingStarted);
       QObject::connect(&vStream->capture, &Capture::recordingStopped, &vStream->view, &ImageViewer::recordingStopped);

       // Select the right argument for the capture stream.
       QString argCamUrl = QString::fromStdString(p.GetProperty(camera.toStdString())).trimmed();
       bool isInt;
       int intCamUrlArg = argCamUrl.toInt(&isInt);

       // And start capturing
       if (isInt) QMetaObject::invokeMethod(&vStream->capture, "start", Qt::QueuedConnection, Q_ARG(int, intCamUrlArg),Q_ARG(QString, camera));
       else QMetaObject::invokeMethod(&vStream->capture, "start", Qt::QueuedConnection, Q_ARG(QString, argCamUrl),Q_ARG(QString, camera));


       // Keep a list of each video stream
       streamList.append(vStream);

       // Make sure we add the video to the main window widget
       viewingGrid->addWidget(&vStream->view, row, col, nullptr);
       col += 1;
       if (col == gridSizeX) {col = 0; row+=1;}
   }

   // Check if full screen has been requested when we start running.
   QString fullScreen = QString::fromStdString(p.GetProperty(PROPKEY_FULLSCREEN, ""));
   if (!fullScreen.isEmpty())
   {
       // Optimise for full screen
       QRect rec = QApplication::desktop()->screenGeometry(&viewingWindow);
       int height = rec.height();
       int width = rec.width();
       viewingWindow.resize(width, height);
   }

   QToolBar *toolbar = viewingWindow.addToolBar("Toolbar");
     QAction * actionRecord = toolbar->addAction( QIcon(":/toolbar/icons/record.png"), "Record ALL videos");
     QAction * actionStop = toolbar->addAction( QIcon(":/toolbar/icons/stop.png"), "Stop recording ALL videos");
     toolbar->addSeparator();
     QAction *actionQuit = toolbar->addAction(QIcon(":/toolbar/icons/exit.png"),"Quit Application");

     QObject::connect( actionQuit, &QAction::triggered, &app, &QApplication::quit);
     QList<ImageViewer *> viewerList = viewingWindow.findChildren<ImageViewer *>();
     foreach (ImageViewer * view, viewerList)
     {
        QObject::connect( actionRecord, &QAction::triggered, view, &ImageViewer::recordPressed);
        QObject::connect( actionStop, &QAction::triggered, view, &ImageViewer::stopPressed);
     }

     qDebug() << "-----------------------FYI----------------------------------";
     QStorageInfo storage = QStorageInfo::root();
     qDebug() << storage.rootPath();
     if (storage.isReadOnly())
         qDebug() << "isReadOnly:" << storage.isReadOnly();

     qDebug() << "name:" << storage.name();
     qDebug() << "fileSystemType:" << storage.fileSystemType();
     qDebug() << "size:" << storage.bytesTotal()/1000/1000 << "MB";
     qDebug() << "availableSize:" << storage.bytesAvailable()/1000/1000 << "MB";

     // Use lambda timer to update the status bar with disk space available...
     QStatusBar * statusBar = new QStatusBar();
     viewingWindow.setStatusBar(statusBar);

     QTimer* diskSpaceTimer = new QTimer;
     diskSpaceTimer->setInterval(1000);
     QObject::connect(diskSpaceTimer, &QTimer::timeout, [&storage, &viewingWindow, &diskSpaceTimer](){
         static bool toggleVal = false;
         storage.refresh();
         qint64 bytesAvailable = storage.bytesAvailable();
         viewingWindow.statusBar()->showMessage(QString((toggleVal) ? "/":"\\") +
                                                QString("Storage space availability...") +
                                                QString::number(bytesAvailable/STANDARD_KB/STANDARD_KB) + "MB/" +
                                                QString::number(storage.bytesTotal()/STANDARD_KB/STANDARD_KB) + "MB");

         // If we run out of space immediately stop
        if (bytesAvailable < DISK_SPACE_STOP_RECORDING_LIMIT)
        {
            QList<ImageViewer *> viewerList = viewingWindow.findChildren<ImageViewer *>();
            foreach (ImageViewer * view, viewerList)
            {
               view->stopPressed();
            }
            diskSpaceTimer->stop();
            diskSpaceTimer->deleteLater();
            viewingWindow.statusBar()->showMessage("Disk space ran out, stopped all recording.Exit, fix and restart!");
        }


         toggleVal = !toggleVal;
     });
     diskSpaceTimer->start(1000);

     qDebug() << "------------------------------------------------------------";

   return app.exec();
}

#include "main.moc"
