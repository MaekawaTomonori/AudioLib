#ifndef UTILS_HPP
#define UTILS_HPP
#include <string>
#include <vector>

namespace Utils {
    std::string Convert(const std::wstring& _str);
    std::wstring Convert(const std::string& _str);
    void DisplayLastErr();
    void Alert(const std::string& _msg);
    std::string DateToString();
    std::string GenerateUniqueId();
    bool EqualsIgnoreCase(const std::string& _str1, const std::string& _str2);
    bool ConfirmDialog(const std::string& _msg);
    std::string OpenFileDialog(const std::string& _filter = "All Files\0*.*\0");
    std::vector<std::string> GetFileNames(const std::string& _directory, const std::string& _extension = "", bool _includeExtension = false);
};

#endif //UTILS_HPP
