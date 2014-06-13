#include "decodedfile.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "utils.h"
#include "unistd.h"

#include "metadata.h"


namespace ZFecFS {

int DecodedFile::Read(char *outBuffer, size_t size, off_t offset)
{
}

DecodedFile* DecodedFile::Open(const std::vector<std::string>& encodedFiles,
                               const FecWrapper& fecWrapper)
{
    if (encodedFiles.size() < fecWrapper.GetSharesRequired()
                            || fecWrapper.GetSharesRequired() < 1)
        throw SimpleException("Not enough encoded files.");

    std::vector<File> files;
    for (unsigned int i = 0; i < encodedFiles.size(); ++i)
        files.push_back(File(encodedFiles[i]));

    Constructor constructor(files, fecWrapper);
    return constructor.CreateDecodedFile();
}

size_t DecodedFile::Size() const
{
    return Size(metadata, encodedFileSize);
}

size_t DecodedFile::Size(const std::string& encodedFilePath)
{
    File file(encodedFilePath);

    char buffer[Metadata::size];
    size_t sizeRead = file.Read(buffer, Metadata::size, 0);
    if (sizeRead != Metadata::size)
        throw SimpleException("Size cannot be read from file.");

    return Size(Metadata(buffer), file.Size());
}

size_t DecodedFile::Size(const Metadata &metadata, size_t encodedSize)
{
    const size_t extraSize = Metadata::size + (metadata.excessBytes == 0 ? 0 : 1);
    if (encodedSize < extraSize)
        throw SimpleException("Invalid encoded file.");
    return (encodedSize - extraSize) * metadata.required + metadata.excessBytes;
}

DecodedFile* DecodedFile::Constructor::CreateDecodedFile()
{
    if (encodedFiles.empty())
        throw SimpleException("Too few encoded files.");

    off_t encodedSize = encodedFiles.front().Size();
    Metadata firstMeta = ReadMetadataOfOneFile(0);
    for (unsigned int i = 1; i < encodedFiles.size(); ++i) {
        Metadata meta = ReadMetadataOfOneFile(i);
        if (meta.required != firstMeta.required)
            throw SimpleException("Inconsistent metadata (required).");
        if (meta.excessBytes != firstMeta.excessBytes)
            throw SimpleException("Inconsistent metadata (excessBytes).");
        if (encodedFiles[i].Size() != encodedSize)
            throw SimpleException("Inconsistent file sizes.");
    }
    if (firstMeta.required != fecWrapper.GetSharesRequired())
        throw SimpleException("'required'-value not consistent with filesystem.");
    if (firstMeta.excessBytes >= firstMeta.required || encodedSize < Metadata::size)
        throw SimpleException("Invalid 'excessBytes'-value");

    return new DecodedFile(encodedFiles, firstMeta, encodedSize, fecWrapper);
}

Metadata DecodedFile::Constructor::ReadMetadataOfOneFile(unsigned int index) const
{
    char buffer[Metadata::size];
    size_t sizeRead = encodedFiles[index].Read(buffer, Metadata::size, 0);
    if (sizeRead != Metadata::size)
        throw SimpleException("Unable to read metadata.");

    return Metadata(buffer);
}

} // namespace ZFecFS
