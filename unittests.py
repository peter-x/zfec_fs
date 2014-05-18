#!/usr/bin/env python

import unittest
import random 

import zfec
import zfec_fs


class FakeFile(zfec_fs.ReadableFile):
    def __init__(self, content):
        self.content = content

    def read(self, offset, length):
        return self.content[offset:offset + length]

    def size(self):
        return len(self.content)

    def stat(self):
        return None # not implemented

    def close(self):
        pass

class ZFecTests(unittest.TestCase):
    def random_fake_file(self, size):
        content = ''.join(chr(random.randint(0, 255)) for i in range(size))
        return FakeFile(content)

    def test_fake_file(self):
        for size in list(range(20)) + [23, 57, 64, 277, 1045]:
            originalFile = self.random_fake_file(size)
            self.assertEqual(size, originalFile.size())
            self.assertEqual(size, len(originalFile.read(0, size)))
            if size > 0:
                self.assertEqual(size - 1, len(originalFile.read(1, size)))
                self.assertEqual(size - 1, len(originalFile.read(0, size - 1)))
                self.assertEqual(1, len(originalFile.read(size - 1, 10)))
            self.assertEqual(0, len(originalFile.read(size, 2)))
            self.assertEqual(0, len(originalFile.read(9, 0)))

    def test_encoder(self):
        for required in range(1, 10):
            zfecEncoder = zfec.Encoder(required, 255)
            for size in list(range(20)) + [23, 57, 64, 277, 1045]:
                index = 2
                enc_size = zfec_fs.FileEncoder.encoded_size(required, size)
                self.assertTrue(enc_size >= 3)
                originalFile = self.random_fake_file(size)
                enc = zfec_fs.FileEncoder(required, index,
                                          originalFile, zfecEncoder)
                encoded_data = enc.read(0, 2 * size + 10)
                # test encoded_size and actual returned size
                self.assertEqual(enc_size, len(encoded_data))
                # test reading with offsets and lengths
                for offset in list(range(20)) + [32, 55, 128, 1024, 1050]:
                    for length in list(range(0, 20)) + [64, 177, 123, 1024, 1050]:
                        self.assertEqual(encoded_data[offset:offset + length],
                                         enc.read(offset, length))
                # test metadata
                self.assertEqual(chr(required), enc.read(0, 1))
                self.assertEqual(chr(index), enc.read(1, 1))
                self.assertEqual(chr(size % required), enc.read(2, 1))

    def test_decoder(self):
        for required in range(1, 10):
            zfecEncoder = zfec.Encoder(required, 255)
            zfecDecoder = zfec.Decoder(required, 255)
            for size in list(range(20)) + [23, 57, 64, 277, 1045]:
                originalFile = self.random_fake_file(size)
                originalData = originalFile.content
                encodedFiles = [FakeFile(zfec_fs.FileEncoder(
                                        required, index,
                                        originalFile, zfecEncoder).read(0, size + 10))
                                for index in range(required)]
                decoder = zfec_fs.FileDecoder(required, encodedFiles,
                                              zfecDecoder)
                for f in encodedFiles:
                    self.assertEqual(size, zfec_fs.FileDecoder.decoded_size(f))

                self.assertEqual(originalData, decoder.read(0, size))
                # test reading with offsets and lengths
                for offset in list(range(1, 20)) + [32, 55, 128, 1024, 1050]:
                    for length in list(range(0, 20)) + [64, 177, 123, 1024, 1050]:
                        self.assertEqual(originalData[offset:offset + length],
                                         decoder.read(offset, length))

if __name__ == '__main__':
    unittest.main()
