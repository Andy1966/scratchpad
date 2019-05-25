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
   Q_SLOT void start(int cam = {}) {
       qDebug() << "Camera " << cam << ".";
      if (!m_videoCapture)
         m_videoCapture.reset(new cv::VideoCapture(cam, cv::CAP_V4L2));
      if (m_videoCapture->isOpened()) {
         m_timer.start(m_msFrameInterval, this);
         qDebug() << "Started playing camera[" << cam << "].";
         emit started();
      } else {
        qDebug() << "Failed to start playing camera[" << cam << "].";
    }
   }
   Q_SLOT void start(QString cam) {
       qDebug() << "video file " << cam << ".";
       m_msFrameInterval = MS_ONE_SECOND / VIDEO_FILE_FRAMES_PER_SECOND;
      if (!m_videoCapture)
         m_videoCapture.reset(new cv::VideoCapture(cam.toStdString()));
      if (m_videoCapture->isOpened()) {
         m_timer.start(m_msFrameInterval, this);
         qDebug() << "Started playing video file " << cam << ".";
         emit started();
      } else {
        qDebug() << "Failed to start playing video file " << cam << ".";
    }
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
      m_track.track(m_frame);
      emit frameReady(m_frame);
   }
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
      int w = frame.cols / 3.0, h = frame.rows / 3.0;
      if (m_image.size() != QSize{w,h})
         m_image = QImage(w, h, QImage::Format_RGB888);
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
         setAttribute(Qt::WA_OpaquePaintEvent);
         p.drawImage(0, 0, m_img);
         painted = true;
      }
   }
public:
   ImageViewer(QWidget * parent = nullptr) : QWidget(parent) {}
   ~ImageViewer() { qDebug() << __FUNCTION__ << "reallocations" << m_track.reallocs; }
   Q_SLOT void setImage(const QImage &img) {
      if (!painted) qDebug() << "Viewer dropped frame!";
      if (m_img.size() == img.size() && m_img.format() == img.format()
          && m_img.bytesPerLine() == img.bytesPerLine())
         std::copy_n(img.bits(), img.sizeInBytes(), m_img.bits());
      else
         m_img = img.copy();
      painted = false;
      if (m_img.size() != size()) setFixedSize(m_img.size());
      m_track.track(m_img);
      update();
   }
   QImage image() const { return m_img; }
};

class Thread final : public QThread { public: ~Thread() { quit(); wait(); } };

class VideoStreamInstance {
public:
    ImageViewer view;
    Capture capture;
    Converter converter;
    Thread captureThread, converterThread;
};

#define CAMERAS_PROPERTY_KEY "cameras"

int main(int argc, char *argv[])
{
    qRegisterMetaType<cv::Mat>();
   QApplication app(argc, argv);

   // Load the ini file that notifies what video streams to use.
   cppproperties::PropertiesParser propParser = cppproperties::PropertiesParser();
   cppproperties::Properties p = propParser.Read("../qt_multicamera/videoProperties.ini");

   QString cameraList = QString::fromStdString(p.GetProperty(CAMERAS_PROPERTY_KEY));
   QStringList camera_list = cameraList.split((","));


   // Calculate the size of the grid required to display evenly
   int numStreams = camera_list.size();
   float root = qSqrt(numStreams);
   int gridSizeY = numStreams / (int) root;
   int gridSizeX = numStreams / (int) gridSizeY;
   gridSizeX += (int) numStreams % (gridSizeX * gridSizeY);

  // For now one window and display all video stream widgets within it
  QMainWindow viewingWindow;
  viewingWindow.setWindowTitle("Multiple Video Streaming Viewer");

  // Make sure we can put the video streams in a grid
  QGridLayout * viewingGrid = new QGridLayout();

  QWidget* widget = new QWidget(&viewingWindow);
  widget->setLayout(viewingGrid);
  viewingWindow.setCentralWidget(widget);

  viewingWindow.setVisible(true);
  viewingWindow.show();

   int row = 0, col = 0;
   QList<VideoStreamInstance *> streamList;
   QString camera;
   foreach (camera, camera_list)
   {
       camera = camera.trimmed();

       VideoStreamInstance * vStream = new VideoStreamInstance;

       // Everything runs at the same priority as the gui, so it won't supply useless frames.
       vStream->converter.setProcessAll(false);
       vStream->captureThread.start();
       vStream->converterThread.start();
       vStream->capture.moveToThread(&vStream->captureThread);
       vStream->converter.moveToThread(&vStream->converterThread);
       QObject::connect(&vStream->capture, &Capture::frameReady, &vStream->converter, &Converter::processFrame);
       QObject::connect(&vStream->converter, &Converter::imageReady, &vStream->view, &ImageViewer::setImage);
       QObject::connect(&vStream->capture, &Capture::started, [](){ qDebug() << "Capture started."; });
//       vStream->view.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
//       vStream->view.show();

       // Select the right argument for the capture stream.
       QString arg = QString::fromStdString(p.GetProperty(camera.toStdString())).trimmed();
       bool isInt;
       int intArg = arg.toInt(&isInt);

       // And start capturing
       if (isInt) QMetaObject::invokeMethod(&vStream->capture, "start", Qt::QueuedConnection, Q_ARG(int, intArg));
       else QMetaObject::invokeMethod(&vStream->capture, "start", Qt::QueuedConnection, Q_ARG(QString, arg));

       // Keep a list of each video stream
       streamList.append(vStream);

       // Make sure we add the video to the main window widget
       viewingGrid->addWidget(&vStream->view, row, col);
       col += 1;
       if (col == gridSizeX) {col = 0; row+=1;}
   }

   // Optimise for full screen
   QRect rec = QApplication::desktop()->screenGeometry(&viewingWindow);
   int height = rec.height();
   int width = rec.width();
//   viewingWindow.resize(width, height);

   return app.exec();
}

#include "main.moc"
