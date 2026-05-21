#pragma once

#include "ImageBuffer.h"

#include <memory>
#include <string>
#include <vector>

namespace ip {

class FilterBase {
public:
    virtual ~FilterBase() = default;

    // 로그와 콘솔 출력에서 어떤 필터가 실행됐는지 표시하기 위한 이름입니다.
    virtual std::string name() const = 0;

    // 모든 필터가 같은 apply 인터페이스를 갖도록 추상 클래스를 둡니다.
    // main.cpp는 실제 필터 종류를 몰라도 FilterBase 포인터만으로 파이프라인을 순서대로 실행할 수 있습니다.
    virtual void apply(ImageBuffer& image, int threadCount) const = 0;
};

using FilterPtr = std::unique_ptr<FilterBase>;

// CLI 문자열을 실제 필터 객체로 바꾸는 팩토리 함수
// ex) "threshold:128" -> ThresholdFilter(128)
FilterPtr createFilter(const std::string& specification);

// 콤마로 구분된 파이프라인 문자열을 여러 필터 객체로 바꿉니다.
// ex) "grayscale,blur,threshold:128" -> 필터 3개
std::vector<FilterPtr> createPipeline(const std::string& pipelineSpecification);

} // namespace ip
