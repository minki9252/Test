#include "CommandLineParser.h"
#include "Exceptions.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace ip {

namespace {

std::string nextArg(int argc, char* argv[], int& i, const std::string& flag) {
    if (i + 1 >= argc) {
        throw ArgumentError(flag + ": missing value");
    }
    return argv[++i];
}

int parseInt(const std::string& value, const std::string& flag) {
    // stoi는 "123abc"도 123까지는 변환할 수 있으므로,
    // 사용된 문자 수를 확인해서 CLI 인자 오타를 조기에 잡아내도록 합니다.
    try {
        std::size_t used = 0;
        const int parsed = std::stoi(value, &used);
        if (used != value.size()) {
            throw ArgumentError(flag + ": invalid integer value '" + value + "'");
        }
        return parsed;
    }
    catch (const std::invalid_argument&) {
        throw ArgumentError(flag + ": invalid integer value '" + value + "'");
    }
    catch (const std::out_of_range&) {
        throw ArgumentError(flag + ": integer value is out of range '" + value + "'");
    }
}

} // namespace

ProgramOptions CommandLineParser::parse(int argc, char* argv[]) {
    ProgramOptions options;

    // CLI 옵션은 과제 예시의 긴 이름(--input)과 짧은 이름(-i)을 함께 지원합니다.
    // 사용 편의성은 높이되, 실제 저장은 ProgramOptions 하나로 통일했습니다.
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        if (arg == "--input" || arg == "-i") {
            options.inputPath = nextArg(argc, argv, i, arg);
        }
        else if (arg == "--output" || arg == "-o") {
            options.outputPath = nextArg(argc, argv, i, arg);
        }
        else if (arg == "--filter" || arg == "-f") {
            options.filterName = nextArg(argc, argv, i, arg);
        }
        else if (arg == "--pipeline" || arg == "-p") {
            options.pipeline = nextArg(argc, argv, i, arg);
        }
        else if (arg == "--threshold" || arg == "-t") {
            options.threshold = parseInt(nextArg(argc, argv, i, arg), arg);
        }
        else if (arg == "--threads") {
            options.threads = parseInt(nextArg(argc, argv, i, arg), arg);
        }
        else if (arg == "--log") {
            options.logPath = nextArg(argc, argv, i, arg);
        }
        else if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            std::exit(0);
        }
        else {
            throw ArgumentError("Unknown option: " + arg);
        }
    }

    // 입력/출력 파일은 BMP 처리의 최소 조건이므로 반드시 필요합니다.
    if (options.inputPath.empty()) {
        throw ArgumentError("--input is required");
    }
    if (options.outputPath.empty()) {
        throw ArgumentError("--output is required");
    }
    // 단일 필터와 파이프라인은 실행 방식이 다르기 때문에 둘 중 하나만 허용합니다.
    // 둘 다 허용하면 적용 순서가 애매해져 결과가 예측하기 어려워지기 때문입니다.
    if (options.filterName.empty() && options.pipeline.empty()) {
        throw ArgumentError("either --filter or --pipeline is required");
    }
    if (!options.filterName.empty() && !options.pipeline.empty()) {
        throw ArgumentError("use --filter or --pipeline, not both");
    }
    if (options.threshold > 255) {
        throw ArgumentError("--threshold must be in the range 0..255");
    }
    if (options.threads < 1) {
        throw ArgumentError("--threads must be 1 or greater");
    }

    return options;
}

void CommandLineParser::printUsage(const std::string& exeName) {
    std::cout
        << "Usage:\n"
        << "  " << exeName << " --input <path> --output <path> --filter <name> [options]\n"
        << "  " << exeName << " --input <path> --output <path> --pipeline <chain> [options]\n\n"
        << "Options:\n"
        << "  -i, --input      <path>   Input 24-bit BMP file\n"
        << "  -o, --output     <path>   Output BMP file\n"
        << "  -f, --filter     <name>   Single filter to apply\n"
        << "  -p, --pipeline   <chain>  Comma-separated filter chain\n"
        << "  -t, --threshold  <0..255> Threshold applied after --filter\n"
        << "      --threads    <count>  Worker thread count (default: 1)\n"
        << "      --log        <path>   Write processing log\n"
        << "  -h, --help                Show this message\n\n"
        << "Filters:\n"
        << "  grayscale, brightness:<delta>, threshold:<value>, blur,\n"
        << "  sharpen, sobel, flip-h, flip-v, resize:<width>x<height>\n\n"
        << "Examples:\n"
        << "  " << exeName << " -i input.bmp -o gray.bmp -f grayscale\n"
        << "  " << exeName << " -i input.bmp -o result.bmp -f blur --threshold 128\n"
        << "  " << exeName << " -i input.bmp -o result.bmp -p \"grayscale,blur,threshold:128\" --threads 4 --log run.log\n";
}

} // namespace ip
