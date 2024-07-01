#pragma once
#define NOMINMAX 

#include <stdint.h>

struct HSVPix {
    float h; // Hue, 范围 [0, 360)
    float s; // Saturation, 范围 [0, 1]
    float v; // Value, 范围 [0, 1]

    HSVPix() : h(0), s(0), v(0) {}
    HSVPix(float h, float s, float v) : h(h), s(s), v(v) {}

    bool operator==(const HSVPix& other) const {
        return h == other.h && s == other.s && v == other.v;
    }

    uint8_t find_max() const {
        return std::max({h, s, v});
    }
};

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

    uint8_t find_max() const {
        return std::max({r, g, b});
    }

    HSVPix to_hsv() const {
        HSVPix hsv;
        float r_f = r / 255.0f;
        float g_f = g / 255.0f;
        float b_f = b / 255.0f;
        
        float c_max = std::max({r_f, g_f, b_f});
        float c_min = std::min({r_f, g_f, b_f});
        float diff = c_max - c_min;

        // Calculate V
        hsv.v = c_max;

        // Calculate S
        hsv.s = (c_max == 0) ? 0 : (diff / c_max);

        // Calculate H
        if (c_max == c_min) {
            hsv.h = 0; // Undefined, set to 0
        } else if (c_max == r_f) {
            hsv.h = 60 * ((g_f - b_f) / diff);
        } else if (c_max == g_f) {
            hsv.h = 60 * (2 + (b_f - r_f) / diff);
        } else { // c_max == b_f
            hsv.h = 60 * (4 + (r_f - g_f) / diff);
        }

        if (hsv.h < 0) {
            hsv.h += 360;
        }

        return hsv;
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
        for (int y_ = 0; y_ < h; y_++) {
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

    int less_50;
    int more_200;
    int sun_count;

    Card() {
        raw_img = nullptr;
        start_x = start_y = width = height = 0;
    }

    Card(ImageType *raw_img, int start_x, int start_y, int width, int height) {
        this->raw_img = raw_img;
        this->start_x = start_x;
        this->start_y = start_y;
        this->width = width;
        this->height = height;

        less_50 = more_200 = sun_count = 0;
    }

    bool enable() {
        return 1.0 * more_200 / less_50 > 1;
    }

    bool count() {
        less_50 = more_200 = sun_count = 0;
        for (int i = 0; i < height; i++) {
            for (int j = 0; j < width; j++) {
                // for (int c = 0; c < 3; c++) {
                //     uint8_t p = raw_img->data[
                //         (start_x + j + (start_y + i) * raw_img->cols) * 4 + c
                //     ];
                //     if (p < 50) {
                //         less_50++;
                //     } else if (p > 200) {
                //         more_200++;
                //     }

                // }
                auto rgb = raw_img->operator()(start_x + j, start_y + i);
                auto hsv = rgb.to_hsv();

                if (hsv.h >= 53 && hsv.h <= 60 && hsv.v > 0.7) {
                    sun_count++;
                    continue;
                }
                auto max = rgb.find_max();
                if (max < 100) {
                    less_50++;
                } else if (max > 200) {
                    more_200++;
                }
            }
        }
        return enable();
    }

};
struct PVZSize {
    int width;
    int height;
    int card_width;
    int card_height;
    int card_top;
};

PVZSize pvz_size = {
    880, 600, 50, 70, 8
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