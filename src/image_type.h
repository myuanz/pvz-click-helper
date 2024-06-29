#pragma once
#include <stdint.h>
// #include <Eigen/Core>

// const int 植物卡开始x = 1;
// using ImageType = Eigen::Matrix<uint8_t, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;

struct RGBPix {
    uint8_t b;
    uint8_t g;
    uint8_t r;
    uint8_t a;

    RGBPix() : r(0), g(0), b(0), a(0) {}
    RGBPix(uint8_t r, uint8_t g, uint8_t b) : r(r), g(g), b(b), a(0) {}

    bool operator==(const RGBPix& other) const {
        // printf("r: (%d, %d) g: (%d, %d) b: (%d, %d)\n", r, other.r, g, other.g, b, other.b);
        return r == other.r && g == other.g && b == other.b;
    }
};
enum class Channel {
    R = 0,
    G = 1,
    B = 2,
    A = 3,

};

class ImageType {
public:
    uint8_t* data; // BGRA
    int rows;
    int cols;

    ImageType(int rows, int cols) {
        this->rows = rows;
        this->cols = cols;
        if (rows == 0 || cols == 0) {
            data = nullptr;
        } else {
            data = (uint8_t*)malloc(rows * cols * 4);
        }
    }

    ~ImageType() {
        if (data != nullptr)
            free(data);
    }
    RGBPix& operator()(int x, int y) const {
        return *(RGBPix*)(data + (x + y * cols) * 4);
    }

    uint8_t& operator()(int x, int y, Channel c) const {
        return data[(x + y * cols) * 4 + static_cast<int>(c)];
    }

    int width() const {
        return cols;
    }
    int height() const {
        return rows;
    }

    void resize(int rows, int cols) {
        if (data != nullptr)
            free(data);
        this->rows = rows;
        this->cols = cols;
        data = (uint8_t*)malloc(rows * cols * 4);
        memset(data, 0, rows * cols * 4);
    }
};
/* 
全屏模式下:
总宽度: 2560(屏幕宽度)
总高度: 1440(屏幕高度)
非黑屏区域宽度: 2121
非黑屏区域高度: 1440
首张卡片大小: (116, 162)

非全屏模式下:
带标题栏大小: (886, 629)
无标题栏大小: (886, 601)

---

PVZ 杂交版 是固定长宽比游戏, 长宽比为 1:1.47421
 */