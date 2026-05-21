# ImageProcessor 과제 제출

CLI 기반 24-bit BMP 이미지 처리 프로그램입니다. STL만 사용하며 OpenCV 같은 외부 이미지 처리 라이브러리는 사용하지 않았습니다.

## 구현 항목

- BMP 파일 입출력: 제공된 `BmpParser` 코드를 사용해 24-bit BMP를 로드/저장합니다.
- CLI 인자 처리: 제공된 CLI 파서 구조를 확장해 `--input`, `--output`, `--filter`, `--pipeline`, `--threshold`, `--threads`, `--log`를 처리합니다.
- 이미지 처리 결과 저장: 제공된 BMP 저장 흐름을 그대로 사용합니다.
- 이미지 처리 기능
  - `grayscale`: RGB 가중치 기반 흑백 변환
  - `brightness:<delta>`: 밝기 조절
  - `threshold:<value>`: 임계값 기반 이진화
  - `blur`: 3x3 평균 블러 컨볼루션
  - `sharpen`: 3x3 샤픈 컨볼루션
  - `sobel`: 소벨 에지 검출
  - `flip-h`, `flip-v`: 좌우/상하 반전
  - `resize:<width>x<height>`: 최근접 보간 리사이즈

## 고급 요구사항 반영

- 필터 파이프라인 체인
  - `--pipeline "grayscale,blur,threshold:128"` 형태로 여러 필터를 순차 적용합니다.

- 추상 클래스 기반 설계
  - `FilterBase` 추상 클래스를 만들고 각 필터가 이를 상속합니다.
  - `main.cpp`는 `FilterBase` 포인터만 사용하므로 새 필터를 추가해도 실행 흐름을 크게 바꾸지 않아 OCP(Open-Closed Principle)에 맞습니다.

- 멀티스레드 병렬 처리
  - `std::thread`를 사용합니다.
  - 이미지를 64x64 사각형 타일로 나누고, 여러 스레드가 타일을 하나씩 가져가 처리합니다.

- 로그 파일 출력
  - `--log <path>` 옵션으로 로그 파일을 기록합니다.
  - 입력/출력 경로, 이미지 크기, 스레드 수, 필터 이름, 처리 시간, 성공/실패 여부를 기록합니다.

## 설계 및 수정 이유

- 제공 코드가 담당하는 BMP 입출력과 결과 저장 흐름은 그대로 활용했습니다.
- 직접 구현해야 하는 이미지 처리 기능은 `Filters.h`, `Filters.cpp`에 모았습니다.
- 필터 생성은 `createFilter()` 팩토리 함수로 분리했습니다. CLI 문자열(`threshold:128`)을 실제 필터 객체로 바꾸는 책임을 한 곳에 모아 새 필터 추가가 쉽습니다.
- 파이프라인 생성은 `createPipeline()`에서 처리합니다. 콤마로 구분된 문자열을 순서대로 필터 객체 목록으로 바꾸기 때문에 명령어에 적은 순서 그대로 적용됩니다.
- 컨볼루션 계열 필터는 원본 이미지를 복사한 뒤 계산합니다. 원본과 결과를 같은 버퍼에서 동시에 읽고 쓰면 주변 픽셀 계산이 오염될 수 있기 때문입니다.
- 일부 기존 파일은 주석 인코딩이 깨져 선언이 주석처럼 붙어 보이는 문제가 있어 빌드 안정성을 위해 정리했습니다.

## 실행 명령어

```powershell
# Grayscale 변환
.\x64\Release\ImageProcessor.exe --input .\Resource\1_astronaut.bmp --output .\Resource\1_astronaut_grayscale.bmp --filter grayscale
```

```powershell
# Blur 처리 + 임계값 지정
.\x64\Release\ImageProcessor.exe --input .\Resource\2_coffee.bmp --output .\Resource\2_coffee_blur_threshold.bmp --filter blur --threshold 128
```

```powershell
# 필터 파이프라인 체인
.\x64\Release\ImageProcessor.exe --input .\Resource\3_chelsea_cat.bmp --output .\Resource\3_chelsea_cat_pipeline.bmp --pipeline "grayscale,blur,threshold:128" --threads 4 --log .\Resource\pipeline.log
```

```powershell
# 추가 예시: 샤픈, 소벨, 리사이즈
.\x64\Release\ImageProcessor.exe --input .\Resource\4_text_page.bmp --output .\Resource\4_text_page_sharpen.bmp --filter sharpen
.\x64\Release\ImageProcessor.exe --input .\Resource\5_checkerboard.bmp --output .\Resource\5_checkerboard_sobel.bmp --filter sobel
.\x64\Release\ImageProcessor.exe --input .\Resource\1_astronaut.bmp --output .\Resource\1_astronaut_small.bmp --filter resize:256x256
```

## Visual Studio에서 실행 인자 설정

Visual Studio에서 F5로 실행하려면 다음 위치에 명령 인자를 넣어야 합니다.

`프로젝트 우클릭` -> `속성` -> `디버깅` -> `명령 인수`

예시:

```text
--input .\Resource\1_astronaut.bmp --output .\Resource\1_astronaut_grayscale.bmp --filter grayscale
```

## 빌드

Visual Studio 2022에서 `ImageProcessor.sln`을 열고 `Release | x64`로 빌드합니다.

명령줄 빌드 예시:

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" ImageProcessor.sln /p:Configuration=Release /p:Platform=x64 /m
```
