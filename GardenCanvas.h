#pragma once

#include <afxwin.h>
#include <vector>

constexpr UINT WM_GARDEN_CANVAS_CHANGED = WM_APP + 10;

class CGardenCanvas : public CWnd
{
public:
    void SetPointRadius(int radius);
    void SetGardenBorderWidth(int width);
    void ResetAll();
    void RandomMovePoints();

    int GetPointCount() const;

protected:
    afx_msg void OnPaint();
    afx_msg void OnLButtonDown(UINT flags, CPoint point);
    afx_msg void OnMouseMove(UINT flags, CPoint point);
    afx_msg void OnLButtonUp(UINT flags, CPoint point);

    DECLARE_MESSAGE_MAP()

private:
    struct Circle
    {
        CPoint center{};
        int radius = 0;
    };

    bool FindClickedPoint(CPoint mousePoint, int& index) const;
    bool CalculateGardenCircle(Circle& circle) const;
    CPoint ClampToClient(CPoint point) const;

    void DrawScene(CDC& dc);
    void ClearBuffer(COLORREF color);
    void PutPixelSafe(int x, int y, COLORREF color);
    void DrawFilledPoint(CPoint center, int radius, COLORREF color);
    void DrawCircleBorder(CPoint center, int radius, int thickness, COLORREF color);
    void NotifyChanged();

private:
    std::vector<CPoint> m_points;
    int m_pointRadius = 6;
    int m_gardenBorderWidth = 2;
    int m_draggingIndex = -1;
    bool m_isDragging = false;

    std::vector<COLORREF> m_pixels;
    int m_bufferWidth = 0;
    int m_bufferHeight = 0;
};
