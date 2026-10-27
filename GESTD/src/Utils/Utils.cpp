#include "Utils.hpp"
#include <Windows.h>
#include <chrono>
#include <filesystem>

namespace Utils {
    std::string Convert(const std::wstring& _str) {
        if (_str.empty())return {};

        auto size_needed = WideCharToMultiByte(CP_UTF8,0,_str.data(),static_cast<int>(_str.size()),nullptr, 0, nullptr,nullptr);
        if (size_needed == 0) return {};

        std::string result(size_needed, 0);
        if (!WideCharToMultiByte(CP_UTF8,0, _str.data(),static_cast<int>(_str.size()),result.data(), size_needed,nullptr, nullptr)) {
            result.clear();
        }

        return result;
    }

    std::wstring Convert(const std::string& _str) {
        if (_str.empty())return {};

        auto size_needed = MultiByteToWideChar(CP_UTF8, 0, _str.data(),static_cast<int>(_str.size()),nullptr, 0);
        if (size_needed == 0)return {};

        std::wstring result(size_needed, 0);
        if (!MultiByteToWideChar(CP_UTF8, 0, _str.data(),static_cast<int>(_str.size()),result.data(), size_needed)) {
            result.clear();
        }

        return result;
    }

    void DisplayLastErr() {
#ifdef _WIN32
        LPVOID lpMsgBuf;
        DWORD dw = GetLastError();

        if (dw == 0)return;

        if (FormatMessage(
            FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            dw,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            reinterpret_cast<LPTSTR>(&lpMsgBuf),
            0, nullptr) == 0){
            MessageBox(nullptr, TEXT("FormatMessage failed"), TEXT("Error"), MB_OK);
            return;
        }
        MessageBox(nullptr, static_cast<LPCTSTR>(lpMsgBuf), TEXT("Error"), MB_OK);
        LocalFree(lpMsgBuf);
#endif
    }

    void Alert(const std::string &_msg) {
#ifdef _WIN32
        MessageBoxA(nullptr, _msg.c_str(), _msg.c_str(), MB_ICONSTOP);
#endif
    }

    std::string DateToString() {
        using namespace std::chrono;
        auto now = system_clock::now();
        auto now_time_t = system_clock::to_time_t(now);
        auto now_ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

        std::tm tm_snapshot;
        #if defined(_WIN32) || defined(_WIN64)
        localtime_s(&tm_snapshot, &now_time_t);
        #else
        localtime_r(&now_time_t, &tm_snapshot);
        #endif

        std::ostringstream oss;
        oss << std::put_time(&tm_snapshot, "%Y-%m-%d %H:%M:%S");
        oss << '.' << std::setw(3) << std::setfill('0') << now_ms.count();
        return oss.str();
    }

    #include <rpc.h>
    #pragma comment(lib, "rpcrt4.lib")

    std::string GenerateUniqueId() {
        UUID uuid;
        UuidCreate(&uuid);
        RPC_CSTR szUuid = nullptr;
        UuidToStringA(&uuid, &szUuid);
        struct UUIDCleaner{
            RPC_CSTR& ptr;
            ~UUIDCleaner() {
                if (ptr)RpcStringFreeA(&ptr);
            }
        } cleaner {szUuid};
        return reinterpret_cast<char*>(szUuid);
    }

    bool EqualsIgnoreCase(const std::string& _str1, const std::string& _str2) {
        return _str1.size() == _str2.size() && std::equal(_str1.begin(), _str1.end(), _str2.begin(),
                                                        [](char _a, char _b){ return tolower(_a) == tolower(_b); });
    }

    bool ConfirmDialog([[maybe_unused]]const std::string& _msg) {
#ifdef _DEBUG
#ifdef _WIN32
        int result = MessageBoxA(nullptr, _msg.c_str(), "Confirm", MB_YESNO | MB_ICONQUESTION);
        return result == IDYES;
#else
        return false;
#endif
#else
        return false;
#endif
    }

    std::string OpenFileDialog(const std::string& _filter) {
        OPENFILENAMEA ofn;
        char szFile[260] = { 0 };

        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = NULL;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile);
        ofn.lpstrFilter = _filter.c_str();
        ofn.nFilterIndex = 1;
        ofn.lpstrFileTitle = NULL;
        ofn.nMaxFileTitle = 0;

        std::filesystem::path initialDir = std::filesystem::current_path() / "Assets" / "Resources";
        std::string initialDirStr = initialDir.string();
        ofn.lpstrInitialDir = initialDirStr.c_str();

        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

        if (GetOpenFileNameA(&ofn) == TRUE) {
            return std::string(ofn.lpstrFile);
        }

        return "";
    }

    std::vector<std::string> GetFileNames(const std::string& _directory, const std::string& _extension, bool _includeExtension) {
        if (!std::filesystem::exists(_directory) || !std::filesystem::is_directory(_directory)) {
            return {};
        }

        std::vector<std::string> files;

        for (const auto& entry : std::filesystem::recursive_directory_iterator(_directory)) {
            if (!entry.is_regular_file()) continue;
            if (!_extension.empty() && entry.path().extension() != _extension) continue;

            std::string file = _includeExtension ? entry.path().filename().string()
                : entry.path().stem().string();

            files.push_back(file);
        }
        return files;
    }
}
