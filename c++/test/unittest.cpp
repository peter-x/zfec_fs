#define BOOST_TEST_MODULE UnitTest

#include <boost/test/included/unit_test.hpp>
#include <boost/make_shared.hpp>
#include <boost/scoped_ptr.hpp>

#include "metadata.h"
#include "fecwrapper.h"
#include "testfile.h"
#include "fileencoder.h"
#include "filedecoder.h"

using namespace ZFecFS;

boost::shared_ptr<FileEncoder> CreateEncoder(const FecWrapper& fecWrapper,
                                             unsigned int shareIndex,
                                             const std::string& fileContents)
{
    boost::shared_ptr<TestFile> testFile = boost::make_shared<TestFile>(fileContents);
    return boost::make_shared<FileEncoder>(testFile, shareIndex, fecWrapper);
}


BOOST_AUTO_TEST_CASE(encoded_read_size_test_without_extra)
{
    // original data is distributed over 3 chunks of two bytes
    FecWrapper fecWrapper(3, 20);
    boost::shared_ptr<FileEncoder> encoder = CreateEncoder(fecWrapper, 0, "123456");
    char buffer[50];
    BOOST_CHECK_EQUAL(encoder->Read(buffer, 0, 0), 0);
    BOOST_CHECK_EQUAL(encoder->Read(buffer, 1, 0), 1);
    BOOST_CHECK_EQUAL(encoder->Read(buffer, 2, 0), 2);
    BOOST_CHECK_EQUAL(encoder->Read(buffer, 3, 0), 3);
    BOOST_CHECK_EQUAL(encoder->Read(buffer, 4, 0), 4);
    BOOST_CHECK_EQUAL(encoder->Read(buffer, 5, 0), 5);
    BOOST_CHECK_EQUAL(encoder->Read(buffer, 50, 0), Metadata::size + 2);
    BOOST_CHECK_EQUAL(encoder->Read(buffer, 50, 1), Metadata::size + 1);
    BOOST_CHECK_EQUAL(encoder->Read(buffer, 50, 2), Metadata::size);
    BOOST_CHECK_EQUAL(encoder->Read(buffer, 50, 3), Metadata::size - 1);

    BOOST_CHECK_EQUAL(encoder->Read(buffer, Metadata::size, 0), Metadata::size);
    Metadata meta(buffer);
    BOOST_CHECK_EQUAL(meta.index, 0);
    BOOST_CHECK_EQUAL(meta.excessBytes, 0);
    BOOST_CHECK_EQUAL(meta.required, 3);
}

BOOST_AUTO_TEST_CASE(encoded_read_size_test_with_extra)
{
    // original data is distributed over 5 chunks of one byte plus an additinal byte
    FecWrapper fecWrapper(5, 20);
    boost::shared_ptr<FileEncoder> encoder = CreateEncoder(fecWrapper, 1, "123456");
    char buffer[50];
    BOOST_CHECK_EQUAL(encoder->Read(buffer, 0, 0), 0);
    BOOST_CHECK_EQUAL(encoder->Read(buffer, 1, 0), 1);
    BOOST_CHECK_EQUAL(encoder->Read(buffer, 2, 0), 2);
    BOOST_CHECK_EQUAL(encoder->Read(buffer, 3, 0), 3);
    BOOST_CHECK_EQUAL(encoder->Read(buffer, 4, 0), 4);
    BOOST_CHECK_EQUAL(encoder->Read(buffer, 5, 0), 5);
    BOOST_CHECK_EQUAL(encoder->Read(buffer, 50, 0), Metadata::size + 2);
    BOOST_CHECK_EQUAL(encoder->Read(buffer, 50, 1), Metadata::size + 1);
    BOOST_CHECK_EQUAL(encoder->Read(buffer, 50, 2), Metadata::size);
    BOOST_CHECK_EQUAL(encoder->Read(buffer, 50, 3), Metadata::size - 1);

    BOOST_CHECK_EQUAL(encoder->Read(buffer, Metadata::size, 0), Metadata::size);
    Metadata meta(buffer);
    BOOST_CHECK_EQUAL(meta.index, 1);
    BOOST_CHECK_EQUAL(meta.excessBytes, 1);
    BOOST_CHECK_EQUAL(meta.required, 5);
}

BOOST_AUTO_TEST_CASE(encoder_principal_data_test)
{
    // original data is distributed over 5 chunks of three bytes plus two additinal bytes
    const char* originalData = "12345abcdeABCDE78";
    FecWrapper fecWrapper(5, 20);
    boost::shared_ptr<FileEncoder> encoder = CreateEncoder(fecWrapper, 0, originalData);
    char buffer[50];
    BOOST_CHECK_EQUAL(encoder->Read(buffer + 0,  1, 0), 1);
    BOOST_CHECK_EQUAL(encoder->Read(buffer + 1,  3, 1), 3);
    BOOST_CHECK_EQUAL(encoder->Read(buffer + 4,  Metadata::size, 4), Metadata::size);

    Metadata meta(buffer);
    BOOST_CHECK_EQUAL(meta.index, 0);
    BOOST_CHECK_EQUAL(meta.excessBytes, 2);
    BOOST_CHECK_EQUAL(meta.required, 5);

    const char* data[] = {"1aA7", "2bB8", "3cC\0", "4dD\0", "5eE\0"};
    BOOST_CHECK_EQUAL_COLLECTIONS(data[0], data[0] + 4, buffer + Metadata::size, buffer + Metadata::size + 4);

    // now a bit faster for the others
    for (unsigned int index = 1; index < 5; ++index) {
        BOOST_TEST_CHECKPOINT("Checking principal share at index " << index);
        encoder = CreateEncoder(fecWrapper, index, originalData);
        BOOST_CHECK_EQUAL(encoder->Read(buffer, 50, 0), Metadata::size + 4);
        Metadata metaIndexed(buffer);
        BOOST_CHECK_EQUAL(metaIndexed.index, index);
        BOOST_CHECK_EQUAL(metaIndexed.excessBytes, 2);
        BOOST_CHECK_EQUAL(metaIndexed.required, 5);
        BOOST_CHECK_EQUAL_COLLECTIONS(data[index], data[index] + 4,
                                      buffer + Metadata::size, buffer + Metadata::size + 4);
    }
}

std::vector<boost::shared_ptr<AbstractFile> > EncodeFile(
                                    const FecWrapper& fecWrapper,
                                    unsigned int minIndex, unsigned int maxIndex,
                                    const std::string& fileContents)
{
    std::vector<boost::shared_ptr<AbstractFile> > encodedFiles;
    off_t encodedSize = FileEncoder::Size(fileContents.size(), fecWrapper.GetSharesRequired());
    boost::shared_ptr<TestFile> inputFile = boost::make_shared<TestFile>(fileContents);
    for (unsigned int index = minIndex; index <= maxIndex; ++index) {
        FileEncoder encoder(inputFile, index, fecWrapper);
        std::vector<char> encoded(encodedSize, '\0');
        encoder.Read(encoded.data(), encodedSize, 0);
        encodedFiles.push_back(boost::make_shared<TestFile>(encoded));
    }
    return encodedFiles;
}


BOOST_AUTO_TEST_CASE(encode_decode_check)
{
    std::string contents("1234567abc\n\09abcd");
    FecWrapper fecWrapper(7, 20);
    std::vector<boost::shared_ptr<AbstractFile> > encoded = EncodeFile(fecWrapper, 4, 4 + 7, contents);

    boost::scoped_ptr<FileDecoder> decoder(FileDecoder::Open(encoded, fecWrapper));
    BOOST_REQUIRE_EQUAL(contents.size(), decoder->Size());
    std::vector<char> decoded(contents.size(), '\0');
    decoder->Read(decoded.data(), decoded.size(), 0);
    BOOST_CHECK_EQUAL_COLLECTIONS(contents.begin(), contents.end(),
                                  decoded.begin(), decoded.end());

    char buffer[50];
    for (unsigned int offset = 0; offset < 20; ++offset) {
        for (unsigned int length = 0; offset + length < 50; length ++) {
            BOOST_TEST_CHECKPOINT("Checking encode-decode at offset " << offset << " and length " << length);
            unsigned int lengthToExpect = length;
            if (offset > contents.size())
                lengthToExpect = 0;
            else if (offset + lengthToExpect > contents.size())
                lengthToExpect = contents.size() - offset;
            BOOST_CHECK_EQUAL(decoder->Read(buffer, length, offset), lengthToExpect);
            BOOST_CHECK_EQUAL_COLLECTIONS(contents.begin() + offset, contents.begin() + offset + lengthToExpect,
                                          buffer, buffer + lengthToExpect);
        }
    }
}
