# qt_multicamera Qt 5.9.6

qt_multicamera is a QT project where I wanted to try out capturing multiple USB Web Cameras, as well as other video sources. 

There is an ini file where multiple video sources can be defined, and if OpenCV recognises it then it will create an individual QWidget for each one and display in a main windows. 

This has been tested with 2 WebCameras and 6 MP4 video files at the same time. So 8 video sources and it runs well on my computer. 

## Problems are:

  *  Framerate of video files was unknown so I set to 30 fps
  *  USB camera bandwidth was a problem until I found an article saying put them on different ports/hubs and it will then work...which it did. See https://stackoverflow.com/questions/21246766/how-to-efficiently-display-opencv-video-in-qt

## References to other source code used
I used two other repositories to make this and modified to suit my need here. 
  1. Qt displaying multiple videos in different threads. See https://stackoverflow.com/questions/21246766/how-to-efficiently-display-opencv-video-in-qt and https://github.com/KubaO/stackoverflown/tree/master/questions/opencv-21246766
  2. A Java stype ini file with key value pairs.  See https://github.com/fredyw/cpp-properties




