import pathlib 
import sys
import time

#Simple test function to simply print filename for now. In future we will process it properly.
def process_image_fn(file):
    print (file)

#Simple function to iterate all directories and files, and pass it to be processed. 
# cwd - The pathlib.PATH for the current directory to be processed
# process_image_fn(file) - the function to be called with the pathlib.PATH of the file
def process_all_files(cwd, process_image_fn):
    '''Recursively go through all files in all directories and process each one'''
    filecount = 0

    # Create a path for the current working directory
    cwd = pathlib.Path(cwd)

    # Process all files first
    for afile in [d for d in cwd.iterdir() if d.is_file()]:
        process_image_fn(afile.resolve())
        filecount += 1

    # And then iterate all sub directories
    for dir in [d for d in cwd.iterdir() if d.is_dir()]:
        filecount += process_all_files(dir, process_image_fn)

    #Make sure we update how many files processed
    return filecount

# If this is called individually we can iterate the current working directory, 
# or the required directory can be supplied as an argument if required.
if __name__ == '__main__':

    #Default is current working directory
    cwd = pathlib.Path.cwd()

    # Directory argument supplied
    if len(sys.argv) == 2:
        cwd = pathlib.Path(sys.argv[1])
        if cwd.exists() and cwd.is_dir():
            cwd = pathlib.Path(sys.argv[1])
        else:
            print('ERROR: "{0}" is not a directory.'.format(sys.argv[1]))
            exit(1)

    print('\n -- Processing all files in "{0}" -- \n'.format(cwd))
    start = time.clock()
    print('"{0}" files processed in {1} seconds.\n'.format(process_all_files(cwd, process_image_fn), time.clock()))
