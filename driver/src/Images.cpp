#include "Images.hpp"

#include <algorithm>
#include <string>
#include <cwctype>

#include <shellapi.h>

namespace {

std::wstring trimPathToken(const std::wstring& src) {
	if (src.empty()) {
		return src;
	}

	size_t begin = 0;
	size_t end = src.size();

	while (begin < end && iswspace(src[begin]) != 0) {
		++begin;
	}
	while (end > begin && iswspace(src[end - 1]) != 0) {
		--end;
	}

	if (end > begin && src[begin] == L'"' && src[end - 1] == L'"') {
		++begin;
		--end;
	}

	return src.substr(begin, end - begin);
}

std::wstring parseExecutablePath(const wchar_t* iconPathRaw) {
	if (iconPathRaw == nullptr) {
		return L"";
	}

	std::wstring raw(iconPathRaw);
	const size_t commaPos = raw.find(L',');
	if (commaPos != std::wstring::npos) {
		raw = raw.substr(0, commaPos);
	}
	return trimPathToken(raw);
}

void fillBlack(rgb80x80& image) {
	for (size_t y = 0; y < image.size(); ++y) {
		for (size_t x = 0; x < image[y].size(); ++x) {
			image[y][x][0] = 0;
			image[y][x][1] = 0;
			image[y][x][2] = 0;
		}
	}
}

HICON loadIconFromPath(const std::wstring& path) {
	if (path.empty()) {
		return nullptr;
	}

	HICON largeIcon = nullptr;
	HICON smallIcon = nullptr;
	const UINT extracted = ExtractIconExW(path.c_str(), 0, &largeIcon, &smallIcon, 1);
	if (extracted > 0) {
		if (largeIcon != nullptr) {
			if (smallIcon != nullptr) {
				DestroyIcon(smallIcon);
			}
			return largeIcon;
		}
		return smallIcon;
	}

	SHFILEINFOW fileInfo{};
	if (SHGetFileInfoW(path.c_str(), FILE_ATTRIBUTE_NORMAL, &fileInfo, sizeof(fileInfo), SHGFI_ICON | SHGFI_LARGEICON | SHGFI_USEFILEATTRIBUTES) != 0) {
		return fileInfo.hIcon;
	}

	return nullptr;
}

bool renderIconToRgb80x80(HICON icon, rgb80x80& outImage) {
	if (icon == nullptr) {
		return false;
	}

	HDC screenDc = GetDC(nullptr);
	if (screenDc == nullptr) {
		return false;
	}

	HDC memDc = CreateCompatibleDC(screenDc);
	if (memDc == nullptr) {
		ReleaseDC(nullptr, screenDc);
		return false;
	}

	BITMAPINFO bmi{};
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = 80;
	bmi.bmiHeader.biHeight = -80; // top-down
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;

	void* pixels = nullptr;
	HBITMAP dib = CreateDIBSection(memDc, &bmi, DIB_RGB_COLORS, &pixels, nullptr, 0);
	if (dib == nullptr || pixels == nullptr) {
		if (dib != nullptr) {
			DeleteObject(dib);
		}
		DeleteDC(memDc);
		ReleaseDC(nullptr, screenDc);
		return false;
	}

	HGDIOBJ oldObj = SelectObject(memDc, dib);
	RECT rect{0, 0, 80, 80};
	FillRect(memDc, &rect, reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));

	if (!DrawIconEx(memDc, 0, 0, icon, 80, 80, 0, nullptr, DI_NORMAL)) {
		SelectObject(memDc, oldObj);
		DeleteObject(dib);
		DeleteDC(memDc);
		ReleaseDC(nullptr, screenDc);
		return false;
	}

	const uint8_t* bgra = static_cast<const uint8_t*>(pixels);
	for (int y = 0; y < 80; ++y) {
		for (int x = 0; x < 80; ++x) {
			const size_t idx = static_cast<size_t>((y * 80 + x) * 4);
			outImage[y][x][0] = bgra[idx + 2];
			outImage[y][x][1] = bgra[idx + 1];
			outImage[y][x][2] = bgra[idx + 0];
		}
	}

	SelectObject(memDc, oldObj);
	DeleteObject(dib);
	DeleteDC(memDc);
	ReleaseDC(nullptr, screenDc);
	return true;
}

uint16_t rgb888ToRgb565(uint8_t r, uint8_t g, uint8_t b) {
	const uint16_t r5 = static_cast<uint16_t>(r >> 3);
	const uint16_t g6 = static_cast<uint16_t>(g >> 2);
	const uint16_t b5 = static_cast<uint16_t>(b >> 3);
	return static_cast<uint16_t>((r5 << 11) | (g6 << 5) | b5);
}

void writeRgb565Le(const rgb80x80& src, std::vector<uint8_t>& dst, size_t startPixelIndex) {
	for (size_t y = 0; y < src.size(); ++y) {
		for (size_t x = 0; x < src[y].size(); ++x) {
			const uint16_t pixel565 = rgb888ToRgb565(src[y][x][0], src[y][x][1], src[y][x][2]);
			const size_t pixelIndex = startPixelIndex + y * 80 + x;
			const size_t byteIndex = pixelIndex * 2;
			dst[byteIndex] = static_cast<uint8_t>(pixel565 & 0xFF);
			dst[byteIndex + 1] = static_cast<uint8_t>((pixel565 >> 8) & 0xFF);
		}
	}
}

} // namespace

IconImage::IconImage(LPWSTR path) {
	fillBlack(m_binaryImage);

	const std::wstring iconFile = parseExecutablePath(path);
	HICON icon = loadIconFromPath(iconFile);
	if (icon == nullptr) {
		return;
	}

	renderIconToRgb80x80(icon, m_binaryImage);
	DestroyIcon(icon);
}

rgb80x80& IconImage::getBinaryImage() {
	return m_binaryImage;
}

NumberImage::NumberImage(uint8_t vol, bool isMuted) {
	(void)vol;
	(void)isMuted;
	fillBlack(m_binaryImage);
}

rgb80x80& NumberImage::getBinaryImage() {
	return m_binaryImage;
}

DisplayingImage::DisplayingImage(IconImage& icon, NumberImage& num) {
	m_binary.resize(80u * 160u * 2u);

	const rgb80x80& iconRgb = icon.getBinaryImage();
	const rgb80x80& numRgb = num.getBinaryImage();

	// Top half: icon (80x80), bottom half: number (80x80)
	writeRgb565Le(iconRgb, m_binary, 0);
	writeRgb565Le(numRgb, m_binary, 80u * 80u);
}

std::vector<uint8_t>& DisplayingImage::getBinary() {
	return m_binary;
}

