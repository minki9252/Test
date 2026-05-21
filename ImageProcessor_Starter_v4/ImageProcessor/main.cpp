#include "BmpParser.h"
#include "CommandLineParser.h"
#include "Exceptions.h"
#include "Filters.h"
#include "ImageBuffer.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

class RunLogger {
public:
    explicit RunLogger(const std::string& path) {
        // 경로가 비어 있으면 아무 파일도 열지 않아 기본 실행 흐름에 영향을 주지 않습니다.
        if (!path.empty()) {
            m_file.open(path, std::ios::out | std::ios::trunc);
            if (!m_file) {
                throw ip::ArgumentError("Cannot open log file for writing: " + path);
            }
        }
    }

    void write(const std::string& message) {
        if (m_file) {
            m_file << message << '\n';
        }
    }

private:
    std::ofstream m_file;
};

std::vector<ip::FilterPtr> buildFilters(const ip::ProgramOptions& options) {
    std::vector<ip::FilterPtr> filters;

    // --pipeline이 있으면 "grayscale,blur,threshold:128" 같은 문자열을 여러 필터 객체로 분해합니다.
    // 고급 요구사항의 필터 체인을 main에 직접 하드코딩하지 않고 Filters.cpp에 위임했습니다.
    if (!options.pipeline.empty()) {
        filters = ip::createPipeline(options.pipeline);
    }
    else {
        // --filter는 단일 필터 실행용입니다.
        // 과제 예시처럼 "--filter blur --threshold 128"을 쓰면 blur 뒤에 threshold를 추가 적용했습니다.
        filters.push_back(ip::createFilter(options.filterName));
        if (options.threshold >= 0) {
            filters.push_back(ip::createFilter("threshold:" + std::to_string(options.threshold)));
        }
    }

    return filters;
}

} // namespace

int main(int argc, char* argv[]) {
    try {
        // main은 전체 실행 순서만 담당합니다.
        // 인자 해석, BMP 입출력, 필터 생성/적용을 각각 별도 클래스로 나눠 유지보수성을 높였습니다.
        const ip::ProgramOptions options = ip::CommandLineParser::parse(argc, argv);
        RunLogger logger(options.logPath);

        const auto totalStart = std::chrono::steady_clock::now();
        ip::ImageBuffer image = ip::BmpParser::loadFromFile(options.inputPath);

        std::cout << "Loaded: " << image.width() << " x " << image.height() << "\n";
        logger.write("input=" + options.inputPath);
        logger.write("output=" + options.outputPath);
        logger.write("size=" + std::to_string(image.width()) + "x" + std::to_string(image.height()));
        logger.write("threads=" + std::to_string(options.threads));

        std::vector<ip::FilterPtr> filters = buildFilters(options);
        for (const ip::FilterPtr& filter : filters) {
            // 각 필터별 소요 시간과 성공/실패 여부를 로그 파일에 남깁니다.
            // 실패 시에도 어떤 필터에서 문제가 발생했는지 확인할 수 있습니다.
            const auto filterStart = std::chrono::steady_clock::now();
            try {
                filter->apply(image, options.threads);
                const auto filterEnd = std::chrono::steady_clock::now();
                const auto elapsedMs =
                    std::chrono::duration_cast<std::chrono::milliseconds>(filterEnd - filterStart).count();

                std::cout << "Applied: " << filter->name() << " (" << elapsedMs << " ms)\n";
                logger.write("filter=" + filter->name() +
                             ", status=success, elapsed_ms=" + std::to_string(elapsedMs));
            }
            catch (const std::exception& e) {
                const auto filterEnd = std::chrono::steady_clock::now();
                const auto elapsedMs =
                    std::chrono::duration_cast<std::chrono::milliseconds>(filterEnd - filterStart).count();

                logger.write("filter=" + filter->name() +
                             ", status=failed, elapsed_ms=" + std::to_string(elapsedMs) +
                             ", error=" + e.what());
                throw;
            }
        }

        ip::BmpParser::saveToFile(options.outputPath, image);

        const auto totalEnd = std::chrono::steady_clock::now();
        const auto totalMs =
            std::chrono::duration_cast<std::chrono::milliseconds>(totalEnd - totalStart).count();
        logger.write("total_elapsed_ms=" + std::to_string(totalMs));
        logger.write("result=success");

        std::cout << "Saved:  " << options.outputPath << "\n";
        return 0;
    }
    catch (const ip::ArgumentError& e) {
        std::cerr << e.what() << "\n\n";
        ip::CommandLineParser::printUsage(argc > 0 ? argv[0] : "ImageProcessor");
        return 4;
    }
    catch (const ip::BmpParseError& e) {
        std::cerr << e.what() << std::endl;
        return 2;
    }
    catch (const ip::FilterError& e) {
        std::cerr << e.what() << std::endl;
        return 3;
    }
    catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << std::endl;
        return 1;
    }
}
