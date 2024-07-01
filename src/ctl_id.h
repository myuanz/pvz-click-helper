#pragma once
#include <array>
#include <windows.h>

const int ID_SNAPSHOT_TIMER = 1;
const int ID_FINDWINDOW_TIMER = 2;


const std::array<int, 14> ID_SEQ_HOTKEYS = {
    VK_F1, VK_F2, VK_F3, VK_F4, VK_F5, VK_F6, VK_F7, 
    VK_F8, VK_F9, VK_F10, VK_F11, VK_F12, VK_OEM_MINUS, VK_OEM_PLUS
    // 从 F1 到 F12, 然后绕到 - 和 = 键
};
const int ID_CHANCHU_HOTKEY = 0x43; // C, 铲除
const int ID_ZHONGZHI_HOTKEY = 0x5A; // Z, 种植
