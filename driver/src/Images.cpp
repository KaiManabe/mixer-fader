#include "Images.hpp"

#include <algorithm>
#include <string>
#include <cwctype>

#include <shellapi.h>

namespace {

/// @brief パス文字列の前後にある空白や引用符を除去する
/// @param src 加工対象の文字列
/// @return 整形後の文字列
std::wstring trimPathToken(const std::wstring& src) {
	// --------------------- 空文字列を判定 ---------------------
	if (src.empty()) {
		return src;
	}

	size_t begin = 0;
	size_t end = src.size();

	// --------------------- 前後空白を除去 ---------------------
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

	// --------------------- 切り出して返す ---------------------
	return src.substr(begin, end - begin);
}

/// @brief 画像全体を黒で塗りつぶす
/// @param image 塗りつぶし対象
void fillBlack(rgb80x80& image) {
	// ---------------------- 全画素を初期化 ----------------------
	for (size_t y = 0; y < image.size(); ++y) {
		for (size_t x = 0; x < image[y].size(); ++x) {
			image[y][x][0] = 0;
			image[y][x][1] = 0;
			image[y][x][2] = 0;
		}
	}
}

/// @brief 実行ファイルパスからアイコンを取得する
/// @param path 実行ファイルパス
/// @return 読み込んだアイコンハンドル
HICON loadIconFromPath(const std::wstring& path) {
	// --------------------- 空パスを判定 ---------------------
	if (path.empty()) {
		return nullptr;
	}

	SHFILEINFOW fileInfo{};
	if (SHGetFileInfoW(path.c_str(), 0, &fileInfo, sizeof(fileInfo), SHGFI_ICON | SHGFI_LARGEICON) != 0) {
		return fileInfo.hIcon;
	}

	// -------------------- 取得失敗時のみ代替取得 --------------------
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

	return nullptr;
}

/// @brief アイコンを 80x80 RGB 画像へ描画する
/// @param icon 描画元アイコン
/// @param outImage 描画先バッファ
/// @return 描画に成功したら true
bool renderIconToRgb80x80(HICON icon, rgb80x80& outImage) {
	// ------------------ アイコンハンドルを検証 ------------------
	if (icon == nullptr) {
		return false;
	}

	// -------------------- 描画コンテキストを確保 --------------------
	HDC screenDc = GetDC(nullptr);
	if (screenDc == nullptr) {
		return false;
	}

	HDC memDc = CreateCompatibleDC(screenDc);
	if (memDc == nullptr) {
		ReleaseDC(nullptr, screenDc);
		return false;
	}

	// -------------------- 描画用 DIB を作成 --------------------
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

	// -------------------- アイコンを描画 --------------------
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

	// -------------------- RGB 配列へ変換 --------------------
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

/// @brief RGB888 を RGB565 に変換する
/// @param r 赤成分
/// @param g 緑成分
/// @param b 青成分
/// @return RGB565 の 16bit 値
uint16_t rgb888ToRgb565(uint8_t r, uint8_t g, uint8_t b) {
	const uint16_t r5 = static_cast<uint16_t>(r >> 3);
	const uint16_t g6 = static_cast<uint16_t>(g >> 2);
	const uint16_t b5 = static_cast<uint16_t>(b >> 3);
	return static_cast<uint16_t>((r5 << 11) | (g6 << 5) | b5);
}

/// @brief RGB 画像を RGB565 little endian で書き込む
/// @param src 変換元画像
/// @param dst 書き込み先バッファ
/// @param startPixelIndex 書き込み開始ピクセル位置
void writeRgb565Le(const rgb80x80& src, std::vector<uint8_t>& dst, size_t startPixelIndex) {
	// ----------------- 画像全体を順に書き込む -----------------
	for (size_t y = 0; y < src.size(); ++y) {
		for (size_t x = 0; x < src[y].size(); ++x) {
			const size_t flippedY = src.size() - 1 - y;
			const size_t flippedX = src[y].size() - 1 - x;
			const uint16_t pixel565 = rgb888ToRgb565(
				src[flippedY][flippedX][0],
				src[flippedY][flippedX][1],
				src[flippedY][flippedX][2]);
			const size_t pixelIndex = startPixelIndex + y * 80 + x;
			const size_t byteIndex = pixelIndex * 2;
			dst[byteIndex] = static_cast<uint8_t>(pixel565 & 0xFF);
			dst[byteIndex + 1] = static_cast<uint8_t>((pixel565 >> 8) & 0xFF);
		}
	}
}

} // namespace

/// @brief 実行ファイルパスからアイコン画像を読み込む
/// @param path 実行ファイルパス
IconImage::IconImage(const std::wstring& path) {
	// -------------------- 初期状態を黒で埋める --------------------
	fillBlack(m_binaryImage);

	// ------------------- パスからアイコンを取得 -------------------
	const std::wstring iconFile = trimPathToken(path);
	HICON icon = loadIconFromPath(iconFile);
	if (icon == nullptr) {
		return;
	}

	// -------------------- 画像へ描画して解放 --------------------
	if (!renderIconToRgb80x80(icon, m_binaryImage)) {
		DestroyIcon(icon);
		return;
	}
	DestroyIcon(icon);
}

/// @brief アイコン画像の RGB バッファを返す
/// @return 80x80 RGB 画像
rgb80x80& IconImage::getBinaryImage() {
	return m_binaryImage;
}

/// @brief 数値画像を初期化する
/// @param vol 音量値
/// @param isMuted ミュート状態
NumberImage::NumberImage(uint8_t vol, bool isMuted) {
	// ---------------- 未使用引数の警告を抑制 ----------------
	(void)vol;
	(void)isMuted;

	// -------------------- 現状は黒で初期化 --------------------
	fillBlack(m_binaryImage);
}

/// @brief 数値画像の RGB バッファを返す
/// @return 80x80 RGB 画像
rgb80x80& NumberImage::getBinaryImage() {
	return m_binaryImage;
}

/// @brief アイコン画像と数値画像を縦に結合する
/// @param icon 上段に表示するアイコン画像
/// @param num 下段に表示する数値画像
DisplayingImage::DisplayingImage(IconImage& icon, NumberImage& num) {
	// ------------------ 出力バッファを確保 ------------------
	m_binary.resize(80u * 160u * 2u);

	const rgb80x80& iconRgb = icon.getBinaryImage();
	const rgb80x80& numRgb = num.getBinaryImage();

	// -------------------- 上下 2 枚を書き込む --------------------
	writeRgb565Le(iconRgb, m_binary, 0);
	writeRgb565Le(numRgb, m_binary, 80u * 80u);
}

/// @brief 結合済みの表示バッファを返す
/// @return RGB565 の表示データ
std::vector<uint8_t>& DisplayingImage::getBinary() {
	return m_binary;
}
