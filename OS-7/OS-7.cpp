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
#define STACK_SIZE (64*1024) // Размер стека для нового потока
#define НОЛИК false // Размер стека для нового потока
#define КРЕСТИК true // Размер стека для нового потока

TCHAR szName[] = L"Global\SettingsMappingObject";
TCHAR szGameName[] = L"Global\GameMappingObject";
TCHAR szEventTurn[] = L"Global\turnEventerObject";
TCHAR szSemName[] = L"Global\VeryCoolSemaphore";
TCHAR szTeamOfree[] = L"Global\isPlayerOhere";

const TCHAR szWinClass[] = _T("Win32SampleApp");
const TCHAR szWinName[] = _T("Win32SampleWindow");
HWND hwnd;               /* This is the handle for our window */
HBRUSH hBrush;           /* Current brush */
HDC hdc;
HPEN hPen;
UINT figChange;
UINT setChange;
UINT gameOver;
RECT rect;

char* buff;
bool isGivenN = false;

boolean lDown = 0; //Нажат ЛКМ
boolean rDown = 0; //Нажат ПКМ

int xMousePos = 0; //позиция мышки по х
int yMousePos = 0; //позиция мышки по у
int zDelta = 0; //направление колеса мышки

//Настройки ширины, высоты, размер сетки, цвет сетки, цвет фона по умолчанию
int realWidth = 300;
int realHeight = 300;
int width = realWidth - 17;
int height = realHeight - 39;
int n = 3;
#define FIGUREAMOUNT (n*n)*3+1
int gridR = 0;
int gridG = 255;
int gridB = 255;

int gridWidth = width / n;
int gridHeight = height / n;

int* gameMap;

HANDLE backgroundLock; //Мьютекс изменения фона
bool lockFlag = false; //Заблокирован ли пользователем мьютекс
bool isBackground = false; //Находится ли поток на фоновом приоритете

HANDLE turnEvent; //Мьютекс запрета хода
HANDLE playersSem; //Семафор кол-ва игроков
CRITICAL_SECTION gameover; //Секция конца игры
HANDLE oIsFree; //Есть ли игрок нолик
bool playerTeam;

bool isMyTurn = false;

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
    //Переводим в string
    string settings = buffer;

    //Заменяем все концы строк на пробелы (для более удобной работы)
    while (settings.find("\r\n") != string::npos)
    {
        settings[settings.find("\r\n")] = ' ';
    }

    //Ввод настроек
    int start = 0; //Начало линии
    int end; //Конец линии
    string curLine; //Текущая линия
    int countF = 0; //Текущая настройка
    for (int i = 0; i < settings.length(); i++) {
        if (settings[i] == ' ') {
            end = i;
            curLine = settings.substr(start, (end - start));
            if (settings[i + 1] != NULL) { start = i + 1; }
            switch (countF) {
            case 0: {realWidth = stoi(curLine); break; }
            case 1: {realHeight = stoi(curLine); break; }
            case 2: {if (flag == 0) { n = stoi(curLine); }; break; }
            case 3: {gridR = stoi(curLine); break; }
            case 4: {gridG = stoi(curLine); break; }
            case 5: {gridB = stoi(curLine); break; }
            }
            countF++;
        }
    }
}

void WriteSettings(char* buffer)
{
    string set = to_string(realWidth) + "\r\n" + to_string(realHeight) + "\r\n" + to_string(n) + "\r\n" + to_string(gridR) + "\r\n" + to_string(gridG) + "\r\n" + to_string(gridB) + "\r\n";
    char* set_char = new char[set.length() + 1];
    strcpy(set_char, set.c_str());
    size_t size = strlen(set_char);

    sprintf(buffer, set_char);
}

int GameCheck()
{
    for (int i = 1; i <= n * n; i += n) {
        //cout << gameMap[3 * i] << gameMap[3 * i + 3] << gameMap[3 * i + 6] << endl;
        if (gameMap[3 * i] != NULL && gameMap[3 * i + 3] != NULL && gameMap[3 * i + 6] != NULL &&
            ((gameMap[3 * i] == gameMap[3 * i + 3]) && (gameMap[3 * i] == gameMap[3 * i + 6]))) //По горизонтали
        {
            switch (gameMap[3 * i]) {
            case 1:
                MessageBox(NULL, L"Победа ноликов!", L"Игра окончена", MB_OK);
                break;
            case 2:
                MessageBox(NULL, L"Победа крестиков!", L"Игра окончена", MB_OK);
                break;
            }
            return 1;
        }
    }
    for (int i = 3; i <= n*n; i+=n) 
    {
        if (gameMap[i] != NULL && gameMap[i + 9] != NULL && gameMap[i + 18] != NULL &&
            ((gameMap[i] == gameMap[i + 9]) && (gameMap[i] == gameMap[i + 18]))) //По вертикали
        {
            switch (gameMap[i]) {
            case 1:
                MessageBox(NULL, L"Победа ноликов!", L"Игра окончена", MB_OK);
                break;
            case 2:
                MessageBox(NULL, L"Победа крестиков!", L"Игра окончена", MB_OK);
                break;
            }
            return 1;
        }
    }

    //Проверка диагоналей
    if (gameMap[3] != NULL && gameMap[15] != NULL && gameMap[27] != NULL &&
        ((gameMap[3] == gameMap[15]) && (gameMap[3] == gameMap[27])))
    {
        switch (gameMap[3]) {
        case 1:
            MessageBox(NULL, L"Победа ноликов!", L"Игра окончена", MB_OK);
            break;
        case 2:
            MessageBox(NULL, L"Победа крестиков!", L"Игра окончена", MB_OK);
            break;
        }
        return 1;
    }
    else if (gameMap[9] != NULL && gameMap[15] != NULL && gameMap[21] != NULL &&
        ((gameMap[9] == gameMap[15]) && (gameMap[9] == gameMap[21])))
    {
        switch (gameMap[9]) {
        case 1:
            MessageBox(NULL, L"Победа ноликов!", L"Игра окончена", MB_OK);
            break;
        case 2:
            MessageBox(NULL, L"Победа крестиков!", L"Игра окончена", MB_OK);
            break;
        }
        return 1;
    }

    if (gameMap[0] == n * n + 1) { MessageBox(NULL, L"Ничья!", L"Игра окончена", MB_OK); return 1; }
    return 0;
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
    if (message == setChange) {
        GetSettings(buff, isGivenN);
        SetWindowPos(hwnd, NULL, 0, 0, realWidth, realHeight, SWP_NOMOVE);
        width = realWidth - 17;
        height = realHeight - 39;
        gridWidth = width / n;
        gridHeight = height / n;
        InvalidateRect(hwnd, NULL, TRUE);
    }
    else if (message == figChange) {
        if (isMyTurn) isMyTurn = false;
        else isMyTurn = true;
        InvalidateRect(hwnd, NULL, TRUE);
    }
    else if (message == gameOver) {
        PostQuitMessage(0);
    }
    switch (message)                  /* handle the messages */
    {
    case WM_SIZE:
        if (GetWindowRect(hwnd, &rect))
        {
            realWidth = rect.right - rect.left;
            realHeight = rect.bottom - rect.top;
            WriteSettings(buff);
            PostMessage(HWND_BROADCAST, setChange, NULL, NULL);
        }
        return 0;
    case WM_KEYDOWN: //Отработка нажатия клавиатуры
        switch (wParam) 
        {
        case VK_SPACE:
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
    case WM_LBUTTONDOWN: //Если зажата левая кнопка мышки запомнить местоположение курсора
        xMousePos = LOWORD(lParam);
        yMousePos = HIWORD(lParam);
        lDown = 1;
        return 0;
    case WM_LBUTTONUP: //Если отжата левая кнопка мышки нарисовать круг
        if (lDown)
        {
            if (WaitForSingleObject(turnEvent, 100) == WAIT_TIMEOUT) { return 0; }
            if ((playerTeam == НОЛИК && gameMap[0] % 2 == 0) || (playerTeam == КРЕСТИК && gameMap[0] % 2 == 1)) {
                MessageBox(NULL, L"Сейчас не ваш ход!", L"Ошибка", MB_OK);
                return 0;
            }
            ResetEvent(turnEvent);
            bool exists = false;
            int xCenter = (xMousePos / gridWidth) * gridWidth + gridWidth / 2;
            int yCenter = (yMousePos / gridHeight) * gridHeight + gridHeight / 2;
            for (int i = 1; i <= n*n*3; i+=3) {
                //cout << gameMap[i] << " " << xCenter << endl;
                if (gameMap[i] == xCenter and gameMap[i + 1] == yCenter) {
                    exists = true;
                    break;
                }
            }
            if (exists) {
                MessageBox(NULL, L"Фигура уже существует в данном поле!", L"Ошибка", MB_OK);
                return 0;
            }
            else if (gameMap[0] % 2 != 0) {
                int leftC = xCenter - gridWidth / 2;
                int topC = yCenter - gridHeight / 2;
                int rightC = xCenter + gridWidth / 2;
                int bottomC = yCenter + gridHeight / 2;
                hdc = GetDC(hwnd);
                hPen = CreatePen(PS_SOLID, 3, RGB(255, 0, 0));
                hBrush = CreateSolidBrush(RGB(255, 255, 255));
                SelectObject(hdc, hBrush);
                SelectObject(hdc, hPen);
                Ellipse(hdc, leftC + 5, topC + 5, rightC - 5, bottomC - 5);
                DeleteObject(hBrush);
                DeleteObject(hPen);
                ReleaseDC(hwnd, hdc);
                int cellNum = ((xCenter - gridWidth / 2) / gridWidth) + n * ((yCenter - gridHeight / 2) / gridHeight) + 1; //Номер клетки, в которую поставили фигуру
                gameMap[3 * cellNum - 2] = xCenter; //Куда записать коодинату х
                gameMap[3 * cellNum - 1] = yCenter; //Куда записать коодинату у
                gameMap[3 * cellNum] = 1; //Тип записанной фигуры
                gameMap[0]++; //Увеличиваем кол-во фигур
            }
            else {
                int leftX = (xMousePos / gridWidth) * gridWidth;
                int topX = (yMousePos / gridHeight) * gridHeight;
                int rightX = leftX + gridWidth;
                int bottomX = topX + gridHeight;
                hdc = GetDC(hwnd);
                hPen = CreatePen(PS_SOLID, 3, RGB(0, 255, 0));
                SelectObject(hdc, hPen);
                MoveToEx(hdc, leftX + 5, topX + 5, NULL);
                LineTo(hdc, rightX - 5, bottomX - 5);
                MoveToEx(hdc, rightX - 5, topX + 5, NULL);
                LineTo(hdc, leftX + 5, bottomX - 5);
                DeleteObject(hPen);
                ReleaseDC(hwnd, hdc);
                int cellNum = ((xCenter - gridWidth / 2) / gridWidth) + n * ((yCenter - gridHeight / 2) / gridHeight) + 1; //Номер клетки, в которую поставили фигуру
                gameMap[3 * cellNum - 2] = xCenter;
                gameMap[3 * cellNum - 1] = yCenter;
                gameMap[3 * cellNum] = 2;
                gameMap[0]++;
            }
            PostMessage(HWND_BROADCAST, figChange, NULL, NULL);
            if (GameCheck()) {
                PostMessage(HWND_BROADCAST, gameOver, NULL, NULL);
            }
            else {
                SetEvent(turnEvent);
            }
        }
        lDown = 0;
        return 0;
    case WM_PAINT: //Фунцкия рисования фона
        PAINTSTRUCT ps;
        hdc = BeginPaint(hwnd, &ps);
        hPen = CreatePen(PS_SOLID, 3, RGB(gridR, gridG, gridB));
        SelectObject(hdc, hPen);
        //Начинаем рисовать сетку
        for (int i = 0; i <= width; i += gridWidth) {
            MoveToEx(hdc, i, 0, NULL);
            LineTo(hdc, i, height);
        }
        for (int i = 0; i <= height; i += gridHeight) {
            MoveToEx(hdc, 0, i, NULL);
            LineTo(hdc, width, i);
        }
        //Сетка нарисована
        DeleteObject(hPen);

        hBrush = CreateSolidBrush(RGB(255, 255, 255));
        SelectObject(hdc, hBrush);

        int xPos, yPos;
        for (int i = 1; i <= n*n*3+1; i+=3) {
            if (gameMap[i] != NULL) {
                xPos = gameMap[i];
                yPos = gameMap[i + 1];
                //cout << gameMap[i] << " " << gameMap[i+1] << " " << gameMap[i+2] << "\n";
                if (gameMap[i + 2] == 1)
                {
                    int leftC = xPos - gridWidth / 2;
                    int topC = yPos - gridHeight / 2;
                    int rightC = xPos + gridWidth / 2;
                    int bottomC = yPos + gridHeight / 2;
                    hPen = CreatePen(PS_SOLID, 3, RGB(255, 0, 0));
                    SelectObject(hdc, hPen);
                    Ellipse(hdc, leftC + 5, topC + 5, rightC - 5, bottomC - 5);
                    DeleteObject(hPen);
                }
                else if (gameMap[i + 2] == 2)
                {
                    int leftX = xPos - gridWidth / 2;
                    int topX = yPos - gridHeight / 2;
                    int rightX = leftX + gridWidth;
                    int bottomX = topX + gridHeight;
                    hPen = CreatePen(PS_SOLID, 3, RGB(0, 255, 0));
                    SelectObject(hdc, hPen);
                    MoveToEx(hdc, leftX + 5, topX + 5, NULL);
                    LineTo(hdc, rightX - 5, bottomX - 5);
                    MoveToEx(hdc, rightX - 5, topX + 5, NULL);
                    LineTo(hdc, leftX + 5, bottomX - 5);
                    DeleteObject(hPen);
                }
            }
        }

        //for (int i = 0; i <= n * 3 + 1; i++) { if (gameMap[i] != NULL) { cout << gameMap[i] << " "; } } cout << endl;

        //Отчистка
        DeleteObject(hBrush);
        EndPaint(hwnd, &ps);
        return 0;
    case WM_DESTROY: //Уничтожение окна
        //EnterCriticalSection(&gameover);
        if (playerTeam == НОЛИК) {
            SetEvent(oIsFree);
        }
        if (ReleaseSemaphore(playersSem, 1, NULL) == 0) {
            cout << "Error releasing semaphore\n";
        }
        DeleteObject(playersSem);
        DeleteObject(oIsFree);
        DeleteObject(turnEvent);
        DeleteObject(backgroundLock);
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

    playersSem = CreateSemaphore(NULL, 2, 2, szSemName);
    if (playersSem == NULL) {
        cout << "Semaphore exists." << endl;
        playersSem = OpenSemaphore(SEMAPHORE_ALL_ACCESS, FALSE, szSemName);
    }
    if (WaitForSingleObject(playersSem, 0) == WAIT_TIMEOUT) {
        CloseHandle(playersSem);
        PostQuitMessage(0);
    }

    backgroundLock = CreateMutex(NULL, FALSE, NULL);
    turnEvent = CreateEvent(NULL, TRUE, TRUE, szEventTurn);
    if (turnEvent == NULL) {
        cout << "turnEvent event exists.\n";
        turnEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, szEventTurn);
    }

    oIsFree = CreateEvent(NULL, TRUE, TRUE, szTeamOfree);
    if (oIsFree == NULL) {
        cout << "oIsFree event exists.\n";
        oIsFree = OpenEvent(EVENT_ALL_ACCESS, FALSE, szTeamOfree);
    }
    if (WaitForSingleObject(oIsFree, 0) == WAIT_TIMEOUT) {
        cout << "I'm a X\n";
        playerTeam = КРЕСТИК;
    }
    else {
        cout << "I'm a O\n";
        ResetEvent(oIsFree);
        playerTeam = НОЛИК;
    }

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

            string set = to_string(realWidth) + "\r\n" + to_string(realHeight) + "\r\n" + to_string(n) + "\r\n" + to_string(gridR) + "\r\n" + to_string(gridG) + "\r\n" + to_string(gridB) + "\r\n";
            char* set_char = new char[set.length() + 1];
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

    figChange = RegisterWindowMessageA("newFigures");
    setChange = RegisterWindowMessageA("newSettings");
    gameOver = RegisterWindowMessageA("gameIsOver");

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
    hBrush = CreateSolidBrush(RGB(0, 0, 255));
    wincl.hbrBackground = hBrush;

    /* Register the window class, and if it fails quit the program */
    if (!RegisterClass(&wincl))
        return 0;

    /* The class is registered, let's create the program*/
    //Создания окна
    hwnd = CreateWindow(
        szWinClass,          /* Classname */
        szWinName,           /* Title Text */
        WS_OVERLAPPED | 
        WS_CAPTION | 
        WS_SYSMENU,          /* default window */
        CW_USEDEFAULT,       /* Windows decides the position */
        CW_USEDEFAULT,       /* where the window ends up on the screen */
        realWidth,               /* The programs width */
        realHeight,              /* and height in pixels */
        HWND_DESKTOP,        /* The window is a child-window to desktop */
        NULL,                /* No menu */
        hThisInstance,       /* Program Instance handler */
        NULL                 /* No Window Creation data */
    );

    /* Make the window visible on the screen */
    ShowWindow(hwnd, nCmdShow);

    CreateThread(NULL, STACK_SIZE, background, (LPVOID)hwnd, 0, NULL);

    InitializeCriticalSection(&gameover);
    
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
    if (buff != 0) UnmapViewOfFile(buff);
    CloseHandle(hMapFile);
    UnmapViewOfFile(gameMap);
    CloseHandle(hGameMap);
    if (hFile != NULL) CloseHandle(hFile);
    if (oIsFree != NULL) CloseHandle(oIsFree);
    if (turnEvent != NULL) CloseHandle(turnEvent);
    if (backgroundLock != NULL) CloseHandle(backgroundLock);
    CloseHandle(playersSem);
    return 0;
}