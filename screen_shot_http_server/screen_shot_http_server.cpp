// Copyright (c) 2016 by Suwings
// All rights reserved

#include <iostream>
#include "mongoose.h"
#include <Windows.h>

using namespace std;

#pragma warning(disable: 4996)

#define IMAGE_NAME "/screen.bmp"

static char *s_http_port = "11011";

static struct mg_serve_http_opts s_http_server_opts;

void SaveScreanWindow();
void ScreenWindow(LPCTSTR fileName);

static void ev_handler(struct mg_connection *nc, int ev, void *p)
{
	struct http_message *hm = (struct http_message *) p;
	if (ev == MG_EV_HTTP_REQUEST)
	{
		if (mg_vcmp(&hm->uri, IMAGE_NAME) == 0) {
			SaveScreanWindow();
		}
		mg_serve_http(nc, (struct http_message *) p, s_http_server_opts);
	}
}

int main(void) {
	struct mg_mgr mgr;
	struct mg_connection *nc;

	SaveScreanWindow();
	mg_mgr_init(&mgr, NULL);

	nc = mg_bind(&mgr, s_http_port, ev_handler);
	if (nc == NULL) {
		printf("Failed to create listener\n");
		return 1;
	}

	// Set up HTTP server parameters
	mg_set_protocol_http_websocket(nc);
	s_http_server_opts.document_root = ".";  // Serve current directory
	s_http_server_opts.enable_directory_listing = "yes";    //Set if show dir
	for (;;) {
		mg_mgr_poll(&mgr, 1000);
	}
	mg_mgr_free(&mgr);

	return 0;
}

void SaveScreanWindow()
{
	//Get bmpDataFile path
	TCHAR path[MAX_PATH]; //Make it large enough
	GetModuleFileName(0, path, MAX_PATH);
	std::string str_path = path;
	size_t index = str_path.find_last_of("\\");
	if (index != std::string::npos)
	{
		str_path = str_path.substr(0, index);
	}
	str_path += IMAGE_NAME;
	printf("path: %s\n", str_path.c_str());
	ScreenWindow(str_path.c_str());
}

void ScreenWindow(LPCTSTR fileName)
{
	HWND window = GetDesktopWindow();

	if (window == NULL)
	{
		return;
	}
	HDC _dc = ::GetDC(window);

	if (_dc == NULL)
	{
		//MessageBox(0,"_dc is empty","提示信息",4);
		//OutputDebugString("_dc is empty");
		return;
	}

	RECT re;
	GetWindowRect(window, &re);
	int width = re.right - re.left;
	int	height = re.bottom - re.top;

	HDC memDC = ::CreateCompatibleDC(_dc);//内存DC
	if (memDC == NULL)
	{
		return;
	}
	HBITMAP memBitmap = ::CreateCompatibleBitmap(_dc, width, height);
	if (memBitmap == NULL)
	{
		return;
	}
	HBITMAP oldmemBitmap = (HBITMAP)::SelectObject(memDC, memBitmap);//将memBitmap选入内存DC
	if (oldmemBitmap == NULL)
	{
		return;
	}

	typedef BOOL(__stdcall *pfPrintWindow)(HWND hWnd, HDC hdcBlt, UINT nFlags);

	HMODULE handle;
	handle = LoadLibrary("user32.dll");
	pfPrintWindow pPrintWindow;
	if (handle)
	{
		pPrintWindow = (pfPrintWindow)::GetProcAddress(handle, "PrintWindow");
	}

	BOOL bret = pPrintWindow(window, memDC, 0);
	//BOOL bret = PrintWindow(window, memDC, 0);
	if (!bret)
	{
		BitBlt(memDC, 0, 0, width, height, _dc, 0, 0, SRCCOPY);//图像宽度高度和截取位置
	}
	else
	{
		if (handle)
			FreeLibrary(handle);
		return;
	}
	if (handle)
		FreeLibrary(handle);

	BITMAP bmp;
	::GetObject(memBitmap, sizeof(BITMAP), &bmp);;//获得位图信息

	BITMAPFILEHEADER bfh;
	bfh.bfType = MAKEWORD('B', 'M');
	bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);//到位图数据的偏移量
	bfh.bfSize = bfh.bfOffBits + bmp.bmWidthBytes * bmp.bmHeight;//文件总的大小

	BITMAPINFOHEADER bih;
	bih.biSize = sizeof(BITMAPINFOHEADER);
	bih.biWidth = bmp.bmWidth;//宽度
	bih.biHeight = bmp.bmHeight;//高度
	bih.biPlanes = 1;
	bih.biBitCount = bmp.bmBitsPixel;//每个像素字节大小
	bih.biCompression = BI_RGB;
	bih.biSizeImage = bmp.bmWidthBytes * bmp.bmHeight;//图像数据大小

	byte * buf = new byte[bmp.bmWidthBytes * bmp.bmHeight];//申请内存保存位图数据
	GetDIBits(memDC, (HBITMAP)memBitmap, 0, height, buf, (LPBITMAPINFO)&bih, DIB_RGB_COLORS);//获取位图数据

	DWORD r;
	void* f = CreateFile(fileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, 0);
	WriteFile(f, &bfh, sizeof(bfh), &r, NULL);
	WriteFile(f, &bih, sizeof(bih), &r, NULL);
	WriteFile(f, buf, bmp.bmWidthBytes * bmp.bmHeight, &r, NULL);

	CloseHandle(f);
	DeleteDC(_dc);
	DeleteDC(memDC);
	::DeleteObject(memBitmap);

	delete[] buf;
}