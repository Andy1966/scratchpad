import pathlib 
import sys
import hashlib
import time
from collections import defaultdict

# We will create a dictionary of sha256sum as the key for each file, and then a list of the files under each key of the paths. 
duplicate_dict = defaultdict(lambda: {})

#Simple test function to simply populate a duplicate dictionary with
#the hash code for a file the file path and name.  In this way we can find duplicate
#files with the same key hash.
def gather_image_sha_meta_fn(file):
    hash = file_hash(file, 128 * 1024)
    duplicate_dict[hash][file] = 1    #for each has, add file name to dictionary so we have all duplicates
    print('{0}, {1}'.format(hash, file))

# A simple function to find the hash for a file
def file_hash(filename, blocksize = 64 * 1024):
    h = hashlib.sha256()
    with open(filename, 'rb', buffering=0) as f:
        for block in iter(lambda : f.read(blocksize), b''):
            h.update(block)
    return h.hexdigest()

#A function to iterate through the duplicate dictionary. Any count
#greater than one means there are duplicate files and so they
#will be displayed.
def show_duplicates():
    duplicate_count = 0
    if len(duplicate_dict) == 0:
        print('\n -- No duplicates found, empty list. -- \n')
        return
    print('\n -- Duplicates are: -- \n')
    for sha_key, sha_file_dict in duplicate_dict.items():
        if len(sha_file_dict) > 1: 
            for file_key,v in sha_file_dict.items():
                print('\t{0}: {1}'.format(duplicate_count, file_key))
            duplicate_count += 1
    print('\n -- {0} duplicates found. -- \n'.format(duplicate_count))

 
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
    print('"{0}" files processed in {1} seconds.\n'.format(process_all_files(cwd, gather_image_sha_meta_fn), time.clock()))
    show_duplicates()
