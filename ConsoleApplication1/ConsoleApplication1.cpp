// ConsoleApplication1.cpp
#include <iostream>
#include <conio.h>
#include <windows.h>
#include <thread>
#include <atomic>

using namespace std;
HANDLE hCOM;
atomic<bool> running(true);

void send_char(const char c)
{
    DWORD nb = 0;
    if (!WriteFile(hCOM, &c, sizeof(c), &nb, NULL)) {
        cout << "Ошибка отправки: " << GetLastError() << endl;
    }
}

void read_thread_func()
{
    unsigned char c = 0;
    DWORD nb = 0;

    while (running) {
        // Пытаемся прочитать один символ
        BOOL res = ReadFile(hCOM, &c, sizeof(c), &nb, NULL);

        if (res && nb > 0) {
            printf("\n[ПОЛУЧЕНО] 0x%.2x '%c'\n", c, (c >= 32 && c <= 126) ? c : '.');
            fflush(stdout);
        }

        // Небольшая задержка для снижения нагрузки
        Sleep(10);
    }
}

void close_port()
{
    running = false;
    if (hCOM != INVALID_HANDLE_VALUE && hCOM != NULL) {
        CloseHandle(hCOM);
        hCOM = NULL;
    }
}

void open_port(const char* portname)
{
    // Открываем порт в СИНХРОННОМ режиме (без FILE_FLAG_OVERLAPPED)
    hCOM = CreateFileA(portname,
        GENERIC_READ | GENERIC_WRITE,
        0,           // эксклюзивный доступ
        NULL,
        OPEN_EXISTING,
        0,           // ВАЖНО: 0 вместо FILE_FLAG_OVERLAPPED
        NULL);

    if (hCOM == INVALID_HANDLE_VALUE) {
        cout << "Ошибка открытия порта: " << GetLastError() << endl;
        cout << "Возможные причины:" << endl;
        cout << "- Порт не существует или занят" << endl;
        cout << "- Нет прав доступа" << endl;
        cout << "- Virtual Serial Port Tools не настроен правильно" << endl;
        exit(1);
    }

    // Настройка DCB (скорость, биты и т.д.)
    DCB dcb = { 0 };
    dcb.DCBlength = sizeof(DCB);

    if (!GetCommState(hCOM, &dcb)) {
        cout << "Ошибка получения состояния порта: " << GetLastError() << endl;
        close_port();
        return;
    }

    // Настройка параметров последовательного порта
    dcb.BaudRate = CBR_9600;     // Скорость 9600
    dcb.ByteSize = 8;            // 8 бит данных
    dcb.Parity = NOPARITY;       // Нет бита четности
    dcb.StopBits = ONESTOPBIT;   // 1 стоп-бит
    dcb.fDtrControl = DTR_CONTROL_ENABLE;  // Включаем DTR
    dcb.fRtsControl = RTS_CONTROL_ENABLE;  // Включаем RTS

    if (!SetCommState(hCOM, &dcb)) {
        cout << "Ошибка настройки параметров порта: " << GetLastError() << endl;
        close_port();
        return;
    }

    // Настройка таймаутов для синхронного режима
    COMMTIMEOUTS CommTimeOuts;
    CommTimeOuts.ReadIntervalTimeout = 10;           // Макс. интервал между символами (мс)
    CommTimeOuts.ReadTotalTimeoutMultiplier = 0;
    CommTimeOuts.ReadTotalTimeoutConstant = 10;      // Таймаут чтения 10 мс
    CommTimeOuts.WriteTotalTimeoutMultiplier = 0;
    CommTimeOuts.WriteTotalTimeoutConstant = 1000;   // Таймаут записи 1 секунда

    if (!SetCommTimeouts(hCOM, &CommTimeOuts)) {
        cout << "Ошибка настройки таймаутов: " << GetLastError() << endl;
        close_port();
        return;
    }

    // Очистка буферов порта
    PurgeComm(hCOM, PURGE_RXCLEAR | PURGE_TXCLEAR);

    cout << "Порт открыт успешно!" << endl;
}

int main(int argc, char** argv)
{
    setlocale(LC_ALL, ".1251");

    if (argc != 2) {
        cout << "Не задано имя порта" << endl;
        return;
    }

    char portname[MAX_PATH] = "\\\\.\\";
    strcat_s(portname, argv[1]);
    cout << "Открываем порт " << portname << endl;

    open_port(portname);

    // Запускаем поток для чтения
    thread read_thread(read_thread_func);

    cout << "\n=====================================" << endl;
    cout << "Программа запущена на порту " << argv[1] << endl;
    cout << "Инструкция:" << endl;
    cout << "- Вводите символы для отправки" << endl;
    cout << "- Полученные символы будут отображаться автоматически" << endl;
    cout << "- ESC - выход" << endl;
    cout << "=====================================\n" << endl;

    char last_char = 0;
    do {
        char c = _getch();

        if (c == 27) {  // ESC
            cout << "\nВыход..." << endl;
            break;
        }

        // Отображаем введенный символ
        if (c == '\r') {
            cout << "\n[ОТПРАВЛЕНО] ENTER (0x0d)\n";
            send_char('\r');
            send_char('\n');  // Некоторые программы ожидают \r\n
        }
        else {
            cout << "[ОТПРАВЛЕНО] '" << c << "' (0x" << hex << (int)(unsigned char)c << dec << ")\n";
            send_char(c);
        }

    } while (true);

    // Ожидаем завершения потока чтения
    running = false;
    read_thread.join();
    close_port();

    return 0;
}