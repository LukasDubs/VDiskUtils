# Experimental Utility for Windows to Read and soon Modify Virtual Disk Images (currently supported: .vhd files)

## Test disk creation with diskpart:
  \# create and attach vdisk  
  create vdisk FILE="path to file.vhd" MAXIMUM='size in Megabytes'  
  list vdisk  
  select vdisk 'path to file.vhd'  
  attach vdisk  
  \# convert to gpt format  
  convert gpt  
  \# create partition  
  create partition efi  
  \# format partition  
  list partition  
  select partition 'partition index'  
  format fs='ntfs or fat32' override  
  detach vdisk  
  \# now the vdisk is ready  

## Example of working with filesystems on a vdisk:  
  \# open vdisk  
  open -f 'path to vdisk'  
  \# select partition ot modify  
  list partition  
  select partition 'partition index'  
  \# enter the FS-Managing mode  
  enter  
  
  \# example of getting the size of a file  
  open 'path to file'  
  attribute get size  
  
  \# close the current file when not needed  
  close  

  \# returning to vdisk managing context  
  exit  
