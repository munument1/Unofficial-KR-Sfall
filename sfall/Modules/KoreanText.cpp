/*
 *    sfall
 *    Korean CP949 text renderer experiment
 */

#include "..\FalloutEngine\Fallout2.h"
#include "..\IniReader.h"
#include "..\main.h"
#include "..\SafeWrite.h"

#include "KoreanText.h"

namespace sfall
{

static bool enabled = false;
static long buttonFontThreshold = 16;
static long activePath = 0;
static const long RenderPathGNW = 0;
static const long RenderPathFM = 1;

struct FontProfile {
	long tag;
	long fontHeight;
	long cellWidth;
	long renderHeight;
	long yOffset;
	long fontWeight;
	long extraBoldRadius;
	wchar_t fontFace[LF_FACESIZE];
};

static FontProfile textProfile = {1, 11, 11, 14, 0, FW_NORMAL, 0, L"Dotum"};
static FontProfile fmButtonProfile = {2, 19, 19, 28, 0, 900, 0, L"Dotum"};
static FontProfile gnwButtonProfile = {3, 16, 16, 20, 0, 700, 0, L"Dotum"};
static FontProfile* activeProfile = &textProfile;

struct GlyphBitmap {
	long width;
	long height;
	std::vector<BYTE> mask;
};

static std::unordered_map<unsigned long long, GlyphBitmap> glyphCache;
static HFONT hFont = nullptr;
static long hFontSize = 0;
static long hFontTag = 0;
static std::vector<std::wstring> loadedFontFiles;

static bool IsCp949Lead(unsigned char ch) {
	if (ch == 149) return false; // Fallout UI console bullet.
	return (ch >= 0x81 && ch <= 0xFE);
}

static bool IsCp949Trail(unsigned char ch) {
	return ((ch >= 0x41 && ch <= 0x5A) || (ch >= 0x61 && ch <= 0x7A) || (ch >= 0x81 && ch <= 0xFE));
}

static bool DecodeCp949(const char* text, wchar_t& outChar) {
	unsigned char bytes[2] = {
		static_cast<unsigned char>(text[0]),
		static_cast<unsigned char>(text[1])
	};

	if (!IsCp949Lead(bytes[0]) || !IsCp949Trail(bytes[1])) return false;

	wchar_t wide[2] = {};
	int count = MultiByteToWideChar(949, MB_ERR_INVALID_CHARS, reinterpret_cast<const char*>(bytes), 2, wide, 2);
	if (count != 1) return false;

	outChar = wide[0];
	return (outChar >= 0xAC00 && outChar <= 0xD7A3);
}

static fo::FontData* CurrentFont() {
	return reinterpret_cast<fo::FontData*>(fo::var::getInt(FO_VAR_gCurrentFont));
}

static long FontHeight() {
	fo::FontData* font = CurrentFont();
	return font ? font->field0 : 16;
}

static FontProfile& ActiveProfile() {
	if (FontHeight() < buttonFontThreshold) {
		activeProfile = &textProfile;
	} else {
		activeProfile = (activePath == RenderPathFM) ? &fmButtonProfile : &gnwButtonProfile;
	}
	return *activeProfile;
}

static long KoreanFontHeight() {
	return ActiveProfile().fontHeight;
}

static long KoreanCellWidth(long pixelHeight) {
	FontProfile& profile = ActiveProfile();
	return (profile.cellWidth > 0) ? profile.cellWidth : pixelHeight;
}

static long KoreanRenderHeight(long pixelHeight) {
	FontProfile& profile = ActiveProfile();
	return (profile.renderHeight > 0) ? profile.renderHeight : pixelHeight;
}

static long CharSpacing() {
	__asm {
		call dword ptr ds:[FO_VAR_text_spacing];
	}
}

static long AsciiCharWidth(unsigned char ch) {
	fo::FontData* font = CurrentFont();
	if (!font) return 0;

	long width;
	if (ch == ' ') {
		width = *reinterpret_cast<long*>(&font->field2);
	} else {
		width = *(reinterpret_cast<long*>(&font->fieldA) + (2 * ch));
	}
	return (width >> 16);
}

static void EnsureGdiFont(long pixelHeight) {
	FontProfile& profile = ActiveProfile();
	if (hFont && hFontSize == pixelHeight && hFontTag == profile.tag) return;

	if (hFont) {
		DeleteObject(hFont);
		hFont = nullptr;
	}

	hFontSize = pixelHeight;
	hFontTag = profile.tag;
	long gdiWeight = profile.fontWeight;
	if (gdiWeight > 1000) gdiWeight = FW_HEAVY;
	hFont = CreateFontW(
		-pixelHeight, 0, 0, 0, gdiWeight, FALSE, FALSE, FALSE,
		HANGEUL_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
		NONANTIALIASED_QUALITY, FIXED_PITCH | FF_DONTCARE, profile.fontFace
	);
}

static GlyphBitmap& GetKoreanGlyph(wchar_t ch, long pixelHeight) {
	FontProfile& profile = ActiveProfile();
	unsigned long long key = (static_cast<unsigned long long>(profile.tag) << 32) | static_cast<DWORD>(ch);
	auto it = glyphCache.find(key);
	if (it != glyphCache.end()) return it->second;

	long cellWidth = KoreanCellWidth(pixelHeight);
	long cellHeight = KoreanRenderHeight(pixelHeight);
	if (cellWidth <= 0) cellWidth = 1;
	if (cellHeight <= 0) cellHeight = 1;

	// 2x Supersampling Scale Factor
	const long scale = 2;

	EnsureGdiFont(pixelHeight * scale);

	HDC dc = CreateCompatibleDC(nullptr);
	HGDIOBJ oldFont = SelectObject(dc, hFont);

	SIZE size = {};
	GetTextExtentPoint32W(dc, &ch, 1, &size);
	TEXTMETRICW tm = {};
	GetTextMetricsW(dc, &tm);

	if (size.cx <= 0) size.cx = cellWidth * scale;
	if (size.cy <= 0) size.cy = cellHeight * scale;

	long targetBitmapHeight = cellHeight;
	if (targetBitmapHeight < (tm.tmHeight / scale) + 2) {
		targetBitmapHeight = (tm.tmHeight / scale) + 2;
	}

	long hiWidth = cellWidth * scale;
	long hiHeight = targetBitmapHeight * scale;

	BITMAPINFO bmi = {};
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = hiWidth;
	bmi.bmiHeader.biHeight = -hiHeight;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;

	void* bits = nullptr;
	HBITMAP dib = CreateDIBSection(dc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
	HGDIOBJ oldBitmap = SelectObject(dc, dib);

	RECT rect = {0, 0, hiWidth, hiHeight};
	FillRect(dc, &rect, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
	SetBkMode(dc, OPAQUE);
	SetBkColor(dc, RGB(0, 0, 0));
	SetTextColor(dc, RGB(255, 255, 255));

	long x = (hiWidth - size.cx) / 2;
	long y = (hiHeight - tm.tmHeight) / 2;
	if (x < 0) x = 0;
	if (y < 0) y = 0;
	TextOutW(dc, x, y, &ch, 1);

	GlyphBitmap glyph;
	glyph.width = cellWidth;
	glyph.height = targetBitmapHeight;
	glyph.mask.resize(cellWidth * targetBitmapHeight);

	const BYTE* src = static_cast<const BYTE*>(bits);
	for (long ty = 0; ty < targetBitmapHeight; ty++) {
		for (long tx = 0; tx < cellWidth; tx++) {
			long sumGray = 0;
			for (long sy = 0; sy < scale; sy++) {
				for (long sx = 0; sx < scale; sx++) {
					long hix = tx * scale + sx;
					long hiy = ty * scale + sy;
					const BYTE* px = src + ((hiy * hiWidth + hix) * 4);
					long gray = (static_cast<long>(px[0]) + px[1] + px[2]) / 3;
					sumGray += gray;
				}
			}
			long avgGray = sumGray / (scale * scale);
			// Map to 0..9 range for Fallout 2's alpha blend table
			long mask = (avgGray * 9 + 127) / 255;
			glyph.mask[ty * cellWidth + tx] = static_cast<BYTE>(mask);
		}
	}

	if (profile.extraBoldRadius > 0) {
		std::vector<BYTE> thickMask = glyph.mask;
		for (long y = 0; y < targetBitmapHeight; y++) {
			for (long x = 0; x < cellWidth; x++) {
				if (!glyph.mask[y * cellWidth + x]) continue;
				for (long dy = -profile.extraBoldRadius; dy <= profile.extraBoldRadius; dy++) {
					long yy = y + dy;
					if (yy < 0 || yy >= targetBitmapHeight) continue;
					for (long dx = -profile.extraBoldRadius; dx <= profile.extraBoldRadius; dx++) {
						long xx = x + dx;
						if (xx < 0 || xx >= cellWidth) continue;
						thickMask[yy * cellWidth + xx] = 9;
					}
				}
			}
		}
		glyph.mask.swap(thickMask);
	}

	SelectObject(dc, oldBitmap);
	DeleteObject(dib);
	SelectObject(dc, oldFont);
	DeleteDC(dc);

	auto inserted = glyphCache.emplace(key, std::move(glyph));
	return inserted.first->second;
}

static long KoreanCharWidth(wchar_t ch) {
	GlyphBitmap& glyph = GetKoreanGlyph(ch, KoreanFontHeight());
	return glyph.width;
}

static long __fastcall TextWidthHookImpl(const char* text) {
	long width = 0;
	long spacing = CharSpacing();

	while (*text) {
		wchar_t korean;
		if (DecodeCp949(text, korean)) {
			width += KoreanCharWidth(korean) + spacing;
			text += 2;
		} else {
			width += AsciiCharWidth(static_cast<unsigned char>(*text)) + spacing;
			text++;
		}
	}

	return width;
}

static void DrawAsciiChar(BYTE*& dst, unsigned char ch, long pitch, fo::BlendColorTableData* blendTable) {
	fo::FontData* font = CurrentFont();
	if (!font) return;

	long width = AsciiCharWidth(ch);
	if (ch != ' ' && width > 0) {
		long height = (*(reinterpret_cast<long*>(&font->fieldC) + (2 * ch)) >> 16);
		BYTE* dstPixels = &dst[pitch * (font->field0 - height)];
		BYTE* fontPixels = reinterpret_cast<BYTE*>(*(reinterpret_cast<long*>(&font->eUnkArray[8 * ch + 2])) + font->field80C);

		for (long y = 0; y < height; y++) {
			for (long x = 0; x < width; x++) {
				*dstPixels = blendTable->data[*fontPixels].colors[*dstPixels];
				dstPixels++;
				fontPixels++;
			}
			dstPixels += pitch - width;
		}
	}

	dst += width + CharSpacing();
}

static void DrawKoreanChar(BYTE*& dst, wchar_t ch, long pitch, fo::BlendColorTableData* blendTable) {
	GlyphBitmap& glyph = GetKoreanGlyph(ch, KoreanFontHeight());
	FontProfile& profile = ActiveProfile();
	long fontHeight = FontHeight();
	long top = 0;
	long srcY = 0;
	long drawHeight = glyph.height;

	if (glyph.height > fontHeight) {
		srcY = ((glyph.height - fontHeight) / 2) + profile.yOffset;
		if (srcY < 0) srcY = 0;
		if (srcY >= glyph.height) srcY = glyph.height - 1;
		drawHeight = fontHeight;
		if (srcY + drawHeight > glyph.height) drawHeight = glyph.height - srcY;
	} else {
		top = ((fontHeight - glyph.height) / 2) + profile.yOffset;
		if (top < 0) {
			srcY = -top;
			drawHeight -= srcY;
			top = 0;
		}
	}
	if (drawHeight <= 0) {
		dst += glyph.width + CharSpacing();
		return;
	}
	long maxDrawHeight = fontHeight - top;
	if (maxDrawHeight < drawHeight) drawHeight = maxDrawHeight;
	if (drawHeight <= 0) {
		dst += glyph.width + CharSpacing();
		return;
	}

	BYTE* dstPixels = &dst[pitch * top];
	for (long y = srcY; y < srcY + drawHeight; y++) {
		for (long x = 0; x < glyph.width; x++) {
			BYTE mask = glyph.mask[y * glyph.width + x];
			if (mask) {
				dstPixels[x] = blendTable->data[mask].colors[dstPixels[x]];
			}
		}
		dstPixels += pitch;
	}

	dst += glyph.width + CharSpacing();
}

static void __stdcall TextToBufHookImpl(BYTE* dst, const char* text, long maxWidth, long pitch, long colorFlags) {
	if (!text || !*text) return;

	fo::BlendColorTableData* blendTable = fo::func::getColorBlendTable(colorFlags);
	BYTE* cursor = dst;

	while (*text) {
		if ((cursor - dst) > maxWidth) break;

		wchar_t korean;
		if (DecodeCp949(text, korean)) {
			DrawKoreanChar(cursor, korean, pitch, blendTable);
			text += 2;
		} else {
			DrawAsciiChar(cursor, static_cast<unsigned char>(*text), pitch, blendTable);
			text++;
		}
	}

	fo::func::freeColorBlendTable(colorFlags);
}

static __declspec(naked) void fm_text_width_hook() {
	__asm {
		push ecx;
		push edx;
		mov  dword ptr ds:[activePath], 1;
		mov  ecx, eax;
		call TextWidthHookImpl;
		pop  edx;
		pop  ecx;
		retn;
	}
}

static __declspec(naked) void gnw_text_width_hook() {
	__asm {
		push ecx;
		push edx;
		mov  dword ptr ds:[activePath], 0;
		mov  ecx, eax;
		call TextWidthHookImpl;
		pop  edx;
		pop  ecx;
		retn;
	}
}

static __declspec(naked) void fm_text_to_buf_hook() {
	__asm {
		mov  dword ptr ds:[activePath], 1;
		push dword ptr [esp + 4]; // color flags
		push ecx;                 // surface pitch
		push ebx;                 // max text width
		push edx;                 // text
		push eax;                 // destination surface
		call TextToBufHookImpl;
		retn 4;
	}
}

static __declspec(naked) void gnw_text_to_buf_hook() {
	__asm {
		mov  dword ptr ds:[activePath], 0;
		push dword ptr [esp + 4]; // color flags
		push ecx;                 // surface pitch
		push ebx;                 // max text width
		push edx;                 // text
		push eax;                 // destination surface
		call TextToBufHookImpl;
		retn 4;
	}
}

static void LoadFace(const char* key, const char* defaultFace, wchar_t* dest) {
	auto face = IniReader::GetConfigString("Main", key, defaultFace);
	std::memset(dest, 0, sizeof(wchar_t) * LF_FACESIZE);
	MultiByteToWideChar(CP_ACP, 0, face.c_str(), -1, dest, LF_FACESIZE);
	dest[LF_FACESIZE - 1] = 0;
}

static void LoadPrivateFontFile(const char* key) {
	auto path = IniReader::GetConfigString("Main", key, "");
	if (path.empty()) return;

	std::wstring widePath(MAX_PATH, L'\0');
	int len = MultiByteToWideChar(CP_ACP, 0, path.c_str(), -1, &widePath[0], static_cast<int>(widePath.size()));
	if (len <= 0) return;
	widePath.resize(len - 1);

	if (AddFontResourceExW(widePath.c_str(), FR_PRIVATE, nullptr) > 0) {
		loadedFontFiles.push_back(widePath);
	}
}

void KoreanText::init() {
	enabled = IniReader::GetConfigInt("Main", "KoreanText", 0) != 0;
	if (!enabled || !nonEngLang) return;

	buttonFontThreshold = IniReader::GetConfigInt("Main", "KoreanTextButtonThreshold", 16);

	textProfile.fontHeight = IniReader::GetConfigInt("Main", "KoreanTextTextFontHeight", 11);
	textProfile.cellWidth = IniReader::GetConfigInt("Main", "KoreanTextTextCellWidth", 11);
	textProfile.renderHeight = IniReader::GetConfigInt("Main", "KoreanTextTextRenderHeight", 14);
	textProfile.yOffset = IniReader::GetConfigInt("Main", "KoreanTextTextYOffset", 0);
	textProfile.fontWeight = IniReader::GetConfigInt("Main", "KoreanTextTextFontWeight", FW_NORMAL);
	textProfile.extraBoldRadius = IniReader::GetConfigInt("Main", "KoreanTextTextExtraBold", 0);
	LoadPrivateFontFile("KoreanTextTextFontFile");
	LoadFace("KoreanTextTextFontFace", "Dotum", textProfile.fontFace);

	fmButtonProfile.fontHeight = IniReader::GetConfigInt("Main", "KoreanTextFontHeight", 19);
	fmButtonProfile.cellWidth = IniReader::GetConfigInt("Main", "KoreanTextCellWidth", 19);
	fmButtonProfile.renderHeight = IniReader::GetConfigInt("Main", "KoreanTextRenderHeight", 28);
	fmButtonProfile.yOffset = IniReader::GetConfigInt("Main", "KoreanTextYOffset", 0);
	fmButtonProfile.fontWeight = IniReader::GetConfigInt("Main", "KoreanTextFontWeight", IniReader::GetConfigInt("Main", "KoreanTextBold", 0) ? FW_BOLD : 900);
	fmButtonProfile.extraBoldRadius = IniReader::GetConfigInt("Main", "KoreanTextExtraBold", (fmButtonProfile.fontWeight > 1000) ? 1 : 0);
	LoadPrivateFontFile("KoreanTextFontFile");
	LoadFace("KoreanTextFontFace", "Dotum", fmButtonProfile.fontFace);

	gnwButtonProfile.fontHeight = IniReader::GetConfigInt("Main", "KoreanTextGnwFontHeight", 16);
	gnwButtonProfile.cellWidth = IniReader::GetConfigInt("Main", "KoreanTextGnwCellWidth", 16);
	gnwButtonProfile.renderHeight = IniReader::GetConfigInt("Main", "KoreanTextGnwRenderHeight", 20);
	gnwButtonProfile.yOffset = IniReader::GetConfigInt("Main", "KoreanTextGnwYOffset", 0);
	gnwButtonProfile.fontWeight = IniReader::GetConfigInt("Main", "KoreanTextGnwFontWeight", 700);
	gnwButtonProfile.extraBoldRadius = IniReader::GetConfigInt("Main", "KoreanTextGnwExtraBold", 0);
	LoadPrivateFontFile("KoreanTextGnwFontFile");
	LoadFace("KoreanTextGnwFontFace", "Dotum", gnwButtonProfile.fontFace);

	MakeJump(fo::funcoffs::FMtext_width_, fm_text_width_hook);
	MakeJump(fo::funcoffs::GNW_text_width_, gnw_text_width_hook);
	MakeJump(fo::funcoffs::FMtext_to_buf_, fm_text_to_buf_hook);
	MakeJump(fo::funcoffs::GNW_text_to_buf_, gnw_text_to_buf_hook);
}

void KoreanText::exit() {
	glyphCache.clear();
	if (hFont) {
		DeleteObject(hFont);
		hFont = nullptr;
	}
	for (const auto& fontFile : loadedFontFiles) {
		RemoveFontResourceExW(fontFile.c_str(), FR_PRIVATE, nullptr);
	}
	loadedFontFiles.clear();
}

}
