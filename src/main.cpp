﻿#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#define NOMINMAX 

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
#include <commctrl.h>

#include "image_type.h"
#include "ctl_id.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "Gdiplus.lib")

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void CaptureAndSetStatus(HWND hwnd);

HBITMAP g_hBitmap = NULL;

int g_captureWidth = 0;   // 截图宽度
int g_captureHeight = 0;  // 截图高度
int g_captureLeft = 0;    // 截图左上角 x 坐标
int g_captureTop = 0;     // 截图左上角 y 坐标
int g_captureTitleBarHeight = 0;  // 窗口标题栏高度

bool g_isDragging = false;

Gdiplus::SolidBrush* g_enable_background_brush;
Gdiplus::SolidBrush* g_disable_background_brush;
Gdiplus::SolidBrush* g_enable_string_brush;
Gdiplus::SolidBrush* g_disable_string_brush;
Gdiplus::Font* g_font;

HWND g_targetWindow = NULL;

ImageType g_img = ImageType(pvz_size.height, pvz_size.width);
auto g_screen_buffer = std::vector<uint8_t>();

const int MAX_CARD_COUNT = 14;
std::vector<int> card_border_peaks;
std::vector<bool> card_enable;

static int g_snapshot_timer_event_id = 0;
static int g_findwindow_timer_event_id = 0;

HHOOK g_hHook = NULL;

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
    g_captureWidth = rect.right - rect.left;
    g_captureHeight = rect.bottom - rect.top;

    GetWindowRect(g_targetWindow, &rect);
    g_captureLeft = rect.left;
    g_captureTop = rect.top;
    g_captureTitleBarHeight = GetSystemMetrics(SM_CYCAPTION);

    // fmt::print("Capture area: {} x {} @ {}\n", g_captureWidth, g_captureHeight, fmt::ptr(g_targetWindow));
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

void MoveClickAndBack(int x, int y) {
  HWND hwnd = GetForegroundWindow();
  if (hwnd != g_targetWindow) {
    return;
  }

  POINT startPos;
  GetCursorPos(&startPos);
  // fmt::print("Start position: ({}, {})\n", startPos.x, startPos.y);
  // fmt::print("Click position: ({}, {})\n", x, y);

  SetCursorPos(x, y);
  Sleep(10);
  mouse_event(MOUSEEVENTF_LEFTDOWN, x, y, 0, 0);
  mouse_event(MOUSEEVENTF_LEFTUP, x, y, 0, 0);
  Sleep(10);
  SetCursorPos(startPos.x, startPos.y);
  Sleep(10);
  mouse_event(MOUSEEVENTF_LEFTDOWN, x, y, 0, 0);
  mouse_event(MOUSEEVENTF_LEFTUP, x, y, 0, 0);
}

void PlantingCard(int card_index) {
  if (card_index < 0 || card_index >= card_border_peaks.size()) {
    return;
  }
  int x = card_border_peaks[card_index] + pvz_size.card_width / 2;
  int y = g_captureTop + pvz_size.card_height / 2 + pvz_size.card_top;
  MoveClickAndBack(x, y);
}

void OnKey(DWORD vkCode) {
  int toPlant = -1;

  if (vkCode == ID_ZHONGZHI_HOTKEY) {
    for (int i = 0; i < card_enable.size(); i++) {
      if (card_enable[i]) {
        toPlant = i;
        break;
      }
    }
  } else if (vkCode == ID_CHANCHU_HOTKEY) {
    MoveClickAndBack(
      g_captureLeft + pvz_size.width - pvz_size.card_width / 2,
      g_captureTop + pvz_size.card_top + pvz_size.card_height / 2
    );
  } else if (vkCode >= ID_SEQ_HOTKEYS[0] && vkCode <= ID_SEQ_HOTKEYS[ID_SEQ_HOTKEYS.size()-3]) {
    toPlant = vkCode - ID_SEQ_HOTKEYS[0];
  } else if (vkCode == ID_SEQ_HOTKEYS[ID_SEQ_HOTKEYS.size()-2]) {
    toPlant = 12;
  } else if (vkCode == ID_SEQ_HOTKEYS[ID_SEQ_HOTKEYS.size()-1]) {
    toPlant = 13;
  }
  if (toPlant >= 0) {
    PlantingCard(toPlant);
    // fmt::print("Planting card: {}\n", toPlant);
  }
  // fmt::print("Key pressed: {}\n", vkCode);
}

LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
  if (nCode == HC_ACTION) {
    if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
      KBDLLHOOKSTRUCT* pKbdStruct = (KBDLLHOOKSTRUCT*)lParam;
      OnKey(pKbdStruct->vkCode);
    }
  }
  return CallNextHookEx(g_hHook, nCode, wParam, lParam);
}
void RegisterHotKeys(HWND hwnd) {
  for (int i = 0; i < ID_SEQ_HOTKEYS.size(); i++) {
    RegisterHotKey(hwnd, ID_SEQ_HOTKEYS[i], NULL, ID_SEQ_HOTKEYS[i]);
  }
  RegisterHotKey(hwnd, ID_CHANCHU_HOTKEY, MOD_CONTROL, ID_CHANCHU_HOTKEY);
  RegisterHotKey(hwnd, ID_ZHONGZHI_HOTKEY, MOD_CONTROL, ID_ZHONGZHI_HOTKEY);
}

Gdiplus::Font* CreateDefaultFont() {
    NONCLIENTMETRICS ncm;
    ncm.cbSize = sizeof(NONCLIENTMETRICS);
    SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncm, 0);

    Gdiplus::FontFamily fontFamily(ncm.lfMessageFont.lfFaceName);
    float emSize = abs(ncm.lfMessageFont.lfHeight);

    return new Gdiplus::Font(&fontFamily, emSize, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
  InitConsole();
  g_hHook = SetWindowsHookEx(
    WH_KEYBOARD_LL, KeyboardProc, GetModuleHandle(NULL), 0
  );
  if (g_hHook == NULL) {
    MessageBox(NULL, L"无法初始化非独占的键盘热键, 一般是由于杀毒软件阻止. \n接下来将使用独占热键, 以下按键将由本程序占用:\n\tF1、F2、...、F12、减号键、加号键、C、Z\n\n如欲恢复关闭本程序即可", L"注册热键错误", MB_ICONERROR);
    return 1;
  }

  INITCOMMONCONTROLSEX icex;
  icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
  icex.dwICC = ICC_WIN95_CLASSES;
  InitCommonControlsEx(&icex);

  Gdiplus::GdiplusStartupInput gdiplusStartupInput;
  ULONG_PTR gdiplusToken;

  Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
  g_enable_background_brush = new Gdiplus::SolidBrush(Gdiplus::Color(128, 0, 255, 0));
  g_disable_background_brush = new Gdiplus::SolidBrush(Gdiplus::Color(128, 255, 0, 0));
  g_enable_string_brush = new Gdiplus::SolidBrush(Gdiplus::Color(255, 255, 0, 255));
  g_disable_string_brush = new Gdiplus::SolidBrush(Gdiplus::Color(255, 255, 255, 255));
  g_font = CreateDefaultFont();

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

  RECT rect = {0, 0, pvz_size.width, pvz_size.height};
  AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

  HWND hwnd = CreateWindowEx(
    0, CLASS_NAME, CLASS_NAME, WS_OVERLAPPEDWINDOW, 
    CW_USEDEFAULT, CW_USEDEFAULT, 
    rect.right - rect.left, rect.bottom - rect.top, 
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
      g_findwindow_timer_event_id = SetTimer(hwnd, ID_FINDWINDOW_TIMER, 1000, NULL);
      if (g_hHook == NULL) {
        RegisterHotKeys(hwnd);
      }

      CreateControl(hwnd);
      HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
      EnumChildWindows(hwnd, [](HWND hwnd, LPARAM lParam) -> BOOL {
        SendMessage(hwnd, WM_SETFONT, lParam, TRUE);
        return TRUE;
      }, (LPARAM)hFont);
    }
    case WM_TIMER:
      if (g_targetWindow && wParam == ID_SNAPSHOT_TIMER) {
        UpdateCaptureArea();
        if (g_captureHeight > 0 && g_captureWidth > 0) {
          CaptureAndSetStatus(hwnd);
        }
      } else if (wParam == ID_FINDWINDOW_TIMER) {
        g_targetWindow = FindProcessWindow(L"PlantsVsZombies.exe");
        if (g_targetWindow) {
          UpdateCaptureArea();
          wchar_t title[256];
          wsprintf(title, L"[%p] PVZ 点击助手", g_targetWindow);
          SetWindowText(hwnd, title);
          if (g_snapshot_timer_event_id == 0) {
            g_snapshot_timer_event_id = SetTimer(hwnd, ID_SNAPSHOT_TIMER, 100, NULL);
            printf("Snapshot Timer started %d\n", g_snapshot_timer_event_id);
          }
        } else {
          static int find_count = 0;
          wchar_t title[256];
          wsprintf(title, L"[%d] 未找到 PVZ 窗口", find_count++);
          SetWindowText(hwnd, title);
          KillTimer(hwnd, g_snapshot_timer_event_id);
          printf("Snapshot Timer stopped %d\n", g_snapshot_timer_event_id);
          g_snapshot_timer_event_id = 0;
        }
      }
      break;

    case WM_PAINT: {
      if (g_hBitmap && !g_isDragging && g_captureHeight > 0 && g_captureWidth > 0) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        HDC hdcMem = CreateCompatibleDC(hdc);
        HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, g_hBitmap);
        Gdiplus::Graphics graphics(hdcMem);

        for(int i=0; i<card_border_peaks.size(); i++) {
          auto background_brush = card_enable[i] ? g_enable_background_brush : g_disable_background_brush;
          auto string_brush = card_enable[i] ? g_enable_string_brush : g_disable_string_brush;

          auto draw_h = 15;
          auto peak = card_border_peaks[i];
          auto draw_string_p = Gdiplus::PointF(peak-pvz_size.card_width, pvz_size.card_top+pvz_size.card_height-draw_h);

          graphics.FillRectangle(
            background_brush, 
            peak-pvz_size.card_width, pvz_size.card_top+pvz_size.card_height-draw_h, 
            pvz_size.card_width, draw_h
          );
          graphics.DrawString(
            std::to_wstring(i+1).c_str(), 
            -1, 
            g_font,
            draw_string_p,
            string_brush
          );
        }

        SetStretchBltMode(hdc, HALFTONE);
        StretchBlt(
          hdc, 0, 0, g_captureWidth, g_captureHeight, 
          hdcMem, 0, 0, g_captureWidth, g_captureHeight, SRCCOPY
        );

        SelectObject(hdcMem, hbmOld);
        DeleteDC(hdcMem);
        EndPaint(hwnd, &ps);
      }

      break;
    }
    case WM_HOTKEY:
      OnKey(wParam);
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

HBITMAP getScaledBitmapBuffer(HDC hdc, HBITMAP hBitmap, ImageType& output_img) {
  BITMAP bmp;
  GetObject(hBitmap, sizeof(BITMAP), &bmp);

  int width = bmp.bmWidth;
  int height = bmp.bmHeight;

  BITMAPINFOHEADER bi = {0};
  bi.biSize = sizeof(BITMAPINFOHEADER);
  bi.biWidth = pvz_size.width;
  bi.biHeight = -pvz_size.height;  // Top-down DIB
  bi.biPlanes = 1;
  bi.biBitCount = 32;
  bi.biCompression = BI_RGB;

  if (width != pvz_size.width || height != pvz_size.height) {
    // 表明现在窗口不是标准大小, 应当获取图片后裁剪到标准大小再放到 output_img.data
    int new_width = width, new_height = height;
    int new_x_start = 0, new_y_start = 0;

    if (1.0 * width / height > 1.0 * pvz_size.width / pvz_size.height) {
      // 宽度过大, 应当裁剪左右
      new_width = height * pvz_size.width / pvz_size.height;
      new_x_start = (width - new_width) / 2;
    } else if (1.0 * width / height < 1.0 * pvz_size.width / pvz_size.height) {
      // 高度过大, 应当裁剪上下
      new_height = width * pvz_size.height / pvz_size.width;
      new_y_start = (height - new_height) / 2;
    }
    HDC hdcPartial = CreateCompatibleDC(NULL);
    HBITMAP hBitmapPartial = CreateCompatibleBitmap(hdc, new_width, new_height);
    SelectObject(hdcPartial, hBitmapPartial);
    BitBlt(hdcPartial, 0, 0, new_width, new_height, hdc, new_x_start, new_y_start, SRCCOPY);

    // 裁剪完成后再把 hBitmapPartial 缩放到标准大小
    HDC hdcResize = CreateCompatibleDC(NULL);
    HBITMAP hBitmapResize = CreateCompatibleBitmap(hdc, pvz_size.width, pvz_size.height);
    SelectObject(hdcResize, hBitmapResize);
    SetStretchBltMode(hdcResize, HALFTONE);
    StretchBlt(
      hdcResize, 0, 0, pvz_size.width, pvz_size.height, 
      hdcPartial, 0, 0, new_width, new_height, SRCCOPY
    );
    GetDIBits(
      hdcResize, hBitmapResize, 0, pvz_size.height, 
      output_img.data.get(), (BITMAPINFO*)&bi, DIB_RGB_COLORS
    );
    
    SelectObject(hdc, hBitmapResize);

    DeleteDC(hdcPartial);
    DeleteDC(hdcResize);
    return hBitmapResize;
  } else {
    GetDIBits(
      hdc, hBitmap, 0, height, 
      output_img.data.get(), (BITMAPINFO*)&bi, DIB_RGB_COLORS
    );
    return hBitmap;
  }
}

// 从屏幕截图并缩放到标准大小, 返回缩放后的 HDC, 图像保存在 g_img 中
HDC CapturePVZAndResize() {
  HDC hdcScreen = GetDC(NULL);
  HDC hdcMemDC = CreateCompatibleDC(hdcScreen);

  if (g_hBitmap) {
    DeleteObject(g_hBitmap);
  }

  g_hBitmap = CreateCompatibleBitmap(hdcScreen, g_captureWidth, g_captureHeight);
  HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMemDC, g_hBitmap);

  PrintWindow(g_targetWindow, hdcMemDC, PW_CLIENTONLY);
  g_hBitmap = getScaledBitmapBuffer(hdcMemDC, g_hBitmap, g_img);
  ReleaseDC(NULL, hdcScreen);
  return hdcMemDC;
}

void FindCardBorder() {
  RGBPix target_rgb = {110, 50, 19};

  auto col_sum = std::vector<int>(pvz_size.width, 0);
  for (int y = 0; y < pvz_size.height; ++y) {
    for (int x = 0; x < pvz_size.width; ++x) {
      if (target_rgb == g_img(x, y)) {
        col_sum[x]++;
      }
    }
  }

  for (int i = 1; i < col_sum.size() - 1; ++i) {
    if (col_sum[i] > 0 && col_sum[i] > col_sum[i - 1] &&
        col_sum[i] >= col_sum[i + 1]) {
      card_border_peaks.push_back(i);
    }
  }
  
  int window_size = 5;
  int i = 0;
  for (; i + window_size <= card_border_peaks.size(); ++i) {
    auto window = std::vector<int>(card_border_peaks.begin() + i, card_border_peaks.begin() + i + window_size);
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
  card_border_peaks.erase(card_border_peaks.begin(), card_border_peaks.begin() + i);
  if (card_border_peaks.size() > 0) {
    fmt::print("Card border peaks: {}\n", fmt::join(card_border_peaks, ", "));
  }
}

void CaptureAndSetStatus(HWND hwnd) {
  HDC hdcMemDC = CapturePVZAndResize();

  // fmt::print("Image size: {} x {}\n", img.width(), img.height());
  if (MAX_CARD_COUNT != card_border_peaks.size()) {
    card_border_peaks.clear();
    FindCardBorder();
  }
  card_enable.resize(card_border_peaks.size());

  Card card;

  auto strings = std::vector<std::string>();
  for (int i=0; const auto peak : card_border_peaks) {
    int x = peak - pvz_size.card_width;
    int y = pvz_size.card_top;
    card = Card(&g_img, x, y, pvz_size.card_width, pvz_size.card_height);
    auto enable = card.count();
    if (
      (1.0 * card.sun_count / (card.more_200 + 1) > 0.5) 
      // || (1.0 * card.sun_count / (card.less_50 + 1) > 2)
    ) {
      // 阳光比例较高时, 设为不可用
      enable = false;
    }
    card_enable[i] = enable;

    // fmt::print("[{}] less: {:03d}, more: {:03d}, f: {:.2f}, sun: {}\n", peak, card.less_50, card.more_200, 1.0 * card.more_200 / card.less_50, card.sun_count);
    // strings.push_back(fmt::format(
    //   "[{:02d}|{}] less: {:03d}, more: {:03d}, f: {:.2f}, sun: {}\n", 
    //   i+1, peak, card.less_50, card.more_200, 1.0 * card.more_200 / card.less_50, card.sun_count
    // ));

    i++;
  }
  // fmt::print("\n");
  // fmt::println("{}", fmt::join(strings, ""));

  DeleteDC(hdcMemDC);
  InvalidateRect(hwnd, NULL, FALSE);
}
