# qt_multicamera Qt 5.9.6

qt_multicamera is a QT project where I wanted to try out capturing multiple USB Web Cameras, as well as other video sources. 

There is an ini file where multiple video sources can be defined, and if OpenCV recognises it then it will create an individual QWidget for each one and display in a main windows. 

This has been tested with 2 WebCameras and 6 MP4 video files at the same time. So 8 video sources and it runs well on my computer. 

## Problems are:

  *  Framerate of video files was unknown so I set to 30 fps
  *  USB camera bandwidth was a problem until I found an article saying put them on different ports/hubs and it will then work...which it did. See https://stackoverflow.com/questions/21246766/how-to-efficiently-display-opencv-video-in-qt
  *  Should do some error checking and make sure all works properly. Just stitched together. 
  
## References to other source code used
I used two other repositories to make this and modified to suit my need here. 
  1. Qt displaying multiple videos in different threads. See https://stackoverflow.com/questions/21246766/how-to-efficiently-display-opencv-video-in-qt and https://github.com/KubaO/stackoverflown/tree/master/questions/opencv-21246766
  2. A Java stype ini file with key value pairs.  See https://github.com/fredyw/cpp-properties
  
## Example
The following image shows 6 running MP4 videos and two web cameras simultaneously as described. Each is in a separate widget, placed in a QGridLayout in a central widget of the QMainWindow. 
![Screenshot_running_videos](https://user-images.githubusercontent.com/5513887/58373323-02e3f980-7f35-11e9-8d5e-978df4514471.png)

## Videos not supplied

As I do not want to distribute video the videos directory is empty. Simply download any video and put the name into the videoProperties.ini file. I used the clipcanvas_14348.mp4 video in the snapshot above and then copied renaming as 1..5 so I had size videos to test with.

I googled free video and found https://www.veedybox.com/blog/32-resources-for-free-stock-videos/ and found https://coverr.co/ where I downloaded free videos for testing.

## Platforms
### Linux
I did development on Linux, Ubuntu. And it all just seemed to go fine so I have not noted anything down.
### Windows 10
I seemed to have problems as I did not understand the environment. I wanted to use Qt creator and hence had to compile OpenCV on the machine with Mingw. Here are two links I found very useful:
  * https://wiki.qt.io/How_to_setup_Qt_and_openCV_on_Windows - Old but worked on OpenCV 4.1 on Windows 10.
  * https://github.com/opencv/opencv/issues/14286 - Solved the compilation error because of dependency.
  * https://forum.qt.io/topic/96460/opencv_highgui-seems-that-it-can-t-find-qt-libs/2 Cmake-GUI referenced wrong Qt
  * Lastly I ran it and got no output in any windows in QtCreator. I found running command line it showed that Qt runtime libraries and then OpenCV libraries were not on the PATH environment variable when running. Strange no output so just added comments here in case anyone has same problem. 

