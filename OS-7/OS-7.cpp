/*
 * Dependencies:
 *  gdi32
 *  (kernel32)
 *  user32
 *  (comctl32)
 *
 */

#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <tchar.h>

#include <iostream>
#include <string>
#include <fstream>

using namespace std;

#define KEY_SHIFTED     0x8000
#define KEY_TOGGLED     0x0001
#define FIGUREAMOUNT    100
#define STACK_SIZE (64*1024) // Размер стека для нового потока

TCHAR szName[] = TEXT("SettingsMappingObject");
TCHAR szGameName[] = TEXT("GameMappingObject");

const TCHAR szWinClass[] = _T("Win32SampleApp");
const TCHAR szWinName[] = _T("Win32SampleWindow");
HWND hwnd;               /* This is the handle for our window */
HBRUSH hBrush;           /* Current brush */
HDC hdc;
HPEN hPen;
UINT figChange;
UINT setChange;
UINT backChange;
RECT rect;

char* buff;
bool isGivenN = false;

boolean lDown = 0;
boolean rDown = 0;

int xMousePos = 0; //позиция мышки по х
int yMousePos = 0; //позиция мышки по у
int zDelta = 0; //направление колеса мышки

//Настройки ширины, высоты, размер сетки, цвет сетки, цвет фона по умолчанию
int width = 320;
int height = 240;
int n = 40;
int scrollR = 0;
int scrollG = 255;
int scrollB = 255;
int randR = 0;
int randG = 0;
int randB = 255;

int* gameMap;
//int appointed = 1;
bool changed = false;

HANDLE backgroundLock; //Мьютекс изменения фона
bool lockFlag = false; //Заблокирован ли пользователем мьютекс
bool isBackground = false; //Находится ли поток на фоновом приоритете

/* Функция запуска блокнота */
void RunNotepad(void)
{
    STARTUPINFO sInfo;
    PROCESS_INFORMATION pInfo;

    ZeroMemory(&sInfo, sizeof(STARTUPINFO));

    if (!CreateProcess(_T("C:\\Windows\\Notepad.exe"),
        NULL, NULL, NULL, FALSE, 0, NULL, NULL, &sInfo, &pInfo)) {
        MessageBox(NULL, L"Не удалось открыть блокнот.", L"Ошибка", MB_OK);
    }
    else {
        puts("Starting Notepad...");
    }
}

void GetSettings(char* buffer, bool flag)
{
    int countF = 0;
    //Переводим в string
    string settings = buffer;

    //Заменяем все концы строк на пробелы (для более удобной работы)
    while (settings.find("\r\n") != string::npos)
    {
        settings[settings.find("\r\n")] = ' ';
    }

    int start = 0;
    int end;
    string curLine;

    //Ввод настроек
    for (int i = 0; i < settings.length(); i++) {
        if (settings[i] == ' ') {
            end = i;
            curLine = settings.substr(start, (end - start));
            if (settings[i + 1] != NULL) { start = i + 1; }
            switch (countF) {
            case 0: {width = stoi(curLine); break; }
            case 1: {height = stoi(curLine); break; }
            case 2: {if (flag == 0) { n = stoi(curLine); }; break; }
            case 3: {scrollR = stoi(curLine); break; }
            case 4: {scrollG = stoi(curLine); break; }
            case 5: {scrollB = stoi(curLine); break; }
            case 6: {randR = stoi(curLine); break; }
            case 7: {randG = stoi(curLine); break; }
            case 8: {randB = stoi(curLine); break; }
            }
            countF++;
        }
    }
}

void WriteSettings(char* buffer)
{
    string set = to_string(width) + "\r\n" + to_string(height) + "\r\n" + to_string(n) + "\r\n" + to_string(scrollR) + "\r\n" + to_string(scrollG) + "\r\n" + to_string(scrollB) + "\r\n" + to_string(randR) + "\r\n" + to_string(randG) + "\r\n" + to_string(randB) + "\r\n";
    const int length = set.length();

    char* set_char = new char[length + 1];
    strcpy(set_char, set.c_str());
    size_t size = strlen(set_char);

    sprintf(buffer, set_char);
}

//Поток отрисовки фона (не рисует решетку или фигуры)
DWORD WINAPI background(LPVOID param) {
    HWND hwnd = (HWND)param;
    HBRUSH hBrush;
    PAINTSTRUCT ps;
    int colSwitch = 0; //Переключатель градиента
    int backR = 0; //Красный
    int backG = 128; //Зелёный
    int backB = 128; //Синий
    while (true) {
        WaitForSingleObject(backgroundLock, INFINITE); //Мьютекс, если был нажат Enter

        /*
        Переключатель градиента.
        Добавлет 1 к одному цвету, отнимает 1 у другого.
        Если один цвет переполнен, то переключается на другой.
        Переключение через colSwitch.
        */
        switch (colSwitch)
        {
        case 0:
            if (backR < 255)
            {
                backR++;
                if (backB > 0) backB--;
            }
            else colSwitch = 1;
            break;
        case 1:
            if (backG < 255)
            {
                backG++;
                if (backR > 0) backR--;
            }
            else colSwitch = 2;
            break;
        case 2:
            if (backB < 255)
            {
                backB++;
                if (backG > 0) backG--;
            }
            else colSwitch = 0;
            break;
        }
        //cout << "Colors are:" << backR << " " << backG << " " << backB << endl; //Вывод текущего цвета в консоль
        hdc = BeginPaint(hwnd, &ps); //Начинаем красить
        hBrush = CreateSolidBrush(RGB(backR, backG, backB)); //Новая кисть для фона
        DeleteObject((HBRUSH)SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)hBrush)); //Подмена кисточки фона. Получаем старую кисть, которую тут же удаляем. Иначе память переполниться
        EndPaint(hwnd, &ps); //Заканчиваем красить
        InvalidateRect(hwnd, NULL, TRUE); //Говорим окну перекраситься (вызывает сообщение WM_PAINT)
        ReleaseMutex(backgroundLock); //Отпускаем мьютекс, чтобы Enter работал
        Sleep(100); //Отдыхаем
    }
    cout << "Oops, thread broke.\n"; //Будет странно, если код сюда дойдёт.
    return 0;
}

/*  This function is called by the Windows function DispatchMessage()  */
LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    COLORREF scrollColorf = RGB(
        (BYTE)(scrollR), // red component of color
        (BYTE)(scrollG), // green component of color
        (BYTE)(scrollB) // blue component of color
    );
    PAINTSTRUCT ps;
    if (message == setChange) {
        GetSettings(buff, isGivenN);
        SetWindowPos(hwnd, NULL, 0, 0, width, height, SWP_NOMOVE);
        InvalidateRect(hwnd, NULL, TRUE);
    }
    else if (message == figChange) {
        InvalidateRect(hwnd, NULL, TRUE);
    }
    else if (message == backChange) {
        GetSettings(buff, isGivenN);
        COLORREF randColorf = RGB(
            (BYTE)(randR), // red component of color
            (BYTE)(randG), // green component of color
            (BYTE)(randB) // blue component of color
        );
        hBrush = CreateSolidBrush(randColorf);
        DeleteObject((HBRUSH)SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)hBrush));
        InvalidateRect(hwnd, NULL, TRUE);
    }
    switch (message)                  /* handle the messages */
    {
    case WM_SIZE:
        if (GetWindowRect(hwnd, &rect))
        {
            width = rect.right - rect.left;
            height = rect.bottom - rect.top;
            WriteSettings(buff);
            PostMessage(HWND_BROADCAST, setChange, NULL, NULL);
        }
        return 0;
    case WM_KEYDOWN: //Отработка нажатия клавиатуры
        switch (wParam) 
        {
        case VK_RETURN:
            if (lockFlag) {
                lockFlag = false;
                ReleaseMutex(backgroundLock);
                cout << "Поток разблокирован\n";
            }
            else {
                lockFlag = true;
                WaitForSingleObject(backgroundLock, INFINITE);
                cout << "Поток заблокирован\n";
            }
            break;
        default:
            if (isBackground) {
                SetThreadPriority(background, THREAD_MODE_BACKGROUND_END);
                isBackground = false;
            }
            break;
        case 0x31:
            SetThreadPriority(background, THREAD_MODE_BACKGROUND_BEGIN);
            isBackground = true;
            cout << "Фоновый приоритет потока\n";
            break;
        case 0x32:
            SetThreadPriority(background, THREAD_PRIORITY_LOWEST);
            cout << "Низший приоритет потока\n";
            break;
        case 0x33:
            SetThreadPriority(background, THREAD_PRIORITY_BELOW_NORMAL);
            cout << "Ниже нормального приоритет потока\n";
            break;
        case 0x34:
            SetThreadPriority(background, THREAD_PRIORITY_NORMAL);
            cout << "Нормальный приоритет потока\n";
            break;
        case 0x35:
            SetThreadPriority(background, THREAD_PRIORITY_ABOVE_NORMAL);
            cout << "Выше нормального приоритет потока\n";
            break;
        case 0x36:
            SetThreadPriority(background, THREAD_PRIORITY_HIGHEST);
            cout << "Наивысочайший приоритет потока\n";
            break;
        case 0x37:
            SetThreadPriority(background, THREAD_PRIORITY_TIME_CRITICAL);
            cout << "Критический приоритет потока\n";
            break;
        }

        return 0;
    case WM_MOUSEWHEEL: //Если прокручено колёсико мышки
        zDelta = GET_WHEEL_DELTA_WPARAM(wParam); //Берем направление колесика
        if (zDelta == 120) //Если прокрутка вверх
        {
            if (scrollR < 255) scrollR++;
            else if (scrollG < 255) scrollG++;
            else if (scrollB < 255) scrollB++;
            InvalidateRect(hwnd, NULL, TRUE);
        }
        else //Если прокрутка вниз
        {
            if (scrollR > 0) scrollR--;
            else if (scrollG > 0) scrollG--;
            else if (scrollB > 0) scrollB--;
            InvalidateRect(hwnd, NULL, TRUE);
        }
        WriteSettings(buff);
        PostMessage(HWND_BROADCAST, setChange, NULL, NULL);
        return 0;
    case WM_LBUTTONDOWN: //Если зажата левая кнопка мышки запомнить местоположение курсора
        xMousePos = LOWORD(lParam);
        yMousePos = HIWORD(lParam);
        lDown = 1;
        return 0;
    case WM_LBUTTONUP: //Если отжата левая кнопка мышки нарисовать круг
        if (lDown)
        {
            bool exists = false;
            xMousePos = (xMousePos / n) * n + n / 2;
            yMousePos = (yMousePos / n) * n + n / 2;
            for (int i = 1; i < gameMap[0]; i++) {
                if (gameMap[i * 3] == xMousePos and gameMap[i * 3 + 1] == yMousePos) {
                    exists = true;
                    break;
                }
            }
            if (exists) {
                MessageBox(NULL, L"Фигура уже существует в данном поле!", L"Ошибка", MB_OK);
            }
            else {
                hdc = GetDC(hwnd);
                hPen = CreatePen(PS_SOLID, 3, RGB(255, 0, 0));
                hBrush = CreateSolidBrush(RGB(255, 255, 255));
                SelectObject(hdc, hBrush);
                SelectObject(hdc, hPen);
                Ellipse(hdc, xMousePos - n / 2, yMousePos - n / 2, xMousePos + n / 2, yMousePos + n / 2);
                DeleteObject(hBrush);
                DeleteObject(hPen);
                ReleaseDC(hwnd, hdc);
                gameMap[gameMap[0] * 3] = xMousePos;
                gameMap[gameMap[0] * 3 + 1] = yMousePos;
                gameMap[gameMap[0] * 3 + 2] = 0;
                gameMap[0]++;
                changed = true;
                ReleaseDC(hwnd, hdc);
            }

        }
        lDown = 0;
        PostMessage(HWND_BROADCAST, figChange, NULL, NULL);
        return 0;
    case WM_RBUTTONDOWN: //Если зажата правая кнопка мышки запомнить местоположение курсора
        xMousePos = LOWORD(lParam);
        yMousePos = HIWORD(lParam);
        rDown = 1;
        return 0;
    case WM_RBUTTONUP: //Если отжата правая кнопка мышки нарисовать крест
        if (rDown)
        {
            //Левый верхний угол
            bool exists = false;
            int xMousePos_ch = (xMousePos / n) * n + n / 2;
            int yMousePos_ch = (yMousePos / n) * n + n / 2;

            xMousePos = (xMousePos / n) * n;
            yMousePos = (yMousePos / n) * n;

            for (int i = 1; i < gameMap[0]; i++) {
                if (gameMap[i * 3] == xMousePos_ch and gameMap[i * 3 + 1] == yMousePos_ch) {
                    exists = true;
                    break;
                }
            }
            if (exists) {
                MessageBox(NULL, L"Фигура уже существует в данном поле!", L"Ошибка", MB_OK);
            }
            else {
                hdc = GetDC(hwnd);
                hPen = CreatePen(PS_SOLID, 3, RGB(0, 255, 0));
                SelectObject(hdc, hPen);
                MoveToEx(hdc, xMousePos, yMousePos, NULL);
                LineTo(hdc, xMousePos + n, yMousePos + n);
                MoveToEx(hdc, xMousePos + n, yMousePos, NULL);
                LineTo(hdc, xMousePos, yMousePos + n);
                DeleteObject(hPen);
                ReleaseDC(hwnd, hdc);
                gameMap[gameMap[0] * 3] = xMousePos_ch;
                gameMap[gameMap[0] * 3 + 1] = yMousePos_ch;
                gameMap[gameMap[0] * 3 + 2] = 1;
                gameMap[0]++;
                changed = true;
                ReleaseDC(hwnd, hdc);
            }
        }
        rDown = 0;
        PostMessage(HWND_BROADCAST, figChange, NULL, NULL);
        return 0;
    case WM_PAINT: //Фунцкия рисования фона
        PAINTSTRUCT ps;
        hdc = BeginPaint(hwnd, &ps);
        scrollColorf = RGB( //Цвет сетки
            (BYTE)(scrollR), // red component of color
            (BYTE)(scrollG), // green component of color
            (BYTE)(scrollB) // blue component of color
        );
        hPen = CreatePen(PS_SOLID, 3, scrollColorf);
        SelectObject(hdc, hPen);
        //Начинаем рисовать сетку
        for (int i = 0; i <= width; i += n) {
            MoveToEx(hdc, i, 0, NULL);
            LineTo(hdc, i, height);
        }
        for (int i = 0; i <= height; i += n) {
            MoveToEx(hdc, 0, i, NULL);
            LineTo(hdc, width, i);
        }
        //Сетка нарисована
        DeleteObject(hPen);

        hBrush = CreateSolidBrush(RGB(255, 255, 255));
        SelectObject(hdc, hBrush);

        int xPos, yPos;
        for (int i = 1; i < gameMap[0]; i++) {
            xPos = gameMap[i * 3];
            yPos = gameMap[i * 3 + 1];
            if (gameMap[i * 3 + 2] == 0)
            {
                hPen = CreatePen(PS_SOLID, 3, RGB(255, 0, 0));
                SelectObject(hdc, hPen);
                Ellipse(hdc, xPos - n / 2, yPos - n / 2, xPos + n / 2, yPos + n / 2);
                DeleteObject(hPen);
            }
            else if (gameMap[i * 3 + 2] == 1)
            {
                xPos -= n / 2;
                yPos -= n / 2;
                hPen = CreatePen(PS_SOLID, 3, RGB(0, 255, 0));
                SelectObject(hdc, hPen);
                MoveToEx(hdc, xPos, yPos, NULL);
                LineTo(hdc, xPos + n, yPos + n);
                MoveToEx(hdc, xPos + n, yPos, NULL);
                LineTo(hdc, xPos, yPos + n);
                DeleteObject(hPen);
            }
        }

        //Отчистка
        DeleteObject(hBrush);
        EndPaint(hwnd, &ps);
        return 0;
    case WM_DESTROY: //Уничтожение окна
        PostQuitMessage(0);       /* send a WM_QUIT to the message queue */
        return 0;
    }

    /* for messages that we don't deal with */
    return DefWindowProc(hwnd, message, wParam, lParam);
}

int main(int argc, char** argv)
{
    SetConsoleCP(1251);
    SetConsoleOutputCP(1251);
    LPWSTR* szArgList;
    int argCount;

    /*
    * Ввод аргумента:
    * os-3.exe [тип ввода] [размер сетки]
    */
    szArgList = CommandLineToArgvW(GetCommandLine(), &argCount); //Получаем аргумент командной строки
    if (szArgList == NULL) //Если не смогли получить аргумент
    {
        MessageBox(NULL, L"Ошибка получения аргумента командной строки", L"Ошибка", MB_OK);
        return 10;
    }
    if (argCount == 2) {
        try {
            n = stoi(szArgList[1]); //Тип ввода от аргумента
            if (n <= 0) {
                MessageBox(NULL, L"Неверный аргумент ввода файла!", L"Ошибка", MB_OK);
                return 2;
            }
            isGivenN = true;
        }
        catch (...) {
            MessageBox(NULL, L"В качестве аргумента введён не номер!", L"Ошибка", MB_OK);
            return 10;
        }
    }
    else if (argCount > 2) {
        MessageBox(NULL, L"Слишком много аргументов! В качестве аргумента принимается только один номер.\n os-3.exe [размер сетки]", L"Ошибка", MB_OK);
        return 11;
    }
    LocalFree(szArgList);

    //НАЧАЛО ВВОДА ФАЙЛА//
    bool fileExists = false;
    HANDLE hFile = NULL;
    HANDLE hMapFile = OpenFileMapping( //Пытаемся открыть файл из памяти
        FILE_MAP_ALL_ACCESS,   // read/write access
        FALSE,                 // do not inherit the name
        szName);
    if (hMapFile == NULL) { //Файла нет в памяти
        hFile = CreateFile( //Берем файл
            TEXT("settings.txt"),
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_WRITE,
            NULL,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            NULL);
        if (hFile == INVALID_HANDLE_VALUE) //Если не смогли сделать файл, то ошибка
        {
            printf("Could not open file (error %d)\n", GetLastError());
            return 2;
        }
        else if (GetLastError() == 183) { fileExists = true; } //Файл существует
        if (!fileExists) { //Создаем карту файла
            string set = to_string(width) + "\r\n" + to_string(height) + "\r\n" + to_string(n) + "\r\n" + to_string(scrollR) + "\r\n" + to_string(scrollG) + "\r\n" + to_string(scrollB) + "\r\n" + to_string(randR) + "\r\n" + to_string(randG) + "\r\n" + to_string(randB) + "\r\n";
            const int length = set.length();
            char* set_char = new char[length + 1];
            strcpy(set_char, set.c_str());
            size_t size = strlen(set_char);
            HANDLE hFileMapping = CreateFileMapping(hFile, NULL, PAGE_READWRITE, 0, size, NULL);
            if (hFileMapping == NULL) {
                printf("Could not create mapping (error %d)\n", GetLastError());
                return 3;
            }
            LPVOID hMap = MapViewOfFile(hFileMapping, FILE_MAP_WRITE, 0, 0, 0);
            sprintf((char*)hMap, set_char);
            UnmapViewOfFile(hMap);
            CloseHandle(hFileMapping);
        }
        hMapFile = CreateFileMapping(hFile, NULL, PAGE_READWRITE, 0, 0, szName);
        if (hMapFile == NULL) {
            printf("Could not create mapping (error %d)\n", GetLastError());
            CloseHandle(hFile);
            return 3;
        }
    }

    //Создаем обзор карты файла
    buff = (char*)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    GetSettings(buff, isGivenN);

    bool mapExists = true;

    HANDLE hGameMap = OpenFileMapping(
        FILE_MAP_ALL_ACCESS,   // read/write access
        FALSE,                 // do not inherit the name
        szGameName);
    if (hGameMap == NULL) {
        mapExists = false;
        printf("Could not open game mapping (error %d). Creating...\n", GetLastError());
        hGameMap = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, FIGUREAMOUNT * 3 * sizeof(int*), szGameName);
        if (hGameMap == NULL) {
            printf("Could not create game mapping (error %d)\n", GetLastError());
            return 4;
        }
    }
    gameMap = (int*)MapViewOfFile(hGameMap, FILE_MAP_ALL_ACCESS, 0, 0, FIGUREAMOUNT * 3 * sizeof(int*));
    if (gameMap == NULL) {
        printf("Could not map view (error %d)\n", GetLastError());
        return 5;
    }
    if (gameMap[0] == NULL) {
        gameMap[0] = 1;
    }

    COLORREF randColorf = RGB( //Цвет фона
        (BYTE)(randR), // red component of color
        (BYTE)(randG), // green component of color
        (BYTE)(randB) // blue component of color
    );

    //ОТВЕТ 1
    //В коде окна не регистрируются (т.е. они не знают о существовании друг друга). 
    //Вместо этого каждое окно регистрирует сообщение, которое оно может получить.
    //Если в программе изменился размер окна, цвет сетки, цвет фона или программа нарисовала фигуру, 
    //то она посылает одно из трёх сообщений всем окнам.
    figChange = RegisterWindowMessageA("newFigures");
    setChange = RegisterWindowMessageA("newSettings");
    backChange = RegisterWindowMessageA("newBack");

    //Настройка генератора случайных чисел от времени
    time_t t;
    srand((unsigned)time(&t));

    BOOL bMessageOk;
    MSG message;            /* Here message to the application are saved */
    WNDCLASS wincl = { 0 };         /* Data structure for the windowclass */

    /* Harcode show command num when use non-winapi entrypoint */
    int nCmdShow = SW_SHOW;
    /* Get handle */
    HINSTANCE hThisInstance = GetModuleHandle(NULL);

    /* The Window structure */
    wincl.hInstance = hThisInstance;
    wincl.lpszClassName = szWinClass;
    wincl.lpfnWndProc = WindowProcedure;      /* This function is called by Windows */
    RegisterHotKey(hwnd, 1, MOD_CONTROL | MOD_NOREPEAT, 0x51);

    /* Use custom brush to paint the background of the window */
    hBrush = CreateSolidBrush(randColorf);
    wincl.hbrBackground = hBrush;

    /* Register the window class, and if it fails quit the program */
    if (!RegisterClass(&wincl))
        return 0;

    /* The class is registered, let's create the program*/
    //Создания окна
    hwnd = CreateWindow(
        szWinClass,          /* Classname */
        szWinName,           /* Title Text */
        WS_OVERLAPPEDWINDOW, /* default window */
        CW_USEDEFAULT,       /* Windows decides the position */
        CW_USEDEFAULT,       /* where the window ends up on the screen */
        width,               /* The programs width */
        height,              /* and height in pixels */
        HWND_DESKTOP,        /* The window is a child-window to desktop */
        NULL,                /* No menu */
        hThisInstance,       /* Program Instance handler */
        NULL                 /* No Window Creation data */
    );

    /* Make the window visible on the screen */
    ShowWindow(hwnd, nCmdShow);

    backgroundLock = CreateMutex(NULL, FALSE, NULL);
    CreateThread(NULL, STACK_SIZE, background, (LPVOID)hwnd, 0, NULL);

    /* Run the message loop. It will run until GetMessage() returns 0 */
    //Обработка сообщений от приложения
    while ((bMessageOk = GetMessage(&message, NULL, 0, 0)) != 0)
    {
        /* BOOL mb not only 1 or 0.
         * See msdn at https://msdn.microsoft.com/en-us/library/windows/desktop/ms644936(v=vs.85).aspx
         */
        if (bMessageOk == -1)
        {
            puts("Suddenly, GetMessage failed! You can call GetLastError() to see what happend");
            break;
        }

        /* Translate virtual-key message into character message */
        TranslateMessage(&message);
        /* Send message to WindowProcedure */
        DispatchMessage(&message);
    }

    //СОХРАНЕНИЕ ДАННЫХ В ФАЙЛ//
    WriteSettings(buff);

    /* Очистка */
    DestroyWindow(hwnd);
    UnregisterClass(szWinClass, hThisInstance);
    DeleteObject(hBrush);
    if (buff != 0) { UnmapViewOfFile(buff); }
    CloseHandle(hMapFile);
    UnmapViewOfFile(gameMap);
    CloseHandle(hGameMap);
    if (hFile != NULL) { CloseHandle(hFile); }

    return 0;
}