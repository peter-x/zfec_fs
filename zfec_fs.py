#!/usr/bin/env python

import os, sys
from errno import *
from stat import *
import fcntl
import fuse
import zfec
from fuse import Fuse


fuse.fuse_python_api = (0, 2)

fuse.feature_assert('stateful_files')


class ZfecFs(Fuse):

    METADATA_LENGTH = 3

    def __init__(self, *args, **kw):

        Fuse.__init__(self, *args, **kw)

        self.restore = 0
        self.source = '/'
        self.shares = 20
        self.required = 3

    # encode helpers

    def decode_storage_index(self, index):
        if len(index) != 2: return None
        index = int(index, 16)
        if index < self.shares:
            return index
        return None

    def decode_path(self, path):
        parts = path.split('/')
        while len(parts) > 0 and parts[0] == '':
            parts = parts[1:]
        if len(parts) == 0: return (-1, '/')
        index = self.decode_storage_index(parts[0])
        if index is None: return (None, None)
        return (index, self.source + '/' + '/'.join(parts[1:]))

    def shared_file_stat(self, stat):
        # divide by self.required, round up and add metadata length
        size = (stat.st_size + self.required - 1) // self.required + \
               ZfecFs.METADATA_LENGTH
        return os.stat_result((stat.st_mode, stat.st_ino, stat.st_dev,
                               stat.st_nlink, stat.st_uid, stat.st_gid,
                               size,
                               stat.st_atime, stat.st_mtime, stat.st_ctime))
    # restore helpers

    def first_match_in_share(self, path):
        for share in os.listdir(self.source):
            sharepath = '/'.join((self.source, share, path))
            if os.path.exists(sharepath):
                return sharepath
        return None

    def original_file_stat(self, stat):
        # TODO subtract metadata length from size, multiply by three
        # and correctly add padding found in metadata
        return stat


    # fuse filesystem functions

    def getattr(self, path):
        if self.restore:
            sharepath = self.first_match_in_share(path)
            if sharepath is None:
                return -ENOENT
            else:
                return self.original_file_stat(os.lstat(sharepath))
        else:
            index, path = self.decode_path(path)
            if index is None:
                return -ENOENT
            else:
                return self.shared_file_stat(os.lstat(path))

    def readlink(self, path):
        if self.restore:
            sharepath = self.first_match_in_share(path)
            if sharepath is None:
                return -ENOENT
            else:
                return os.readlink(sharepath)
        else:
            index, path = self.decode_path(path)
            if index is None:
                return -ENOENT
            else:
                return os.readlink(path)

    def readdir(self, path, offset):
        if self.restore:
            found = set()
            for share in os.listdir(self.source):
                sharepath = '/'.join((self.source, share, path))
                try:
                    for e in os.listdir(sharepath):
                        if e not in found:
                            found.add(e)
                            yield fuse.Direntry(e)
                except OSError:
                    pass
        else:
            index, path = self.decode_path(path)
            if index is None:
                return
            elif index == -1:
                for e in range(self.shares):
                    yield fuse.Direntry('%02x' % e)
            else:
                for e in os.listdir(path):
                    yield fuse.Direntry(e)

    def utime(self, path, times):
        if self.restore:
            sharepath = self.first_match_in_share(path)
            if sharepath is None:
                return -ENOENT
            else:
                return os.utime(sharepath, times)
        else:
            index, path = self.decode_path(path)
            if index is None:
                return -ENOENT
            else:
                return os.utime(path, times)

    def access(self, path, mode):
        if self.restore:
            sharepath = self.first_match_in_share(path)
            if sharepath is None:
                return -ENOENT
            elif not os.access(sharepath, mode):
                return -EACCESS
        else:
            index, path = self.decode_path(path)
            if index is None:
                return -ENOENT
            elif not os.access(path, mode):
                return -EACCES

    def statfs(self):
        return os.statvfs(self.source)

    def main(self, *a, **kw):
        self.restore = int(self.restore) != 0
        self.shares = int(self.shares)
        self.required = int(self.required)
        assert self.required >= 1
        assert self.required <= self.shares
        if self.restore:
            self.file_class = self.ZfecRestoreFile
            self.decoder = zfec.Decoder(self.required, self.shares)
        else:
            self.file_class = self.ZfecFile
            self.encoder = zfec.Encoder(self.required, self.shares)

        return Fuse.main(self, *a, **kw)

    class ZfecFile(object):

        def __init__(self, path, flags, *mode):
            global server
            self.index, self.path = server.decode_path(path)

            self.required = server.required
            self.shares = server.shares
            self.fd = os.open('/'.join((server.source, self.path)), os.O_RDONLY)
            self.file = os.fdopen(self.fd, 'r')
            self.direct_io = False

        def _filesize(self):
            self.file.seek(0, os.SEEK_END)
            return self.file.tell()

        def _correct_datasize(self, data, fileoffset):
            excess = len(data) % self.required
            if excess == 0:
                return data
            # short read, check whether we are at the end of the file
            if fileoffset + len(data) < self._filesize():
                # end of file not reached, just remove excess bytes
                return data[:-excess]
            else:
                # end of file reached, add surplus zeros
                return data + '\0' * (self.required - excess)

        def read(self, length, offset):
            metadata_part = min(ZfecFs.METADATA_LENGTH - offset, length)
            if metadata_part > 0:
                return self._read_metadata(metadata_part, offset) + \
                       self._read_data(length - metadata_part, 0)
            else:
                return self._read_data(length, offset - ZfecFs.METADATA_LENGTH)

        def _read_metadata(self, length, offset):
            if length <= 0: return ''
            metadata = chr(self.required) + chr(self.index) + \
                       chr(self._filesize() % self.required)
            return metadata[offset:offset + length]

        def _read_data(self, length, offset):
            req = self.required
            self.file.seek(offset * req)
            data = self.file.read(length * req)
            data = self._correct_datasize(data, offset * req)

            parts = tuple(data[p:len(data):req] for p in range(req))

            global server
            return server.encoder.encode(parts, (self.index,))[0]

        def release(self, flags):
            self.file.close()

        def flush(self):
            os.close(os.dup(self.fd))

        def fgetattr(self):
            global server
            return server.shared_file_stat(os.fstat(self.fd))

        def lock(self, cmd, owner, **kw):
            op = { fcntl.F_UNLCK : fcntl.LOCK_UN,
                   fcntl.F_RDLCK : fcntl.LOCK_SH,
                   fcntl.F_WRLCK : fcntl.LOCK_EX }[kw['l_type']]
            if cmd == fcntl.F_GETLK:
                return -EOPNOTSUPP
            elif cmd == fcntl.F_SETLK:
                if op != fcntl.LOCK_UN:
                    op |= fcntl.LOCK_NB
            elif cmd == fcntl.F_SETLKW:
                pass
            else:
                return -EINVAL

            fcntl.lockf(self.fd, op, kw['l_start'], kw['l_len'])


    class ZfecRestoreFile(object):

        def __init__(self, path, flags, *mode):
            global server
            self.source = server.source
            self.required = server.required
            self.shares = server.shares
            self.fds = []
            self.files = []
            for share in os.listdir(self.source):
                sharepath = '/'.join((self.source, share, path))
                try:
                    fd = os.open(sharepath, os.O_RDONLY)
                    f = os.fdopen(fd, 'r')
                except OSError:
                    continue
                self.fds.append(fd)
                self.files.append(f)
                if len(self.fds) >= self.required:
                    break
            if len(self.fds) < self.required:
                raise OSError(EIO, '') # TODO more precise

            self.direct_io = False
            self.keep_cache = False
 
        def _read_metadata(self):
            leftover = None
            sharesize = None
            indices = []
            for f in self.files:
                f.seek(0)
                (req, index, left) = [ord(c) for c in
                                            f.read(ZfecFs.METADATA_LENGTH)]
                if req != self.required:
                    print("req value is wrong")
                    raise OSError(EIO, '') # TODO more precise
                if leftover is None:
                    leftover = left
                elif leftover != left:
                    print("leftover values differ")
                    raise OSError(EIO, '') # TODO more precise
                f.seek(0, os.SEEK_END)
                size = f.tell()
                if sharesize is None:
                    sharesize = size
                elif sharesize != size:
                    print("file sizes differ")
                    raise OSError(EIO, '') # TODO more precise
                indices.append(index)
            if leftover >= self.required or sharesize < 3:
                print("leftover value invalid")
                raise OSError(EIO, '') # TODO more precise
            sharesize -= 3
            if sharesize == 0 and leftover != 0:
                print("leftover too large for size")
                raise OSError(EIO, '') # TODO more precise
            return ((sharesize - 1) * self.required + leftover, indices)

        def read(self, length, offset):
            required = self.required
            (filesize, indices) = self._read_metadata()
            if offset >= filesize:
                return ''
            data = []
            for i in range(required):
                f = self.files[i]
                f.seek(offset // required + ZfecFs.METADATA_LENGTH)
                data.append(f.read(length))
            l = min(len(x) for x in data)
            data = tuple(x[:l] for x in data)
            global server
            decoded = server.decoder.decode(data, indices)[0]
            if offset + len(decoded) > filesize:
                return decoded[:filesize - offset]
            else:
                return decoded


        def release(self, flags):
            for f in self.files:
                f.close()

        def flush(self):
            for fd in self.fds:
                os.close(os.dup(fd))

        def fgetattr(self):
            global server
            return server.original_file_stat(os.fstat(self.fds[0]))

        def lock(self, cmd, owner, **kw):
            op = { fcntl.F_UNLCK : fcntl.LOCK_UN,
                   fcntl.F_RDLCK : fcntl.LOCK_SH,
                   fcntl.F_WRLCK : fcntl.LOCK_EX }[kw['l_type']]
            if cmd == fcntl.F_GETLK:
                return -EOPNOTSUPP
            elif cmd == fcntl.F_SETLK:
                if op != fcntl.LOCK_UN:
                    op |= fcntl.LOCK_NB
            elif cmd == fcntl.F_SETLKW:
                pass
            else:
                return -EINVAL

            # TODO locking all will certainly result in deadlocks...
            fcntl.lockf(self.fds[0], op, kw['l_start'], kw['l_len'])


server = None

def main():
    usage = """zfec filesystem. Uses zfec to provide several shared views
on a certain directory hierarchy.""" + Fuse.fusage

    global server
    server = ZfecFs(version="%prog " + fuse.__version__,
                    usage=usage,
                    dash_s_do='setsingle')

    server.multithreaded = False

    server.parser.add_option(mountopt="restore", metavar="RESTORE",
                             default=server.restore,
                             help="switch to restore mode")
    server.parser.add_option(mountopt="source", metavar="PATH",
                             default=server.source,
                             help="provide shared view for filesystem from " +
                                  "under PATH [default: %default]")
    server.parser.add_option(mountopt="required", metavar="REQUIRED",
                             default=server.required,
                             help="number of shares required for restore " +
                                  "(will influence storage size) [default: %default]")
    server.parser.add_option(mountopt="shares", metavar="SHARES",
                             default=server.shares,
                             help="number of overall shares (must be at " +
                                  "least REQUIRED, does not harm to be " +
                                  "large) [default: %default]")
    server.parse(values=server, errex=1)

    try:
        if server.fuse_args.mount_expected():
            os.chdir(server.source)
    except OSError:
        print >> sys.stderr, "can't enter root of source filesystem"
        sys.exit(1)

    server.main()


if __name__ == '__main__':
    main()
