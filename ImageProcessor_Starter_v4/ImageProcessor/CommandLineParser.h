#pragma once

#include <string>

namespace ip {

struct ProgramOptions {
    // main.cpp가 argv를 직접 해석하지 않도록, CLI에서 받은 값을 한 구조체에 모아 전달합니다.
    // 이렇게 해두면 필터 구현부는 "어떤 옵션이 들어왔는지"만 보고 동작할 수 있어 책임이 분리됩니다.
    std::string inputPath;
    std::string outputPath;
    std::string filterName;
    std::string pipeline;
    std::string logPath;
    // --filter blur --threshold 128 형태를 지원하기 위한 보조 옵션입니다.
    // threshold:<value> 필터도 따로 지원하지만, 과제 예시 명령어와 호환되도록 별도 필드로 뒀습니다.
    int threshold = -1;
    // 병렬 처리는 선택 기능이므로 기본값은 1로 두어 단일 스레드와 동일하게 동작하게 했습니다.
    int threads = 1;
};

class CommandLineParser {
public:
    CommandLineParser() = delete;

    static ProgramOptions parse(int argc, char* argv[]);
    static void printUsage(const std::string& exeName);
};

} // namespace ip
