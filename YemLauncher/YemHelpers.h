#pragma once
#include <string>
#include <windows.h>



//-----------------------------------------------------------------------------------------------------------------
// Returns a normalized path.
//-----------------------------------------------------------------------------------------------------------------
std::wstring _NormalizeToFullPath(const std::wstring& input);

//-----------------------------------------------------------------------------------------------------------------
// Properly quote a command line argument for Windows CreateProcess, handling spaces, tabs, quotes, and 
// backslashes according to Windows parsing rules.
//-----------------------------------------------------------------------------------------------------------------
std::wstring _QuoteArgWindows(const std::wstring& arg);

//-----------------------------------------------------------------------------------------------------------------
// Returns true if the file exists and is not a directory.
//-----------------------------------------------------------------------------------------------------------------
bool _FileExists(const std::wstring& path);

//-----------------------------------------------------------------------------------------------------------------
// Returns the input string, with expanded environment variables.
//-----------------------------------------------------------------------------------------------------------------
std::wstring _ExpandEnvVarsW(const std::wstring& input);

//-----------------------------------------------------------------------------------------------------------------
// Returns the full path of the application given. If not found it will search for the application on the system
// PATH. If a path is returned, it is normalized.
//-----------------------------------------------------------------------------------------------------------------
std::wstring _FindExecutableOnPath(const std::wstring& applicationPath);

//-----------------------------------------------------------------------------------------------------------------
// Convert a wide string to lowercase.
//-----------------------------------------------------------------------------------------------------------------
std::wstring _ToLower(const std::wstring& input);

//-----------------------------------------------------------------------------------------------------------------
// Return the filename from the given path, converted to lowercase.
//-----------------------------------------------------------------------------------------------------------------
std::wstring _GetFileNameLower(const std::wstring& path);


//-----------------------------------------------------------------------------------------------------------------
// RAII wrapper for Windows HANDLE objects
//-----------------------------------------------------------------------------------------------------------------
class HandleGuard
{
public:
    explicit HandleGuard(HANDLE handle = nullptr) noexcept
        : m_handle(handle)
    {
    }

    ~HandleGuard() noexcept
    {
        Close();
    }

    //-------------------------------------------------------------------------------------------------------------
    // Delete copy operations to prevent double-close bugs
    //-------------------------------------------------------------------------------------------------------------
    HandleGuard(const HandleGuard&) = delete;
    HandleGuard& operator=(const HandleGuard&) = delete;

    //-------------------------------------------------------------------------------------------------------------
    // Allow move operations for transfer of ownership
    //-------------------------------------------------------------------------------------------------------------
    HandleGuard(HandleGuard&& other) noexcept
        : m_handle(other.Release())
    {
    }

    HandleGuard& operator=(HandleGuard&& other) noexcept
    {
        if (this != &other)
        {
            Close();
            m_handle = other.Release();
        }
        return *this;
    }

    //-------------------------------------------------------------------------------------------------------------
    // Getters
    //-------------------------------------------------------------------------------------------------------------
    HANDLE Get() const noexcept
    {
        return m_handle;
    }

    operator HANDLE() const noexcept
    {
        return m_handle;
    }

    bool IsValid() const noexcept
    {
        return m_handle != nullptr && m_handle != INVALID_HANDLE_VALUE;
    }

    //-------------------------------------------------------------------------------------------------------------
    // Release ownership without closing
    //-------------------------------------------------------------------------------------------------------------
    HANDLE Release() noexcept
    {
        HANDLE temp = m_handle;
        m_handle = nullptr;
        return temp;
    }

    //-------------------------------------------------------------------------------------------------------------
    // Reset with a new handle
    //-------------------------------------------------------------------------------------------------------------
    void Reset(HANDLE handle = nullptr) noexcept
    {
        Close();
        m_handle = handle;
    }

    //-------------------------------------------------------------------------------------------------------------
    // Explicitly close the handle
    //-------------------------------------------------------------------------------------------------------------
    void Close() noexcept
    {
        if (m_handle != nullptr && m_handle != INVALID_HANDLE_VALUE)
        {
            CloseHandle(m_handle);
            m_handle = nullptr;
        }
    }

private:
    HANDLE m_handle;
};



//-----------------------------------------------------------------------------------------------------------------
// RAII wrapper for CommandLineToArgvW allocated memory
//-----------------------------------------------------------------------------------------------------------------
class CommandLineArgvGuard
{
public:
    explicit CommandLineArgvGuard(wchar_t** argv = nullptr) noexcept
        : m_argv(argv)
    {
    }

    ~CommandLineArgvGuard() noexcept
    {
        if (m_argv != nullptr)
        {
            LocalFree(m_argv);
            m_argv = nullptr;
        }
    }

    //-------------------------------------------------------------------------------------------------------------
    // Delete copy operations
    //-------------------------------------------------------------------------------------------------------------
    CommandLineArgvGuard(const CommandLineArgvGuard&) = delete;
    CommandLineArgvGuard& operator=(const CommandLineArgvGuard&) = delete;

    //-------------------------------------------------------------------------------------------------------------
    // Allow move operations
    //-------------------------------------------------------------------------------------------------------------
    CommandLineArgvGuard(CommandLineArgvGuard&& other) noexcept
        : m_argv(other.Release())
    {
    }

    CommandLineArgvGuard& operator=(CommandLineArgvGuard&& other) noexcept
    {
        if (this != &other)
        {
            if (m_argv != nullptr)
            {
                LocalFree(m_argv);
            }
            m_argv = other.Release();
        }
        return *this;
    }

    //-------------------------------------------------------------------------------------------------------------
    // Getters
    //-------------------------------------------------------------------------------------------------------------
    wchar_t** Get() const noexcept
    {
        return m_argv;
    }

    operator wchar_t**() const noexcept
    {
        return m_argv;
    }

    //-------------------------------------------------------------------------------------------------------------
    // Release ownership without freeing
    //-------------------------------------------------------------------------------------------------------------
    wchar_t** Release() noexcept
    {
        wchar_t** temp = m_argv;
        m_argv = nullptr;
        return temp;
    }

    //-------------------------------------------------------------------------------------------------------------
    // Reset with new argv
    //-------------------------------------------------------------------------------------------------------------
    void Reset(wchar_t** argv = nullptr) noexcept
    {
        if (m_argv != nullptr)
        {
            LocalFree(m_argv);
        }
        m_argv = argv;
    }

private:
    wchar_t** m_argv;
};


//-----------------------------------------------------------------------------------------------------------------
// RAII wrapper for Windows registry HKEY handles.
//-----------------------------------------------------------------------------------------------------------------
class RegKey
{
public:
    RegKey() noexcept 
        : _h(nullptr)
    {
    }

    explicit RegKey(HKEY h) noexcept 
        : _h(h)
    {
    }

    ~RegKey()
    {
        if (_h != nullptr)
        {
            RegCloseKey(_h);
        }
    }

    //-------------------------------------------------------------------------------------------------------------
    // Non-copyable: copying HKEYs is unsafe and would cause double-closing.
    //-------------------------------------------------------------------------------------------------------------
    RegKey(const RegKey&) = delete;
    RegKey& operator=(const RegKey&) = delete;

    //-------------------------------------------------------------------------------------------------------------
    // Movable: allows returning RegKey from helper functions or reassigning safely.
    //-------------------------------------------------------------------------------------------------------------
    RegKey(RegKey&& other) noexcept
        : _h(other._h)
    {
        other._h = nullptr;
    }

    RegKey& operator=(RegKey&& other) noexcept
    {
        if (this != &other)
        {
            if (_h != nullptr)
            {
                RegCloseKey(_h);
            }

            _h      = other._h;
            other._h = nullptr;
        }

        return *this;
    }

    //-------------------------------------------------------------------------------------------------------------
    // Getter
    //-------------------------------------------------------------------------------------------------------------
    HKEY get() const noexcept
    {
        return _h;
    }

    explicit operator bool() const noexcept
    {
        return _h != nullptr;
    }

private:
    HKEY _h;
};
