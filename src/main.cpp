#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <stdio.h>
#include <fmt/format.h>
#include <vector>
#include <cmath>
#include <iostream>
#include <stdint.h>
#include <algorithm>
#include <functional>
#include <numeric>

#include "image_type.h"
#include "ctl_id.h"
#include <commctrl.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "Gdiplus.lib")

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void CaptureAndDrawBitmap(HWND hwnd, int x, int y, int w, int h);

HBITMAP g_hBitmap = NULL;
int g_captureX = 0;       // 截图起始X坐标
int g_captureY = 0;       // 截图起始Y坐标
int g_captureWidth = 666;   // 截图宽度
int g_captureHeight = 888;  // 截图高度
bool g_isDragging = false;

auto g_enable_brush = new Gdiplus::SolidBrush(Gdiplus::Color(128, 0, 255, 0));
auto g_disable_brush = new Gdiplus::SolidBrush(Gdiplus::Color(128, 255, 0, 0));

HWND g_targetWindow = NULL;

ImageType img = ImageType(0, 0);

VOID InitConsole() {
  if (AttachConsole(ATTACH_PARENT_PROCESS)) {
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);
    freopen("CONIN$", "r", stdin);
  } else {
    if (GetConsoleWindow() == NULL) {
      if (AllocConsole()) {
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
        freopen("CONIN$", "r", stdin);
      }
    }
  }
}

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
  DWORD processId;
  GetWindowThreadProcessId(hwnd, &processId);
  if (processId == (DWORD)lParam) {
    g_targetWindow = hwnd;
    return FALSE;  // 停止枚举
  }
  return TRUE;  // 继续枚举
}

HWND FindProcessWindow(const wchar_t* processName) {
  DWORD processId = 0;
  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snapshot != INVALID_HANDLE_VALUE) {
    PROCESSENTRY32W processEntry;
    processEntry.dwSize = sizeof(PROCESSENTRY32W);
    if (Process32FirstW(snapshot, &processEntry)) {
      do {
        if (_wcsicmp(processEntry.szExeFile, processName) == 0) {
          processId = processEntry.th32ProcessID;
          break;
        }
      } while (Process32NextW(snapshot, &processEntry));
    }
    CloseHandle(snapshot);
  }

  if (processId == 0) return NULL;

  g_targetWindow = NULL;
  EnumWindows(EnumWindowsProc, (LPARAM)processId);
  return g_targetWindow;
}

void UpdateCaptureArea() {
  if (g_targetWindow) {
    RECT rect;
    GetClientRect(g_targetWindow, &rect);
    g_captureX = rect.left;
    g_captureY = rect.top;

    g_captureWidth = rect.right - rect.left;
    g_captureHeight = rect.bottom - rect.top;
    fmt::print("Capture area: {} x {} at ({}, {})\n", g_captureWidth, g_captureHeight, g_captureX, g_captureY);
  }
}

void CreateControl(HWND hwnd) {
  // auto hButton = CreateWindow(
  //   L"BUTTON",  // Predefined class; Unicode assumed 
  //   L"点我",      // Button text 
  //   WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,  // Styles 
  //   10,         // x position 
  //   10,         // y position 
  //   100,        // Button width
  //   30,         // Button height
  //   hwnd,       // Parent window
  //   (HMENU)ID_BUTTON,
  //   (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
  //   NULL
  // );
  // CreateWindow(
  //   L"BUTTON",  // 预定义的按钮类（复选框也是按钮的一种）
  //   L"选项",    // 复选框文本
  //   WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,  // 样式
  //   10,         // x 位置
  //   50,         // y 位置
  //   100,        // 宽度
  //   30,         // 高度
  //   hwnd,       // 父窗口句柄
  //   (HMENU)ID_CHECKBOX, // 控件 ID
  //   (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
  //   NULL
  // );      // 指向额外数据的指针

}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow) {
  InitConsole();
  INITCOMMONCONTROLSEX icex;
  icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
  icex.dwICC = ICC_WIN95_CLASSES;
  InitCommonControlsEx(&icex);

  Gdiplus::GdiplusStartupInput gdiplusStartupInput;
  ULONG_PTR gdiplusToken;

  Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

  const auto CLASS_NAME = L"PVZ-helper";

  WNDCLASS wc = {
    .style = CS_HREDRAW | CS_VREDRAW,
    .lpfnWndProc = WindowProc,
    .hInstance = hInstance,
    .lpszClassName = CLASS_NAME,
  };
  wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  RegisterClass(&wc);

  HWND hwnd = CreateWindowEx(
    0, CLASS_NAME, CLASS_NAME, WS_OVERLAPPEDWINDOW, 
    CW_USEDEFAULT, CW_USEDEFAULT, 
    500, 500, 
    NULL, NULL, hInstance, NULL
  );

  if (hwnd == NULL) {
    MessageBox(NULL, L"窗口创建失败", L"错误", MB_OK | MB_ICONERROR);
    return 0;
  }

  ShowWindow(hwnd, nCmdShow);

  MSG msg = {};
  while (GetMessage(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  Gdiplus::GdiplusShutdown(gdiplusToken);
  return 0;
}

LRESULT CALLBACK WindowProc(
  HWND hwnd, UINT uMsg, WPARAM wParam,
  LPARAM lParam
) {
  switch (uMsg) {
    case WM_CREATE: {
      SetTimer(hwnd, 1, 100, NULL);

      g_targetWindow = FindProcessWindow(L"PlantsVsZombies.exe");
      if (g_targetWindow) {
        UpdateCaptureArea();
        SetTimer(hwnd, 1, 100, NULL);
        if (!RegisterHotKey(
          hwnd, ID_HOT_KEY, MOD_CONTROL | MOD_ALT, 'G'
        )) {
          // std::cout << "热键注册失败" << std::endl;
          MessageBox(
            hwnd, L"热键注册失败", L"错误", MB_OK | MB_ICONERROR
          );
          return 1;
        }

      } else {
        MessageBox(
          hwnd, L"无法找到 PlantsVsZombies.exe 窗口", L"错误",
          MB_OK | MB_ICONERROR
        );
        exit(1);
      }
      CreateControl(hwnd);
      HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
      EnumChildWindows(hwnd, [](HWND hwnd, LPARAM lParam) -> BOOL {
        SendMessage(hwnd, WM_SETFONT, lParam, TRUE);
        return TRUE;
      }, (LPARAM)hFont);
    }
    case WM_TIMER:
      UpdateCaptureArea();
      if (g_captureHeight > 0 && g_captureWidth > 0){
        CaptureAndDrawBitmap(
          hwnd, g_captureX, g_captureY, 
          g_captureWidth, g_captureHeight
        );
      }
      return 0;

    case WM_PAINT: {
      PAINTSTRUCT ps;
      HDC hdc = BeginPaint(hwnd, &ps);

      if (g_hBitmap && !g_isDragging) {
        HDC hdcMem = CreateCompatibleDC(hdc);
        HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, g_hBitmap);

        SetStretchBltMode(hdc, HALFTONE);
        StretchBlt(
          hdc, 0, 0, g_captureWidth, g_captureHeight, hdcMem, 0, 0,
          g_captureWidth, g_captureHeight, SRCCOPY
        );

        SelectObject(hdcMem, hbmOld);
        DeleteDC(hdcMem);
      }

      EndPaint(hwnd, &ps);
      break;
    }
    case WM_HOTKEY:
      fmt::print("Hot key triggered {}\n", wParam);
      if (wParam == ID_HOT_KEY) {
        std::cout << "热键被触发！" << std::endl;
      }
      break;
    case WM_DESTROY:
      if (g_hBitmap) {
        DeleteObject(g_hBitmap);
      }
      PostQuitMessage(0);
      break;
    case WM_ENTERSIZEMOVE:
      // 开始拖动
      g_isDragging = true;
      // 通知系统不要绘制窗口内容
      SendMessage(hwnd, WM_SETREDRAW, FALSE, 0);
      return 0;

    case WM_EXITSIZEMOVE:
      // 结束拖动
      g_isDragging = false;
      // 恢复绘制
      SendMessage(hwnd, WM_SETREDRAW, TRUE, 0);
      // 强制重绘整个窗口
      InvalidateRect(hwnd, NULL, TRUE);
      UpdateWindow(hwnd);
      return 0;
  }

  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void getBitmapBuffer(HBITMAP hBitmap, ImageType& output_img) {
  BITMAP bmp;
  GetObject(hBitmap, sizeof(BITMAP), &bmp);

  int width = bmp.bmWidth;
  int height = bmp.bmHeight;

  BITMAPINFOHEADER bi = {0};
  bi.biSize = sizeof(BITMAPINFOHEADER);
  bi.biWidth = width;
  bi.biHeight = -height;  // Top-down DIB
  bi.biPlanes = 1;
  bi.biBitCount = 32;
  bi.biCompression = BI_RGB;

  HDC hdc = GetDC(NULL);
  GetDIBits(
    hdc, hBitmap, 0, height, 
    output_img.data.get(), (BITMAPINFO*)&bi, DIB_RGB_COLORS
  );
  ReleaseDC(NULL, hdc);
}

void CaptureAndDrawBitmap(HWND hwnd, int x, int y, int w, int h) {
  HDC hdcScreen = GetDC(NULL);
  HDC hdcMemDC = CreateCompatibleDC(hdcScreen);

  if (g_hBitmap) {
    DeleteObject(g_hBitmap);
  }

  g_hBitmap = CreateCompatibleBitmap(hdcScreen, w, h);
  HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMemDC, g_hBitmap);

  PrintWindow(g_targetWindow, hdcMemDC, PW_CLIENTONLY);

  if (img.rows != h || img.cols != w) {
    img.resize(h, w);
    
    RECT rect;
    GetClientRect(hwnd, &rect);
    rect.right = rect.left + w;
    rect.bottom = rect.top + h;
    SetWindowPos(hwnd, NULL, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER | SWP_NOMOVE);
  }

  getBitmapBuffer(g_hBitmap, img);
  // fmt::print("Image size: {} x {}\n", img.width(), img.height());
  RGBPix target_rgb = {110, 50, 19};

  auto col_sum = std::vector<int>(w, 0);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      if (target_rgb == img(x, y)) {
        col_sum[x]++;
        // fmt::print("eq {} {}\n", x, y);
      }
    }
  }

  std::vector<int> peaks;
  for (int i = 1; i < col_sum.size() - 1; ++i) {
    if (col_sum[i] > 0 && col_sum[i] > col_sum[i - 1] &&
        col_sum[i] >= col_sum[i + 1]) {
      peaks.push_back(i);
    }
  }
  int window_size = 5;
  int i = 0;
  for (; i + window_size <= peaks.size(); ++i) {
    auto window = std::vector<int>(peaks.begin() + i, peaks.begin() + i + window_size);
    auto delta = std::vector<int>(window_size - 1, 0);
    std::transform(
      window.begin() + 1, window.end(), window.begin(),
      delta.begin(), std::minus<>()
    );

    auto mean = std::accumulate(
      delta.begin(), delta.end(), 0
    ) / (window_size - 1);
    auto variance = std::accumulate(
      delta.begin(), delta.end(), 0,
      [mean](int acc, int x) {
        return acc + (x - mean) * (x - mean);
      }) / (window_size - 1);

    if (variance < 1 && mean > 20) {
      break;
    }
  }
  peaks.erase(peaks.begin(), peaks.begin() + i);

  int card_top = 8;
  int card_width = 50;
  int card_height = 70;
  Gdiplus::Graphics graphics(hdcMemDC);
  Card card;

  for (const auto peak : peaks) {
    int x = peak - card_width;
    int y = 0 + card_top;
    card = Card(&img, x, y, card_width, card_height);
    auto enable = card.count();

    graphics.FillRectangle(
      enable ? g_enable_brush : g_disable_brush, 
      peak-card_width, 0+card_top+card_height, card_width, 30
    );
  }

  SelectObject(hdcMemDC, hbmOld);
  DeleteDC(hdcMemDC);
  ReleaseDC(NULL, hdcScreen);

  InvalidateRect(hwnd, NULL, FALSE);
}
