#pragma once

#include <afxdialogex.h>
#include <afxwin.h>

#include "GardenCanvas.h"
#include "resource.h"

class CGardenCircleDlg : public CDialogEx
{
public:
    CGardenCircleDlg(CWnd* parent = nullptr);

protected:
    BOOL OnInitDialog() override;
    void DoDataExchange(CDataExchange* dx) override;

    afx_msg void OnDestroy();
    afx_msg void OnBnClickedReset();
    afx_msg void OnBnClickedRandom();
    afx_msg void OnEnChangeRadius();
    afx_msg void OnEnChangeBorder();
    afx_msg LRESULT OnRandomMoveStep(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnRandomMoveFinished(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnCanvasChanged(WPARAM wParam, LPARAM lParam);

    DECLARE_MESSAGE_MAP()

private:
    void ApplyInputValues();
    int ReadIntFromEdit(UINT controlId, int fallback) const;
    void SetStatus(const CString& message);
    void StartRandomWorker();
    void StopRandomWorker();

    static UINT RandomWorkerProc(LPVOID parameter);

private:
    CGardenCanvas m_canvas;
    CEdit m_radiusEdit;
    CEdit m_borderEdit;
    CButton m_randomButton;
    CStatic m_status;

    CWinThread* m_randomThread = nullptr;
    volatile bool m_stopRandomThread = false;
    int m_randomMoveCount = 0;
};
