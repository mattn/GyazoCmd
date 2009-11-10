#include <windows.h>
#include <atlbase.h>
#include <tchar.h>
#include <gdiplus.h>
#pragma comment (lib, "gdiplus.lib")
#pragma comment (lib, "user32.lib")
#pragma comment (lib, "gdi32.lib")
#include "wininet.h" 
#pragma comment(lib,"wininet.lib")
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <time.h>

HWND GetConsoleHwnd(void) {
	HWND hwnd;
	TCHAR newname[1024];
	TCHAR oldname[1024];

	GetConsoleTitle(oldname, sizeof(oldname));
	wsprintf(newname, _T("%d/%d"), GetTickCount(), GetCurrentProcessId());
	SetConsoleTitle(newname);
	Sleep(40);
	hwnd = FindWindow(NULL, newname);
	SetConsoleTitle(oldname);
	return hwnd;
}

int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
	UINT num = 0, size = 0;
	Gdiplus::GetImageEncodersSize(&num, &size);
	if (size == 0) return -1;

	Gdiplus::ImageCodecInfo* ici = new Gdiplus::ImageCodecInfo[size];
	if (ici == NULL) return -1;

	Gdiplus::GetImageEncoders(num, size, ici);
	for(UINT n = 0; n < num; ++n) {
		if (wcscmp(ici[n].MimeType, format) == 0) {
			*pClsid = ici[n].Clsid;
			break;
		}    
	}
	delete[] ici;
	return -1;
}

bool SavePNG(LPCTSTR fileName, HBITMAP newBMP) {
	USES_CONVERSION;
	Gdiplus::GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR gdiplusToken;
	CLSID clsidEncoder;

	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
	Gdiplus::Bitmap *bmp = new Gdiplus::Bitmap(newBMP, NULL);
	bool res = false;
	if (GetEncoderClsid(L"image/png", &clsidEncoder)) {
		res = (bmp->Save(T2W((LPTSTR)fileName), &clsidEncoder, 0) == 0);
	}
	delete bmp;
	Gdiplus::GdiplusShutdown(gdiplusToken);
	return res;
}

std::string getId() {
	const char* idFile = "id.txt";
	std::string idStr;
	std::ifstream ifs;
	ifs.open(idFile);
	if (! ifs.fail()) {
		ifs >> idStr;
		ifs.close();
	} else{
		char timebuf[64];
		struct tm dt;
		time_t now = time(NULL);
		localtime_s(&dt, &now);
		strftime(timebuf, 64, "%Y%m%d%H%M%S", &dt);
		idStr = timebuf;
		std::ofstream ofs;
		ofs.open(idFile);
		ofs << idStr;
		ofs.close();
	}
	return idStr;
}

bool UploadFile(LPCTSTR fileName) {
	const TCHAR* UPLOAD_SERVER = _T("gyazo.com");
	const TCHAR* UPLOAD_PATH = _T("/upload.cgi");
	const char* sBoundary = "----BOUNDARYBOUNDARY----";
	const TCHAR* szHeader =
		_T("Content-type: multipart/form-data; boundary=----BOUNDARYBOUNDARY----");
	std::ostringstream	buf;
	std::string idStr;
	idStr = getId();

	buf << "--" << sBoundary
		<< "\r\ncontent-disposition: form-data; name=\"id\"\r\n\r\n"
		<< idStr << "\r\n";

	buf << "--" << sBoundary
		<< "\r\ncontent-disposition: form-data; name=\"imagedata\"\r\n\r\n";

	std::ifstream png;
	png.open(fileName, std::ios::binary);
	if (png.fail()) {
		std::cerr << "Can't open png file." << std::endl;
		png.close();
		return false;
	}
	buf << png.rdbuf();
	png.close();

	buf << "\r\n--" << sBoundary << "--\r\n";

	std::string oMsg(buf.str());
	HINTERNET hSession = InternetOpen(_T("gyazocmd"), 
			INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
	if (NULL == hSession) {
		std::cerr << "Can't configure wininet." << std::endl;
		return false;
	}

	HINTERNET hConnection = InternetConnect(hSession,
			UPLOAD_SERVER, INTERNET_DEFAULT_HTTP_PORT,
			NULL, NULL, INTERNET_SERVICE_HTTP, 0, NULL);
	if (NULL == hSession) {
		std::cerr << "Can't connect server." << std::endl;
		return false;
	}

	HINTERNET hRequest = HttpOpenRequest(hConnection,
			_T("POST"), UPLOAD_PATH, NULL,
			NULL, NULL, INTERNET_FLAG_DONT_CACHE | INTERNET_FLAG_RELOAD, NULL);
	if(NULL == hSession) {
		std::cerr << "Can't compose post request." << std::endl;
		return false;
	}

	if (HttpSendRequest(hRequest,
				szHeader,
				lstrlen(szHeader),
				(LPVOID)oMsg.c_str(),
				(DWORD) oMsg.length())) {
		DWORD resLen = 8;
		TCHAR resCode[8];

		HttpQueryInfo(hRequest, HTTP_QUERY_STATUS_CODE, resCode, &resLen, 0);
		if( _ttoi(resCode) != 200 ) {
			std::cerr << "Failed to upload "
				"(unexpected result code, under maintainance?)" << std::endl;
		} else {
			DWORD len;
			char  resbuf[1024];
			std::string result;
			while(InternetReadFile(hRequest, (LPVOID) resbuf, 1024, &len) 
					&& len != 0) {
				result.append(resbuf, len);
			}
			std::cout << result << std::endl;
			return true;
		}
	} else {
		std::cerr << "Failed to upload" << std::endl;
	}
	return false;
}

int main(void) {
	HWND hwnd = GetConsoleHwnd();
	RECT r;
	GetWindowRect(hwnd, &r);
	int x, y, w, h;
	x = r.left;
	y = r.top;
	w = r.right - r.left;
	h = r.bottom - r.top;

	HDC hdc = GetDC(NULL);
	HBITMAP newBMP = CreateCompatibleBitmap(hdc, w, h);
	HDC newDC = CreateCompatibleDC(hdc);
	SelectObject(newDC, newBMP);

	TCHAR tmpDir[MAX_PATH], tmpFile[MAX_PATH];
	GetTempPath(MAX_PATH, tmpDir);
	GetTempFileName(tmpDir, _T("gya"), 0, tmpFile);

	BitBlt(newDC, 0, 0, w, h, hdc, x, y, SRCCOPY);
	if (SavePNG(tmpFile, newBMP)) {
		if (!UploadFile(tmpFile)) {
			std::cerr << "Can't upload image." << std::endl;
		}
	} else {
		std::cerr << "Can't save image." << std::endl;
	}

	DeleteFile(tmpFile);
	DeleteDC(newDC);
	DeleteObject(newBMP);
	ReleaseDC(NULL, hdc);
	return 0;
}
