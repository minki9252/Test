#pragma once

/**
 * @file BmpParser.h
 * @brief BMP 파일 입출력을 담당한다.
 *
 * 지원 포맷:
 *   - 24비트 컬러, 무압축 (BI_RGB)
 *   - bottom-up (양수 height)
 *   - 4-byte row alignment
 *
 * 외부 라이브러리 없이 STL만으로 구현한다.
 */

#include "ImageBuffer.h"

#include <string>

namespace ip {

/**
 * @brief BMP 파일 입출력 전담 클래스.
 *
 * 인스턴스를 가질 이유가 없으므로 정적 메서드만 제공한다.
 */
class BmpParser {
public:
    BmpParser() = delete;  // 인스턴스화 금지

    /**
     * @brief BMP 파일을 읽어 ImageBuffer 로 반환한다.
     * @throws BmpParseError 파일을 열 수 없거나, 포맷이 지원되지 않거나, 손상된 경우.
     */
    static ImageBuffer loadFromFile(const std::string& path);

    /**
     * @brief ImageBuffer 를 24비트 BMP 파일로 저장한다.
     * @throws BmpParseError 파일 쓰기 실패 시.
     */
    static void saveToFile(const std::string& path, const ImageBuffer& image);

private:
    /// BMP 매직 넘버 "BM" (little-endian).
    static constexpr std::uint16_t BMP_MAGIC = 0x4D42;

    /// 픽셀당 비트 수: 본 구현에서는 24비트만 지원.
    static constexpr std::uint16_t SUPPORTED_BPP = 24;

    /// BMP의 행은 4바이트 단위로 정렬된다.
    static constexpr int ROW_ALIGNMENT = 4;

    /// 주어진 row stride 가 4-byte alignment 되도록 패딩한 길이를 반환한다.
    static int paddedStride(int width);
};

} // namespace ip
