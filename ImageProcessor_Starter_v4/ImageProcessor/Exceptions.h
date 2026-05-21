#pragma once

/**
 * @file Exceptions.h
 * @brief 애플리케이션 전용 예외 클래스 계층.
 *
 * 도메인별로 예외를 분리하여 main()에서 명확하게 분기 처리할 수 있도록 한다.
 * 모든 예외는 std::runtime_error 를 상속한다.
 */

#include <stdexcept>
#include <string>

namespace ip {

/// BMP 파일 입출력 및 파싱 중 발생하는 오류.
class BmpParseError : public std::runtime_error {
public:
    explicit BmpParseError(const std::string& msg)
        : std::runtime_error("[BMP] " + msg) {}
};

/// 필터 생성 또는 적용 중 발생하는 오류.
class FilterError : public std::runtime_error {
public:
    explicit FilterError(const std::string& msg)
        : std::runtime_error("[Filter] " + msg) {}
};

/// 커맨드라인 인자 파싱 중 발생하는 오류.
class ArgumentError : public std::runtime_error {
public:
    explicit ArgumentError(const std::string& msg)
        : std::runtime_error("[Argument] " + msg) {}
};

} // namespace ip
