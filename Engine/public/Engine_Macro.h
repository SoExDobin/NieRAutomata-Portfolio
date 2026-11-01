#ifndef Engine_Macro_h__
#define Engine_Macro_h__

namespace Engine
{
#define ETOI(ENUM)              static_cast<unsigned int>(ENUM)
#define GAME_INSTANCE                    Engine::Game::GetInstance()


#define KEY_MAX                 256
#define UBYTE(UINT32)           static_cast<unsigned char>(UINT32)


#ifndef MSG_BOX
#define MSG_BOX(_message)                                                      \
	MessageBox(NULL, TEXT(_message), L"System Message", MB_OK)
#endif

#define NS_BEGIN(NAMESPACE) namespace NAMESPACE {
#define NS_END }

#define USING(NAMESPACE) using namespace NAMESPACE;

#ifdef ENGINE_EXPORTS
#define ENGINE_DLL _declspec(dllexport)
#else
#define ENGINE_DLL _declspec(dllimport)
#endif

#define NULL_CHECK(_ptr)                                                       \
  {                                                                            \
    if (_ptr == 0) {                                                           \
      return;                                                                  \
    }                                                                          \
  }

#define NULL_CHECK_RETURN(_ptr, _return)                                       \
	{                                                                          \
        if (_ptr == 0) {                                                       \
			return _return;                                                    \
        }                                                                      \
    }

#define NULL_CHECK_MSG(_ptr, _message)                                         \
    {                                                                          \
        if (_ptr == 0) {                                                       \
			MessageBox(NULL, _message, L"System Message", MB_OK);              \
        }                                                                      \
    }

#define NULL_CHECK_RETURN_MSG(_ptr, _return, _message)                         \
    {                                                                          \
        if (_ptr == 0) {                                                       \
            MessageBox(NULL, _message, L"System Message", MB_OK);              \
            return _return;                                                    \
        }                                                                      \
    }

#define FAILED_CHECK(_hr)                                                      \
    if (((HRESULT)(_hr)) < 0) {                                                \
        MessageBoxW(NULL, L"Failed", L"System Error", MB_OK);                  \
        return E_FAIL;                                                         \
    }

#define FAILED_CHECK_RETURN(_hr, _return)                                      \
    if (((HRESULT)(_hr)) < 0) {                                                \
        MessageBoxW(NULL, L"Failed", L"System Error", MB_OK);                  \
        return _return;                                                        \
    }

#define FAILED_CHECK_MSG(_hr, _message)                                        \
    if (((HRESULT)(_hr)) < 0) {                                                \
        MessageBoxW(NULL, _message, L"System Message", MB_OK);                 \
        return E_FAIL;                                                         \
    }

#define FAILED_CHECK_RETURN_MSG(_hr, _return, _message)                        \
    if (((HRESULT)(_hr)) < 0) {                                                \
        MessageBoxW(NULL, _message, L"System Message", MB_OK);                 \
        return _return;                                                        \
    }

#define NO_COPY(CLASSNAME)                                           \
    private:                                                         \
        CLASSNAME(const CLASSNAME&) = delete;                        \
        CLASSNAME& operator=(const CLASSNAME&) = delete;             \
        CLASSNAME(CLASSNAME&&) = delete;                             \
        CLASSNAME& operator=(CLASSNAME&&) = delete;

#define DECLARE_SINGLETON(CLASSNAME)                                 \
    NO_COPY(CLASSNAME)                                               \
    public:                                                          \
        static std::shared_ptr<CLASSNAME> GetInstance();             \
        static void DestroyInstance();                               \
    private:                                                         \
        static std::shared_ptr<CLASSNAME> m_pInstance;               \
        static std::once_flag m_onceFlag;

#define IMPLEMENT_SINGLETON(CLASSNAME)                               \
    std::shared_ptr<CLASSNAME> CLASSNAME::m_pInstance = nullptr;     \
    std::once_flag CLASSNAME::m_onceFlag;                            \
    std::shared_ptr<CLASSNAME> CLASSNAME::GetInstance() {            \
        std::call_once(m_onceFlag, []() {                            \
            struct MakeSharedEnabler : public CLASSNAME {            \
                MakeSharedEnabler() : CLASSNAME() {}                 \
            };                                                       \
            m_pInstance = std::make_shared<MakeSharedEnabler>();     \
        });                                                          \
        return m_pInstance;                                          \
    }                                                                \
    void CLASSNAME::DestroyInstance() {                              \
        m_pInstance.reset();                                         \
    }

}
#endif // Engine_Macro_h__
