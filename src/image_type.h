#pragma once
#include <stdint.h>
// #include <fmt/core.h>
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
    std::unique_ptr<uint8_t[]> data; // BGRA
    int rows;
    int cols;

    ImageType(int rows, int cols) {
        this->rows = rows;
        this->cols = cols;
        if (rows > 0 && cols > 0) {
            data = std::make_unique<uint8_t[]>(rows * cols * 4);
        }
    }

    RGBPix& operator()(int x, int y) const {
        return *(RGBPix*)(data.get() + (x + y * cols) * 4);
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
        this->rows = rows;
        this->cols = cols;
        data = std::make_unique<uint8_t[]>(rows * cols * 4);
        std::memset(data.get(), 0, rows * cols * 4);
    }

    ImageType subimage(int x, int y, int w, int h) const {
        ImageType img(h, w);
        // fmt::print(
        //     "src img p: {:p}, length: {:x}, end: {:p}\n", 
        //     fmt::ptr(data), rows * cols * 4, 
        //     fmt::ptr(data + rows * cols * 4)
        // );
        // fmt::print(
        //     "dst img p: {:p}, length: {:x}, end: {:p}\n", 
        //     fmt::ptr(img.data), w * h * 4, 
        //     fmt::ptr(img.data + w * h * 4)
        // );
        for (int y_ = 0; y_ < h; y_++) {
            // fmt::print(
            //     "\twill copy: ({}, {}) -> ({}, {})\n",
            //     fmt::ptr(data + (x + (y + y_) * cols) * 4),
            //     fmt::ptr(data + (x + (y + y_) * cols) * 4 + w * 4),
            //     fmt::ptr(img.data + y_ * w * 4),
            //     fmt::ptr(img.data + y_ * w * 4 + w * 4)
            // );
                
            std::memcpy(img.data.get() + y_ * w * 4, data.get() + (x + (y + y_) * cols) * 4, w * 4);
        }
        return img;
    }
};

class Card {
public:
    ImageType *raw_img;
    int start_x;
    int start_y;
    int width;
    int height;

    Card(ImageType *raw_img, int start_x, int start_y, int width, int height) {
        this->raw_img = raw_img;
        this->start_x = start_x;
        this->start_y = start_y;
        this->width = width;
        this->height = height;
    }

    bool enable() {
        return 1.0 * more_200 / less_50 > 1;
    }

    bool count() {
        less_50 = more_200 = 0;
        for (int i = 0; i < height; i++) {
            for (int j = 0; j < width; j++) {
                for (int c = 0; c < 3; c++) {
                    uint8_t p = raw_img->data[
                        (start_x + j + (start_y + i) * raw_img->cols) * 4 + c
                    ];
                    if (p < 50) {
                        less_50++;
                    } else if (p > 200) {
                        more_200++;
                    }

                }
            }
        }
        // fmt::print("less_50: {}, more_200: {}, f: {}\n", less_50, more_200, 1.0 * more_200 / less_50);
        return enable();
    }

private:
    int less_50;
    int more_200;
};
    
/* 
全屏模式下:
总宽度: 2560(屏幕宽度)
总高度: 1440(屏幕高度)
非黑屏区域宽度: 2121
非黑屏区域高度: 1440
首张卡片大小: (116, 162)
卡顶: 19

非全屏模式下:
带标题栏大小: (886, 629)
无标题栏大小: (880, 600)
卡顶: 8
卡大小: (50, 70)
---

PVZ 杂交版 是固定长宽比游戏, 长宽比为 22:15
 */