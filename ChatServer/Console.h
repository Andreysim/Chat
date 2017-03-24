#ifndef _CONSOLE_H_
#define _CONSOLE_H_

#include <sstream>
#include <memory>

class Console
{
private:
    Console();
public:
    enum Color : uint8_t
    {
        Black           = 0x0000,
        Darkblue        = 0x0001,
        Darkgreen       = 0x0002,
        Darkred         = 0x0004,
        Gray            = 0x0008,
        Darkcyan        = Darkblue | Darkgreen,
        Darkmagenta     = Darkblue | Darkred,
        Darkyellow      = Darkgreen | Darkred,
        Darkgray        = Darkblue | Darkgreen | Darkred,
        Blue            = Gray | Darkblue,
        Green           = Gray | Darkgreen,
        Red             = Gray | Darkred,
        Cyan            = Gray | Darkcyan,
        Magenta         = Gray | Darkmagenta,
        Yellow          = Gray | Darkyellow,
        White           = Gray | Darkgray,
        ColorMask       = 0x0F,
        UseCurrent      = 0x10,
    };

    static Console& GetInstance();

    Console(const Console&) = delete;
    Console(Console&&) = delete;
    Console& operator = (const Console&) = delete;
    Console& operator = (Console&&) = delete;

    ~Console();

    // Read operations
    bool ReadChar(char& ch) const;
    bool ReadCharEcho(char& ch) const;
    bool ReadChar(wchar_t& ch) const;
    bool ReadCharEcho(wchar_t& ch) const;

    bool ReadLine(char * buff, size_t buffSize);
    bool ReadLine(std::string& str);
    bool ReadLine(wchar_t* buff, size_t buffSize);
    bool ReadLine(std::wstring& str);

    // Write operations
    bool Write(const char* str)
    {
        auto len = strlen(str);
        return Write(str, len);
    }
    bool Write(const char* str, size_t nChars);
    bool Write(const char* str, Color textColor, Color bkColor = UseCurrent) 
    {
        auto len = strlen(str);
        return Write(str, len, textColor, bkColor);
    }
    bool Write(const char* str, size_t nChars, Color textColor, Color bkColor = UseCurrent);
    bool Write(const std::string& str)
    {
        return Write(str.c_str(), str.size());
    }
    bool Write(const std::string& str, Color textColor, Color bkColor = UseCurrent)
    {
        return Write(str.c_str(), str.size(), textColor, bkColor);
    }

    bool Write(const wchar_t* str)
    {
        auto len = wcslen(str);
        return Write(str, len);
    }
    bool Write(const wchar_t* str, size_t nChars);
    bool Write(const wchar_t* str, Color textColor, Color bkColor = Black) 
    {
        auto len = wcslen(str);
        return Write(str, len, textColor, bkColor);
    }
    bool Write(const wchar_t* str, size_t nChars, Color textColor, Color bkColor = UseCurrent);
    bool Write(const std::wstring& str)
    {
        return Write(str.c_str(), str.size());
    }
    bool Write(const std::wstring& str, Color textColor, Color bkColor = UseCurrent)
    {
        return Write(str.c_str(), str.size(), textColor, bkColor);
    }

    bool EraseChars(uint32_t nChars);

    // Color operations
    Color GetTextColor() const;
    Color GetBackGroundColor() const;
    Color GetConsoleFillColor() const;
    bool SetTextColor(Color textColor);
    bool SetBkColor(Color bkColor, bool redrawBkGnd = false);
    bool SetColor(Color textColor, Color bkColor, bool redrawBkGnd = false);

    // Code page operations
    uint32_t GetConsoleInputCP() const;
    uint32_t GetConsoleOutputCP() const;
    bool SetConsoleInputCP(uint32_t cp) const;
    bool SetConsoleOutputCP(uint32_t cp) const;


    // Other ops
    bool IsMultiThreaded() const noexcept;
    void SetMultiThreaded(bool b);
    bool SetPos(uint16_t x, uint16_t y);
    bool SetPosInd(uint32_t ind);
    uint32_t GetPos() const;
    uint32_t GetPosInd() const;
    uint32_t GetConsoleSize() const;

    // Lock console operations
    void LockIO() const;
    void UnlockIO() const;
    void LockWrite() const;
    void UnlockWrite() const;
    void LockRead() const;
    void UnlockRead() const;
private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};


class ConsoleProxy
{
public:
    ConsoleProxy(ConsoleProxy&&) = default;
    ConsoleProxy& operator = (ConsoleProxy&&) = default;
    ConsoleProxy(Console& con) : console(&con)
    {
    }
    ~ConsoleProxy()
    {
        Flush();
    }

    template<typename T>
    ConsoleProxy& operator << (const T& var)
    {
        wss << var;
        return *this;
    }
    template<typename T>
    ConsoleProxy& operator >> (T& var)
    {
        std::wstring tmp = wss.str();
        if (!tmp.empty())
            console->Write(tmp);

        console->ReadLine(tmp);

        Reset(tmp);
        wss >> var;
        Reset(L""s);

        return *this;
    }

private:
    void Flush() const
    {
        try
        {
            auto wstr = wss.str();
            if(!wstr.empty())
                console->Write(wstr);
        }
        catch (...)
        {

        }
    }
    void Reset(const std::wstring& str)
    {
        wss.str(str);
        wss.clear();
    }
private:
    std::wstringstream wss;
    Console* console;
};

template <typename T>
ConsoleProxy operator << (Console& con, const T& var)
{
    ConsoleProxy proxy(con);
    proxy << var;
    return proxy;
}

template <typename T>
ConsoleProxy operator >> (Console& con, T& var)
{
    ConsoleProxy proxy(con);
    proxy >> var;
    return proxy;
}

#endif // !_CONSOLE_H_
