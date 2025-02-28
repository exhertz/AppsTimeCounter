#include <Windows.h>
#include <ShlObj.h>
#include <Propkey.h>
#include <atlbase.h>
#include <psapi.h>

#include <iostream>
#include <codecvt>
#include <string>
#include <system_error>
#include <chrono>
#include <thread>
#include <fstream>
#include <ctime>
#include <shellapi.h>

#include <nlohmann/json.hpp>

#pragma comment(lib, "shell32.lib")

nlohmann::json data;

char formattedNamefile[24];

/* competently obtaining application properties (by https://stackoverflow.com/a/51950848) */
std::wstring GetShellPropStringFromPath(LPCWSTR pPath, PROPERTYKEY const& key)
{
    CComPtr<IShellItem2> pItem;
    HRESULT hr = SHCreateItemFromParsingName(pPath, nullptr, IID_PPV_ARGS(&pItem));
    if (FAILED(hr))
        throw std::system_error(hr, std::system_category(), "SHCreateItemFromParsingName() failed");

    CComHeapPtr<WCHAR> pValue;
    hr = pItem->GetString(key, &pValue);
    if (FAILED(hr))
        throw std::system_error(hr, std::system_category(), "IShellItem2::GetString() failed");

    return std::wstring(pValue);
}

void saveDataOnJSON(nlohmann::json dataObject) {
    std::ofstream f(formattedNamefile);
    f << std::setw(4) << dataObject << std::endl;
    f.close();
}

void dataIncrement(std::string field) {
    if (data.find(field) == data.end()) {
        data[field] = 1;
    }
    else {
        data[field] = data[field] + 1;
    }
}

HWND cachedWindow = nullptr;
std::string cachedNameAppStr;

void updateTimers() {
    DWORD processID;
    DWORD lenstr;
    WCHAR processPath[MAX_PATH];
    HWND foregroundWindow;
    HANDLE processHandle;

    foregroundWindow = GetForegroundWindow();
    if (foregroundWindow == cachedWindow) {
        dataIncrement(cachedNameAppStr);
        return;
    }

    GetWindowThreadProcessId(foregroundWindow, &processID);

    processHandle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processID);

    if (!processHandle)
        return;

    lenstr = GetModuleFileNameEx(processHandle, NULL, processPath, MAX_PATH);
    CloseHandle(processHandle);

    if (!lenstr)
        return;

    std::wcout << processID << "\tPath : " << processPath << std::endl;

    std::wstring nameApp;
    try {
        nameApp = GetShellPropStringFromPath(processPath, PKEY_FileDescription);
    }
    catch (...) {
        try {
            nameApp = GetShellPropStringFromPath(processPath, PKEY_Software_ProductName);
        }
        catch (...) {
            nameApp = PathFindFileNameW(processPath);
        }
    }

    if (nameApp == L"Application Frame Host") {
        WCHAR windowTitle[256];
        GetWindowTextW(foregroundWindow, windowTitle, 256);
        nameApp = windowTitle;
    }

    std::wcout << processID << "\tName : " << nameApp << std::endl;

    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    std::string nameAppStr = converter.to_bytes(nameApp);
    cachedWindow = foregroundWindow;
    cachedNameAppStr = nameAppStr;
    dataIncrement(nameAppStr);
}

int main() {
    SetConsoleOutputCP(CP_UTF8);
    std::locale::global(std::locale(""));

    struct tm newtime;
    time_t now = time(0);
    localtime_s(&newtime, &now);
    strftime(formattedNamefile, sizeof(formattedNamefile), "session-%d-%m-%Y.json", &newtime);

    HRESULT _ = CoInitialize(nullptr);

    try {
        std::ifstream f(formattedNamefile);
        data = nlohmann::json::parse(f);
    }
    catch (const std::exception& e) {
        std::cout << e.what() << std::endl;
    }

    while (true) {
        updateTimers();
        saveDataOnJSON(data);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    CoUninitialize();
    return 0;
}