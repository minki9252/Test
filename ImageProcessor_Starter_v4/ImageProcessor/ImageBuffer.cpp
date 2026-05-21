/**
 * @file ImageBuffer.cpp
 */

#include "ImageBuffer.h"

#include <stdexcept>
#include <string>

namespace ip {

namespace {
    /// 비정상적인 메모리 할당을 방지하기 위한 상한.
    constexpr std::size_t MAX_PIXEL_COUNT = 100'000'000ULL; // 1억 픽셀
} // anonymous namespace

ImageBuffer::ImageBuffer(int width, int height)
    : m_width(width), m_height(height)
{
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument(
            "ImageBuffer: width/height must be positive (got " +
            std::to_string(width) + "x" + std::to_string(height) + ")");
    }

    const std::size_t pixelCount =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);

    if (pixelCount > MAX_PIXEL_COUNT) {
        throw std::invalid_argument(
            "ImageBuffer: image too large (max " +
            std::to_string(MAX_PIXEL_COUNT) + " pixels)");
    }

    m_data.assign(pixelCount * CHANNELS, 0);
}

std::uint8_t* ImageBuffer::rowPtr(int y) {
    if (y < 0 || y >= m_height) {
        throw std::out_of_range(
            "ImageBuffer::rowPtr: y=" + std::to_string(y) +
            " out of range [0, " + std::to_string(m_height) + ")");
    }
    return m_data.data() + static_cast<std::size_t>(y) * rowStride();
}

const std::uint8_t* ImageBuffer::rowPtr(int y) const {
    if (y < 0 || y >= m_height) {
        throw std::out_of_range(
            "ImageBuffer::rowPtr: y=" + std::to_string(y) +
            " out of range [0, " + std::to_string(m_height) + ")");
    }
    return m_data.data() + static_cast<std::size_t>(y) * rowStride();
}

} // namespace ip
