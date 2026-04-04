#ifndef _IMAGES_HPP_
#define _IMAGES_HPP_

#include <Windows.h>
#include <vector>
#include <array>
#include <stdint.h>

typedef std::array<std::array<std::array<uint8_t, 3>, 80>, 80> rgb80x80;


class IconImage{
public:
    IconImage(LPWSTR path);
    rgb80x80& getBinaryImage();
private:
    rgb80x80 m_binaryImage;
};


class NumberImage{
public:
    NumberImage(uint8_t vol, bool isMuted);
    rgb80x80& getBinaryImage();
private:
    rgb80x80 m_binaryImage;
};


class DisplayingImage{
public:
    DisplayingImage(IconImage& icon, NumberImage& num);
    std::vector<uint8_t> &getBinary();
private:
    std::vector<uint8_t> m_binary;
};

#endif