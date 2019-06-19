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

#define MS_ONE_SECOND  1000
#define VIDEO_FILE_FRAMES_PER_SECOND 30

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
   QBasicTimer m_timer;
   QScopedPointer<cv::VideoCapture> m_videoCapture;
   AddressTracker m_track;
   int m_msFrameInterval = 0; // Blocking calls to camera mean this is irrelevant. however, for videos this can be too fast and need interval
public:
   Capture(QObject *parent = {}) : QObject(parent) {}
   ~Capture() { qDebug() << __FUNCTION__ << "reallocations" << m_track.reallocs; }
   Q_SIGNAL void started();
   Q_SLOT void start(int cam = {}, QString camName = "NoName") {
//       qDebug() << "Camera " << cam << ".";
       m_captureName = cam;
       m_cameraName = camName;
      if (!m_videoCapture)
         m_videoCapture.reset(new cv::VideoCapture(cam, cv::CAP_V4L2));
      if (m_videoCapture->isOpened()) {
         m_timer.start(m_msFrameInterval, this);
         qDebug() << "Started playing camera[" << cam << "].";
         emit started();
      } else {
        qDebug() << "Failed to start playing camera[" << cam << "].";
    }
      m_startPeriod = clock();
   }
   Q_SLOT void start(QString camUrl, QString camName) {
       qDebug() << "video file " << camUrl << ".";
       m_captureName = camUrl;
       m_cameraName = camName;
       m_msFrameInterval = MS_ONE_SECOND / VIDEO_FILE_FRAMES_PER_SECOND;
      if (!m_videoCapture)
         m_videoCapture.reset(new cv::VideoCapture(camUrl.toStdString()));
      if (m_videoCapture->isOpened()) {
         m_timer.start(m_msFrameInterval, this);
         qDebug() << "Started playing video file " << camUrl << ".";
         emit started();
      } else {
        qDebug() << "Failed to start playing video file " << camUrl << ".";
    }
      m_startPeriod = clock();
   }
   Q_SLOT void stop() { m_timer.stop(); }
   Q_SIGNAL void frameReady(const cv::Mat &);
   cv::Mat frame() const { return m_frame; }
private:
   void timerEvent(QTimerEvent * ev) {
      if (ev->timerId() != m_timer.timerId()) return;
      if (!m_videoCapture->read(m_frame)) { // Blocks until a new frame is ready
         m_timer.stop();
         return;
      }
//      qDebug() << "Captured Image [cols,rows] = [" << m_frame.cols << ", " << m_frame.rows << "]";
      m_fps++;
      m_endPeriod = clock();
      if (m_endPeriod - m_startPeriod > 1000) {
          m_startPeriod = m_endPeriod;
          m_measuredFps = "FPS[" + QString::number(m_fps) + "]";
          m_fps = 0;
      }
      int h = m_frame.size().height;
      cv::putText(m_frame, //target image
                  m_cameraName.toStdString(), //text
                  cv::Point(10, h-30), //top-left position
                  cv::FONT_HERSHEY_DUPLEX,
                  1.0,
                  CV_RGB(255, 255, 255), //font color
                  2);
      cv::putText(m_frame, //target image
                  m_measuredFps.toStdString(), //text
                  cv::Point(10, h-70), //top-left position
                  cv::FONT_HERSHEY_DUPLEX,
                  1.0,
                  CV_RGB(255, 255, 255), //font color
                  2);
      m_track.track(m_frame);
      emit frameReady(m_frame);
   }
   // URL and name of the camera
   QString m_captureName = "no name";
   QString m_cameraName = "no name";

   // FPS counting
   clock_t m_startPeriod = 0;
   clock_t m_endPeriod = 0;
   uint64_t m_fps = 0;
   QString m_measuredFps = "FPS[-]";
};

class Converter : public QObject {
   Q_OBJECT
   Q_PROPERTY(QImage image READ image NOTIFY imageReady USER true)
   Q_PROPERTY(bool processAll READ processAll WRITE setProcessAll)
   QBasicTimer m_timer;
   cv::Mat m_frame;
   QImage m_image;
   bool m_processAll = false;
   AddressTracker m_track;
   void queue(const cv::Mat &frame) {
      if (!m_frame.empty()) qDebug() << "Converter dropped frame!";
      m_frame = frame;
      if (! m_timer.isActive()) m_timer.start(0, this);
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
      if (ev->timerId() != m_timer.timerId()) return;
      process(m_frame);
      m_frame.release();
      m_track.track(m_frame);
      m_timer.stop();
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
   void paintEvent(QPaintEvent *) {

      QPainter p(this);

      if (!m_img.isNull()) {
//         setMinimumSize(m_img.width(), m_img.height());
         setAttribute(Qt::WA_OpaquePaintEvent);
         QRectF targetSize(0,0,width(), height());
         QRect sourceSize(0,0,m_img.width(), m_img.height());
         p.drawImage(targetSize, m_img, sourceSize, Qt::DiffuseDither);
         painted = true;
         return;
      }

      // As standard draw a border as no image present.
      QPen pen(QColor(255,0,0,255));
      p.setPen(pen);
      p.drawRect(0,0,width()-1, height()-1);
      p.drawLine((QLine(0,0,width() -1, height()-1)));
      p.drawLine((QLine(width() -1,0,0, height()-1)));

   }
public:
   ImageViewer(QWidget * parent = nullptr) : QWidget(parent) {
       setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
    }
   ~ImageViewer() { qDebug() << __FUNCTION__ << "reallocations" << m_track.reallocs; }
   Q_SLOT void setImage(const QImage &img) {
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
       QObject::connect(&vStream->capture, &Capture::frameReady, &vStream->converter, &Converter::processFrame);
       QObject::connect(&vStream->converter, &Converter::imageReady, &vStream->view, &ImageViewer::setImage);
       QObject::connect(&vStream->capture, &Capture::started, [](){ qDebug() << "Capture started."; });

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

   return app.exec();
}

#include "main.moc"
