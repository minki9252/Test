#include "BmpParser.h"
#include "Exceptions.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <vector>

namespace ip {

namespace {

#pragma pack(push, 1)

struct BitmapFileHeader {
    std::uint16_t bfType;
    std::uint32_t bfSize;
    std::uint16_t bfReserved1;
    std::uint16_t bfReserved2;
    std::uint32_t bfOffBits;
};

struct BitmapInfoHeader {
    std::uint32_t biSize;
    std::int32_t biWidth;
    std::int32_t biHeight;
    std::uint16_t biPlanes;
    std::uint16_t biBitCount;
    std::uint32_t biCompression;
    std::uint32_t biSizeImage;
    std::int32_t biXPelsPerMeter;
    std::int32_t biYPelsPerMeter;
    std::uint32_t biClrUsed;
    std::uint32_t biClrImportant;
};

#pragma pack(pop)

static_assert(sizeof(BitmapFileHeader) == 14, "BitmapFileHeader must be 14 bytes");
static_assert(sizeof(BitmapInfoHeader) == 40, "BitmapInfoHeader must be 40 bytes");

constexpr std::uint32_t BI_RGB = 0;
constexpr std::uint32_t BITMAPINFOHEADER_SIZE = 40;

} // namespace

int BmpParser::paddedStride(int width) {
    const int rawStride = width * ImageBuffer::CHANNELS;
    const int remainder = rawStride % ROW_ALIGNMENT;
    return (remainder == 0) ? rawStride : rawStride + (ROW_ALIGNMENT - remainder);
}

ImageBuffer BmpParser::loadFromFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw BmpParseError("Cannot open file for reading: " + path);
    }

    // BMP는 파일 헤더와 정보 헤더가 정해진 바이너리 순서로 저장합니다.
    // 외부 라이브러리를 쓰지 않고 필요한 헤더만 직접 읽도록 했습니다.
    BitmapFileHeader fileHeader{};
    file.read(reinterpret_cast<char*>(&fileHeader), sizeof(fileHeader));
    if (!file) {
        throw BmpParseError("Failed to read BITMAPFILEHEADER from: " + path);
    }
    if (fileHeader.bfType != BMP_MAGIC) {
        throw BmpParseError("Not a BMP file (magic mismatch): " + path);
    }

    BitmapInfoHeader infoHeader{};
    file.read(reinterpret_cast<char*>(&infoHeader), sizeof(infoHeader));
    if (!file) {
        throw BmpParseError("Failed to read BITMAPINFOHEADER from: " + path);
    }
    // 이 프로그램은 가장 일반적인 BITMAPINFOHEADER(40 bytes), 24-bit, 무압축 BMP만 지원합니다.
    // 지원 범위를 명확히 제한하면 팔레트/압축 BMP 처리 코드를 추가하지 않아도 안정적으로 동작하도록 할 수 있습니다.
    if (infoHeader.biSize != BITMAPINFOHEADER_SIZE) {
        throw BmpParseError("Unsupported BMP info header size: " + std::to_string(infoHeader.biSize));
    }
    if (infoHeader.biPlanes != 1) {
        throw BmpParseError("Invalid BMP plane count: " + std::to_string(infoHeader.biPlanes));
    }
    if (infoHeader.biBitCount != SUPPORTED_BPP) {
        throw BmpParseError("Unsupported bit depth: " + std::to_string(infoHeader.biBitCount) +
                            " (only 24-bit BMP is supported)");
    }
    if (infoHeader.biCompression != BI_RGB) {
        throw BmpParseError("Compressed BMP is not supported (compression=" +
                            std::to_string(infoHeader.biCompression) + ")");
    }
    if (infoHeader.biWidth <= 0) {
        throw BmpParseError("Invalid BMP width: " + std::to_string(infoHeader.biWidth));
    }

    // BMP height가 양수면 bottom-up, 음수면 top-down
    // 내부 ImageBuffer는 항상 top-down 좌표계로 통일해 필터 구현했습니다.
    const bool isTopDown = (infoHeader.biHeight < 0);
    const int width = infoHeader.biWidth;
    const int height = isTopDown ? -infoHeader.biHeight : infoHeader.biHeight;
    if (height <= 0) {
        throw BmpParseError("Invalid BMP height: " + std::to_string(infoHeader.biHeight));
    }

    file.seekg(fileHeader.bfOffBits, std::ios::beg);
    if (!file) {
        throw BmpParseError("Failed to seek to pixel data");
    }

    ImageBuffer image(width, height);
    const int stride = paddedStride(width);
    std::vector<std::uint8_t> rowBuffer(static_cast<std::size_t>(stride));

    // BMP의 각 행은 4바이트 단위로 패딩되어 저장됩니다.
    // 파일에서 읽을 때는 패딩까지 읽고, ImageBuffer에는 실제 픽셀 데이터만 복사합니다.
    for (int row = 0; row < height; ++row) {
        file.read(reinterpret_cast<char*>(rowBuffer.data()), stride);
        if (!file) {
            throw BmpParseError("Unexpected EOF while reading pixel rows (row=" +
                                std::to_string(row) + ")");
        }

        const int dstY = isTopDown ? row : height - 1 - row;
        std::memcpy(image.rowPtr(dstY), rowBuffer.data(), image.rowStride());
    }

    return image;
}

void BmpParser::saveToFile(const std::string& path, const ImageBuffer& image) {
    if (image.empty()) {
        throw BmpParseError("Cannot save an empty image to: " + path);
    }

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        throw BmpParseError("Cannot open file for writing: " + path);
    }

    const int width = image.width();
    const int height = image.height();
    const int stride = paddedStride(width);
    const std::uint32_t pixelDataSize =
        static_cast<std::uint32_t>(stride) * static_cast<std::uint32_t>(height);

    // 저장할 때도 표준 24-bit bottom-up BMP로 기록합니다.
    // 그러면 Windows 기본 뷰어와 대부분의 이미지 도구에서 바로 열 수 있기 때문입니다.
    BitmapFileHeader fileHeader{};
    fileHeader.bfType = BMP_MAGIC;
    fileHeader.bfOffBits = sizeof(BitmapFileHeader) + sizeof(BitmapInfoHeader);
    fileHeader.bfSize = fileHeader.bfOffBits + pixelDataSize;

    BitmapInfoHeader infoHeader{};
    infoHeader.biSize = sizeof(BitmapInfoHeader);
    infoHeader.biWidth = width;
    infoHeader.biHeight = height;
    infoHeader.biPlanes = 1;
    infoHeader.biBitCount = SUPPORTED_BPP;
    infoHeader.biCompression = BI_RGB;
    infoHeader.biSizeImage = pixelDataSize;

    file.write(reinterpret_cast<const char*>(&fileHeader), sizeof(fileHeader));
    file.write(reinterpret_cast<const char*>(&infoHeader), sizeof(infoHeader));

    std::vector<std::uint8_t> rowBuffer(static_cast<std::size_t>(stride), 0);
    for (int row = 0; row < height; ++row) {
        const int srcY = height - 1 - row;
        std::memcpy(rowBuffer.data(), image.rowPtr(srcY), image.rowStride());
        file.write(reinterpret_cast<const char*>(rowBuffer.data()), stride);
    }

    if (!file) {
        throw BmpParseError("Failed to write pixel data to: " + path);
    }
}

} // namespace ip
