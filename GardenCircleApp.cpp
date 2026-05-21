#include "GardenCircleApp.h"

#include "GardenCircleDlg.h"
#include "resource.h"

CGardenCircleApp theApp;

BOOL CGardenCircleApp::InitInstance()
{
    CWinApp::InitInstance();

    // MFC Dialog 기반 프로젝트 요구사항:
    // 애플리케이션의 메인 윈도우를 Dialog 객체로 두고 DoModal()로 실행합니다.
    CGardenCircleDlg dialog;
    m_pMainWnd = &dialog;
    dialog.DoModal();
    return FALSE;
}
