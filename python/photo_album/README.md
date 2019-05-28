# photo_album
Experiements in finding faces and people in a photo album collection at home using some ML or AI methods.

Background is that I have a family of 5 where phone images over a number of years have been dumped. They are not organised. There are currently over 22000 images and videos. So automating person detection, or even object, to automatically group and then label metadata would be useful. 

At the moment these are a collection of evolutionary individual python files to work on that data. 

The environemnt used is Anaconda with python 3.6, Opencv and Dlib installed.

Currently the following are available and can be run like this:

python process_file.py <optional path>    - This will just recursively walk through path and use a process function on each file. 
python find_duplicates.py <optional path> - Recursively walk through the path and find any duplicate images
python find_faces.py <optional path>      - Recusively walk through the path and find faces using dlib and opencv. Run landmakrs from dlib and show the images where faces have been found in a window for viewing. Press a button in the window to continue to the next one. Only images recognised by opencv will be used, otherwise they will be ignored. 

