#include "Console.h"
#include <Windows.h>
#include <algorithm>
#include <mutex>

struct MutexLock;


typedef std::recursive_mutex Mutex;
typedef std::pair<MutexLock, MutexLock> RWLock;
typedef Console::Color Color;



// Mutex lock ----------------------------------------------------------------------

struct MutexLock
{
    MutexLock(const MutexLock&) = delete;
    MutexLock& operator = (const MutexLock&) = delete;

    MutexLock(MutexLock&& lk) : mtx(lk.mtx), locked(lk.locked)
    {
        lk.mtx = nullptr;
        lk.locked = false;
    }
    MutexLock& operator = (MutexLock&& lk)
    {
        if (this != &lk)
        {
            if (locked)
                Unlock();
            mtx = lk.mtx;
            locked = lk.locked;
            lk.mtx = nullptr;
            lk.locked = false;
        }
        return *this;
    }

    MutexLock(Mutex* mtx) : mtx(mtx), locked(false) { Lock(); }
    ~MutexLock() { Unlock(); }
    void Lock()
    {
        if (mtx)
            mtx->lock();
        locked = true;
    }
    void Unlock()
    {
        if (locked && mtx)
            mtx->unlock();
        locked = false;
    }

    Mutex* mtx;
    bool locked;
};





// Console::Impl ----------------------------------------------------------------------

class Console::Impl
{
private:
    static const uint16_t BK_COLOR_SHIFT = 4;

    enum Flags : uint32_t
    {
        Multithreaded = 0x00000001,
    };

public:
    Impl(bool multithreaded);
    ~Impl() {}

    // Read operations
    bool ReadChar(char& ch) const
    {
        DWORD tmp;
        auto rlk = GetReadLock();
        tmp = ReadConsoleA(m_hIn, &ch, 1, &tmp, nullptr);
        if (ch == '\r')
            ch = '\n';
        return tmp;
    }
    bool ReadCharEcho(char& ch) const
    {
        if (!ReadChar(ch))
            return false;
        auto wlk = GetWriteLock();
        DWORD dw;
        return WriteConsoleA(m_hOut, &ch, 1, &dw, nullptr);
    }
    bool ReadChar(wchar_t& ch) const
    {
        DWORD tmp;
        auto rlk = GetReadLock();
        tmp = ReadConsoleW(m_hIn, &ch, 1, &tmp, nullptr);
        if (ch == L'\r')
            ch = L'\n';
        return tmp;
    }
    bool ReadCharEcho(wchar_t& ch) const
    {
        if (!ReadChar(ch))
            return false;
        auto wlk = GetWriteLock();
        DWORD dw;
        return WriteConsoleW(m_hOut, &ch, 1, &dw, nullptr);
    }

    bool ReadLine(char* buff, size_t buffSize);
    bool ReadLine(std::string& str);
    bool ReadLine(wchar_t* buff, size_t buffSize);
    bool ReadLine(std::wstring& str);


    // Write operations
    bool Write(const char* str, size_t nChars);
    bool Write(const char* str, size_t nChars, Color textColor, Color bkColor);

    bool Write(const wchar_t* str, size_t nChars);
    bool Write(const wchar_t* str, size_t nChars, Color textColor, Color bkColor);
    
    bool EraseNPrevChars(uint32_t nChars) const;

    // Color operations
    Color GetTextColor() const
    {
        auto lk = GetWriteLock();
        return m_textColor;
    }
    Color GetBackGroundColor() const
    {
        auto lk = GetWriteLock();
        return m_bkColor;    
    }
    Color GetConsoleFillColor() const
    {
        auto lk = GetWriteLock();
        return static_cast<Color>(m_fillColor & Color::ColorMask);
    }
    bool SetTextColor(Color textColor)
    {
        auto lk = GetWriteLock();
        if (textColor == Color::UseCurrent || (textColor & Color::ColorMask) == m_textColor)
            return true;
        m_textColor = static_cast<Color>(textColor & Color::ColorMask);
        return SetConsoleColor(m_currColor);
    }
    bool SetBkColor(Color bkColor, bool redrawBkGnd)
    {
        auto lk = GetWriteLock();
        if (bkColor == Color::UseCurrent || (bkColor & Color::ColorMask) == m_bkColor)
            return true;
        m_bkColor = static_cast<Color>(bkColor & Color::ColorMask);
        bool ret = SetConsoleColor(m_currColor);
        if (redrawBkGnd)
            ret = ret && RedrawBackGround();
        return ret;
    }
    bool SetColor(Color textColor, Color bkColor, bool redrawBkGnd)
    {
        auto lk = GetWriteLock();

        if (textColor == Color::UseCurrent)
            textColor = m_textColor;
        else
            m_textColor = static_cast<Color>(textColor & Color::ColorMask);

        if (bkColor == Color::UseCurrent)
        {
            redrawBkGnd = false;
            bkColor = m_bkColor;
        }
        else
            m_bkColor = static_cast<Color>(bkColor & Color::ColorMask);

        bool ret = SetConsoleColor(m_currColor);
        if (redrawBkGnd)
            ret = ret && RedrawBackGround();
        return ret;
    }

    // Code page operations
    uint32_t GetConsoleInputCP() const
    {
        auto rlk = GetReadLock();
        return ::GetConsoleCP();
    }
    uint32_t GetConsoleOutputCP() const
    {
        auto wlk = GetWriteLock();
        return ::GetConsoleOutputCP();
    }
    bool SetConsoleInputCP(uint32_t cp) const
    {
        auto rlk = GetReadLock();
        return ::SetConsoleCP(cp);
    }
    bool SetConsoleOutputCP(uint32_t cp) const
    {
        auto wlk = GetWriteLock();
        return ::SetConsoleOutputCP(cp);
    }


    // Other ops
    bool IsMultiThreaded() const noexcept
    {
        return m_flags & Multithreaded;
    }
    void SetMultiThreaded(bool b)
    {
        if (b && !IsMultiThreaded())
        {
            m_writeMtx = std::make_unique<Mutex>();
            m_readMtx = std::make_unique<Mutex>();
            m_flags |= Multithreaded;
        }
        else if (!b && IsMultiThreaded())
        {
            auto wlk = GetRWLock();
            auto wrtmp = std::move(m_writeMtx);
            auto rdtmp = std::move(m_readMtx);
            m_flags &= ~Multithreaded;
        }
    }

    bool SetPos(uint16_t x, uint16_t y)
    {
        auto wlk = GetWriteLock();
        return ::SetConsoleCursorPosition(m_hOut, COORD{
            static_cast<SHORT>(x), 
            static_cast<SHORT>(y) });
    }
    bool SetPosInd(uint32_t ind)
    {
        auto wlk = GetWriteLock();
        auto size = GetConsoleSize();
        return ::SetConsoleCursorPosition(m_hOut, COORD{
            static_cast<SHORT>(ind % LOWORD(size)),
            static_cast<SHORT>(ind / LOWORD(size))});
    }
    uint32_t GetPos() const
    {
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        if(!::GetConsoleScreenBufferInfo(m_hOut, &csbi))
            return -1;
        return MAKELONG(csbi.dwCursorPosition.X, csbi.dwCursorPosition.Y);
    }
    uint32_t GetPosInd() const
    {
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        if(!::GetConsoleScreenBufferInfo(m_hOut, &csbi))
            return -1;
        return csbi.dwCursorPosition.X + csbi.dwCursorPosition.Y * csbi.dwSize.X;
    }
    uint32_t GetConsoleSize() const
    {
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        if (!GetConsoleScreenBufferInfo(m_hOut, &csbi))
            return -1;
        return MAKELONG(csbi.dwSize.X, csbi.dwSize.Y);
    }


    // Lock console operations
    void LockIO() const
    {
        LockRead();
        LockWrite();
    }
    void UnlockIO() const
    {
        UnlockWrite();
        UnlockRead();
    }
    void LockWrite() const
    {
        if (m_writeMtx)
            m_writeMtx->lock();
    }
    void UnlockWrite() const
    {
        if (m_writeMtx)
            m_writeMtx->unlock();
    }
    void LockRead() const
    {
        if (m_readMtx)
            m_readMtx->lock();
    }
    void UnlockRead() const
    {
        if (m_readMtx)
            m_readMtx->unlock();
    }

private:
    // If input buffer is not empty, inserts str before echoed characters
    // Example:
    // Line No | Impl text
    //       1 | Output 1
    //       2 | Output 2
    //       3 | 1234567     // our current input
    // We receive write operation from another thread - "SomeText\n"
    // After function call we'll see:
    //       1 | Output 1
    //       2 | Output 2
    //       3 | SomeText
    //       4 | 1234567    // our current input 
    bool WriteWithInputWrap(const wchar_t* str, size_t size) const
    {
        // try wrap inputted data and write passed data
        DWORD dw;
        // Erase text that was echoed from keyboard input
        BOOL ret = EraseNPrevChars(m_inputBuffer.size());               
        // Write passed data
        ret = ret && WriteConsoleW(m_hOut, str, size, &dw, nullptr);
        // Set color of input text
        if(m_oldColor != -1)
            ret = ret && SetConsoleColor(m_oldColor);    
        // Write previously erased data immidiatly after passed data
        ret = ret && WriteConsoleW(m_hOut, m_inputBuffer.c_str(), m_inputBuffer.size(), &dw, nullptr);  
        return ret;
    }
    // Change color of console entire background
    bool RedrawBackGround();
    // Set text and text BkGnd
    bool SetConsoleColor(uint16_t color) const
    {
        return SetConsoleTextAttribute(m_hOut, MakeColor(color));
    }

    uint16_t MakeColor(uint8_t tc, uint8_t bc) const
    {
        return tc | (bc << BK_COLOR_SHIFT);
    }
    uint16_t MakeColor(uint16_t c) const
    {
        return (c | (c >> BK_COLOR_SHIFT)) & 0x00FF;
    }

    MutexLock GetWriteLock() const
    {
        return MutexLock(m_writeMtx.get());
    }
    MutexLock GetReadLock() const
    {
        return MutexLock(m_readMtx.get());
    }
    RWLock GetRWLock() const
    {
        return { GetReadLock(), GetWriteLock() };
    }


private:
    std::unique_ptr<Mutex> m_writeMtx;
    std::unique_ptr<Mutex> m_readMtx;   
    HANDLE m_hOut;
    HANDLE m_hIn;
    HANDLE m_hErr;
    std::wstring m_inputBuffer;
    uint32_t m_flags;
    union
    {
        struct
        {
            Color m_textColor;
            Color m_bkColor;
        };
        uint16_t m_currColor;

    };
    uint16_t m_fillColor;
    uint16_t m_oldColor;
};


Console::Impl::Impl(bool multithreaded)
{
    SetMultiThreaded(multithreaded);

    m_hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    m_hIn = GetStdHandle(STD_INPUT_HANDLE);
    m_hErr = GetStdHandle(STD_ERROR_HANDLE);

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(m_hOut, &csbi);
    m_textColor = static_cast<Color>(csbi.wAttributes & Color::ColorMask);
    m_bkColor = static_cast<Color>((csbi.wAttributes >> BK_COLOR_SHIFT) & Color::ColorMask);
    m_fillColor = m_bkColor | (m_bkColor << BK_COLOR_SHIFT);
    m_oldColor = m_textColor;

    DWORD dw;
    GetConsoleMode(m_hIn, &dw);
    dw &= ~ENABLE_LINE_INPUT;
    SetConsoleMode(m_hIn, dw);
}

bool Console::Impl::ReadLine(char * buff, size_t buffSize)
{
    if (!buff || buffSize == 0)
        return false;
    *buff = '\0';

    std::string tmpStr;
    auto ret = ReadLine(tmpStr);
    if (ret)
    {
        size_t nCharsToWrite = (std::min)(buffSize - 1, tmpStr.size());
        memcpy(buff, tmpStr.c_str(), nCharsToWrite);
        buff[nCharsToWrite] = '\0';
    }
    return ret;
}
bool Console::Impl::ReadLine(std::string & str)
{
    str.clear();

    std::wstring tmpStr;
    if (!ReadLine(tmpStr))
        return false;
    if (tmpStr.empty())
        return true;

    size_t buffSize = tmpStr.size() * sizeof(wchar_t);
    do
    {
        std::unique_ptr<char[]> buff(new (std::nothrow) char[buffSize]());
        if (!buff)
            return false;
        auto n = ::WideCharToMultiByte(
            GetConsoleInputCP(),
            0,
            tmpStr.c_str(),
            tmpStr.size(),
            buff.get(),
            buffSize,
            nullptr,
            nullptr);
        if (n == 0)
        {
            if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
                return false;
            buffSize *= 2;
        }
        else
        {
            str.assign(buff.get(), buff.get() + n);
            break;
        }
    } while (1);
    return true;
}
bool Console::Impl::ReadLine(wchar_t * buff, size_t buffSize)
{
    if (!buff || buffSize == 0)
        return false;
    *buff = L'\0';

    std::wstring tmpStr;
    auto ret = ReadLine(tmpStr);
    if (ret)
    {
        size_t nCharsToWrite = (std::min)(buffSize - 1, tmpStr.size());
        wmemcpy(buff, tmpStr.c_str(), nCharsToWrite);
        buff[nCharsToWrite] = L'\0';
    }
    return ret;
}
bool Console::Impl::ReadLine(std::wstring & str)
{
    str.clear();
    auto rlk = GetReadLock();
    DWORD dw = 0;
    wchar_t ch = 0;
    while (true)
    {
        if (!ReadChar(ch))
            return false;
        if (ch == L'\b')
        {
            if (!m_inputBuffer.empty())
            {
                m_inputBuffer.pop_back();
                auto wlk = GetWriteLock();
                EraseNPrevChars(1);
            }
            continue;
        }

        m_inputBuffer.push_back(ch);

        // echo char
        auto wlk = GetWriteLock();
        WriteConsoleW(m_hOut, &ch, 1, &dw, nullptr);

        if (ch == L'\n')
            break;
    }

    m_inputBuffer.pop_back();

    str = std::move(m_inputBuffer);
    return true;
}

bool Console::Impl::Write(const char * str, size_t nChars)
{
    if (!nChars)
        return true;

    // First convert to wchat_t
    std::unique_ptr<wchar_t[]> buff(new (std::nothrow) wchar_t[nChars]());
    if (!buff)
        return false;
    auto n = ::MultiByteToWideChar(
        this->GetConsoleOutputCP(),
        0, 
        str,
        nChars, 
        buff.get(), 
        nChars);
    if (!n)
        return false;

    return Write(buff.get(), n);
}
bool Console::Impl::Write(const char* str, size_t nChars, Color textColor, Color bkColor)
{
    auto lk = GetWriteLock();
    m_oldColor = m_currColor;
    bool ret = true;
    ret = SetColor(textColor, bkColor, false);
    ret = ret && Write(str, nChars);
    ret = ret && SetConsoleColor(m_oldColor);
    m_currColor = m_oldColor;
    m_oldColor = -1;
    return ret;
}
bool Console::Impl::Write(const wchar_t * str, size_t nChars)
{
    if (!nChars)
        return true;

    auto lk = GetWriteLock();

    if (!m_inputBuffer.empty())
        return WriteWithInputWrap(str, nChars);
    else
    {
        DWORD dw;
        return WriteConsoleW(m_hOut, str, nChars, &dw, nullptr);
    }
}
bool Console::Impl::Write(const wchar_t* str, size_t nChars, Color textColor, Color bkColor)
{
    auto lk = GetWriteLock();
    m_oldColor = m_currColor;
    bool ret = true;
    ret = SetColor(textColor, bkColor, false);
    ret = ret && Write(str, nChars);
    ret = ret && SetConsoleColor(m_oldColor);
    m_currColor = m_oldColor;
    m_oldColor = -1;
    return ret;
}

bool Console::Impl::EraseNPrevChars(uint32_t nChars) const
{
    auto wlk = GetWriteLock();
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(m_hOut, &csbi))
        return false;

    DWORD dw = 0;

    // Store in dw index of a console cursor in screen buffer 
    dw = csbi.dwCursorPosition.X + csbi.dwCursorPosition.Y * csbi.dwSize.X;
    // Cursor index after erasing
    dw -= nChars;

    // coordinate of the cursor to set after erasing
    COORD cursor;
    cursor.X = static_cast<WORD>(dw % csbi.dwSize.X);
    cursor.Y = static_cast<WORD>(dw / csbi.dwSize.X);

    BOOL ret = TRUE;
    // Erase nChars characters starting at position 'cursor'
    ret = ret && FillConsoleOutputAttribute(m_hOut, m_fillColor, nChars, cursor, &dw);
    // Set console cursor at begin of erased range
    ret = ret && SetConsoleCursorPosition(m_hOut, cursor);
    return ret;
}
bool Console::Impl::RedrawBackGround()
{
    auto wlk = GetRWLock();

    DWORD dw = 0;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(m_hOut, &csbi))
        return false;

    auto size = csbi.dwSize.X * csbi.dwSize.Y;
    std::unique_ptr<wchar_t[]> buf(new (std::nothrow) wchar_t[size]);

    if (!buf || !ReadConsoleOutputCharacterW(m_hOut, buf.get(), size, COORD{ 0,0 }, &dw))
        return false;

    m_fillColor = m_bkColor | (m_bkColor << BK_COLOR_SHIFT);

    DWORD written = 0, needToWrite = 0;
    for (wchar_t *it1 = buf.get(), *it2, *end = it1 + size; it1 != end; it1 += csbi.dwSize.X)
    {
        it2 = it1 + csbi.dwSize.X - 1;
        while (it1 <= it2 && *it2 == L' ')
            --it2;
        ++it2;
        COORD c;
        c.X = it2 - it1;
        if (c.X == csbi.dwSize.X)
            continue;
        c.Y = (it1 - buf.get()) / csbi.dwSize.X;
        dw = csbi.dwSize.X - c.X;

        needToWrite += dw;
        FillConsoleOutputAttribute(m_hOut, m_fillColor, dw, c, &dw);
        written += dw;
    }
    return needToWrite == written;
}





// Console ----------------------------------------------------------------------------------

Console& Console::GetInstance()
{
    static Console instance;
    return instance;
}

Console::Console() : m_impl(new Impl(false))
{
}
Console::~Console()
{
}

bool Console::ReadChar(char& ch) const
{
    return m_impl->ReadChar(ch);
}
bool Console::ReadCharEcho(char& ch) const
{
    return m_impl->ReadCharEcho(ch);
}
bool Console::ReadChar(wchar_t & ch) const
{
    return m_impl->ReadChar(ch);
}
bool Console::ReadCharEcho(wchar_t & ch) const
{
    return m_impl->ReadCharEcho(ch);
}

bool Console::ReadLine(char* buff, size_t buffSize)
{
    return m_impl->ReadLine(buff, buffSize);
}
bool Console::ReadLine(std::string& str)
{
    return m_impl->ReadLine(str);
}
bool Console::ReadLine(wchar_t* buff, size_t buffSize)
{
    return m_impl->ReadLine(buff, buffSize);
}
bool Console::ReadLine(std::wstring & str)
{
    return m_impl->ReadLine(str);
}

bool Console::Write(const char* str, size_t nChars)
{
    return m_impl->Write(str, nChars);
}
bool Console::Write(const char* str, size_t nChars, Color textColor, Color bkColor)
{
    return m_impl->Write(str, nChars, textColor, bkColor);
}
bool Console::Write(const wchar_t* str, size_t nChars)
{
    return m_impl->Write(str, nChars);
}
bool Console::Write(const wchar_t* str, size_t nChars, Color textColor, Color bkColor)
{
    return m_impl->Write(str, nChars, textColor, bkColor);
}

bool Console::EraseChars(uint32_t nChars)
{
    return m_impl->EraseNPrevChars(nChars);
}

Color Console::GetTextColor() const
{
    return m_impl->GetTextColor();
}
Color Console::GetBackGroundColor() const
{
    return m_impl->GetBackGroundColor();
}
Color Console::GetConsoleFillColor() const
{
    return m_impl->GetConsoleFillColor();
}
bool Console::SetTextColor(Color textColor)
{
    return m_impl->SetTextColor(textColor);
}
bool Console::SetBkColor(Color bkColor, bool redrawBkGnd)
{
    return m_impl->SetBkColor(bkColor, redrawBkGnd);
}
bool Console::SetColor(Color textColor, Color bkColor, bool redrawBkGnd)
{
    return m_impl->SetColor(textColor, bkColor, redrawBkGnd);
}

uint32_t Console::GetConsoleInputCP() const
{
    return m_impl->GetConsoleInputCP();
}
uint32_t Console::GetConsoleOutputCP() const
{
    return m_impl->GetConsoleOutputCP();
}
bool Console::SetConsoleInputCP(uint32_t cp) const
{
    return m_impl->SetConsoleInputCP(cp);
}
bool Console::SetConsoleOutputCP(uint32_t cp) const
{
    return m_impl->SetConsoleOutputCP(cp);
}

bool Console::IsMultiThreaded() const noexcept
{
    return m_impl->IsMultiThreaded();
}
void Console::SetMultiThreaded(bool b)
{
    m_impl->SetMultiThreaded(b);
}

bool Console::SetPos(uint16_t x, uint16_t y)
{
    return m_impl->SetPos(x, y);
}
bool Console::SetPosInd(uint32_t ind)
{
    return m_impl->SetPosInd(ind);
}
uint32_t Console::GetPos() const
{
    return m_impl->GetPos();
}
uint32_t Console::GetPosInd() const
{
    return m_impl->GetPosInd();
}
uint32_t Console::GetConsoleSize() const
{
    return m_impl->GetConsoleSize();
}

void Console::LockIO() const
{
    m_impl->LockIO();
}
void Console::UnlockIO() const
{
    m_impl->UnlockIO();
}
void Console::LockWrite() const
{
    m_impl->LockWrite();
}
void Console::UnlockWrite() const
{
    m_impl->UnlockWrite();
}
void Console::LockRead() const
{
    m_impl->LockRead();
}
void Console::UnlockRead() const
{
    m_impl->UnlockRead();
}



