#include "Renderer/VideoEncoding.h"

#include "Core/Platform.h"

#if AXIOM_PLATFORM_MACOS

#include <CoreFoundation/CoreFoundation.h>
#include <CoreMedia/CoreMedia.h>
#include <CoreVideo/CoreVideo.h>
#include <VideoToolbox/VideoToolbox.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <vector>

namespace Axiom {
namespace {
class MacOSVideoToolboxH264Encoder final : public IVideoEncoder {
public:
  MacOSVideoToolboxH264Encoder() = default;
  ~MacOSVideoToolboxH264Encoder() override { ResetSession(); }

  bool EncodeFrame(const VideoEncoderInputFrame &Frame) override {
    if (Frame.Format != ViewportFrameFormat::R8G8B8A8Unorm ||
        Frame.Width == 0 || Frame.Height == 0 || Frame.Pixels.empty()) {
      return false;
    }

    std::scoped_lock Lock(m_Mutex);
    if (!EnsureSession(Frame.Width, Frame.Height)) {
      return false;
    }

    CVPixelBufferRef PixelBuffer = CreatePixelBuffer(Frame.Width, Frame.Height);
    if (PixelBuffer == nullptr) {
      return false;
    }

    const bool FilledBuffer = FillPixelBuffer(PixelBuffer, Frame);
    if (!FilledBuffer) {
      CVPixelBufferRelease(PixelBuffer);
      return false;
    }

    uint64_t *FrameIndex = new uint64_t(Frame.FrameIndex);
    const CMTime PresentationTime =
        CMTimeMake(static_cast<int64_t>(Frame.FrameIndex), 60);
    const OSStatus EncodeStatus =
        VTCompressionSessionEncodeFrame(
            m_Session, PixelBuffer, PresentationTime, kCMTimeInvalid,
            nullptr, FrameIndex, nullptr);
    CVPixelBufferRelease(PixelBuffer);
    if (EncodeStatus != noErr) {
      delete FrameIndex;
      return false;
    }

    const OSStatus CompleteStatus =
        VTCompressionSessionCompleteFrames(m_Session, kCMTimeInvalid);
    if (CompleteStatus != noErr) {
      return false;
    }

    return true;
  }

  void SetOutput(IEncodedVideoPacketOutput *Output) override {
    std::scoped_lock Lock(m_Mutex);
    m_Output = Output;
  }

private:
  static void CompressionOutputCallback(
      void *OutputCallbackRefCon, void *SourceFrameRefCon, OSStatus Status,
      VTEncodeInfoFlags InfoFlags, CMSampleBufferRef SampleBuffer) {
    auto *Encoder =
        static_cast<MacOSVideoToolboxH264Encoder *>(OutputCallbackRefCon);
    uint64_t FrameIndex = 0;
    if (SourceFrameRefCon != nullptr) {
      auto *StoredFrameIndex = static_cast<uint64_t *>(SourceFrameRefCon);
      FrameIndex = *StoredFrameIndex;
      delete StoredFrameIndex;
    }

    if (Encoder == nullptr) {
      return;
    }
    Encoder->HandleEncodedFrame(FrameIndex, Status, InfoFlags, SampleBuffer);
  }

  bool EnsureSession(uint32_t Width, uint32_t Height) {
    if (m_Session != nullptr && m_Width == Width && m_Height == Height) {
      return true;
    }

    ResetSession();
    m_Width = Width;
    m_Height = Height;

    const int32_t PixelFormat = static_cast<int32_t>(kCVPixelFormatType_32BGRA);
    const int32_t WidthValue = static_cast<int32_t>(Width);
    const int32_t HeightValue = static_cast<int32_t>(Height);
    CFNumberRef WidthNumber =
        CFNumberCreate(nullptr, kCFNumberSInt32Type, &WidthValue);
    CFNumberRef HeightNumber =
        CFNumberCreate(nullptr, kCFNumberSInt32Type, &HeightValue);
    CFNumberRef PixelFormatNumber =
        CFNumberCreate(nullptr, kCFNumberSInt32Type, &PixelFormat);
    CFDictionaryRef IOSurfaceProperties = CFDictionaryCreate(
        nullptr, nullptr, nullptr, 0, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    const void *AttributeKeys[] = {
        kCVPixelBufferPixelFormatTypeKey,
        kCVPixelBufferWidthKey,
        kCVPixelBufferHeightKey,
        kCVPixelBufferIOSurfacePropertiesKey,
    };
    const void *AttributeValues[] = {
        PixelFormatNumber,
        WidthNumber,
        HeightNumber,
        IOSurfaceProperties,
    };
    CFDictionaryRef SourceAttributes = CFDictionaryCreate(
        nullptr, AttributeKeys, AttributeValues, 4,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    const OSStatus SessionStatus =
        VTCompressionSessionCreate(nullptr, static_cast<int32_t>(Width),
                                   static_cast<int32_t>(Height),
                                   kCMVideoCodecType_H264, nullptr,
                                   SourceAttributes,
                                   nullptr, &CompressionOutputCallback, this,
                                   &m_Session);
    if (SourceAttributes != nullptr) {
      CFRelease(SourceAttributes);
    }
    if (IOSurfaceProperties != nullptr) {
      CFRelease(IOSurfaceProperties);
    }
    if (PixelFormatNumber != nullptr) {
      CFRelease(PixelFormatNumber);
    }
    if (HeightNumber != nullptr) {
      CFRelease(HeightNumber);
    }
    if (WidthNumber != nullptr) {
      CFRelease(WidthNumber);
    }
    if (SessionStatus != noErr || m_Session == nullptr) {
      ResetSession();
      return false;
    }

    SetSessionProperty(kVTCompressionPropertyKey_RealTime, kCFBooleanTrue);
    SetSessionProperty(kVTCompressionPropertyKey_AllowFrameReordering,
                       kCFBooleanFalse);
    SetSessionProperty(kVTCompressionPropertyKey_ProfileLevel,
                       kVTProfileLevel_H264_Baseline_AutoLevel);

    const int32_t AverageBitRate =
        static_cast<int32_t>(std::max<uint32_t>(Width * Height * 4u, 250000u));
    CFNumberRef BitRate = CFNumberCreate(nullptr, kCFNumberSInt32Type,
                                         &AverageBitRate);
    if (BitRate != nullptr) {
      SetSessionProperty(kVTCompressionPropertyKey_AverageBitRate, BitRate);
      CFRelease(BitRate);
    }

    const int32_t MaxKeyFrameInterval = 30;
    CFNumberRef KeyFrameInterval =
        CFNumberCreate(nullptr, kCFNumberSInt32Type, &MaxKeyFrameInterval);
    if (KeyFrameInterval != nullptr) {
      SetSessionProperty(kVTCompressionPropertyKey_MaxKeyFrameInterval,
                         KeyFrameInterval);
      CFRelease(KeyFrameInterval);
    }

    const int32_t ExpectedFrameRate = 60;
    CFNumberRef FrameRate =
        CFNumberCreate(nullptr, kCFNumberSInt32Type, &ExpectedFrameRate);
    if (FrameRate != nullptr) {
      SetSessionProperty(kVTCompressionPropertyKey_ExpectedFrameRate, FrameRate);
      CFRelease(FrameRate);
    }

    if (VTCompressionSessionPrepareToEncodeFrames(m_Session) != noErr) {
      ResetSession();
      return false;
    }

    return true;
  }

  void ResetSession() {
    if (m_Session != nullptr) {
      VTCompressionSessionInvalidate(m_Session);
      CFRelease(m_Session);
      m_Session = nullptr;
    }
    m_Width = 0;
    m_Height = 0;
  }

  void SetSessionProperty(CFStringRef Key, CFTypeRef Value) {
    if (m_Session == nullptr || Key == nullptr || Value == nullptr) {
      return;
    }
    VTSessionSetProperty(m_Session, Key, Value);
  }

  static CVPixelBufferRef CreatePixelBuffer(uint32_t Width, uint32_t Height) {
    const int32_t WidthValue = static_cast<int32_t>(Width);
    const int32_t HeightValue = static_cast<int32_t>(Height);
    const int32_t PixelFormat = static_cast<int32_t>(kCVPixelFormatType_32BGRA);
    CFNumberRef WidthNumber =
        CFNumberCreate(nullptr, kCFNumberSInt32Type, &WidthValue);
    CFNumberRef HeightNumber =
        CFNumberCreate(nullptr, kCFNumberSInt32Type, &HeightValue);
    CFNumberRef PixelFormatNumber =
        CFNumberCreate(nullptr, kCFNumberSInt32Type, &PixelFormat);
    CFDictionaryRef IOSurfaceProperties = CFDictionaryCreate(
        nullptr, nullptr, nullptr, 0, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    const void *AttributeKeys[] = {
        kCVPixelBufferPixelFormatTypeKey,
        kCVPixelBufferWidthKey,
        kCVPixelBufferHeightKey,
        kCVPixelBufferIOSurfacePropertiesKey,
    };
    const void *AttributeValues[] = {
        PixelFormatNumber,
        WidthNumber,
        HeightNumber,
        IOSurfaceProperties,
    };
    CFDictionaryRef Attributes = CFDictionaryCreate(
        nullptr, AttributeKeys, AttributeValues, 4,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    CVPixelBufferRef PixelBuffer = nullptr;
    const CVReturn Result = CVPixelBufferCreate(
        nullptr, Width, Height, kCVPixelFormatType_32BGRA, Attributes,
        &PixelBuffer);

    if (Attributes != nullptr) {
      CFRelease(Attributes);
    }
    if (IOSurfaceProperties != nullptr) {
      CFRelease(IOSurfaceProperties);
    }
    if (PixelFormatNumber != nullptr) {
      CFRelease(PixelFormatNumber);
    }
    if (HeightNumber != nullptr) {
      CFRelease(HeightNumber);
    }
    if (WidthNumber != nullptr) {
      CFRelease(WidthNumber);
    }

    if (Result != kCVReturnSuccess) {
      return nullptr;
    }
    return PixelBuffer;
  }

  static bool FillPixelBuffer(CVPixelBufferRef PixelBuffer,
                              const VideoEncoderInputFrame &Frame) {
    if (PixelBuffer == nullptr) {
      return false;
    }

    if (CVPixelBufferLockBaseAddress(PixelBuffer, 0) != kCVReturnSuccess) {
      return false;
    }

    auto Unlock = [&]() { CVPixelBufferUnlockBaseAddress(PixelBuffer, 0); };
    void *BaseAddress = CVPixelBufferGetBaseAddress(PixelBuffer);
    if (BaseAddress == nullptr) {
      Unlock();
      return false;
    }

    const size_t BytesPerRow = CVPixelBufferGetBytesPerRow(PixelBuffer);
    const auto *Source = reinterpret_cast<const uint8_t *>(Frame.Pixels.data());
    auto *Destination = static_cast<uint8_t *>(BaseAddress);

    for (uint32_t Y = 0; Y < Frame.Height; ++Y) {
      auto *DestinationRow = Destination + (static_cast<size_t>(Y) * BytesPerRow);
      const auto *SourceRow =
          Source + (static_cast<size_t>(Y) * static_cast<size_t>(Frame.Width) *
                    4u);
      for (uint32_t X = 0; X < Frame.Width; ++X) {
        const size_t PixelOffset = static_cast<size_t>(X) * 4u;
        DestinationRow[PixelOffset + 0u] = SourceRow[PixelOffset + 2u];
        DestinationRow[PixelOffset + 1u] = SourceRow[PixelOffset + 1u];
        DestinationRow[PixelOffset + 2u] = SourceRow[PixelOffset + 0u];
        DestinationRow[PixelOffset + 3u] = SourceRow[PixelOffset + 3u];
      }
    }

    Unlock();
    return true;
  }

  void HandleEncodedFrame(uint64_t FrameIndex, OSStatus Status,
                          VTEncodeInfoFlags InfoFlags,
                          CMSampleBufferRef SampleBuffer) {
    (void)InfoFlags;
    if (Status != noErr || SampleBuffer == nullptr || m_Output == nullptr) {
      return;
    }

    if (!CMSampleBufferDataIsReady(SampleBuffer)) {
      return;
    }

    EncodedVideoPacket Packet{};
    Packet.Codec = EncodedVideoCodec::H264;
    Packet.FrameIndex = FrameIndex;
    Packet.Width = m_Width;
    Packet.Height = m_Height;
    Packet.IsKeyframe = IsKeyframeSample(SampleBuffer);

    if (Packet.IsKeyframe) {
      AppendParameterSets(SampleBuffer, Packet.Bytes);
    }
    if (!AppendSampleNalUnits(SampleBuffer, Packet.Bytes)) {
      return;
    }

    m_Output->OnEncodedVideoPacket(Packet);
  }

  static bool IsKeyframeSample(CMSampleBufferRef SampleBuffer) {
    CFArrayRef Attachments =
        CMSampleBufferGetSampleAttachmentsArray(SampleBuffer, false);
    if (Attachments == nullptr || CFArrayGetCount(Attachments) == 0) {
      return true;
    }

    auto *Attachment = static_cast<CFDictionaryRef>(
        const_cast<void *>(CFArrayGetValueAtIndex(Attachments, 0)));
    if (Attachment == nullptr) {
      return true;
    }

    return !CFDictionaryContainsKey(
        Attachment, kCMSampleAttachmentKey_NotSync);
  }

  static void AppendStartCode(std::vector<std::byte> &Bytes) {
    Bytes.push_back(std::byte{0x00});
    Bytes.push_back(std::byte{0x00});
    Bytes.push_back(std::byte{0x00});
    Bytes.push_back(std::byte{0x01});
  }

  static void AppendNalUnit(std::vector<std::byte> &Bytes, const uint8_t *Data,
                            size_t Size) {
    if (Data == nullptr || Size == 0) {
      return;
    }
    AppendStartCode(Bytes);
    Bytes.insert(Bytes.end(), reinterpret_cast<const std::byte *>(Data),
                 reinterpret_cast<const std::byte *>(Data + Size));
  }

  static void AppendParameterSets(CMSampleBufferRef SampleBuffer,
                                  std::vector<std::byte> &Bytes) {
    CMFormatDescriptionRef FormatDescription =
        CMSampleBufferGetFormatDescription(SampleBuffer);
    if (FormatDescription == nullptr) {
      return;
    }

    size_t ParameterSetCount = 0;
    if (CMVideoFormatDescriptionGetH264ParameterSetAtIndex(
            FormatDescription, 0, nullptr, nullptr, &ParameterSetCount,
            nullptr) != noErr) {
      return;
    }

    for (size_t Index = 0; Index < ParameterSetCount; ++Index) {
      const uint8_t *ParameterSet = nullptr;
      size_t ParameterSetSize = 0;
      if (CMVideoFormatDescriptionGetH264ParameterSetAtIndex(
              FormatDescription, Index, &ParameterSet, &ParameterSetSize,
              nullptr, nullptr) == noErr) {
        AppendNalUnit(Bytes, ParameterSet, ParameterSetSize);
      }
    }
  }

  static bool AppendSampleNalUnits(CMSampleBufferRef SampleBuffer,
                                   std::vector<std::byte> &Bytes) {
    CMBlockBufferRef BlockBuffer = CMSampleBufferGetDataBuffer(SampleBuffer);
    if (BlockBuffer == nullptr) {
      return false;
    }

    char *DataPointer = nullptr;
    size_t TotalLength = 0;
    if (CMBlockBufferGetDataPointer(BlockBuffer, 0, nullptr, &TotalLength,
                                    &DataPointer) != kCMBlockBufferNoErr ||
        DataPointer == nullptr) {
      return false;
    }

    size_t Offset = 0;
    while (Offset + 4u <= TotalLength) {
      uint32_t NalUnitLength = 0;
      std::memcpy(&NalUnitLength, DataPointer + Offset, sizeof(NalUnitLength));
      NalUnitLength = CFSwapInt32BigToHost(NalUnitLength);
      Offset += 4u;
      if (Offset + NalUnitLength > TotalLength) {
        return false;
      }

      AppendNalUnit(Bytes,
                    reinterpret_cast<const uint8_t *>(DataPointer + Offset),
                    NalUnitLength);
      Offset += NalUnitLength;
    }

    return !Bytes.empty();
  }

private:
  std::mutex m_Mutex;
  IEncodedVideoPacketOutput *m_Output{nullptr};
  VTCompressionSessionRef m_Session{nullptr};
  uint32_t m_Width{0};
  uint32_t m_Height{0};
};
} // namespace

std::unique_ptr<IVideoEncoder> CreateMacOSVideoToolboxH264Encoder() {
  return std::make_unique<MacOSVideoToolboxH264Encoder>();
}
} // namespace Axiom

#endif
