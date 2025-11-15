## make disk
```shell
$ dd if=/dev/zero of=disk.bin bs=512 count=131072
$ mkfs.vfat -F 32 -n TESTDISK disk.bin
$ mkdir mnt
$ sudo mount -o loop disk.bin mnt
$ cd mnt
$ echo -e 'meowmeowmeow\n' | sudo tee meow.txt
$ sudo mkdir teddy
$ echo -e 'I can fly!\n' | sudo tee teddy/fly.txt
$ cd ..
$ sudo umount mnt
```
