#include "GardenCircleDlg.h"

#include <cstdlib>
#include <ctime>

namespace
{
constexpr UINT WM_RANDOM_MOVE_STEP = WM_APP + 1;
constexpr UINT WM_RANDOM_MOVE_FINISHED = WM_APP + 2;
constexpr int kDefaultPointRadius = 6;
constexpr int kDefaultGardenBorder = 2;
constexpr int kRandomMoveRepeatCount = 10;
constexpr int kRandomMoveDelayMs = 2000;
}

BEGIN_MESSAGE_MAP(CGardenCircleDlg, CDialogEx)
    ON_WM_DESTROY()
    ON_BN_CLICKED(IDC_BUTTON_RESET, &CGardenCircleDlg::OnBnClickedReset)
    ON_BN_CLICKED(IDC_BUTTON_RANDOM, &CGardenCircleDlg::OnBnClickedRandom)
    ON_EN_CHANGE(IDC_EDIT_RADIUS, &CGardenCircleDlg::OnEnChangeRadius)
    ON_EN_CHANGE(IDC_EDIT_BORDER, &CGardenCircleDlg::OnEnChangeBorder)
    ON_MESSAGE(WM_RANDOM_MOVE_STEP, &CGardenCircleDlg::OnRandomMoveStep)
    ON_MESSAGE(WM_RANDOM_MOVE_FINISHED, &CGardenCircleDlg::OnRandomMoveFinished)
    ON_MESSAGE(WM_GARDEN_CANVAS_CHANGED, &CGardenCircleDlg::OnCanvasChanged)
END_MESSAGE_MAP()

CGardenCircleDlg::CGardenCircleDlg(CWnd* parent)
    : CDialogEx(IDD_GARDENCIRCLEMFC_DIALOG, parent)
{
}

void CGardenCircleDlg::DoDataExchange(CDataExchange* dx)
{
    CDialogEx::DoDataExchange(dx);
    DDX_Control(dx, IDC_CANVAS, m_canvas);
    DDX_Control(dx, IDC_EDIT_RADIUS, m_radiusEdit);
    DDX_Control(dx, IDC_EDIT_BORDER, m_borderEdit);
    DDX_Control(dx, IDC_BUTTON_RANDOM, m_randomButton);
    DDX_Control(dx, IDC_STATIC_STATUS, m_status);
}

BOOL CGardenCircleDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    // 요구사항: 클릭 지점 원을 그릴 때의 반지름 크기는 사용자로부터 입력 받습니다.  
    // Dialog의 Edit 컨트롤을 초기화해서 실행 직후에도 바로 조작 가능하게 합니다.
    SetDlgItemInt(IDC_EDIT_RADIUS, kDefaultPointRadius);
    SetDlgItemInt(IDC_EDIT_BORDER, kDefaultGardenBorder);
    ApplyInputValues();

    SetStatus(L"상태: 점 3개를 클릭하세요.");
    return TRUE;
}

void CGardenCircleDlg::OnDestroy()
{
    StopRandomWorker();
    CDialogEx::OnDestroy();
}

void CGardenCircleDlg::OnBnClickedReset()
{
    // 요구사항: [초기화] 버튼을 누르면 그려졌던 모든 내용들을 삭제하고 처음부터 입력 받을 수 있는 상태가 되어야 합니다.  
    StopRandomWorker();
    m_randomMoveCount = 0;
    m_canvas.ResetAll();
    m_randomButton.EnableWindow(TRUE);
    m_randomButton.SetWindowTextW(L"랜덤 이동");
    SetStatus(L"상태: 초기화 완료. 점 3개를 클릭하세요.");
}

void CGardenCircleDlg::OnBnClickedRandom()
{
    ApplyInputValues();

    if (m_canvas.GetPointCount() != 3)
    {
        SetStatus(L"상태: 랜덤 이동 전에 점 3개를 먼저 입력하세요.");
        return;
    }

    StartRandomWorker();
}

void CGardenCircleDlg::OnEnChangeRadius()
{
    ApplyInputValues();
}

void CGardenCircleDlg::OnEnChangeBorder()
{
    ApplyInputValues();
}

LRESULT CGardenCircleDlg::OnRandomMoveStep(WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);

    // 요구사항: 랜덤한 위치로 이동 및 정원 그리기 동작을 초당 2회, 총 10번 자동으로 반복하되, 메인 UI가 프리징 상태가 되지 않도록 별도 쓰레드로 구현해야 합니다.  
    // UI 컨트롤은 메인 UI 스레드에서만 안전하게 접근해야 하므로,
    // 작업 스레드는 메시지만 보내고 실제 화면 갱신은 여기서 수행한다.
    m_canvas.RandomMovePoints();
    m_randomMoveCount = static_cast<int>(wParam);

    CString status;
    status.Format(L"상태: 랜덤 이동 %d/%d", m_randomMoveCount, kRandomMoveRepeatCount);
    SetStatus(status);
    return 0;
}

LRESULT CGardenCircleDlg::OnRandomMoveFinished(WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(wParam);
    UNREFERENCED_PARAMETER(lParam);

    delete m_randomThread;
    m_randomThread = nullptr;
    m_stopRandomThread = false;
    m_randomButton.EnableWindow(TRUE);
    m_randomButton.SetWindowTextW(L"랜덤 이동");
    SetStatus(L"상태: 랜덤 이동 10회 완료.");
    return 0;
}

LRESULT CGardenCircleDlg::OnCanvasChanged(WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);

    const int pointCount = static_cast<int>(wParam);
    if (pointCount < 3)
    {
        CString status;
        status.Format(L"상태: 점 %d/3 입력됨. 흰 영역을 클릭하세요.", pointCount);
        SetStatus(status);
    }
    else
    {
        SetStatus(L"상태: 정원 그리기 완료. 점을 드래그하거나 랜덤 이동을 누르세요.");
    }
    return 0;
}

void CGardenCircleDlg::ApplyInputValues()
{
    const int radius = ReadIntFromEdit(IDC_EDIT_RADIUS, kDefaultPointRadius);
    const int border = ReadIntFromEdit(IDC_EDIT_BORDER, kDefaultGardenBorder);
    m_canvas.SetPointRadius(radius);
    m_canvas.SetGardenBorderWidth(border);
}

int CGardenCircleDlg::ReadIntFromEdit(UINT controlId, int fallback) const
{
    BOOL translated = FALSE;
    const int value = GetDlgItemInt(controlId, &translated, FALSE);
    if (!translated || value <= 0)
    {
        return fallback;
    }
    return value;
}

void CGardenCircleDlg::SetStatus(const CString& message)
{
    m_status.SetWindowTextW(message);
}

void CGardenCircleDlg::StartRandomWorker()
{
    if (m_randomThread != nullptr)
    {
        return;
    }

    // 요구사항 : 랜덤한 위치로 이동 및 정원 그리기 동작을 초당 2회, 총 10번 자동으로 반복하되, 메인 UI가 프리징 상태가 되지 않도록 별도 쓰레드로 구현해야 합니다.  
    m_stopRandomThread = false;
    m_randomMoveCount = 0;
    m_randomButton.EnableWindow(FALSE);
    m_randomButton.SetWindowTextW(L"랜덤 이동 중...");
    SetStatus(L"상태: 랜덤 이동 시작.");

    m_randomThread = AfxBeginThread(
        &CGardenCircleDlg::RandomWorkerProc,
        this,
        THREAD_PRIORITY_NORMAL,
        0,
        CREATE_SUSPENDED,
        nullptr);
    if (m_randomThread == nullptr)
    {
        m_randomButton.EnableWindow(TRUE);
        m_randomButton.SetWindowTextW(L"랜덤 이동");
        SetStatus(L"상태: 랜덤 이동 스레드 생성 실패.");
        return;
    }

    // 작업 스레드 객체를 MFC가 자동 삭제하지 않게 해야 StopRandomWorker/완료 메시지에서
    // 핸들 대기와 포인터 정리를 확실히 할 수 있습니다.
    m_randomThread->m_bAutoDelete = FALSE;
    m_randomThread->ResumeThread();
}

void CGardenCircleDlg::StopRandomWorker()
{
    if (m_randomThread == nullptr)
    {
        return;
    }

    m_stopRandomThread = true;
    WaitForSingleObject(m_randomThread->m_hThread, kRandomMoveDelayMs + 500);
    delete m_randomThread;
    m_randomThread = nullptr;
    m_stopRandomThread = false;
}

UINT CGardenCircleDlg::RandomWorkerProc(LPVOID parameter)
{
    auto* dialog = reinterpret_cast<CGardenCircleDlg*>(parameter);
    if (dialog == nullptr)
    {
        return 0;
    }

    for (int count = 1; count <= kRandomMoveRepeatCount; ++count)
    {
        for (int elapsed = 0; elapsed < kRandomMoveDelayMs; elapsed += 50)
        {
            if (dialog->m_stopRandomThread)
            {
                return 0;
            }
            Sleep(50);
        }

        if (dialog->m_stopRandomThread)
        {
            return 0;
        }

        dialog->PostMessageW(WM_RANDOM_MOVE_STEP, static_cast<WPARAM>(count), 0);
    }

    dialog->PostMessageW(WM_RANDOM_MOVE_FINISHED, 0, 0);
    return 0;
}
