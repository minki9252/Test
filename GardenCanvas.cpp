#include "GardenCanvas.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>

BEGIN_MESSAGE_MAP(CGardenCanvas, CWnd)
    ON_WM_PAINT()
    ON_WM_LBUTTONDOWN()
    ON_WM_MOUSEMOVE()
    ON_WM_LBUTTONUP()
END_MESSAGE_MAP()

void CGardenCanvas::SetPointRadius(int radius)
{
    // 요구사항: 클릭 지점 원의 반지름은 사용자 입력받습니다.
    // 잘못된 0 이하 입력이 들어와도 화면에 최소 1픽셀 원은 보이도록 방어했습니다.
    m_pointRadius = std::max(1, radius);
    Invalidate(FALSE);
}

void CGardenCanvas::SetGardenBorderWidth(int width)
{
    // 요구사항: 세 클릭 지점을 지나가는 정원의 내부는 채워지지 않아야 하며, 가장자리 두께는 사용자로부터 입력 받습니다.  
    m_gardenBorderWidth = std::max(1, width);
    Invalidate(FALSE);
}

void CGardenCanvas::ResetAll()
{
    // 요구사항: [초기화] 버튼을 누르면 그려졌던 모든 내용들을 삭제하고 처음부터 입력 받을 수 있는 상태가 되어야 합니다.  
    m_points.clear();
    m_draggingIndex = -1;
    m_isDragging = false;
    ReleaseCapture();
    Invalidate(FALSE);
    UpdateWindow();
    NotifyChanged();
}

void CGardenCanvas::RandomMovePoints()
{
    // 요구사항: 정원이 그려진 상태에서 [랜덤 이동] 버튼을 누르면 3개의 클릭 지점 원 모두를 랜덤한 위치로 이동시킵니다.  
    if (m_points.size() != 3)
    {
        return;
    }

    CRect rect;
    GetClientRect(&rect);

    const int margin = std::max(20, m_pointRadius + m_gardenBorderWidth + 4);
    const int minX = margin;
    const int minY = margin;
    const int maxX = std::max(minX, rect.Width() - margin);
    const int maxY = std::max(minY, rect.Height() - margin);

    for (CPoint& point : m_points)
    {
        point.x = minX + (std::rand() % std::max(1, maxX - minX + 1));
        point.y = minY + (std::rand() % std::max(1, maxY - minY + 1));
    }

    Invalidate(FALSE);
    UpdateWindow();
    NotifyChanged();
}

int CGardenCanvas::GetPointCount() const
{
    return static_cast<int>(m_points.size());
}

void CGardenCanvas::OnPaint()
{
    CPaintDC dc(this);
    DrawScene(dc);
}

void CGardenCanvas::OnLButtonDown(UINT flags, CPoint point)
{
    CWnd::OnLButtonDown(flags, point);

    int hitIndex = -1;
    if (FindClickedPoint(point, hitIndex))
    {
        // 요구사항: 세 클릭 지점을 지나가는 정원의 내부는 채워지지 않아야 하며, 가장자리 두께는 사용자로부터 입력 받습니다.  
        m_draggingIndex = hitIndex;
        m_isDragging = true;
        SetCapture();
        return;
    }

    if (m_points.size() < 3)
    {
        // 요구사항: 세 번째 클릭 이후에 클릭 지점 3개를 모두 지나가는 정원 1개를 그립니다.  
        // 여기서는 점만 저장하고, OnPaint에서 점이 3개가 되는 순간 외접원을 계산해 그립니다.
        m_points.push_back(ClampToClient(point));
        Invalidate(FALSE);
        UpdateWindow();
        NotifyChanged();
        return;
    }

    // 요구사항: 네 번째 클릭부터는 클릭 지점 원을 그리지 않습니다.  
    // 따라서 점이 이미 3개면 아무 것도 추가하지 않습니다.
}

void CGardenCanvas::OnMouseMove(UINT flags, CPoint point)
{
    CWnd::OnMouseMove(flags, point);

    if (!m_isDragging || m_draggingIndex < 0 || m_draggingIndex >= static_cast<int>(m_points.size()))
    {
        return;
    }

    // 요구사항: 이 때, 마우스 커서 좌표가 바뀌는 동안, 즉 마우스 드래그 상태가 끝날 때까지 정원이 계속해서 이동하며 그려져야 합니다.  
    m_points[m_draggingIndex] = ClampToClient(point);
    Invalidate(FALSE);
    UpdateWindow();
    NotifyChanged();
}

void CGardenCanvas::OnLButtonUp(UINT flags, CPoint point)
{
    CWnd::OnLButtonUp(flags, point);

    if (!m_isDragging)
    {
        return;
    }

    m_points[m_draggingIndex] = ClampToClient(point);
    m_draggingIndex = -1;
    m_isDragging = false;
    ReleaseCapture();
    Invalidate(FALSE);
    UpdateWindow();
    NotifyChanged();
}

bool CGardenCanvas::FindClickedPoint(CPoint mousePoint, int& index) const
{
    const int hitRadius = std::max(12, m_pointRadius + 5);
    const int hitRadiusSquared = hitRadius * hitRadius;

    for (int i = 0; i < static_cast<int>(m_points.size()); ++i)
    {
        const int dx = mousePoint.x - m_points[i].x;
        const int dy = mousePoint.y - m_points[i].y;
        if (dx * dx + dy * dy <= hitRadiusSquared)
        {
            index = i;
            return true;
        }
    }
    return false;
}

bool CGardenCanvas::CalculateGardenCircle(Circle& circle) const
{
    if (m_points.size() != 3)
    {
        return false;
    }

    // 요구사항: 세 번째 클릭 이후에 클릭 지점 3개를 모두 지나가는 정원 1개를 그립니다.  
    // 세 점을 지나는 원은 삼각형의 외접원이며, 아래 공식은 세 점 좌표로
    // 외접원의 중심과 반지름을 직접 계산합니다.
    const double x1 = m_points[0].x;
    const double y1 = m_points[0].y;
    const double x2 = m_points[1].x;
    const double y2 = m_points[1].y;
    const double x3 = m_points[2].x;
    const double y3 = m_points[2].y;

    const double d = 2.0 * (x1 * (y2 - y3) + x2 * (y3 - y1) + x3 * (y1 - y2));
    if (std::abs(d) < 0.0001)
    {
        // 세 점이 거의 한 직선이면 유한한 원을 만들 수 없으므로 점만 보여줍니다.
        return false;
    }

    const double ux =
        ((x1 * x1 + y1 * y1) * (y2 - y3) +
         (x2 * x2 + y2 * y2) * (y3 - y1) +
         (x3 * x3 + y3 * y3) * (y1 - y2)) /
        d;
    const double uy =
        ((x1 * x1 + y1 * y1) * (x3 - x2) +
         (x2 * x2 + y2 * y2) * (x1 - x3) +
         (x3 * x3 + y3 * y3) * (x2 - x1)) /
        d;

    circle.center = CPoint(static_cast<int>(std::round(ux)), static_cast<int>(std::round(uy)));
    circle.radius = static_cast<int>(std::round(std::hypot(ux - x1, uy - y1)));
    return circle.radius > 0;
}

CPoint CGardenCanvas::ClampToClient(CPoint point) const
{
    CRect rect;
    GetClientRect(&rect);

    point.x = std::clamp(point.x, rect.left, rect.right - 1);
    point.y = std::clamp(point.y, rect.top, rect.bottom - 1);
    return point;
}

void CGardenCanvas::DrawScene(CDC& dc)
{
    CRect rect;
    GetClientRect(&rect);
    m_bufferWidth = rect.Width();
    m_bufferHeight = rect.Height();
    if (m_bufferWidth <= 0 || m_bufferHeight <= 0)
    {
        return;
    }

    m_pixels.assign(static_cast<size_t>(m_bufferWidth) * static_cast<size_t>(m_bufferHeight), RGB(255, 255, 255));

    Circle garden;
    if (CalculateGardenCircle(garden))
    {
        // 요구사항: 정원의 내부는 채우지 않습니다.
        // drawEllipse/Ellipse 계열 API를 쓰지 않고, 원의 반지름에 가까운 픽셀만 골라 테두리로 찍습니다.
        DrawCircleBorder(garden.center, garden.radius, m_gardenBorderWidth, RGB(0, 0, 0));
    }

    for (const CPoint& point : m_points)
    {
        // 요구사항: 클릭 지점 원을 그릴 때의 반지름 크기는 사용자로부터 입력 받습니다.  
        // FillPolygon, Ellipse, GDI+ 없이 픽셀 거리 검사로 직접 채운 원을 만듭니다.
        DrawFilledPoint(point, m_pointRadius, RGB(0, 0, 0));
    }

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = m_bufferWidth;
    bmi.bmiHeader.biHeight = -m_bufferHeight;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    // 화면으로 복사하는 마지막 단계만 Win32 DIB 전송을 사용합니다.
    // 실제 원/점 모양 생성은 위의 픽셀 버퍼 루프에서 직접 수행합니다.
    SetDIBitsToDevice(
        dc.GetSafeHdc(),
        0,
        0,
        m_bufferWidth,
        m_bufferHeight,
        0,
        0,
        0,
        m_bufferHeight,
        m_pixels.data(),
        &bmi,
        DIB_RGB_COLORS);
}

void CGardenCanvas::ClearBuffer(COLORREF color)
{
    std::fill(m_pixels.begin(), m_pixels.end(), color);
}

void CGardenCanvas::PutPixelSafe(int x, int y, COLORREF color)
{
    if (x < 0 || y < 0 || x >= m_bufferWidth || y >= m_bufferHeight)
    {
        // 요구사항 : 정원이 그리기 영역을 벗어나는 경우는 아래와 같이 전체 원이 표시되지 않아도 됩니다.  
        // 즉, 영역 밖 픽셀은 무시해서 원 전체가 표시되지 않아도 되게 처리합니다.
        return;
    }

    m_pixels[static_cast<size_t>(y) * static_cast<size_t>(m_bufferWidth) + static_cast<size_t>(x)] = color;
}

void CGardenCanvas::DrawFilledPoint(CPoint center, int radius, COLORREF color)
{
    const int radiusSquared = radius * radius;
    for (int y = center.y - radius; y <= center.y + radius; ++y)
    {
        for (int x = center.x - radius; x <= center.x + radius; ++x)
        {
            const int dx = x - center.x;
            const int dy = y - center.y;
            if (dx * dx + dy * dy <= radiusSquared)
            {
                PutPixelSafe(x, y, color);
            }
        }
    }
}

void CGardenCanvas::DrawCircleBorder(CPoint center, int radius, int thickness, COLORREF color)
{
    const double half = std::max(0, thickness - 1) / 2.0;
    const double inner = std::max(0.0, radius - half);
    const double outer = radius + half;
    const double innerSquared = inner * inner;
    const double outerSquared = outer * outer;

    for (int y = static_cast<int>(center.y - outer - 1); y <= static_cast<int>(center.y + outer + 1); ++y)
    {
        for (int x = static_cast<int>(center.x - outer - 1); x <= static_cast<int>(center.x + outer + 1); ++x)
        {
            const double dx = x - center.x;
            const double dy = y - center.y;
            const double distanceSquared = dx * dx + dy * dy;

            if (distanceSquared >= innerSquared && distanceSquared <= outerSquared)
            {
                PutPixelSafe(x, y, color);
            }
        }
    }
}

void CGardenCanvas::NotifyChanged()
{
    // 사용자가 점을 찍거나 드래그/랜덤 이동/초기화를 했을 때
    // 부모 Dialog에 현재 점 개수를 알려 상태 문구를 즉시 바꿉니다.
    CWnd* parent = GetParent();
    if (parent != nullptr)
    {
        parent->SendMessageW(WM_GARDEN_CANVAS_CHANGED, static_cast<WPARAM>(m_points.size()), 0);
    }
}
