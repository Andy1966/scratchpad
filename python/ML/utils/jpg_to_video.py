import pathlib 
import sys
import time
import cv2 as cv2

#
# Expected the command line format to be:
# python jpg_to_video.py <directory> <outputVideoFilename>.mp4
#
# Very basic so no command line checks for this util!!!!

#Globals
width = 0
height = 0
# Define the codec and create VideoWriter object
fourcc = cv2.VideoWriter_fourcc(*'mp4v') # Be sure to use lower case
out = None
outputFile = 'test.mp4'

#Simple test function to simply print filename for now. In future we will process it properly.
def process_image_fn(file, outputFile):
    global out, width, height
    
    filepath = '{}'.format(file).replace('\\','\\\\')
    
    frame = cv2.imread(filepath)
    if (out is None):
        height, width, channels = frame.shape
        out = cv2.VideoWriter(outputFile, fourcc, 20.0, (width, height))

    out.write(frame) # Write out frame to video
    print ('Processed {}'.format(filepath))
    

#Simple function to iterate all directories and files, and pass it to be processed. 
# cwd - The pathlib.PATH for the current directory to be processed
# process_image_fn(file) - the function to be called with the pathlib.PATH of the file
# outputFile - The output file that will be written. 
def process_all_files(cwd, process_image_fn, outputFile):
    '''Recursively go through all files in all directories and process each one'''
    filecount = 0

    # Create a path for the current working directory
    cwd = pathlib.Path(cwd)

    # Process all files first that are jpg. Assume user was sensible and supplied them in single directory!
    for afile in [d for d in cwd.iterdir() if d.is_file() and '{}'.format(d).endswith('jpg')]:
        process_image_fn(afile.resolve(), outputFile)
        filecount += 1

    # And then iterate all sub directories
    for dir in [d for d in cwd.iterdir() if d.is_dir()]:
        filecount += process_all_files(dir, process_image_fn, outputFile)

    #Make sure we update how many files processed
    return filecount

# If this is called individually we can iterate the current working directory, 
# or the required directory can be supplied as an argument if required.
if __name__ == '__main__':

    #Default is current working directory
    cwd = pathlib.Path.cwd()

    # Directory argument supplied
    if len(sys.argv) > 2:
        cwd = pathlib.Path(sys.argv[1])
        if cwd.exists() and cwd.is_dir():
            cwd = pathlib.Path(sys.argv[1])
        else:
            print('ERROR: "{0}" is not a directory.'.format(sys.argv[1]))
            exit(1)
        outputFile = sys.argv[2]
    else:
        print ('Expecting "python jpg_to_video.py <directory> <outputVideoFilename>.mp4". Exiting now!')
        sys.exit(-1)

    print('\n -- Processing all files in "{0}" -- \n'.format(cwd))
    start = time.clock()
    print('"{0}" files processed in {1} seconds.\n'.format(process_all_files(cwd, process_image_fn, outputFile), time.clock()))# -*- coding: utf-8 -*-

