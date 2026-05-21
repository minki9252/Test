#include "Filters.h"
#include "Exceptions.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cmath>
#include <functional>
#include <sstream>
#include <string>
#include <thread>
#include <utility>

namespace ip {

namespace {

struct Tile {
    int xBegin;
    int xEnd;
    int yBegin;
    int yEnd;
};

std::uint8_t clampToByte(int value) {
    return static_cast<std::uint8_t>(std::clamp(value, 0, 255));
}

std::string trim(const std::string& text) {
    const auto begin = std::find_if_not(text.begin(), text.end(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    });
    const auto end = std::find_if_not(text.rbegin(), text.rend(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    }).base();
    return (begin < end) ? std::string(begin, end) : std::string();
}

std::string lower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return text;
}

int parseIntStrict(const std::string& value, const std::string& context) {
    try {
        std::size_t used = 0;
        const int parsed = std::stoi(value, &used);
        if (used != value.size()) {
            throw FilterError(context + ": invalid integer '" + value + "'");
        }
        return parsed;
    }
    catch (const std::invalid_argument&) {
        throw FilterError(context + ": invalid integer '" + value + "'");
    }
    catch (const std::out_of_range&) {
        throw FilterError(context + ": integer out of range '" + value + "'");
    }
}

std::vector<Tile> buildTiles(int width, int height) {
    // 고급 요구사항의 "타일 분할 병렬 처리"에 맞춰 이미지를 2D 사각형 블록으로 나눴습니다.
    // 64x64는 작은 이미지에서도 여러 타일이 생기고, 너무 많은 스레드 작업을 만들지 않는 무난한 크기라 생각했습니다.
    constexpr int TILE_SIZE = 64;
    std::vector<Tile> tiles;

    for (int y = 0; y < height; y += TILE_SIZE) {
        for (int x = 0; x < width; x += TILE_SIZE) {
            tiles.push_back(Tile{
                x,
                std::min(x + TILE_SIZE, width),
                y,
                std::min(y + TILE_SIZE, height)
            });
        }
    }

    return tiles;
}

void parallelTiles(int width, int height, int threadCount, const std::function<void(const Tile&)>& worker) {
    if (width <= 0 || height <= 0) {
        return;
    }

    const std::vector<Tile> tiles = buildTiles(width, height);
    const int workers = std::max(1, std::min(threadCount, static_cast<int>(tiles.size())));

    if (workers == 1) {
        for (const Tile& tile : tiles) {
            worker(tile);
        }
        return;
    }

    // 타일 인덱스를 atomic으로 하나씩 가져가게 하면 이미지 크기가 딱 나누어떨어지지 않아도
    // 각 스레드가 비슷한 양의 작업을 처리합니다.
    std::atomic<std::size_t> nextTile{0};
    std::vector<std::thread> threads;
    threads.reserve(static_cast<std::size_t>(workers));

    for (int i = 0; i < workers; ++i) {
        threads.emplace_back([&]() {
            while (true) {
                const std::size_t index = nextTile.fetch_add(1);
                if (index >= tiles.size()) {
                    break;
                }
                worker(tiles[index]);
            }
        });
    }

    for (std::thread& thread : threads) {
        thread.join();
    }
}

class GrayscaleFilter final : public FilterBase {
public:
    std::string name() const override { return "grayscale"; }

    void apply(ImageBuffer& image, int threadCount) const override {
        // BMP 픽셀은 BGR 순서로 저장되어 있으므로 pixel[0]=B, pixel[1]=G, pixel[2]=R
        // 사람 눈은 초록색에 더 민감하므로 단순 평균 대신 표준 가중치 공식을 사용했습니다.
        parallelTiles(image.width(), image.height(), threadCount, [&](const Tile& tile) {
            for (int y = tile.yBegin; y < tile.yEnd; ++y) {
                std::uint8_t* row = image.rowPtr(y);
                for (int x = tile.xBegin; x < tile.xEnd; ++x) {
                    std::uint8_t* pixel = row + x * ImageBuffer::CHANNELS;
                    const int gray = static_cast<int>(0.114 * pixel[0] + 0.587 * pixel[1] + 0.299 * pixel[2]);
                    pixel[0] = pixel[1] = pixel[2] = clampToByte(gray);
                }
            }
        });
    }
};

class BrightnessFilter final : public FilterBase {
public:
    explicit BrightnessFilter(int delta) : m_delta(delta) {}
    std::string name() const override { return "brightness:" + std::to_string(m_delta); }

    void apply(ImageBuffer& image, int threadCount) const override {
        // 밝기 조절은 모든 채널에 같은 delta를 더합니다.
        // 0보다 작거나 255보다 큰 값은 BMP 채널 범위를 벗어나므로 clampToByte로 보정했습니다.
        parallelTiles(image.width(), image.height(), threadCount, [&](const Tile& tile) {
            for (int y = tile.yBegin; y < tile.yEnd; ++y) {
                std::uint8_t* row = image.rowPtr(y);
                for (int x = tile.xBegin; x < tile.xEnd; ++x) {
                    std::uint8_t* pixel = row + x * ImageBuffer::CHANNELS;
                    for (int c = 0; c < ImageBuffer::CHANNELS; ++c) {
                        pixel[c] = clampToByte(static_cast<int>(pixel[c]) + m_delta);
                    }
                }
            }
        });
    }

private:
    int m_delta;
};

class ThresholdFilter final : public FilterBase {
public:
    explicit ThresholdFilter(int threshold) : m_threshold(threshold) {
        if (threshold < 0 || threshold > 255) {
            throw FilterError("threshold must be in the range 0..255");
        }
    }
    std::string name() const override { return "threshold:" + std::to_string(m_threshold); }

    void apply(ImageBuffer& image, int threadCount) const override {
        // threshold는 픽셀 밝기가 기준값 이상이면 흰색, 아니면 검은색으로 바꾸는 이진화 필터
        parallelTiles(image.width(), image.height(), threadCount, [&](const Tile& tile) {
            for (int y = tile.yBegin; y < tile.yEnd; ++y) {
                std::uint8_t* row = image.rowPtr(y);
                for (int x = tile.xBegin; x < tile.xEnd; ++x) {
                    std::uint8_t* pixel = row + x * ImageBuffer::CHANNELS;
                    const int gray = (static_cast<int>(pixel[0]) + pixel[1] + pixel[2]) / 3;
                    const std::uint8_t value = (gray >= m_threshold) ? 255 : 0;
                    pixel[0] = pixel[1] = pixel[2] = value;
                }
            }
        });
    }

private:
    int m_threshold;
};

class ConvolutionFilter final : public FilterBase {
public:
    ConvolutionFilter(std::string filterName, std::array<int, 9> kernel, int divisor, int bias = 0)
        : m_name(std::move(filterName)), m_kernel(kernel), m_divisor(divisor), m_bias(bias) {
        if (divisor == 0) {
            throw FilterError("convolution divisor cannot be zero");
        }
    }

    std::string name() const override { return m_name; }

    void apply(ImageBuffer& image, int threadCount) const override {
        // blur와 sharpen은 같은 3x3 컨볼루션 구조를 공유합니다.
        // 원본 픽셀을 읽으면서 동시에 결과를 쓰면 주변 픽셀 계산이 오염되므로 source 복사본을 기준으로 계산합니다.
        const ImageBuffer source = image;
        parallelTiles(image.width(), image.height(), threadCount, [&](const Tile& tile) {
            for (int y = tile.yBegin; y < tile.yEnd; ++y) {
                std::uint8_t* dstRow = image.rowPtr(y);
                for (int x = tile.xBegin; x < tile.xEnd; ++x) {
                    std::array<int, 3> sum{0, 0, 0};

                    // 가장자리 픽셀은 주변 좌표가 이미지 밖으로 나갈 수 있으므로
                    // 가장 가까운 유효 좌표로 고정(clamp)해 별도 패딩 이미지를 만들지 않았습니다.
                    for (int ky = -1; ky <= 1; ++ky) {
                        const int sy = std::clamp(y + ky, 0, source.height() - 1);
                        const std::uint8_t* srcRow = source.rowPtr(sy);
                        for (int kx = -1; kx <= 1; ++kx) {
                            const int sx = std::clamp(x + kx, 0, source.width() - 1);
                            const int kernelValue = m_kernel[static_cast<std::size_t>((ky + 1) * 3 + (kx + 1))];
                            const std::uint8_t* srcPixel = srcRow + sx * ImageBuffer::CHANNELS;
                            for (int c = 0; c < ImageBuffer::CHANNELS; ++c) {
                                sum[static_cast<std::size_t>(c)] += kernelValue * srcPixel[c];
                            }
                        }
                    }

                    std::uint8_t* dstPixel = dstRow + x * ImageBuffer::CHANNELS;
                    for (int c = 0; c < ImageBuffer::CHANNELS; ++c) {
                        dstPixel[c] = clampToByte(sum[static_cast<std::size_t>(c)] / m_divisor + m_bias);
                    }
                }
            }
        });
    }

private:
    std::string m_name;
    std::array<int, 9> m_kernel;
    int m_divisor;
    int m_bias;
};

class SobelFilter final : public FilterBase {
public:
    std::string name() const override { return "sobel"; }

    void apply(ImageBuffer& image, int threadCount) const override {
        // Sobel은 밝기 변화량을 이용해 윤곽선을 찾는 필터입니다.
        // 색상보다는 밝기 차이가 중요하므로 먼저 grayscale로 변환한 뒤 X/Y 방향 기울기를 계산했습니다.
        GrayscaleFilter grayscale;
        grayscale.apply(image, threadCount);
        const ImageBuffer source = image;
        constexpr std::array<int, 9> gx{-1, 0, 1, -2, 0, 2, -1, 0, 1};
        constexpr std::array<int, 9> gy{-1, -2, -1, 0, 0, 0, 1, 2, 1};

        parallelTiles(image.width(), image.height(), threadCount, [&](const Tile& tile) {
            for (int y = tile.yBegin; y < tile.yEnd; ++y) {
                std::uint8_t* dstRow = image.rowPtr(y);
                for (int x = tile.xBegin; x < tile.xEnd; ++x) {
                    int sumX = 0;
                    int sumY = 0;
                    for (int ky = -1; ky <= 1; ++ky) {
                        const int sy = std::clamp(y + ky, 0, source.height() - 1);
                        const std::uint8_t* srcRow = source.rowPtr(sy);
                        for (int kx = -1; kx <= 1; ++kx) {
                            const int sx = std::clamp(x + kx, 0, source.width() - 1);
                            const int value = srcRow[sx * ImageBuffer::CHANNELS];
                            const std::size_t index = static_cast<std::size_t>((ky + 1) * 3 + (kx + 1));
                            sumX += gx[index] * value;
                            sumY += gy[index] * value;
                        }
                    }

                    const int magnitude = static_cast<int>(std::sqrt(static_cast<double>(sumX * sumX + sumY * sumY)));
                    const std::uint8_t value = clampToByte(magnitude);
                    std::uint8_t* dstPixel = dstRow + x * ImageBuffer::CHANNELS;
                    dstPixel[0] = dstPixel[1] = dstPixel[2] = value;
                }
            }
        });
    }
};

class FlipFilter final : public FilterBase {
public:
    explicit FlipFilter(bool horizontal) : m_horizontal(horizontal) {}
    std::string name() const override { return m_horizontal ? "flip-h" : "flip-v"; }

    void apply(ImageBuffer& image, int threadCount) const override {
        // 반전도 source 복사본을 기준으로 했습니다.
        // 같은 버퍼 안에서 바로 교환하면 아직 읽지 않은 픽셀이 덮어써질 수 있기 때문입니다.
        const ImageBuffer source = image;
        parallelTiles(image.width(), image.height(), threadCount, [&](const Tile& tile) {
            for (int y = tile.yBegin; y < tile.yEnd; ++y) {
                std::uint8_t* dstRow = image.rowPtr(y);
                for (int x = tile.xBegin; x < tile.xEnd; ++x) {
                    const int sx = m_horizontal ? image.width() - 1 - x : x;
                    const int sy = m_horizontal ? y : image.height() - 1 - y;
                    const std::uint8_t* srcPixel = source.rowPtr(sy) + sx * ImageBuffer::CHANNELS;
                    std::uint8_t* dstPixel = dstRow + x * ImageBuffer::CHANNELS;
                    std::copy(srcPixel, srcPixel + ImageBuffer::CHANNELS, dstPixel);
                }
            }
        });
    }

private:
    bool m_horizontal;
};

class ResizeFilter final : public FilterBase {
public:
    ResizeFilter(int width, int height) : m_width(width), m_height(height) {
        if (width <= 0 || height <= 0) {
            throw FilterError("resize dimensions must be positive");
        }
    }

    std::string name() const override {
        return "resize:" + std::to_string(m_width) + "x" + std::to_string(m_height);
    }

    void apply(ImageBuffer& image, int threadCount) const override {
        // 리사이즈는 최근접 보간을 사용했습니다.
        const ImageBuffer source = image;
        ImageBuffer resized(m_width, m_height);
        parallelTiles(resized.width(), resized.height(), threadCount, [&](const Tile& tile) {
            for (int y = tile.yBegin; y < tile.yEnd; ++y) {
                const int sy = std::min(source.height() - 1, y * source.height() / resized.height());
                std::uint8_t* dstRow = resized.rowPtr(y);
                const std::uint8_t* srcRow = source.rowPtr(sy);
                for (int x = tile.xBegin; x < tile.xEnd; ++x) {
                    const int sx = std::min(source.width() - 1, x * source.width() / resized.width());
                    const std::uint8_t* srcPixel = srcRow + sx * ImageBuffer::CHANNELS;
                    std::uint8_t* dstPixel = dstRow + x * ImageBuffer::CHANNELS;
                    std::copy(srcPixel, srcPixel + ImageBuffer::CHANNELS, dstPixel);
                }
            }
        });
        image = std::move(resized);
    }

private:
    int m_width;
    int m_height;
};

std::pair<std::string, std::string> splitNameValue(const std::string& specification) {
    // 필터 옵션은 "이름:값" 형태를 공통 규칙으로 사용합니다.
    // 하나의 파서로 threshold:128, brightness:30, resize:320x240을 모두 처리할 수 있습니다.
    const std::size_t separator = specification.find(':');
    if (separator == std::string::npos) {
        return {lower(trim(specification)), ""};
    }
    return {lower(trim(specification.substr(0, separator))), trim(specification.substr(separator + 1))};
}

} // namespace

FilterPtr createFilter(const std::string& specification) {
    // 문자열을 if/else 한 곳에서만 실제 필터 객체로 바꿉니다.
    // 새 필터를 추가할 때 main.cpp를 수정하지 않고 이 함수에만 분기를 추가하면 됩니다.
    const auto [name, value] = splitNameValue(specification);
    if (name.empty()) {
        throw FilterError("empty filter specification");
    }

    if (name == "grayscale" || name == "gray") {
        return std::make_unique<GrayscaleFilter>();
    }
    if (name == "brightness" || name == "bright") {
        if (value.empty()) {
            throw FilterError("brightness requires a delta, for example brightness:30");
        }
        return std::make_unique<BrightnessFilter>(parseIntStrict(value, "brightness"));
    }
    if (name == "threshold" || name == "binary") {
        if (value.empty()) {
            throw FilterError("threshold requires a value, for example threshold:128");
        }
        return std::make_unique<ThresholdFilter>(parseIntStrict(value, "threshold"));
    }
    if (name == "blur" || name == "boxblur") {
        return std::make_unique<ConvolutionFilter>("blur", std::array<int, 9>{1, 1, 1, 1, 1, 1, 1, 1, 1}, 9);
    }
    if (name == "sharpen") {
        return std::make_unique<ConvolutionFilter>("sharpen", std::array<int, 9>{0, -1, 0, -1, 5, -1, 0, -1, 0}, 1);
    }
    if (name == "sobel" || name == "edge") {
        return std::make_unique<SobelFilter>();
    }
    if (name == "flip-h" || name == "fliph" || name == "flip-horizontal") {
        return std::make_unique<FlipFilter>(true);
    }
    if (name == "flip-v" || name == "flipv" || name == "flip-vertical") {
        return std::make_unique<FlipFilter>(false);
    }
    if (name == "resize") {
        const std::size_t xPos = lower(value).find('x');
        if (xPos == std::string::npos) {
            throw FilterError("resize requires WIDTHxHEIGHT, for example resize:320x240");
        }
        const int width = parseIntStrict(value.substr(0, xPos), "resize width");
        const int height = parseIntStrict(value.substr(xPos + 1), "resize height");
        return std::make_unique<ResizeFilter>(width, height);
    }

    throw FilterError("Unknown filter: " + specification);
}

std::vector<FilterPtr> createPipeline(const std::string& pipelineSpecification) {
    // 파이프라인은 입력 문자열 순서를 그대로 보존합니다.
    // 따라서 "grayscale,blur,threshold:128"은 반드시 grayscale -> blur -> threshold 순서로 적용됩니다.
    std::vector<FilterPtr> filters;
    std::stringstream stream(pipelineSpecification);
    std::string item;

    while (std::getline(stream, item, ',')) {
        item = trim(item);
        if (!item.empty()) {
            filters.push_back(createFilter(item));
        }
    }

    if (filters.empty()) {
        throw FilterError("pipeline must contain at least one filter");
    }
    return filters;
}

} // namespace ip
