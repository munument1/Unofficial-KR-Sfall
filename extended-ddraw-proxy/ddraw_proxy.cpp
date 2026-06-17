#include <windows.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <fstream>

static char g_moduleDir[MAX_PATH] = {};
static char g_gameDir[MAX_PATH] = {};
static char g_iniPath[MAX_PATH] = {};

static void WriteLog(const std::string& msg) {
	char logPath[MAX_PATH];
	strcpy_s(logPath, g_gameDir[0] ? g_gameDir : g_moduleDir);
	strcat_s(logPath, "\\proxy_log.txt");
	std::ofstream log(logPath, std::ios::app);
	if (log.is_open()) {
		log << msg << "\n";
	}
}

static HMODULE g_realDdraw = nullptr;

static FARPROC pAcquireDDThreadLock = nullptr;
static FARPROC pCheckFullscreen = nullptr;
static FARPROC pCompleteCreateSysmemSurface = nullptr;
static FARPROC pD3DParseUnknownCommand = nullptr;
static FARPROC pDDGetAttachedSurfaceLcl = nullptr;
static FARPROC pDDInternalLock = nullptr;
static FARPROC pDDInternalUnlock = nullptr;
static FARPROC pDSoundHelp = nullptr;
static FARPROC pDirectDrawCreate = nullptr;
static FARPROC pDirectDrawCreate2 = nullptr;
static FARPROC pDirectDrawCreateClipper = nullptr;
static FARPROC pDirectDrawCreateEx = nullptr;
static FARPROC pDirectDrawEnumerateA = nullptr;
static FARPROC pDirectDrawEnumerateExA = nullptr;
static FARPROC pDirectDrawEnumerateExW = nullptr;
static FARPROC pDirectDrawEnumerateW = nullptr;
static FARPROC pDirectInputCreateA = nullptr;
static FARPROC pDllCanUnloadNow = nullptr;
static FARPROC pDllGetClassObject = nullptr;
static FARPROC pGetDDSurfaceLocal = nullptr;
static FARPROC pGetOLEThunkData = nullptr;
static FARPROC pGetSurfaceFromDC = nullptr;
static FARPROC pRegisterSpecialCase = nullptr;
static FARPROC pReleaseDDThreadLock = nullptr;
static FARPROC pSetAppCompatData = nullptr;



static std::string ReadIniString(const char* section, const char* key, const char* fallback) {
	char buf[MAX_PATH] = {};
	GetPrivateProfileStringA(section, key, fallback, buf, MAX_PATH, g_iniPath);
	return std::string(buf);
}

static int ReadIniInt(const char* section, const char* key, int fallback) {
	return GetPrivateProfileIntA(section, key, fallback, g_iniPath);
}

static std::wstring ToWideACP(const std::string& text) {
	if (text.empty()) return std::wstring();
	int len = MultiByteToWideChar(CP_ACP, 0, text.c_str(), -1, nullptr, 0);
	if (len <= 0) return std::wstring();
	std::wstring wide(len, L'\0');
	MultiByteToWideChar(CP_ACP, 0, text.c_str(), -1, &wide[0], len);
	if (!wide.empty() && wide.back() == L'\0') wide.pop_back();
	return wide;
}

static std::wstring PathFromGameRoot(const std::string& path) {
	if (path.empty()) return std::wstring();
	if (path.size() > 2 && path[1] == ':') return ToWideACP(path);
	std::string full = std::string(g_gameDir[0] ? g_gameDir : g_moduleDir) + "\\" + path;
	return ToWideACP(full);
}

static void SafeWriteBytes(DWORD addr, const void* data, size_t count) {
	DWORD oldProtect = 0;
	VirtualProtect(reinterpret_cast<void*>(addr), count, PAGE_EXECUTE_READWRITE, &oldProtect);
	std::memcpy(reinterpret_cast<void*>(addr), data, count);
	VirtualProtect(reinterpret_cast<void*>(addr), count, oldProtect, &oldProtect);
	FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(addr), count);
}

static void MakeJump(DWORD addr, void* func) {
	BYTE patch[5] = {0xE9, 0, 0, 0, 0};
	*reinterpret_cast<DWORD*>(&patch[1]) = reinterpret_cast<DWORD>(func) - (addr + 5);
	SafeWriteBytes(addr, patch, sizeof(patch));
}

static void MakeJumpLen(DWORD addr, void* func, size_t len) {
	std::vector<BYTE> patch(len, 0x90);
	patch[0] = 0xE9;
	*reinterpret_cast<DWORD*>(&patch[1]) = reinterpret_cast<DWORD>(func) - (addr + 5);
	SafeWriteBytes(addr, patch.data(), patch.size());
}

static bool JumpMatches(DWORD addr, void* func) {
	BYTE* current = reinterpret_cast<BYTE*>(addr);
	if (current[0] != 0xE9) return false;
	DWORD rel = *reinterpret_cast<DWORD*>(current + 1);
	return rel == (reinterpret_cast<DWORD>(func) - (addr + 5));
}

static void MakeJumpIfNeeded(DWORD addr, void* func, const char* name) {
	if (JumpMatches(addr, func)) return;
	MakeJump(addr, func);
	WriteLog(std::string("KoreanText::Rehook: patched ") + name);
}

static void MakeCall(DWORD addr, void* func) {
	BYTE patch[5] = {0xE8, 0, 0, 0, 0};
	*reinterpret_cast<DWORD*>(&patch[1]) = reinterpret_cast<DWORD>(func) - (addr + 5);
	SafeWriteBytes(addr, patch, sizeof(patch));
}

static bool CallMatches(DWORD addr, void* func) {
	BYTE* current = reinterpret_cast<BYTE*>(addr);
	if (current[0] != 0xE8) return false;
	DWORD rel = *reinterpret_cast<DWORD*>(current + 1);
	return rel == (reinterpret_cast<DWORD>(func) - (addr + 5));
}

static void MakeCallIfNeeded(DWORD addr, void* func, const char* name) {
	if (CallMatches(addr, func)) return;
	MakeCall(addr, func);
	WriteLog(std::string("KoreanText::Rehook: patched ") + name);
}

static DWORD MakeTrampoline(DWORD addr, size_t len) {
	BYTE* gateway = reinterpret_cast<BYTE*>(VirtualAlloc(nullptr, len + 5, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
	if (!gateway) return 0;
	std::memcpy(gateway, reinterpret_cast<void*>(addr), len);
	gateway[len] = 0xE9;
	*reinterpret_cast<DWORD*>(gateway + len + 1) = (addr + static_cast<DWORD>(len)) - (reinterpret_cast<DWORD>(gateway) + static_cast<DWORD>(len) + 5);
	FlushInstructionCache(GetCurrentProcess(), gateway, len + 5);
	return reinterpret_cast<DWORD>(gateway);
}

namespace KoreanText {

static DWORD exeBase = 0x400000;
#define EXE_ADDR(addr) (exeBase + ((addr) - 0x400000))

static DWORD FO_VAR_gCurrentFont = 0x58E93C;
static DWORD FO_VAR_text_spacing = 0x51E3CC;
static DWORD FMtext_width_ = 0x442188;
static DWORD FMtext_to_buf_ = 0x4422B4;
static DWORD GNW_text_to_buf_ = 0x4D59B0;
static DWORD GNW_text_width_ = 0x4D5B60;
static DWORD FO_VAR_active_text_to_buf = 0x51E3B8;
static DWORD FO_VAR_active_text_width = 0x51E3C0;
static DWORD FO_VAR_fm_text_to_buf = 0x506C94;
static DWORD FO_VAR_fm_text_width = 0x506C9C;
static DWORD FO_VAR_gnw_text_to_buf = 0x4C593C;
static DWORD FO_VAR_gnw_text_width = 0x4C5944;
static DWORD getColorBlendTable_ = 0x4C7DC0;
static DWORD freeColorBlendTable_ = 0x4C7E20;
static DWORD win_print_ = 0x4D684C;
static DWORD win_print_continue_ = 0x4D6852;
static DWORD display_print_ = 0x43186C;
static DWORD display_print_continue_ = 0x431875;
static DWORD gdialog_display_msg_ = 0x445448;
static DWORD gdialog_display_msg_continue_ = 0x445451;
static DWORD inven_display_msg_ = 0x472D24;
static DWORD inven_display_msg_continue_ = 0x472D2D;
static DWORD text_object_create_ = 0x4B036C;
static DWORD text_object_create_continue_ = 0x4B0375;
static DWORD win_print_text_region_ = 0x4B5634;
static DWORD win_print_text_region_continue_ = 0x4B563C;
static DWORD win_print_substr_region_ = 0x4B5714;
static DWORD win_print_substr_region_continue_ = 0x4B571B;
static DWORD message_search_ = 0x484C30;
static DWORD message_search_gateway_ = 0;
static bool enableMessageSearchHook = false;
static DWORD getmsg_ = 0x48504C;
static DWORD getmsg_gateway_ = 0;
static bool enableGetMsgHook = false;
static bool enableExtraTextHooks = false;
static bool enableDialogTextRectHook = false;
static bool enableFontTableScan = true;
static long delayedRehookSeconds = 15;
static long minKoreanRenderWidth = 48;
static DWORD dialog_text_rect_ = 0x447FA0;
static DWORD dialog_reply_print_call_ = 0x446D19;
static DWORD dialog_option_print_call_ = 0x447071;
static bool debugFill = false;
static int debugFillColor = 0xFF;
static int debugFillLogCount = 0;
static bool directColor = false;
static int scanLogCount = 0;

struct MessageNode {
	long number;
	long flags;
	char* audio;
	char* message;
};

struct BlendColorTableData {
	struct BlendData {
		BYTE colors[256];
	};
	BlendData data[16];
};

struct FontData {
	short field0;
	short field2;
	short field4;
	short field6;
	short field8;
	short fieldA;
	short fieldC;
	char eUnkArray[2046];
	long field80C;
};

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

struct GlyphBitmap {
	long width;
	long height;
	std::vector<BYTE> mask;
};

static bool enabled = false;
static long buttonFontThreshold = 16;
static long activePath = 0;
static constexpr long RenderPathGNW = 0;
static constexpr long RenderPathFM = 1;
static FontProfile textProfile = {1, 10, 10, 11, 0, FW_NORMAL, 0, L"NanumGothic"};
static FontProfile fmButtonProfile = {2, 19, 19, 28, 0, 700, 0, L"NanumGothicExtraBold"};
static FontProfile gnwButtonProfile = {3, 14, 14, 16, 0, 700, 0, L"NanumGothicExtraBold"};
static FontProfile* activeProfile = &textProfile;
static std::unordered_map<unsigned long long, GlyphBitmap> glyphCache;
static HFONT hFont = nullptr;
static long hFontSize = 0;
static long hFontTag = 0;
static std::vector<std::wstring> loadedFontFiles;
static bool configured = false;

static bool IsCp949Lead(unsigned char ch) {
	if (ch == 149) return false;
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

static FontData* CurrentFont() {
	return reinterpret_cast<FontData*>(*reinterpret_cast<DWORD*>(FO_VAR_gCurrentFont));
}

static long FontHeight() {
	FontData* font = CurrentFont();
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
	DWORD addr = FO_VAR_text_spacing;
	__asm {
		mov eax, addr
		call dword ptr [eax]
	}
}

static long AsciiCharWidth(unsigned char ch) {
	FontData* font = CurrentFont();
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

	EnsureGdiFont(pixelHeight);
	HDC dc = CreateCompatibleDC(nullptr);
	HGDIOBJ oldFont = SelectObject(dc, hFont);
	SIZE size = {};
	GetTextExtentPoint32W(dc, &ch, 1, &size);
	TEXTMETRICW tm = {};
	GetTextMetricsW(dc, &tm);

	long cellWidth = KoreanCellWidth(pixelHeight);
	long cellHeight = KoreanRenderHeight(pixelHeight);
	if (cellWidth <= 0) cellWidth = 1;
	if (cellHeight <= 0) cellHeight = 1;
	if (size.cx <= 0) size.cx = cellWidth;
	long bitmapHeight = cellHeight;
	if (bitmapHeight < tm.tmHeight + 2) bitmapHeight = tm.tmHeight + 2;

	BITMAPINFO bmi = {};
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = cellWidth;
	bmi.bmiHeader.biHeight = -bitmapHeight;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;

	void* bits = nullptr;
	HBITMAP dib = CreateDIBSection(dc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
	HGDIOBJ oldBitmap = SelectObject(dc, dib);

	RECT rect = {0, 0, cellWidth, bitmapHeight};
	FillRect(dc, &rect, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
	SetBkMode(dc, OPAQUE);
	SetBkColor(dc, RGB(0, 0, 0));
	SetTextColor(dc, RGB(255, 255, 255));

	long x = (cellWidth - size.cx) / 2;
	long y = (bitmapHeight - tm.tmHeight) / 2;
	if (x < 0) x = 0;
	if (y < 0) y = 0;
	TextOutW(dc, x, y, &ch, 1);

	GlyphBitmap glyph;
	glyph.width = cellWidth;
	glyph.height = bitmapHeight;
	glyph.mask.resize(cellWidth * bitmapHeight);

	const BYTE* src = static_cast<const BYTE*>(bits);
	for (long yy = 0; yy < bitmapHeight; yy++) {
		for (long xx = 0; xx < cellWidth; xx++) {
			const BYTE* px = src + ((yy * cellWidth + xx) * 4);
			glyph.mask[yy * cellWidth + xx] = (px[0] || px[1] || px[2]) ? 9 : 0;
		}
	}

	if (profile.extraBoldRadius > 0) {
		std::vector<BYTE> thickMask = glyph.mask;
		for (long yy = 0; yy < cellHeight; yy++) {
			for (long xx = 0; xx < cellWidth; xx++) {
				if (!glyph.mask[yy * cellWidth + xx]) continue;
				for (long dy = -profile.extraBoldRadius; dy <= profile.extraBoldRadius; dy++) {
					long y2 = yy + dy;
					if (y2 < 0 || y2 >= cellHeight) continue;
					for (long dx = -profile.extraBoldRadius; dx <= profile.extraBoldRadius; dx++) {
						long x2 = xx + dx;
						if (x2 < 0 || x2 >= cellWidth) continue;
						thickMask[y2 * cellWidth + x2] = 9;
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

static BlendColorTableData* GetColorBlendTable(long colorFlags) {
	__asm {
		mov eax, colorFlags
		call dword ptr ds:[getColorBlendTable_]
	}
}

static void FreeColorBlendTable(long colorFlags) {
	__asm {
		mov eax, colorFlags
		call dword ptr ds:[freeColorBlendTable_]
	}
}

static void DrawAsciiChar(BYTE*& dst, unsigned char ch, long pitch, BlendColorTableData* blendTable) {
	FontData* font = CurrentFont();
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

static void DrawKoreanChar(BYTE*& dst, wchar_t ch, long pitch, BlendColorTableData* blendTable, long colorFlags) {
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
				dstPixels[x] = directColor
					? static_cast<BYTE>(colorFlags & 0xFF)
					: blendTable->data[mask].colors[dstPixels[x]];
			}
		}
		dstPixels += pitch;
	}
	dst += glyph.width + CharSpacing();
}

static int textLogCount = 0;
static int extendedTextLogCount = 0;
static int winPrintLogCount = 0;

static void LogTextPreview(const char* prefix, const char* text) {
	if (!text || !*text) return;

	bool hasExtended = false;
	for (const char* p = text; *p; p++) {
		if (static_cast<unsigned char>(*p) >= 128) {
			hasExtended = true;
			break;
		}
	}
	if (!hasExtended && winPrintLogCount >= 120) return;
	if (hasExtended && winPrintLogCount >= 2200) return;
	winPrintLogCount++;

	std::string hexStr;
	for (const char* p = text; *p && (p - text) < 120; p++) {
		char buf[6];
		sprintf_s(buf, "%02X ", static_cast<unsigned char>(*p));
		hexStr += buf;
	}
	std::string raw(text);
	if (raw.size() > 120) raw.resize(120);
	WriteLog(std::string(prefix) + " extended=" + (hasExtended ? "1" : "0") + " | " + hexStr + " | RAW: " + raw);
}

static void __stdcall WinPrintLogImpl(const char* text) {
	LogTextPreview("WinPrintHook", text);
}

static __declspec(naked) void win_print_hook() {
	__asm {
		pushfd
		pushad
		push edx
		call WinPrintLogImpl
		popad
		popfd
		push esi
		push edi
		push ebp
		sub  esp, 0x1C
		jmp  dword ptr ds:[win_print_continue_]
	}
}

static void __stdcall DisplayPrintLogImpl(const char* text) {
	LogTextPreview("DisplayPrintHook", text);
}

static __declspec(naked) void display_print_hook() {
	__asm {
		pushfd
		pushad
		push eax
		call DisplayPrintLogImpl
		popad
		popfd
		push ebx
		push ecx
		push edx
		push esi
		push edi
		push ebp
		sub  esp, 8
		jmp  dword ptr ds:[display_print_continue_]
	}
}

static void __stdcall GDialogDisplayMsgLogImpl(const char* text) {
	LogTextPreview("GDialogDisplayMsgHook", text);
}

static __declspec(naked) void gdialog_display_msg_hook() {
	__asm {
		pushfd
		pushad
		push eax
		call GDialogDisplayMsgLogImpl
		popad
		popfd
		push ebx
		push ecx
		push edx
		push esi
		push edi
		push ebp
		sub  esp, 4
		jmp  dword ptr ds:[gdialog_display_msg_continue_]
	}
}

static void __stdcall InvenDisplayMsgLogImpl(const char* text) {
	LogTextPreview("InvenDisplayMsgHook", text);
}

static __declspec(naked) void inven_display_msg_hook() {
	__asm {
		pushfd
		pushad
		push eax
		call InvenDisplayMsgLogImpl
		popad
		popfd
		push ebx
		push ecx
		push edx
		push esi
		push edi
		push ebp
		sub  esp, 4
		jmp  dword ptr ds:[inven_display_msg_continue_]
	}
}

static void __stdcall TextObjectCreateLogImpl(const char* text) {
	LogTextPreview("TextObjectCreateHook", text);
}

static __declspec(naked) void text_object_create_hook() {
	__asm {
		pushfd
		pushad
		push edx
		call TextObjectCreateLogImpl
		popad
		popfd
		push esi
		push edi
		push ebp
		sub  esp, 0xAC
		jmp  dword ptr ds:[text_object_create_continue_]
	}
}

static void __stdcall WinPrintTextRegionLogImpl(const char* text) {
	LogTextPreview("WinPrintTextRegionHook", text);
}

static __declspec(naked) void win_print_text_region_hook() {
	__asm {
		pushfd
		pushad
		push edx
		call WinPrintTextRegionLogImpl
		popad
		popfd
		push ebx
		push ecx
		push esi
		push edi
		push ebp
		sub  esp, 4
		jmp  dword ptr ds:[win_print_text_region_continue_]
	}
}

static void __stdcall WinPrintSubstrRegionLogImpl(const char* text) {
	LogTextPreview("WinPrintSubstrRegionHook", text);
}

static __declspec(naked) void win_print_substr_region_hook() {
	__asm {
		pushfd
		pushad
		push edx
		call WinPrintSubstrRegionLogImpl
		popad
		popfd
		push ecx
		push esi
		push edi
		push ebp
		sub  esp, 4
		jmp  dword ptr ds:[win_print_substr_region_continue_]
	}
}

static void __stdcall MessageSearchLogImpl(long result, MessageNode* msg) {
	if (result != 1 || !msg || !msg->message || !*msg->message) return;
	bool hasExtended = false;
	for (const char* p = msg->message; *p; p++) {
		if (static_cast<unsigned char>(*p) >= 128) {
			hasExtended = true;
			break;
		}
	}
	if (!hasExtended) return;
	std::string prefix = "MessageSearchHook id=" + std::to_string(msg->number);
	LogTextPreview(prefix.c_str(), msg->message);
}

static __declspec(naked) void message_search_hook() {
	__asm {
		push edx
		push eax
		call dword ptr ds:[message_search_gateway_]
		push eax
		push dword ptr [esp + 8]
		push dword ptr [esp + 4]
		call MessageSearchLogImpl
		pop  eax
		add  esp, 8
		retn
	}
}

static int getMsgLogCount = 0;
static void __stdcall GetMsgLogImpl(long id, const char* text) {
	if (!text || !*text || getMsgLogCount >= 1800) return;
	bool hasExtended = false;
	for (const char* p = text; *p; p++) {
		if (static_cast<unsigned char>(*p) >= 128) {
			hasExtended = true;
			break;
		}
	}
	if (!hasExtended && getMsgLogCount >= 160) return;
	getMsgLogCount++;
	std::string prefix = "GetMsgHook id=" + std::to_string(id);
	LogTextPreview(prefix.c_str(), text);
}

static __declspec(naked) void getmsg_hook() {
	__asm {
		push ebx
		push edx
		call dword ptr ds:[getmsg_gateway_]
		push eax
		push eax
		push dword ptr [esp + 12]
		call GetMsgLogImpl
		pop  eax
		add  esp, 8
		retn
	}
}

static void __stdcall DialogTextRectLogImpl(const char* text) {
	LogTextPreview("DialogTextRectCall", text);
}

static __declspec(naked) void dialog_text_rect_call_hook() {
	__asm {
		pushfd
		pushad
		push ebx
		call DialogTextRectLogImpl
		popad
		popfd
		call dword ptr ds:[dialog_text_rect_]
		retn
	}
}

static void __stdcall TextToBufHookImpl(BYTE* dst, const char* text, long maxWidth, long pitch, long colorFlags) {
	if (!text || !*text) return;

	bool hasExtended = false;
	for (const char* p = text; *p; p++) {
		if (static_cast<unsigned char>(*p) >= 128) {
			hasExtended = true;
			break;
		}
	}

	if (hasExtended && debugFill && dst && maxWidth > 0 && pitch > 0) {
		long fontHeight = FontHeight();
		long markW = (maxWidth < 96) ? maxWidth : 96;
		long markH = (fontHeight < 8) ? fontHeight : 8;
		if (markW > 0 && markH > 0) {
			BYTE color = static_cast<BYTE>(debugFillColor & 0xFF);
			for (long y = 0; y < markH; y++) {
				BYTE* row = dst + (pitch * y);
				for (long x = 0; x < markW; x++) {
					row[x] = color;
				}
			}
			if (debugFillLogCount < 120) {
				debugFillLogCount++;
				WriteLog(
					"TextToBufHookImpl: debug fill wrote marker dst=" + std::to_string(reinterpret_cast<DWORD>(dst)) +
					" w=" + std::to_string(markW) +
					" h=" + std::to_string(markH) +
					" pitch=" + std::to_string(pitch) +
					" color=" + std::to_string(color)
				);
			}
		}
	}

	bool shouldLog = false;
	if (hasExtended && extendedTextLogCount < 2000) {
		extendedTextLogCount++;
		shouldLog = true;
	} else if (!hasExtended && textLogCount < 80) {
		textLogCount++;
		shouldLog = true;
	}

	if (shouldLog) {
		std::string hexStr;
		for (const char* p = text; *p && (p - text) < 120; p++) {
			char buf[6];
			sprintf_s(buf, "%02X ", static_cast<unsigned char>(*p));
			hexStr += buf;
		}
		std::string raw(text);
		if (raw.size() > 120) raw.resize(120);
		WriteLog(
			std::string("TextToBufHookImpl: path=") + ((activePath == RenderPathFM) ? "FM" : "GNW") +
			" extended=" + (hasExtended ? "1" : "0") +
			" fontHeight=" + std::to_string(FontHeight()) +
			" maxWidth=" + std::to_string(maxWidth) +
			" pitch=" + std::to_string(pitch) +
			" colorFlags=" + std::to_string(colorFlags) +
			" | " + hexStr + " | RAW: " + raw
		);
	}

	if (hasExtended && (maxWidth < minKoreanRenderWidth || pitch < minKoreanRenderWidth)) {
		if (extendedTextLogCount < 2300) {
			WriteLog("TextToBufHookImpl: skipped narrow Korean buffer maxWidth=" + std::to_string(maxWidth) + " pitch=" + std::to_string(pitch));
		}
		return;
	}

	BlendColorTableData* blendTable = GetColorBlendTable(colorFlags);
	BYTE* cursor = dst;
	while (*text) {
		long usedWidth = static_cast<long>(cursor - dst);
		if (usedWidth >= maxWidth) break;
		wchar_t korean;
		if (DecodeCp949(text, korean)) {
			long drawWidth = KoreanCharWidth(korean) + CharSpacing();
			if (usedWidth + drawWidth > maxWidth) break;
			DrawKoreanChar(cursor, korean, pitch, blendTable, colorFlags);
			text += 2;
		} else {
			unsigned char ch = static_cast<unsigned char>(*text);
			long drawWidth = AsciiCharWidth(ch) + CharSpacing();
			if (usedWidth + drawWidth > maxWidth) break;
			DrawAsciiChar(cursor, ch, pitch, blendTable);
			text++;
		}
	}
	FreeColorBlendTable(colorFlags);
}

static __declspec(naked) void fm_text_width_hook() {
	__asm {
		push ecx
		push edx
		mov  dword ptr ds:[activePath], 1
		mov  ecx, eax
		call TextWidthHookImpl
		pop  edx
		pop  ecx
		retn
	}
}

static __declspec(naked) void gnw_text_width_hook() {
	__asm {
		push ecx
		push edx
		mov  dword ptr ds:[activePath], 0
		mov  ecx, eax
		call TextWidthHookImpl
		pop  edx
		pop  ecx
		retn
	}
}

static __declspec(naked) void fm_text_to_buf_hook() {
	__asm {
		mov  dword ptr ds:[activePath], 1
		push dword ptr [esp + 4]
		push ecx
		push ebx
		push edx
		push eax
		call TextToBufHookImpl
		retn 4
	}
}

static __declspec(naked) void gnw_text_to_buf_hook() {
	__asm {
		mov  dword ptr ds:[activePath], 0
		push dword ptr [esp + 4]
		push ecx
		push ebx
		push edx
		push eax
		call TextToBufHookImpl
		retn 4
	}
}

static std::wstring ResolveFontFaceAlias(const std::string& face) {
	if (_stricmp(face.c_str(), "AtoZ8ExtraBold") == 0 || _stricmp(face.c_str(), "AtoZ 8 ExtraBold") == 0) {
		static const wchar_t atoz8ExtraBold[] = {
			0xC5D0, 0xC774, 0xD22C, 0xC9C0, 0xCCB4, L' ', L'8', L' ',
			L'E', L'x', L't', L'r', L'a', L'B', L'o', L'l', L'd', 0
		};
		return atoz8ExtraBold;
	}
	return ToWideACP(face);
}

static void LoadFace(const char* key, const char* defaultFace, wchar_t* dest) {
	auto face = ResolveFontFaceAlias(ReadIniString("Main", key, defaultFace));
	std::memset(dest, 0, sizeof(wchar_t) * LF_FACESIZE);
	wcsncpy_s(dest, LF_FACESIZE, face.c_str(), _TRUNCATE);
}

static void PatchActiveFontPointers() {
	DWORD gnwBufHook = reinterpret_cast<DWORD>(gnw_text_to_buf_hook);
	DWORD gnwWidthHook = reinterpret_cast<DWORD>(gnw_text_width_hook);
	DWORD fmBufHook = reinterpret_cast<DWORD>(fm_text_to_buf_hook);
	DWORD fmWidthHook = reinterpret_cast<DWORD>(fm_text_width_hook);
	if (*reinterpret_cast<DWORD*>(FO_VAR_active_text_to_buf) != gnwBufHook) {
		SafeWriteBytes(FO_VAR_active_text_to_buf, &gnwBufHook, sizeof(gnwBufHook));
		WriteLog("KoreanText::PatchActiveFontPointers: patched text_to_buf callback");
	}
	if (*reinterpret_cast<DWORD*>(FO_VAR_active_text_width) != gnwWidthHook) {
		SafeWriteBytes(FO_VAR_active_text_width, &gnwWidthHook, sizeof(gnwWidthHook));
		WriteLog("KoreanText::PatchActiveFontPointers: patched text_width callback");
	}
	if (*reinterpret_cast<DWORD*>(FO_VAR_fm_text_to_buf) != fmBufHook) {
		SafeWriteBytes(FO_VAR_fm_text_to_buf, &fmBufHook, sizeof(fmBufHook));
		WriteLog("KoreanText::PatchActiveFontPointers: patched FM text_to_buf callback");
	}
	if (*reinterpret_cast<DWORD*>(FO_VAR_fm_text_width) != fmWidthHook) {
		SafeWriteBytes(FO_VAR_fm_text_width, &fmWidthHook, sizeof(fmWidthHook));
		WriteLog("KoreanText::PatchActiveFontPointers: patched FM text_width callback");
	}
	if (*reinterpret_cast<DWORD*>(FO_VAR_gnw_text_to_buf) != gnwBufHook) {
		SafeWriteBytes(FO_VAR_gnw_text_to_buf, &gnwBufHook, sizeof(gnwBufHook));
		WriteLog("KoreanText::PatchActiveFontPointers: patched GNW table text_to_buf callback");
	}
	if (*reinterpret_cast<DWORD*>(FO_VAR_gnw_text_width) != gnwWidthHook) {
		SafeWriteBytes(FO_VAR_gnw_text_width, &gnwWidthHook, sizeof(gnwWidthHook));
		WriteLog("KoreanText::PatchActiveFontPointers: patched GNW table text_width callback");
	}
}

static bool IsReadableMemory(DWORD protect) {
	if (protect & PAGE_GUARD) return false;
	if (protect == PAGE_NOACCESS) return false;
	return (protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
		PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
}

static void PatchDwordIfNeeded(DWORD addr, DWORD value, const char* label) {
	if (*reinterpret_cast<DWORD*>(addr) == value) return;
	SafeWriteBytes(addr, &value, sizeof(value));
	if (scanLogCount < 80) {
		scanLogCount++;
		char buf[128];
		sprintf_s(buf, "%s at %08X", label, addr);
		WriteLog(std::string("KoreanText::ScanAndPatchFontTables: patched ") + buf);
	}
}

static void ScanAndPatchFontTables() {
	static DWORD lastScanTick = 0;
	DWORD now = GetTickCount();
	if (lastScanTick != 0 && now - lastScanTick < 3000) return;
	lastScanTick = now;

	DWORD fmBufHook = reinterpret_cast<DWORD>(fm_text_to_buf_hook);
	DWORD fmWidthHook = reinterpret_cast<DWORD>(fm_text_width_hook);
	DWORD gnwBufHook = reinterpret_cast<DWORD>(gnw_text_to_buf_hook);
	DWORD gnwWidthHook = reinterpret_cast<DWORD>(gnw_text_width_hook);

	SYSTEM_INFO si = {};
	GetSystemInfo(&si);
	BYTE* cursor = reinterpret_cast<BYTE*>(si.lpMinimumApplicationAddress);
	BYTE* maxAddr = reinterpret_cast<BYTE*>(si.lpMaximumApplicationAddress);

	while (cursor < maxAddr) {
		MEMORY_BASIC_INFORMATION mbi = {};
		if (VirtualQuery(cursor, &mbi, sizeof(mbi)) != sizeof(mbi)) break;

		BYTE* base = reinterpret_cast<BYTE*>(mbi.BaseAddress);
		BYTE* end = base + mbi.RegionSize;
		if (mbi.State == MEM_COMMIT && IsReadableMemory(mbi.Protect)) {
			for (BYTE* p = base; p + 32 < end; p += 4) {
				DWORD* d = reinterpret_cast<DWORD*>(p);

				if (d[0] == 0x00000009 &&
					(d[1] == EXE_ADDR(0x4D58AC) || d[1] == 0x4D58AC) &&
					(d[2] == EXE_ADDR(0x4D59B0) || d[2] == gnwBufHook) &&
					(d[4] == EXE_ADDR(0x4D5B60) || d[4] == gnwWidthHook)) {
					PatchDwordIfNeeded(reinterpret_cast<DWORD>(&d[2]), gnwBufHook, "GNW copy text_to_buf");
					PatchDwordIfNeeded(reinterpret_cast<DWORD>(&d[4]), gnwWidthHook, "GNW copy text_width");
				}

				if ((d[0] == 0x00000064 || d[0] == 0x0000006E) &&
					(d[1] == 0x0000006E || d[1] == EXE_ADDR(0x442120) || d[1] == 0x442120) &&
					(d[2] == EXE_ADDR(0x442120) || d[2] == 0x442120) &&
					(d[3] == EXE_ADDR(0x4422B4) || d[3] == fmBufHook) &&
					(d[5] == EXE_ADDR(0x442188) || d[5] == fmWidthHook)) {
					PatchDwordIfNeeded(reinterpret_cast<DWORD>(&d[3]), fmBufHook, "FM copy text_to_buf");
					PatchDwordIfNeeded(reinterpret_cast<DWORD>(&d[5]), fmWidthHook, "FM copy text_width");
				}
			}
		}

		cursor = end;
	}
}

static void ApplyHooks() {
	MakeJumpIfNeeded(FMtext_width_, fm_text_width_hook, "FMtext_width");
	MakeJumpIfNeeded(GNW_text_width_, gnw_text_width_hook, "GNW_text_width");
	MakeJumpIfNeeded(FMtext_to_buf_, fm_text_to_buf_hook, "FMtext_to_buf");
	MakeJumpIfNeeded(GNW_text_to_buf_, gnw_text_to_buf_hook, "GNW_text_to_buf");
	if (enableExtraTextHooks) {
		if (!JumpMatches(win_print_, win_print_hook)) {
			MakeJumpLen(win_print_, win_print_hook, 6);
			WriteLog("KoreanText::Rehook: patched win_print");
		}
		if (!JumpMatches(gdialog_display_msg_, gdialog_display_msg_hook)) {
			MakeJumpLen(gdialog_display_msg_, gdialog_display_msg_hook, 9);
			WriteLog("KoreanText::Rehook: patched gdialogDisplayMsg");
		}
		if (!JumpMatches(inven_display_msg_, inven_display_msg_hook)) {
			MakeJumpLen(inven_display_msg_, inven_display_msg_hook, 9);
			WriteLog("KoreanText::Rehook: patched inven_display_msg");
		}
		if (!JumpMatches(text_object_create_, text_object_create_hook)) {
			MakeJumpLen(text_object_create_, text_object_create_hook, 9);
			WriteLog("KoreanText::Rehook: patched text_object_create");
		}
		if (!JumpMatches(win_print_text_region_, win_print_text_region_hook)) {
			MakeJumpLen(win_print_text_region_, win_print_text_region_hook, 8);
			WriteLog("KoreanText::Rehook: patched win_print_text_region");
		}
		if (!JumpMatches(win_print_substr_region_, win_print_substr_region_hook)) {
			MakeJumpLen(win_print_substr_region_, win_print_substr_region_hook, 7);
			WriteLog("KoreanText::Rehook: patched win_print_substr_region");
		}
	}
	if (enableMessageSearchHook && !message_search_gateway_) {
		message_search_gateway_ = MakeTrampoline(message_search_, 6);
	}
	if (enableMessageSearchHook && message_search_gateway_ && !JumpMatches(message_search_, message_search_hook)) {
		MakeJumpLen(message_search_, message_search_hook, 6);
		WriteLog("KoreanText::Rehook: patched message_search");
	}
	if (enableGetMsgHook && !getmsg_gateway_) {
		getmsg_gateway_ = MakeTrampoline(getmsg_, 5);
	}
	if (enableGetMsgHook && getmsg_gateway_ && !JumpMatches(getmsg_, getmsg_hook)) {
		MakeJumpLen(getmsg_, getmsg_hook, 5);
		WriteLog("KoreanText::Rehook: patched getmsg");
	}
	if (enableDialogTextRectHook) {
		MakeCallIfNeeded(dialog_reply_print_call_, dialog_text_rect_call_hook, "dialog reply text call");
		MakeCallIfNeeded(dialog_option_print_call_, dialog_text_rect_call_hook, "dialog option text call");
	}
	PatchActiveFontPointers();
	if (enableFontTableScan) ScanAndPatchFontTables();
}

static void LoadPrivateFontFile(const char* key) {
	auto path = ReadIniString("Main", key, "");
	if (path.empty()) return;
	std::wstring widePath = PathFromGameRoot(path);
	if (!widePath.empty() && AddFontResourceExW(widePath.c_str(), FR_PRIVATE, nullptr) > 0) {
		loadedFontFiles.push_back(widePath);
	}
}

static void Init() {
	exeBase = reinterpret_cast<DWORD>(GetModuleHandleA(nullptr));
	WriteLog("Executable base address: " + std::to_string(exeBase));

	FO_VAR_gCurrentFont = EXE_ADDR(0x58E93C);
	FO_VAR_text_spacing = EXE_ADDR(0x51E3CC);
	FMtext_width_ = EXE_ADDR(0x442188);
	FMtext_to_buf_ = EXE_ADDR(0x4422B4);
	GNW_text_to_buf_ = EXE_ADDR(0x4D59B0);
	GNW_text_width_ = EXE_ADDR(0x4D5B60);
	FO_VAR_active_text_to_buf = EXE_ADDR(0x51E3B8);
	FO_VAR_active_text_width = EXE_ADDR(0x51E3C0);
	FO_VAR_fm_text_to_buf = EXE_ADDR(0x506C94);
	FO_VAR_fm_text_width = EXE_ADDR(0x506C9C);
	FO_VAR_gnw_text_to_buf = EXE_ADDR(0x4C593C);
	FO_VAR_gnw_text_width = EXE_ADDR(0x4C5944);
	getColorBlendTable_ = EXE_ADDR(0x4C7DC0);
	freeColorBlendTable_ = EXE_ADDR(0x4C7E20);
	win_print_ = EXE_ADDR(0x4D684C);
	win_print_continue_ = EXE_ADDR(0x4D6852);
	display_print_ = EXE_ADDR(0x43186C);
	display_print_continue_ = EXE_ADDR(0x431875);
	gdialog_display_msg_ = EXE_ADDR(0x445448);
	gdialog_display_msg_continue_ = EXE_ADDR(0x445451);
	inven_display_msg_ = EXE_ADDR(0x472D24);
	inven_display_msg_continue_ = EXE_ADDR(0x472D2D);
	text_object_create_ = EXE_ADDR(0x4B036C);
	text_object_create_continue_ = EXE_ADDR(0x4B0375);
	win_print_text_region_ = EXE_ADDR(0x4B5634);
	win_print_text_region_continue_ = EXE_ADDR(0x4B563C);
	win_print_substr_region_ = EXE_ADDR(0x4B5714);
	win_print_substr_region_continue_ = EXE_ADDR(0x4B571B);
	message_search_ = EXE_ADDR(0x484C30);
	getmsg_ = EXE_ADDR(0x48504C);
	dialog_text_rect_ = EXE_ADDR(0x447FA0);
	dialog_reply_print_call_ = EXE_ADDR(0x446D19);
	dialog_option_print_call_ = EXE_ADDR(0x447071);

	enabled = ReadIniInt("Main", "KoreanText", 0) != 0;
	WriteLog("KoreanText::Init: enabled=" + std::to_string(enabled));
	if (!enabled) return;
	debugFill = ReadIniInt("Main", "KoreanTextDebugFill", 0) != 0;
	debugFillColor = ReadIniInt("Main", "KoreanTextDebugFillColor", 255);
	directColor = ReadIniInt("Main", "KoreanTextDirectColor", 0) != 0;
	enableExtraTextHooks = ReadIniInt("Main", "KoreanTextExtraTextHooks", 0) != 0;
	enableDialogTextRectHook = ReadIniInt("Main", "KoreanTextDialogTextRectHook", 0) != 0;
	enableFontTableScan = ReadIniInt("Main", "KoreanTextFontTableScan", 1) != 0;
	delayedRehookSeconds = ReadIniInt("Main", "KoreanTextRehookSeconds", 15);
	minKoreanRenderWidth = ReadIniInt("Main", "KoreanTextMinRenderWidth", 48);

	if (configured) {
		ApplyHooks();
		return;
	}
	configured = true;

	buttonFontThreshold = ReadIniInt("Main", "KoreanTextButtonThreshold", 16);

	textProfile.fontHeight = ReadIniInt("Main", "KoreanTextTextFontHeight", 10);
	textProfile.cellWidth = ReadIniInt("Main", "KoreanTextTextCellWidth", 10);
	textProfile.renderHeight = ReadIniInt("Main", "KoreanTextTextRenderHeight", 11);
	textProfile.yOffset = ReadIniInt("Main", "KoreanTextTextYOffset", 0);
	textProfile.fontWeight = ReadIniInt("Main", "KoreanTextTextFontWeight", FW_NORMAL);
	textProfile.extraBoldRadius = ReadIniInt("Main", "KoreanTextTextExtraBold", 0);
	LoadPrivateFontFile("KoreanTextTextFontFile");
	LoadFace("KoreanTextTextFontFace", "NanumGothic", textProfile.fontFace);

	fmButtonProfile.fontHeight = ReadIniInt("Main", "KoreanTextFontHeight", 19);
	fmButtonProfile.cellWidth = ReadIniInt("Main", "KoreanTextCellWidth", 19);
	fmButtonProfile.renderHeight = ReadIniInt("Main", "KoreanTextRenderHeight", 28);
	fmButtonProfile.yOffset = ReadIniInt("Main", "KoreanTextYOffset", 0);
	fmButtonProfile.fontWeight = ReadIniInt("Main", "KoreanTextFontWeight", 700);
	fmButtonProfile.extraBoldRadius = ReadIniInt("Main", "KoreanTextExtraBold", 0);
	LoadPrivateFontFile("KoreanTextFontFile");
	LoadFace("KoreanTextFontFace", "NanumGothicExtraBold", fmButtonProfile.fontFace);

	gnwButtonProfile.fontHeight = ReadIniInt("Main", "KoreanTextGnwFontHeight", 14);
	gnwButtonProfile.cellWidth = ReadIniInt("Main", "KoreanTextGnwCellWidth", 14);
	gnwButtonProfile.renderHeight = ReadIniInt("Main", "KoreanTextGnwRenderHeight", 16);
	gnwButtonProfile.yOffset = ReadIniInt("Main", "KoreanTextGnwYOffset", 0);
	gnwButtonProfile.fontWeight = ReadIniInt("Main", "KoreanTextGnwFontWeight", 700);
	gnwButtonProfile.extraBoldRadius = ReadIniInt("Main", "KoreanTextGnwExtraBold", 0);
	LoadPrivateFontFile("KoreanTextGnwFontFile");
	LoadFace("KoreanTextGnwFontFace", "NanumGothicExtraBold", gnwButtonProfile.fontFace);

	WriteLog("KoreanText::Init: applying jumps to " + std::to_string(FMtext_width_) + " etc.");
	ApplyHooks();
	WriteLog("KoreanText::Init: jumps applied");
}

static void Shutdown() {
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

static DWORD WINAPI DelayedRehookThread(LPVOID) {
	WriteLog("KoreanText::DelayedRehookThread: watchdog started");
	for (long i = 0; i < delayedRehookSeconds; i++) {
		Sleep(1000);
		Init();
	}
	WriteLog("KoreanText::DelayedRehookThread: watchdog finished");
	return 0;
}

}

static FARPROC LoadExport(const char* name) {
	return g_realDdraw ? GetProcAddress(g_realDdraw, name) : nullptr;
}

static void LoadRealDdraw() {
	if (g_realDdraw) return;

	char realPath[MAX_PATH] = {};
	lstrcpyA(realPath, g_moduleDir);
	lstrcatA(realPath, "\\ddraw_sfall.dll");
	g_realDdraw = LoadLibraryA(realPath);
	if (!g_realDdraw) {
		char sysPath[MAX_PATH] = {};
		GetSystemDirectoryA(sysPath, MAX_PATH);
		lstrcatA(sysPath, "\\ddraw.dll");
		g_realDdraw = LoadLibraryA(sysPath);
	}

	pAcquireDDThreadLock = LoadExport("AcquireDDThreadLock");
	pCheckFullscreen = LoadExport("CheckFullscreen");
	pCompleteCreateSysmemSurface = LoadExport("CompleteCreateSysmemSurface");
	pD3DParseUnknownCommand = LoadExport("D3DParseUnknownCommand");
	pDDGetAttachedSurfaceLcl = LoadExport("DDGetAttachedSurfaceLcl");
	pDDInternalLock = LoadExport("DDInternalLock");
	pDDInternalUnlock = LoadExport("DDInternalUnlock");
	pDSoundHelp = LoadExport("DSoundHelp");
	pDirectDrawCreate = LoadExport("DirectDrawCreate");
	pDirectDrawCreate2 = LoadExport("DirectDrawCreate2");
	pDirectDrawCreateClipper = LoadExport("DirectDrawCreateClipper");
	pDirectDrawCreateEx = LoadExport("DirectDrawCreateEx");
	pDirectDrawEnumerateA = LoadExport("DirectDrawEnumerateA");
	pDirectDrawEnumerateExA = LoadExport("DirectDrawEnumerateExA");
	pDirectDrawEnumerateExW = LoadExport("DirectDrawEnumerateExW");
	pDirectDrawEnumerateW = LoadExport("DirectDrawEnumerateW");
	pDirectInputCreateA = LoadExport("DirectInputCreateA");
	pDllCanUnloadNow = LoadExport("DllCanUnloadNow");
	pDllGetClassObject = LoadExport("DllGetClassObject");
	pGetDDSurfaceLocal = LoadExport("GetDDSurfaceLocal");
	pGetOLEThunkData = LoadExport("GetOLEThunkData");
	pGetSurfaceFromDC = LoadExport("GetSurfaceFromDC");
	pRegisterSpecialCase = LoadExport("RegisterSpecialCase");
	pReleaseDDThreadLock = LoadExport("ReleaseDDThreadLock");
	pSetAppCompatData = LoadExport("SetAppCompatData");
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
	if (reason == DLL_PROCESS_ATTACH) {
		DisableThreadLibraryCalls(module);
		GetModuleFileNameA(module, g_moduleDir, MAX_PATH);
		char* slash = strrchr(g_moduleDir, '\\');
		if (slash) *slash = '\0';
		GetCurrentDirectoryA(MAX_PATH, g_gameDir);
		if (!g_gameDir[0]) lstrcpyA(g_gameDir, g_moduleDir);
		lstrcpyA(g_iniPath, g_gameDir);
		lstrcatA(g_iniPath, "\\ddraw.ini");
		if (GetFileAttributesA(g_iniPath) == INVALID_FILE_ATTRIBUTES) {
			lstrcpyA(g_iniPath, g_moduleDir);
			lstrcatA(g_iniPath, "\\ddraw.ini");
		}
		WriteLog("DllMain PROCESS_ATTACH: Loaded from " + std::string(g_moduleDir) + ", game dir " + std::string(g_gameDir) + ", ini " + std::string(g_iniPath));
		LoadRealDdraw();
		KoreanText::Init();
		HANDLE thread = CreateThread(nullptr, 0, KoreanText::DelayedRehookThread, nullptr, 0, nullptr);
		if (thread) CloseHandle(thread);
	} else if (reason == DLL_PROCESS_DETACH) {
		KoreanText::Shutdown();
		if (g_realDdraw) {
			FreeLibrary(g_realDdraw);
			g_realDdraw = nullptr;
		}
	}
	return TRUE;
}

#define EXPORT_STUB(name, proc) extern "C" __declspec(naked) void name() { __asm { jmp dword ptr [proc] } }

EXPORT_STUB(AcquireDDThreadLock, pAcquireDDThreadLock)
EXPORT_STUB(CheckFullscreen, pCheckFullscreen)
EXPORT_STUB(CompleteCreateSysmemSurface, pCompleteCreateSysmemSurface)
EXPORT_STUB(D3DParseUnknownCommand, pD3DParseUnknownCommand)
EXPORT_STUB(DDGetAttachedSurfaceLcl, pDDGetAttachedSurfaceLcl)
EXPORT_STUB(DDInternalLock, pDDInternalLock)
EXPORT_STUB(DDInternalUnlock, pDDInternalUnlock)
EXPORT_STUB(DSoundHelp, pDSoundHelp)
extern "C" HRESULT WINAPI DirectDrawCreate(GUID* lpGUID, LPVOID* lplpDD, IUnknown* pUnkOuter) {
	using Fn = HRESULT (WINAPI*)(GUID*, LPVOID*, IUnknown*);
	HRESULT result = reinterpret_cast<Fn>(pDirectDrawCreate)(lpGUID, lplpDD, pUnkOuter);
	WriteLog("DirectDrawCreate: re-applying KoreanText hooks");
	KoreanText::Init();
	return result;
}

extern "C" HRESULT WINAPI DirectDrawCreate2(GUID* lpGUID, LPVOID* lplpDD, IUnknown* pUnkOuter) {
	using Fn = HRESULT (WINAPI*)(GUID*, LPVOID*, IUnknown*);
	HRESULT result = reinterpret_cast<Fn>(pDirectDrawCreate2)(lpGUID, lplpDD, pUnkOuter);
	WriteLog("DirectDrawCreate2: re-applying KoreanText hooks");
	KoreanText::Init();
	return result;
}

EXPORT_STUB(DirectDrawCreateClipper, pDirectDrawCreateClipper)
extern "C" HRESULT WINAPI DirectDrawCreateEx(GUID* lpGUID, LPVOID* lplpDD, REFIID iid, IUnknown* pUnkOuter) {
	using Fn = HRESULT (WINAPI*)(GUID*, LPVOID*, REFIID, IUnknown*);
	HRESULT result = reinterpret_cast<Fn>(pDirectDrawCreateEx)(lpGUID, lplpDD, iid, pUnkOuter);
	WriteLog("DirectDrawCreateEx: re-applying KoreanText hooks");
	KoreanText::Init();
	return result;
}

EXPORT_STUB(DirectDrawEnumerateA, pDirectDrawEnumerateA)
EXPORT_STUB(DirectDrawEnumerateExA, pDirectDrawEnumerateExA)
EXPORT_STUB(DirectDrawEnumerateExW, pDirectDrawEnumerateExW)
EXPORT_STUB(DirectDrawEnumerateW, pDirectDrawEnumerateW)
EXPORT_STUB(DirectInputCreateA, pDirectInputCreateA)
extern "C" __declspec(naked) HRESULT STDAPICALLTYPE DllCanUnloadNow() { __asm { jmp dword ptr [pDllCanUnloadNow] } }
extern "C" __declspec(naked) HRESULT STDAPICALLTYPE DllGetClassObject(REFCLSID, REFIID, LPVOID*) { __asm { jmp dword ptr [pDllGetClassObject] } }
EXPORT_STUB(GetDDSurfaceLocal, pGetDDSurfaceLocal)
EXPORT_STUB(GetOLEThunkData, pGetOLEThunkData)
EXPORT_STUB(GetSurfaceFromDC, pGetSurfaceFromDC)
EXPORT_STUB(RegisterSpecialCase, pRegisterSpecialCase)
EXPORT_STUB(ReleaseDDThreadLock, pReleaseDDThreadLock)
EXPORT_STUB(SetAppCompatData, pSetAppCompatData)
