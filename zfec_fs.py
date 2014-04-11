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

        self.root = '/'
        self.shares = 20
        self.required = 3

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
        return (index, self.root + '/'.join(parts[1:]))

    def modified_file_stat(self, stat):
        # divide by self.required, round up and add metadata length
        size = (stat.st_size + self.required - 1) // self.required + \
               ZfecFs.METADATA_LENGTH
        return os.stat_result((stat.st_mode, stat.st_ino, stat.st_dev,
                               stat.st_nlink, stat.st_uid, stat.st_gid,
                               size,
                               stat.st_atime, stat.st_mtime, stat.st_ctime))

    # fuse filesystem functions

    def getattr(self, path):
        index, path = self.decode_path(path)
        if index is None:
            return -ENOENT
        else:
            return self.modified_file_stat(os.lstat(path))

    def readlink(self, path):
        index, path = self.decode_path(path)
        if index is None:
            return -ENOENT
        else:
            return os.readlink(path)

    def readdir(self, path, offset):
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
        index, path = self.decode_path(path)
        if index is None:
            return -ENOENT
        else:
            return os.utime(path, times)

    def access(self, path, mode):
        index, path = self.decode_path(path)
        if index is None:
            return -ENOENT
        elif not os.access(path, mode):
            return -EACCES

    def statfs(self):
        return os.statvfs(self.root)

    def main(self, *a, **kw):
        self.file_class = self.ZfecFile
        self.shares = int(self.shares)
        self.required = int(self.required)
        assert self.required >= 1
        assert self.required <= self.shares

        self.encoder = zfec.Encoder(self.required, self.shares)
        return Fuse.main(self, *a, **kw)

    class ZfecFile(object):

        def __init__(self, path, flags, *mode):
            global server
            self.index, self.path = server.decode_path(path)

            self.required = server.required
            self.shares = server.shares
            self.fd = os.open(self.path, os.O_RDONLY)
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
            return server.modified_file_stat(os.fstat(self.fd))

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


server = None

def main():
    usage = """zfec filesystem. Uses zfec to provide several shared views
on a certain directory hierarchy.""" + Fuse.fusage

    global server
    server = ZfecFs(version="%prog " + fuse.__version__,
                    usage=usage,
                    dash_s_do='setsingle')

    server.multithreaded = False

    server.parser.add_option(mountopt="root", metavar="PATH",
                             default=server.root,
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
            os.chdir(server.root)
    except OSError:
        print >> sys.stderr, "can't enter root of underlying filesystem"
        sys.exit(1)

    server.main()


if __name__ == '__main__':
    main()
