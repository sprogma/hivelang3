#include <windows.h>
#include <stdio.h>

#define RECT_SIZE 50
#define MOVE_SPEED 5

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {    
    // Используем встроенный класс статического элемента
    // "Static" - простейший класс окна, который можно использовать
    const char CLASS_NAME[] = "Static";
    
    HWND hwnd = CreateWindowEx(
        WS_EX_CLIENTEDGE,                    // Расширенный стиль
        CLASS_NAME,                          // Встроенный класс окна
        "Polling Only - Use Arrow Keys, ESC to exit",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,    // Основной стиль
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        NULL, NULL, hInstance, NULL
    );
    
    if (hwnd == NULL) {
        MessageBox(NULL, "Failed to create window", "Error", MB_ICONERROR);
        return 1;
    }
    
    
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
    HBITMAP hbmOld = (HBITMAP)SelectObject(hdcBuffer, hbmBuffer);
    
    // Шрифт для текста
    HFONT hFont = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                            DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial");
    HFONT hOldFont = (HFONT)SelectObject(hdcBuffer, hFont);
    
    // Цвета
    HBRUSH hBackgroundBrush = CreateSolidBrush(RGB(240, 240, 240));
    HBRUSH hRectBrush = CreateSolidBrush(RGB(0, 120, 215));
    HPEN hRectPen = CreatePen(PS_SOLID, 2, RGB(0, 0, 0));
    
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
            SelectObject(hdcBuffer, hbmOld);
            DeleteObject(hbmBuffer);
            
            width = newClientRect.right - newClientRect.left;
            height = newClientRect.bottom - newClientRect.top;
            
            if (width > 0 && height > 0) {
                hbmBuffer = CreateCompatibleBitmap(hdcScreen, width, height);
                hbmOld = (HBITMAP)SelectObject(hdcBuffer, hbmBuffer);
                SelectObject(hdcBuffer, hFont);
            }
            
            clientRect = newClientRect;
        }
        
        // Polling клавиш
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
            running = FALSE;
        }
        
        // Перемещение прямоугольника
        if (GetAsyncKeyState(VK_LEFT) & 0x8000) rectX -= MOVE_SPEED;
        if (GetAsyncKeyState(VK_RIGHT) & 0x8000) rectX += MOVE_SPEED;
        if (GetAsyncKeyState(VK_UP) & 0x8000) rectY -= MOVE_SPEED;
        if (GetAsyncKeyState(VK_DOWN) & 0x8000) rectY += MOVE_SPEED;
        
        // Ограничение движения
        if (rectX < 0) rectX = 0;
        if (rectY < 0) rectY = 0;
        if (rectX > width - RECT_SIZE) rectX = width - RECT_SIZE;
        if (rectY > height - RECT_SIZE) rectY = height - RECT_SIZE;
        
        // ========== РИСОВАНИЕ ==========
        
        // Очищаем весь буфер
        FillRect(hdcBuffer, &clientRect, hBackgroundBrush);
        
        // Рисуем прямоугольник
        RECT rect = {rectX, rectY, rectX + RECT_SIZE, rectY + RECT_SIZE};
        FillRect(hdcBuffer, &rect, hRectBrush);
        
        // Рисуем рамку
        HPEN hOldPen = (HPEN)SelectObject(hdcBuffer, hRectPen);
        HBRUSH hOldBrush = (HBRUSH)SelectObject(hdcBuffer, GetStockObject(NULL_BRUSH));
        Rectangle(hdcBuffer, rectX, rectY, rectX + RECT_SIZE, rectY + RECT_SIZE);
        SelectObject(hdcBuffer, hOldBrush);
        SelectObject(hdcBuffer, hOldPen);
        
        // Текст с информацией
        SetBkMode(hdcBuffer, TRANSPARENT);
        SetTextColor(hdcBuffer, RGB(0, 0, 0));
        
        char info[256];
        
        // Позиция
        sprintf(info, "Position: (%d, %d)", rectX, rectY);
        TextOut(hdcBuffer, 10, 10, info, strlen(info));
        
        // Управление
        TextOut(hdcBuffer, 10, 30, "Arrow Keys: Move rectangle", 26);
        TextOut(hdcBuffer, 10, 50, "ESC: Exit", 9);
        
        // Состояние клавиш
        sprintf(info, "Keys: Left[%c] Right[%c] Up[%c] Down[%c]",
            (GetAsyncKeyState(VK_LEFT) & 0x8000) ? 'X' : ' ',
            (GetAsyncKeyState(VK_RIGHT) & 0x8000) ? 'X' : ' ',
            (GetAsyncKeyState(VK_UP) & 0x8000) ? 'X' : ' ',
            (GetAsyncKeyState(VK_DOWN) & 0x8000) ? 'X' : ' ');
        TextOut(hdcBuffer, 10, 70, info, strlen(info));
        
        // FPS
        frameCount++;
        DWORD currentTime = GetTickCount();
        if (currentTime - lastTime >= 1000) {
            fps = frameCount * 1000.0f / (currentTime - lastTime);
            frameCount = 0;
            lastTime = currentTime;
        }
        sprintf(info, "FPS: %.1f", fps);
        TextOut(hdcBuffer, 10, 90, info, strlen(info));
        
        // Размер окна
        sprintf(info, "Window: %d x %d", width, height);
        TextOut(hdcBuffer, 10, 110, info, strlen(info));
        
        // Копируем буфер на экран
        BitBlt(hdcScreen, 0, 0, width, height, hdcBuffer, 0, 0, SRCCOPY);
        
        // Небольшая пауза
        Sleep(16);
    }
    
    // Очистка ресурсов
    SelectObject(hdcBuffer, hOldFont);
    SelectObject(hdcBuffer, hbmOld);
    DeleteObject(hFont);
    DeleteObject(hbmBuffer);
    DeleteObject(hBackgroundBrush);
    DeleteObject(hRectBrush);
    DeleteObject(hRectPen);
    DeleteDC(hdcBuffer);
    ReleaseDC(hwnd, hdcScreen);
    
    DestroyWindow(hwnd);
    
    return 0;
}
