#ifndef _IMAGES_HPP_
#define _IMAGES_HPP_

#include <Windows.h>
#include <vector>
#include <array>
#include <string>
#include <stdint.h>

/// @brief 80x80 RGB888 画像バッファ
typedef std::array<std::array<std::array<uint8_t, 3>, 80>, 80> rgb80x80;


/// @brief アプリケーションアイコン画像を保持する
class IconImage{
public:
    static constexpr size_t TRANSPORT_BYTES = 80u * 80u * 2u;

    IconImage(const std::wstring& path);
    const rgb80x80& getBinaryImage() const;
    const std::vector<uint8_t>& getTransportBinary() const;
private:
    rgb80x80 m_binaryImage;
    std::vector<uint8_t> m_transportBinary;
};


/// @brief 数値表示画像を保持する
class NumberImage{
public:
    NumberImage(uint8_t vol, bool isMuted);
    rgb80x80& getBinaryImage();
private:
    rgb80x80 m_binaryImage;
};


/// @brief アイコン画像と数値画像を結合した表示データを保持する
class DisplayingImage{
public:
    DisplayingImage(IconImage& icon, NumberImage& num);
    std::vector<uint8_t> &getBinary();
private:
    std::vector<uint8_t> m_binary;
};

#endif
