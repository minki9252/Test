#pragma once

/**
 * @file ImageBuffer.h
 * @brief 픽셀 데이터를 보관하는 컨테이너 클래스.
 */

#include <cstdint>
#include <vector>

namespace ip {

/**
 * @brief 24비트 BGR 이미지 픽셀 데이터를 RAII로 관리한다.
 *
 * 픽셀 데이터는 row-major / top-down 순서로 저장되며,
 * 행 간 패딩 없이 (width * 3) 바이트로 빽빽하게 배치된다.
 * BMP 파일의 4-byte 패딩은 BmpParser 가 처리한다.
 */
class ImageBuffer {
public:
    /// 픽셀당 채널 수 (B, G, R).
    static constexpr int CHANNELS = 3;

    /// 빈 이미지를 생성한다.
    ImageBuffer() = default;

    /**
     * @param width  이미지 너비 (양수).
     * @param height 이미지 높이 (양수).
     * @throws std::invalid_argument 인자가 0 이하이거나 너무 큰 경우.
     */
    ImageBuffer(int width, int height);

    // 복사 및 이동: 명시적으로 default 지정하여 의도를 분명히 한다.
    ImageBuffer(const ImageBuffer&) = default;
    ImageBuffer(ImageBuffer&&) noexcept = default;
    ImageBuffer& operator=(const ImageBuffer&) = default;
    ImageBuffer& operator=(ImageBuffer&&) noexcept = default;
    ~ImageBuffer() = default;

    int width()  const noexcept { return m_width;  }
    int height() const noexcept { return m_height; }
    int rowStride() const noexcept { return m_width * CHANNELS; }
    std::size_t dataSize() const noexcept { return m_data.size(); }
    bool empty() const noexcept { return m_data.empty(); }

    /// 픽셀 데이터의 시작 포인터.
    std::uint8_t* data() noexcept { return m_data.data(); }
    const std::uint8_t* data() const noexcept { return m_data.data(); }

    /**
     * @brief y번째 행의 시작 포인터를 반환한다.
     * @throws std::out_of_range y가 범위를 벗어난 경우.
     */
    std::uint8_t* rowPtr(int y);
    const std::uint8_t* rowPtr(int y) const;

private:
    int m_width  = 0;
    int m_height = 0;
    std::vector<std::uint8_t> m_data;
};

} // namespace ip
