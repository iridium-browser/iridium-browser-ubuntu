/*
 * Copyright (c) 2008, 2009, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ICOImageDecoder_h
#define ICOImageDecoder_h

#include "platform/image-decoders/FastSharedBufferReader.h"
#include "platform/image-decoders/bmp/BMPImageReader.h"
#include <memory>

namespace blink {

class PNGImageDecoder;

// This class decodes the ICO and CUR image formats.
class PLATFORM_EXPORT ICOImageDecoder final : public ImageDecoder {
  WTF_MAKE_NONCOPYABLE(ICOImageDecoder);

 public:
  ICOImageDecoder(AlphaOption, const ColorBehavior&, size_t maxDecodedBytes);
  ~ICOImageDecoder() override;

  // ImageDecoder:
  String filenameExtension() const override { return "ico"; }
  void onSetData(SegmentReader*) override;
  IntSize size() const override;
  IntSize frameSizeAtIndex(size_t) const override;
  bool setSize(unsigned width, unsigned height) override;
  bool frameIsCompleteAtIndex(size_t) const override;
  // CAUTION: setFailed() deletes all readers and decoders.  Be careful to
  // avoid accessing deleted memory, especially when calling this from
  // inside BMPImageReader!
  bool setFailed() override;
  bool hotSpot(IntPoint&) const override;

 private:
  enum ImageType {
    Unknown,
    BMP,
    PNG,
  };

  enum FileType {
    ICON = 1,
    CURSOR = 2,
  };

  struct IconDirectoryEntry {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
    IntSize m_size;
    uint16_t m_bitCount;
    IntPoint m_hotSpot;
    uint32_t m_imageOffset;
    uint32_t m_byteSize;
  };

  // Returns true if |a| is a preferable icon entry to |b|.
  // Larger sizes, or greater bitdepths at the same size, are preferable.
  static bool compareEntries(const IconDirectoryEntry& a,
                             const IconDirectoryEntry& b);

  // ImageDecoder:
  void decodeSize() override { decode(0, true); }
  size_t decodeFrameCount() override;
  void decode(size_t index) override { decode(index, false); }

  // TODO (scroggo): These functions are identical to functions in
  // BMPImageReader. Share code?
  inline uint8_t readUint8(size_t offset) const {
    return m_fastReader.getOneByte(m_decodedOffset + offset);
  }

  inline uint16_t readUint16(int offset) const {
    char buffer[2];
    const char* data =
        m_fastReader.getConsecutiveData(m_decodedOffset + offset, 2, buffer);
    return BMPImageReader::readUint16(data);
  }

  inline uint32_t readUint32(int offset) const {
    char buffer[4];
    const char* data =
        m_fastReader.getConsecutiveData(m_decodedOffset + offset, 4, buffer);
    return BMPImageReader::readUint32(data);
  }

  // If the desired PNGImageDecoder exists, gives it the appropriate data.
  void setDataForPNGDecoderAtIndex(size_t);

  // Decodes the entry at |index|.  If |onlySize| is true, stops decoding
  // after calculating the image size.  If decoding fails but there is no
  // more data coming, sets the "decode failure" flag.
  void decode(size_t index, bool onlySize);

  // Decodes the directory and directory entries at the beginning of the
  // data, and initializes members.  Returns true if all decoding
  // succeeded.  Once this returns true, all entries' sizes are known.
  bool decodeDirectory();

  // Decodes the specified entry.
  bool decodeAtIndex(size_t);

  // Processes the ICONDIR at the beginning of the data.  Returns true if
  // the directory could be decoded.
  bool processDirectory();

  // Processes the ICONDIRENTRY records after the directory.  Keeps the
  // "best" entry as the one we'll decode.  Returns true if the entries
  // could be decoded.
  bool processDirectoryEntries();

  // Stores the hot-spot for |index| in |hotSpot| and returns true,
  // or returns false if there is none.
  bool hotSpotAtIndex(size_t index, IntPoint& hotSpot) const;

  // Reads and returns a directory entry from the current offset into
  // |data|.
  IconDirectoryEntry readDirectoryEntry();

  // Determines whether the desired entry is a BMP or PNG.  Returns true
  // if the type could be determined.
  ImageType imageTypeAtIndex(size_t);

  FastSharedBufferReader m_fastReader;

  // An index into |m_data| representing how much we've already decoded.
  // Note that this only tracks data _this_ class decodes; once the
  // BMPImageReader takes over this will not be updated further.
  size_t m_decodedOffset;

  // Which type of file (ICO/CUR) this is.
  FileType m_fileType;

  // The headers for the ICO.
  typedef Vector<IconDirectoryEntry> IconDirectoryEntries;
  IconDirectoryEntries m_dirEntries;

  // Count of directory entries is parsed from header before initializing
  // m_dirEntries. m_dirEntries is populated only when full header
  // information including directory entries is available.
  size_t m_dirEntriesCount;

  // The image decoders for the various frames.
  typedef Vector<std::unique_ptr<BMPImageReader>> BMPReaders;
  BMPReaders m_bmpReaders;
  typedef Vector<std::unique_ptr<PNGImageDecoder>> PNGDecoders;
  PNGDecoders m_pngDecoders;

  // Valid only while a BMPImageReader is decoding, this holds the size
  // for the particular entry being decoded.
  IntSize m_frameSize;

  // Used to pass on to an internally created PNG decoder.
  const ColorBehavior m_colorBehavior;
};

}  // namespace blink

#endif
