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
