zfec-fs
=======

Simple fuse filesystem that can be used to get an erasure-coded view on a
filesystem.

Erasure-coding provides a way to efficiently distribute data over several
storage locations such that an arbitrary set of (say) 3 locations can be used to
completely recover the data. zfec-fs uses the erasure coding algorithm zfec,
which is part of Tahoe-LAFS.

This fuse filesystem can be used as a simple replacement of Tahoe-LAFS:
Use it to provide the erasure-coded views on your data and keep those views in
sync with your storage using for example rsync. That way, your storage nodes do
not need to run Tahoe-LAFS.

## How to use it?

Just type something like `zfec_fs.py /mnt/zfec` (you can play around with the
parameters later, for help type `zfec_fs.py --help`) to obtain a virtual
filesystem in /mnt/zfec. For each "share", there will be a subdirectory (i.e.
/mnt/zfec/00, /mnt/zfec/01, /mnt/zfec/02, and so on) which contains an
erasure-coded replica of your whole root filesystem. By default, the number of
"required shares" is 3, so all the files will be roughly one third in size and
only contain one third of the information. You can now choose an arbitrary
subset of at least three of these shares and copy files over to your distributed
backup. It is perfectly ok to just copy some files or subdirectories inside the
replicas.

## How to restore the data?

Just provide at least the number of required shares (i.e. three per default)
with a common directory structure (it does not matter if some files are missing
from some shares, as long as they are present in the required number of shares)
in one directory, e.g. `/tmp/restore`. The directory names of the shares do not
matter. Now call `zfec_fs.py -o restore -o source=/tmp/restore /mnt/restored`.
Hopefully, your files should magically appear again in `/mnt/restored`, you just
need to copy them.

## Known limitations

The restore implementation is not yet resilient against corrupted data in
shares. If a share provides a file, it is assumed that all data is intact and
all shares reflect exactly the same state of that file. Note that, though, only
the first required number of shares are used that contain a requested file to
restore it, even if it is present in multiple shares.
