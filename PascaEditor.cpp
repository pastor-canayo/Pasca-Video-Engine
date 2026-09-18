#include "PascaEditor.h"
#include <iostream>
#include <vector>
#include <string>
#include <windows.h> // Native High-Performance Windows SDK

// --- THE NATIVE MULTIMEDIA DECODING CORES ---
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

using namespace std;

// --- GLOBAL ENGINE REGS ---
AVFormatContext* g_formatContext = nullptr;
AVCodecContext* g_codecContext = nullptr;
SwsContext* g_swsContext = nullptr;
AVPacket* g_packet = nullptr;
AVFrame* g_frame = nullptr;
AVFrame* g_rgbFrame = nullptr;
uint8_t* g_pixelBuffer = nullptr;
int g_videoStreamIndex = -1;
int g_videoWidth = 0;
int g_videoHeight = 0;
int g_paddedStride = 0; // Fixed Row Stride Byte Register

// BUTTON & TIMER EVENT WINDOW HANDLES
const int BTN_SPLIT_ID = 101;
const int BTN_DELETE_ID = 102;
const int PLAYBACK_TIMER_ID = 501;

// App tracking states
bool g_isClipSliced = false;
bool g_isClipDeleted = false;
bool g_isPlaying = false; // Tracks if playhead timeline is active

// Setup a clean memory pipeline to fetch the next valid video frame from your clip
bool readNextVideoFrame() {
    if (!g_formatContext || g_videoStreamIndex == -1) return false;
    while (av_read_frame(g_formatContext, g_packet) >= 0) {
        if (g_packet->stream_index == g_videoStreamIndex) {
            if (avcodec_send_packet(g_codecContext, g_packet) >= 0) {
                if (avcodec_receive_frame(g_codecContext, g_frame) == 0) {
                    // Force the conversion layout to write directly into our custom padded row memory array bounds
                    sws_scale(g_swsContext, (uint8_t const* const*)g_frame->data, g_frame->linesize,
                        0, g_videoHeight, g_rgbFrame->data, g_rgbFrame->linesize);
                    av_packet_unref(g_packet);
                    return true;
                }
            }
        }
        av_packet_unref(g_packet);
    }
    return false; // Loop end or file boundary reached cleanly
}

// Standard Window Callback message router loop
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        if (wmId == BTN_SPLIT_ID) {
            g_isClipSliced = true;
            cout << "[Action] Clip Split applied at current frame position." << endl;
            InvalidateRect(hwnd, nullptr, TRUE);
        }
        else if (wmId == BTN_DELETE_ID) {
            g_isClipDeleted = true;
            cout << "[Action] Clip deletion updated. Timeline layout sequence shifted." << endl;
            InvalidateRect(hwnd, nullptr, TRUE);
        }
        return 0;
    }

                   // --- HARDWARE INTERACTIVE KEYBOARD SHORTCUTS ---
    case WM_KEYDOWN: {
        if (wParam == VK_SPACE) { // Spacebar triggers Play / Pause toggles instantly!
            g_isPlaying = !g_isPlaying;
            if (g_isPlaying) {
                // Start a 33ms hardware clock pulse (~30 FPS playback refresh rate)
                SetTimer(hwnd, PLAYBACK_TIMER_ID, 33, nullptr);
                cout << "[Engine Control] Playback started." << endl;
            }
            else {
                // Stop the clock pulse immediately on pause commands
                KillTimer(hwnd, PLAYBACK_TIMER_ID);
                cout << "[Engine Control] Playback paused." << endl;
            }
            // Force status text layer refresh instantly
            RECT textStatusArea = { 50, 430, 800, 470 };
            InvalidateRect(hwnd, &textStatusArea, TRUE);
        }
        return 0;
    }

                   // --- HARDWARE REFRESH TIMING PULSE EVENT ---
    case WM_TIMER: {
        if (wParam == PLAYBACK_TIMER_ID) {
            // Fetch the next visual frame page in real-time
            if (readNextVideoFrame()) {
                // Instruct the viewport window rectangle area to update immediately
                RECT videoPreviewArea = { 50, 40, 750, 420 };
                InvalidateRect(hwnd, &videoPreviewArea, FALSE);
            }
            else {
                // Loop back straight to the beginning frame of the file if we hit the end
                cout << "[Engine Layout] Clip limit reached. Resetting playhead to frame 0." << endl;
                avio_seek(g_formatContext->pb, 0, SEEK_SET);
                avformat_seek_file(g_formatContext, g_videoStreamIndex, 0, 0, 0, AVSEEK_FLAG_ANY);
                avcodec_flush_buffers(g_codecContext);
                readNextVideoFrame();
                InvalidateRect(hwnd, nullptr, FALSE);
            }
        }
        return 0;
    }

    case WM_DESTROY:
        KillTimer(hwnd, PLAYBACK_TIMER_ID);
        PostQuitMessage(0);
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // 1. Draw background panel canvas
        HBRUSH brush = CreateSolidBrush(RGB(30, 33, 40));
        FillRect(hdc, &ps.rcPaint, brush);
        DeleteObject(brush);

        // 2. Paint the high-performance aligned video frame bits
        if (g_rgbFrame && g_rgbFrame->data) {
            BITMAPINFO bmi = {};
            bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth = g_videoWidth;
            bmi.bmiHeader.biHeight = -g_videoHeight; // Negative height flips the image upright
            bmi.bmiHeader.biPlanes = 1;
            bmi.bmiHeader.biBitCount = 24;
            bmi.bmiHeader.biCompression = BI_RGB;

            SetStretchBltMode(hdc, COLORONCOLOR);

            // Hardware Blit Painter: Passes our strict 4-byte packed raw texture array with zero distortion lines!
            StretchDIBits(hdc, 50, 40, 700, 380, 0, 0, g_videoWidth, g_videoHeight, g_rgbFrame->data, &bmi, DIB_RGB_COLORS, SRCCOPY);
        }

        // 3. Draw Timeline Sequence Rails
        HBRUSH timelineBgBrush = CreateSolidBrush(RGB(20, 22, 27));
        RECT timelineRect = { 50, 480, 950, 680 };
        FillRect(hdc, &timelineRect, timelineBgBrush);
        DeleteObject(timelineBgBrush);

        HFONT hTrackFont = CreateFont(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0, 0, DEFAULT_PITCH | FF_DONTCARE, TEXT("Segoe UI"));
        SelectObject(hdc, hTrackFont);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(140, 145, 160));
        TextOutA(hdc, 60, 495, "TRACK V1 (VIDEO)", 16);
        TextOutA(hdc, 60, 585, "TRACK A1 (AUDIO)", 16);

        HBRUSH videoClipBrush = CreateSolidBrush(RGB(230, 160, 35));
        HBRUSH audioClipBrush = CreateSolidBrush(RGB(35, 150, 230));

        if (!g_isClipDeleted) {
            if (!g_isClipSliced) {
                RECT clipV1 = { 200, 520, 900, 565 };
                FillRect(hdc, &clipV1, videoClipBrush);
            }
            else {
                RECT clipPart1 = { 200, 520, 450, 565 };
                RECT clipPart2 = { 460, 520, 900, 565 };
                FillRect(hdc, &clipPart1, videoClipBrush);
                FillRect(hdc, &clipPart2, videoClipBrush);
            }
            RECT clipA1 = { 200, 610, 900, 640 };
            FillRect(hdc, &clipA1, audioClipBrush);
        }
        else {
            RECT closedGapClip = { 200, 520, 640, 565 };
            FillRect(hdc, &closedGapClip, videoClipBrush);
            RECT closedGapAudio = { 200, 610, 640, 640 };
            FillRect(hdc, &closedGapAudio, audioClipBrush);
        }

        DeleteObject(videoClipBrush);
        DeleteObject(audioClipBrush);

        // Print status updates
        SetTextColor(hdc, RGB(0, 255, 150));
        if (g_isPlaying) {
            TextOutA(hdc, 60, 435, "[PLAYING STREAM - ACTIVE TIMELINE FRAME TICK PUMPING]", 53);
        }
        else {
            TextOutA(hdc, 60, 435, "[PAUSED - PRESS SPACEBAR TO PLAY TIMELINE]", 42);
        }
        DeleteObject(hTrackFont);

        EndPaint(hwnd, &ps);
        return 0;
    }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int main() {
    // 1. HARDWARE FILE PIPELINE INITIALIZATION
    string testVideoPath = "C:/Users/pasto/Desktop/CppProjects/MVI_4480.MP4";
    g_formatContext = avformat_alloc_context();
    if (avformat_open_input(&g_formatContext, testVideoPath.c_str(), nullptr, nullptr) != 0) return -1;
    if (avformat_find_stream_info(g_formatContext, nullptr) < 0) return -1;

    for (unsigned int i = 0; i < g_formatContext->nb_streams; i++) {
        if (g_formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            g_videoStreamIndex = i;
            break;
        }
    }
    if (g_videoStreamIndex == -1) return -1;

    AVCodecParameters* codecParams = g_formatContext->streams[g_videoStreamIndex]->codecpar;
    const AVCodec* codec = avcodec_find_decoder(codecParams->codec_id);
    g_codecContext = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(g_codecContext, codecParams);
    avcodec_open2(g_codecContext, codec, nullptr);

    g_videoWidth = g_codecContext->width;
    g_videoHeight = g_codecContext->height;

    g_packet = av_packet_alloc();
    g_frame = av_frame_alloc();
    g_rgbFrame = av_frame_alloc();

    // --- FIX THE ROW ALIGNMENT BOTTLENECK PERMANENTLY ---
    g_paddedStride = (g_videoWidth * 3 + 3) & ~3;
    int numBytes = g_paddedStride * g_videoHeight;
    g_pixelBuffer = (uint8_t*)av_malloc(numBytes * sizeof(uint8_t));
    av_image_fill_arrays(g_rgbFrame->data, g_rgbFrame->linesize, g_pixelBuffer, AV_PIX_FMT_RGB24, g_videoWidth, g_videoHeight, 4);

    g_swsContext = sws_getContext(g_videoWidth, g_videoHeight, g_codecContext->pix_fmt, g_videoWidth, g_videoHeight, AV_PIX_FMT_RGB24, SWS_BILINEAR, nullptr, nullptr, nullptr);

    readNextVideoFrame();
    
    // 2. LAUNCH DESKTOP COMPONENT GRAPHICS SHELL
    HINSTANCE hInstance = GetModuleHandle(nullptr);
    const wchar_t CLASS_NAME[] = L"PascaEditorWindow";
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(0, CLASS_NAME, L"Pasca Studio Core Editor Engine v1.0",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1024, 768, nullptr, nullptr, hInstance, nullptr);

    HWND btnSplit = CreateWindowW(L"BUTTON", L"SPLIT CLIP", 
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 
        780, 100, 180, 45, hwnd, (HMENU)(INT_PTR)BTN_SPLIT_ID, hInstance, nullptr);

    HWND btnDelete = CreateWindowW(L"BUTTON", L"DELETE SELECTED", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 780, 165, 180, 45, hwnd, (HMENU)(INT_PTR)BTN_DELETE_ID, hInstance, nullptr);

    ShowWindow(hwnd, SW_SHOW); 
    UpdateWindow(hwnd);

    MSG msg = {}; 
    while (GetMessage(&msg, nullptr, 0, 0))
    { 
        TranslateMessage(&msg); 
        DispatchMessage(&msg);
    }

    av_free(g_pixelBuffer); 
    av_frame_free(&g_rgbFrame); 
    av_frame_free(&g_frame); 
    av_packet_free(&g_packet); 
    sws_freeContext(g_swsContext); 
    avcodec_free_context(&g_codecContext); 
    avformat_close_input(&g_formatContext); 
    return 0;
}