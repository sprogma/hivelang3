#include <windows.h>
#include <stdio.h>

#define RECT_SIZE 50
#define MOVE_SPEED 5

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    const char CLASS_NAME[] = "PollingOnlyWindow";
    
    WNDCLASS wc = {0};
    wc.lpfnWndProc = DefWindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    
    RegisterClass(&wc);
    
    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        "Polling Only - Use Arrow Keys, ESC to exit",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        NULL, NULL, hInstance, NULL
    );
    
    if (hwnd == NULL) return 0;
    
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    
    // Позиция прямоугольника
    int rectX = 100;
    int rectY = 100;
    
    // Для двойной буферизации
    HDC hdcScreen = GetDC(hwnd);
    HDC hdcBuffer = CreateCompatibleDC(hdcScreen);
    
    RECT clientRect;
    GetClientRect(hwnd, &clientRect);
    int width = clientRect.right - clientRect.left;
    int height = clientRect.bottom - clientRect.top;
    
    HBITMAP hbmBuffer = CreateCompatibleBitmap(hdcScreen, width, height);
    SelectObject(hdcBuffer, hbmBuffer);
    
    // Шрифт для текста
    HFONT hFont = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                            DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial");
    SelectObject(hdcBuffer, hFont);
    
    // Main loop с polling
    MSG msg;
    BOOL running = TRUE;

    DWORD lastTime = GetTickCount();
    int frameCount = 0;
    float fps = 0;
    
    while (running) {
        // Polling сообщений (неблокирующий)
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                running = FALSE;
            }
            
            if (msg.message == WM_DESTROY) {
                PostQuitMessage(0);
            }
            
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        
        // Обработка изменения размера окна
        RECT newClientRect;
        GetClientRect(hwnd, &newClientRect);
        if (newClientRect.right - newClientRect.left != width || 
            newClientRect.bottom - newClientRect.top != height) {
            
            // Пересоздаем буфер под новый размер
            DeleteObject(hbmBuffer);
            width = newClientRect.right - newClientRect.left;
            height = newClientRect.bottom - newClientRect.top;
            hbmBuffer = CreateCompatibleBitmap(hdcScreen, width, height);
            SelectObject(hdcBuffer, hbmBuffer);
            SelectObject(hdcBuffer, hFont);
            clientRect = newClientRect;
        }
        
        // Polling клавиш - основная логика
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
            PostQuitMessage(0);
            running = FALSE;
        }
        
        // Сохраняем старую позицию для частичной перерисовки
        int oldX = rectX;
        int oldY = rectY;
        
        // Перемещение прямоугольника
        if (GetAsyncKeyState(VK_LEFT) & 0x8000) rectX -= MOVE_SPEED;
        if (GetAsyncKeyState(VK_RIGHT) & 0x8000) rectX += MOVE_SPEED;
        if (GetAsyncKeyState(VK_UP) & 0x8000) rectY -= MOVE_SPEED;
        if (GetAsyncKeyState(VK_DOWN) & 0x8000) rectY += MOVE_SPEED;
        
        // Ограничение движения в пределах окна
        if (rectX < 0) rectX = 0;
        if (rectY < 0) rectY = 0;
        if (rectX > width - RECT_SIZE) rectX = width - RECT_SIZE;
        if (rectY > height - RECT_SIZE) rectY = height - RECT_SIZE;
        
        // ========== РИСОВАНИЕ В БУФЕР ==========
        
        // Очищаем только измененные области (оптимизация)
        if (oldX != rectX || oldY != rectY) {
            // Очищаем старую позицию прямоугольника
            RECT oldRect = {oldX, oldY, oldX + RECT_SIZE, oldY + RECT_SIZE};
            FillRect(hdcBuffer, &oldRect, (HBRUSH)(COLOR_WINDOW + 1));
            
            // Очищаем новую позицию (на случай перекрытия)
            RECT newRect = {rectX, rectY, rectX + RECT_SIZE, rectY + RECT_SIZE};
            FillRect(hdcBuffer, &newRect, (HBRUSH)(COLOR_WINDOW + 1));
            
            // Очищаем область текста
            RECT textRect = {0, 0, width, 60};
            FillRect(hdcBuffer, &textRect, (HBRUSH)(COLOR_WINDOW + 1));
        } else {
            // Если позиция не изменилась, очищаем только область текста
            RECT textRect = {0, 0, width, 60};
            FillRect(hdcBuffer, &textRect, (HBRUSH)(COLOR_WINDOW + 1));
        }
        
        // Рисуем прямоугольник в буфере
        RECT rect = {rectX, rectY, rectX + RECT_SIZE, rectY + RECT_SIZE};
        HBRUSH brush = CreateSolidBrush(RGB(0, 120, 215));
        FillRect(hdcBuffer, &rect, brush);
        DeleteObject(brush);
        
        // Рисуем рамку вокруг прямоугольника
        HPEN hPen = CreatePen(PS_SOLID, 2, RGB(0, 0, 0));
        SelectObject(hdcBuffer, hPen);
        SelectObject(hdcBuffer, GetStockObject(NULL_BRUSH));
        Rectangle(hdcBuffer, rectX, rectY, rectX + RECT_SIZE, rectY + RECT_SIZE);
        DeleteObject(hPen);
        
        // Отображаем информацию
        SetBkMode(hdcBuffer, TRANSPARENT);
        SetTextColor(hdcBuffer, RGB(0, 0, 0));
        
        char info[100];
        sprintf(info, "Position: %d, %d | Use Arrow Keys | ESC to exit", rectX, rectY);
        TextOut(hdcBuffer, 10, 10, info, strlen(info));
        
        char keysInfo[200];
        sprintf(keysInfo, "Keys: LEFT=%s RIGHT=%s UP=%s DOWN=%s",
            (GetAsyncKeyState(VK_LEFT) & 0x8000) ? "PRESSED" : "      ",
            (GetAsyncKeyState(VK_RIGHT) & 0x8000) ? "PRESSED" : "       ",
            (GetAsyncKeyState(VK_UP) & 0x8000) ? "PRESSED" : "    ",
            (GetAsyncKeyState(VK_DOWN) & 0x8000) ? "PRESSED" : "      ");
        TextOut(hdcBuffer, 10, 30, keysInfo, strlen(keysInfo));
        
        char fpsInfo[50];
        
        frameCount++;
        DWORD currentTime = GetTickCount();
        if (currentTime - lastTime >= 1000) {
            fps = frameCount * 1000.0f / (currentTime - lastTime);
            frameCount = 0;
            lastTime = currentTime;
        }
        sprintf(fpsInfo, "FPS: %.1f", fps);
        TextOut(hdcBuffer, 10, 50, fpsInfo, strlen(fpsInfo));
        
        // ========== КОПИРОВАНИЕ БУФЕРА НА ЭКРАН ==========
        BitBlt(hdcScreen, 0, 0, width, height, hdcBuffer, 0, 0, SRCCOPY);
        
        // Небольшая пауза для контроля скорости
        Sleep(16); // ~60 FPS
    }
    
    // Очистка ресурсов
    DeleteObject(hFont);
    DeleteObject(hbmBuffer);
    DeleteDC(hdcBuffer);
    ReleaseDC(hwnd, hdcScreen);
    
    return (int)msg.wParam;
}
