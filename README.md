# simple_fat32

`simple_fat32` is a minimal FAT32 file system implementation designed for learning, experimentation, and OS development.  
It provides a straightforward and clean codebase that focuses on correctness and simplicity rather than full specification coverage.

## Features

- `ls_dir`
- `read_file`

More functionality will be added over time, including file creation, writing, caching improvements, and directory manipulation.

## Build Instructions
### make `disk.bin`
```shell
dd if=/dev/zero of=disk.bin bs=512 count=131072
mkfs.vfat -F 32 -n TESTDISK disk.bin
mkdir mnt
sudo mount -o loop disk.bin mnt
cd mnt
echo -e 'meowmeowmeow\n' | sudo tee meow.txt
sudo mkdir teddy
echo -e 'I can fly!\n' | sudo tee teddy/fly.txt
cd ..
sudo umount mnt
```
### build
```shell
make
```
