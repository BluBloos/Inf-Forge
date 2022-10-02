#include <automata_engine.h>
#include <win32_engine.h>

#include <windows.h>
#include <io.h> // TODO(Noah): What is this used for again?
#include <xaudio2.h>
#include <mmreg.h> // TODO(Noah): What is this used for again?

// TODO(Noah): Remove dependency on all this crazy
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>     /* for _O_TEXT and _O_BINARY */

#define MAX_CONSOLE_LINES 500

// TODO(Noah): Hot reloading 😎 baby!.

// on the Windows platform, when a WM_SIZE message is recieved, this callback is invoked.
static void (*GameHandleWindowResize)(game_memory_t *, int, int) = nullptr;
static void (*GameInit)(game_memory_t *) = nullptr;
static void (*GamePreInit)(game_memory_t *) = nullptr;
static void (*GameCleanup)(game_memory_t *) = nullptr;

namespace ae = automata_engine;

// macros should always be in all caps.
bool ae::platform::GLOBAL_RUNNING = true;
ae::update_model_t ae::platform::GLOBAL_UPDATE_MODEL = 
    ae::AUTOMATA_ENGINE_UPDATE_MODEL_ATOMIC; // default.

void ae::platform::freeLoadedFile(loaded_file_t file) {
    VirtualFree(file.contents, 0, MEM_RELEASE);
}

loaded_file ae::platform::readEntireFile(char *fileName) {
	void *result = 0;
	int fileSize32;
	HANDLE fileHandle = CreateFileA(fileName, GENERIC_READ,
		FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
	if (fileHandle != INVALID_HANDLE_VALUE) {
		LARGE_INTEGER fileSize;
		if (GetFileSizeEx(fileHandle, &fileSize)) {
			// TODO(Noah): Add a #define for maximum file size value.
            // Also, is it not the case they we further restrict ourselves to
            // a maximum file size?
			assert(fileSize.QuadPart <= 0xFFFFFFF);
			fileSize32 = (int)fileSize.QuadPart;
			result = VirtualAlloc(0, fileSize32, MEM_COMMIT, PAGE_READWRITE);
			if (result != NULL) {
				DWORD bytesRead;
				if (ReadFile(fileHandle, result, fileSize32, &bytesRead, 0) && 
                    (fileSize32 == (int)bytesRead)) 
				{
					// File read succesfully!
				}	
				else {
					VirtualFree(result, 0, MEM_RELEASE);
					result = 0;
				}		
			}
		}
		CloseHandle(fileHandle);
	}
	loaded_file fileResult = {};
	fileResult.contents = result;
	fileResult.contentSize = fileSize32;
	return fileResult;
}

#if defined(GL_BACKEND)
static bool isImGuiInitialized = false;
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_win32.h" // includes Windows.h for us
// NOTE(Noah): Pretty sure glew and gl must be after imgui headers here.
#include <glew.h>
#include <gl/gl.h>
static HDC gHdc;
typedef BOOL WINAPI wgl_swap_interval_ext(int interval);
static wgl_swap_interval_ext *wglSwapInterval;
HGLRC glContext;
static bool win32_glInitialized = false;
static void InitOpenGL(HWND windowHandle, HDC dc) {
    PIXELFORMATDESCRIPTOR desiredPixelFormat = {};
    desiredPixelFormat.nSize = sizeof(desiredPixelFormat);
    desiredPixelFormat.nVersion = 1;
    desiredPixelFormat.dwFlags = PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW | PFD_DOUBLEBUFFER;
    desiredPixelFormat.cColorBits = 32;
    desiredPixelFormat.cAlphaBits = 8;
    desiredPixelFormat.iLayerType = PFD_MAIN_PLANE;
    int suggestedPixelFormatIndex = ChoosePixelFormat(dc, &desiredPixelFormat);
    PIXELFORMATDESCRIPTOR suggestedPixelFormat = {};
    DescribePixelFormat(dc, suggestedPixelFormatIndex,
        sizeof(suggestedPixelFormat), &suggestedPixelFormat);
    SetPixelFormat(dc, suggestedPixelFormatIndex, &suggestedPixelFormat);
    glContext = wglCreateContext(dc);
    if(win32_glInitialized = wglMakeCurrent(dc, glContext)) {
        // setup VSYNC, even though it's on by default :)
        wglSwapInterval = (wgl_swap_interval_ext *)wglGetProcAddress("wglSwapIntervalEXT");
        if(wglSwapInterval) { 
            wglSwapInterval(1);
        }
        // NOTE(Noah): We are permitted to init glew here and expect this to be fine with our game
        // because we are doing a static linkage between all code at the moment. In this case, there
        // is only one storage location for glewInit param. And at the same time, glew is init everywhere.
        // all glew is doing is loading func pointer via GetProcAddr.
        ae::GL::initGlew();
    } else {
        //TODO(Noah): openGL did not initialize, what the heck do we do?
        PlatformLoggerError("OpenGL did not initialize");
        assert(false);
    }
}

// TODO(Noah): thinking this might belong back in platform.
bool automata_engine::GL::getGLInitialized() {
    return win32_glInitialized;
}
#endif

// TODO(Noah): Need to also impl for DirectX, and same goes with update models ...
void ae::platform::setVsync(bool b) {
#if defined(GL_BACKEND)
    if (b) {
        wglSwapInterval(1);
    } else {
        wglSwapInterval(0);
    }
#endif
}

void automata_engine::platform::free(void *data) {
    VirtualFree((void *)data, 0, MEM_RELEASE);
}

void *automata_engine::platform::alloc(uint32_t bytes) {
    return VirtualAlloc(0, bytes, MEM_COMMIT, PAGE_READWRITE);
}

static user_input_t globalUserInput = {};
static game_memory_t globalGameMemory = {};
static win32_backbuffer_t globalBackBuffer = {};

static void Win32ResizeBackbuffer(win32_backbuffer_t *buffer, int newWidth, int newHeight)
{
	if(buffer->memory) {
		VirtualFree(buffer->memory, 0, MEM_RELEASE);
	}
	// NOTE(Noah): set bitmap heights and widths for global struct
	buffer->width = newWidth;
	buffer->height = newHeight;
	// NOTE(Noah): set values in the bitmapinfo
	buffer->info.bmiHeader.biSize = sizeof(buffer->info.bmiHeader);
	buffer->info.bmiHeader.biWidth = newWidth;
	buffer->info.bmiHeader.biHeight = -newHeight;
	buffer->info.bmiHeader.biPlanes = 1;
	buffer->info.bmiHeader.biBitCount = 32;
	buffer->info.bmiHeader.biCompression = BI_RGB;
	// NOTE(Noah): Here we are allocating memory to our bitmap since we resized it
	buffer->bytesPerPixel = 4;
	int bitmapMemorySize = newWidth * newHeight * buffer->bytesPerPixel;
	buffer->memory = VirtualAlloc(0, bitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);
	// NOTE(Noah): Setting pitch shit, ahah.
	buffer->pitch = newWidth * buffer->bytesPerPixel;
	// TODD(Noah): Probably want to clear to black each time we resize the window?
    // Or is VirtualAlloc already doing this?

    // Update globalGameMemory too
    globalGameMemory.backbufferPixels = (uint32_t *)buffer->memory;
    globalGameMemory.backbufferWidth = newWidth;
    globalGameMemory.backbufferHeight = newHeight;
}

void Win32DisplayBufferWindow(HDC deviceContext, game_window_info_t winInfo) {
    int OffsetX = (winInfo.width - globalBackBuffer.width) / 2;
    int OffsetY = (winInfo.height - globalBackBuffer.height) / 2;
    PatBlt(deviceContext, 0, 0, winInfo.width, OffsetY, BLACKNESS);
    PatBlt(deviceContext, 0, OffsetY + globalBackBuffer.height, winInfo.height, winInfo.height, BLACKNESS);
    PatBlt(deviceContext, 0, 0, OffsetX, winInfo.height, BLACKNESS);
    PatBlt(deviceContext, OffsetX + globalBackBuffer.width, 0, winInfo.width, winInfo.height, BLACKNESS);
    //TODO(Noah): Correct aspect ratio. Also, figure out what this TODO means.
    StretchDIBits(deviceContext,
        OffsetX, OffsetY, globalBackBuffer.width, globalBackBuffer.height,
        0, 0, globalBackBuffer.width, globalBackBuffer.height,
        globalBackBuffer.memory,
        &globalBackBuffer.info,
        DIB_RGB_COLORS,
        SRCCOPY);
}

void automata_engine::platform::getUserInput(user_input_t *userInput) { 
    *userInput = globalUserInput; 
}

static void ProccessKeyboardMessage(unsigned int vkCode, bool down) {
  if (vkCode >= 'A' && vkCode <= 'Z') {
    globalUserInput.keyDown[(uint32_t)GAME_KEY_A + (vkCode - 'A')] = down;
  } else if (vkCode >= '0' && vkCode <= '9') {
    globalUserInput.keyDown[(uint32_t)GAME_KEY_0 + (vkCode - '0')] = down;
  } else if (vkCode == VK_SPACE) {
    globalUserInput.keyDown[GAME_KEY_SPACE] = down;
  } else if (vkCode == VK_SHIFT) {
    globalUserInput.keyDown[GAME_KEY_SHIFT] = down;
  }
}

#if defined(GL_BACKEND)
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

LRESULT CALLBACK Win32WindowProc(HWND window,
  UINT message,
  WPARAM wParam,
  LPARAM lParam)
{    
#if defined(GL_BACKEND)
    if (ImGui_ImplWin32_WndProcHandler(window, message, wParam, lParam))
        return true;
#endif

    PAINTSTRUCT ps; 
    LRESULT result = 0;
    switch(message) {
        case WM_MOUSEMOVE: {
            int x = (int)lParam & 0x0000FFFF;
            int y = ((int)lParam & 0xFFFF0000) >> 16;
            globalUserInput.deltaMouseX = x - globalUserInput.mouseX;
            globalUserInput.deltaMouseY = y - globalUserInput.mouseY;
            globalUserInput.mouseX = x;
            globalUserInput.mouseY = y;
        } break;
        // left mouse button
        case WM_LBUTTONDOWN: {
            globalUserInput.mouseLBttnDown = true;
        } break;
        case WM_LBUTTONUP:{
            globalUserInput.mouseLBttnDown = false;
        } break;
        //keyboard messages
        case WM_KEYUP: {
            ProccessKeyboardMessage(wParam, false);
        } break;
        case WM_KEYDOWN: {
            ProccessKeyboardMessage(wParam, true);
        } break;
        case WM_CREATE: {
#if defined(GL_BACKEND)
            gHdc = GetDC(window);
            InitOpenGL(window, gHdc);
#endif
            CREATESTRUCTA *cstra = (CREATESTRUCTA *)lParam;
            if(cstra != NULL) {
                // TODO(Noah): There's this pattern here where we are checking
                // if the fptr is non-null. Can we abstract this?
                if (GameHandleWindowResize != nullptr) {
                    GameHandleWindowResize(&globalGameMemory, cstra->cx, cstra->cy);
                } else {
                    PlatformLoggerLog("WARN: GameHandleWindowResize == nullptr");
                }
                
            }
        } break;
        case WM_SIZE: {
            int width = (int)lParam & 0x0000FFFF;
            int height = ((int)lParam & 0xFFFF0000) >> 16;
            if (GameHandleWindowResize != nullptr) {
                GameHandleWindowResize(&globalGameMemory, width, height);
            } else {
                PlatformLoggerLog("WARN: GameHandleWindowResize == nullptr");
            }
        } break;
        case WM_PAINT: {
            HDC DeviceContext = BeginPaint(window, &ps);
            // Do a render.
			game_window_info_t winInfo = automata_engine::platform::getWindowInfo();
			Win32DisplayBufferWindow(DeviceContext, winInfo);
            EndPaint(window, &ps);
        } break;
        case WM_DESTROY: {
            // TODO(Noah): Handle as error
            automata_engine::platform::GLOBAL_RUNNING = false;
        } break;
        case WM_CLOSE: {
            //TODO(Noah): Handle as message to user
            automata_engine::platform::GLOBAL_RUNNING = false;
        } break;
        default:
        result =  DefWindowProc(window, message, wParam, lParam);
    }
	return result;
}

static void CheckSanePlatform(void) {
    assert(sizeof(uint8_t) == 1);
    assert(sizeof(uint16_t) == 2);
    assert(sizeof(uint32_t) == 4);
    assert(sizeof(uint64_t) == 8);
    assert(sizeof(int8_t) == 1);
    assert(sizeof(int16_t) == 2);
    assert(sizeof(int32_t) == 4);
    assert(sizeof(int64_t) == 8);
    assert(sizeof(float32_t) == 4);
    assert(sizeof(float64_t) == 8);
}

int automata_engine::platform::GLOBAL_PROGRAM_RESULT = 0;

static XAUDIO2_BUFFER xa2Buffer = {0};
static IXAudio2SourceVoice* pSourceVoice = nullptr;
    
// NOTE(Noah): What happens if there is no buffer to play?? -_-
void automata_engine::platform::playAudioBuffer() {
    if (pSourceVoice != nullptr) {
        pSourceVoice->Start( 0 );
    }
}

void automata_engine::platform::stopAudioBuffer() {
    if (pSourceVoice != nullptr) {
        pSourceVoice->Stop( 0 );
    }
}

// TODO(Noah): Make our platform audio things more low-level.
// I don't like the fact that we are using loaded_wav here.
// What if I want to submit a full-blown buffer? maybe I generated
// programatically or something.
//
// Another TODO would be ... make this NEVER fail! WHY would I
// voluntarily release that sort of paradigm over to the game?
// It's my job to put Xaudio in check. Why would you fail? Like ...
// nah man. None of that shit!
//
// the paradigm should be like so. If things fail that should normally
// work no problem, it means that something is catasrophically wrong.
// Like idk, the sound card was removed? In these cases, 
// our engine should handle it entirely. Game, when making requests,
// will have failures happen silently. This model is fine because
// it will not be too long after that request failure that the engine
// shuts itself down, or does SOMETHING.
//
// Now, there are also things that WILL fail. Like file reads and shader
// compilation. i.e. file might not exist and shader could have syntax errs.
// in these cases, the game has to deal with such errors.
bool automata_engine::platform::submitAudioBuffer(struct loaded_wav myWav) {
    if (pSourceVoice != nullptr) {
        if (FAILED(pSourceVoice->Stop(0))) { return false; }
        if (FAILED(pSourceVoice->FlushSourceBuffers())) { return false; }
    } else {
        return false; // no source voice...
    }
    xa2Buffer.AudioBytes = myWav.sampleCount * myWav.channels * sizeof(short);
    xa2Buffer.pAudioData = (const BYTE *)myWav.sampleData;
    return !FAILED(pSourceVoice->SubmitSourceBuffer(&xa2Buffer));
}

static HWND globalWin32Handle = NULL;

game_window_info_t automata_engine::platform::getWindowInfo() {
    game_window_info_t winInfo;
    winInfo.hWnd = (intptr_t)globalWin32Handle;
    if (globalWin32Handle == NULL) {
        PlatformLoggerError("globalWin32Handle == NULL");
    }
    RECT rect;
    if (GetClientRect(globalWin32Handle, &rect)) {
        winInfo.width = rect.right - rect.left;
        winInfo.height = fabs(rect.top - rect.bottom);
    }
    return winInfo;
}

int CALLBACK WinMain(HINSTANCE instance,
  HINSTANCE prevInstance,
  LPSTR cmdLine,
  int showCode)
{
    CheckSanePlatform();
    automata_engine::super::init();

    // TODO: Do some hot reloading stuff! Game is a DLL to the engine :)
    {
        GameInit = automata_engine::Init; // after window creation
        GamePreInit = ae::PreInit; // before window creation
        GameCleanup = automata_engine::Close;
        GameHandleWindowResize = automata_engine::HandleWindowResize;
    }

    // NOTE(Noah): There is a reason that when you read C code, all the variables are defined
    // at the top of the function. I'm just guessing here, but maybe back in the day people 
    // liked to use "goto" a lot. It's a dangerous play because you might skip over the initialization
    // of a variable. Then the contents are unknown!
    ATOM classAtom = 0;
    HWND windowHandle = NULL;
    HRESULT comResult = S_FALSE;
    IXAudio2* pXAudio2 = nullptr;
    bool sound_playing = false;
    WNDCLASS windowClass = {};

    // Before doing ANYTHING, we alloc memory.
    globalGameMemory.isInitialized = false;
    globalGameMemory.dataBytes = 67108864;
    globalGameMemory.data =
        automata_engine::platform::alloc(globalGameMemory.dataBytes); // will allocate 64 MB
    if (globalGameMemory.data == nullptr) {
        automata_engine::platform::GLOBAL_PROGRAM_RESULT = -1;
        goto WinMainEnd;
    }

    // Create a console and redirect stdout to the console.
    #ifndef RELEASE
    {
        int hConHandle;
        intptr_t lStdHandle;
        CONSOLE_SCREEN_BUFFER_INFO coninfo;
        FILE *fp;

        // allocate a console for this app
        // https://docs.microsoft.com/en-us/windows/console/allocconsole
        AllocConsole();
        SetConsoleTitleA("Automata Console");

        // set the screen buffer to be big enough to let us scroll text
        // https://docs.microsoft.com/en-us/windows/console/getconsolescreenbufferinfo
        GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &coninfo);
        coninfo.dwSize.Y = MAX_CONSOLE_LINES;
        SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), coninfo.dwSize);
        
        // redirect unbuffered STDOUT to the console
        // https://msdn.microsoft.com/en-us/library/bdts1c9x.aspx
        // https://msdn.microsoft.com/en-us/library/88k7d7a7.aspx
        
        // TODO(Noah): Understand what the heck is going on here...and abstract these redirects!
        // redirect stderr
        lStdHandle = (intptr_t)GetStdHandle(STD_ERROR_HANDLE);
        hConHandle = _open_osfhandle(lStdHandle, _O_TEXT); // convert windows handle to c runtime handle
        fp = _fdopen( hConHandle, "w");
        *stderr = *fp;
        freopen_s(&fp, "CONOUT$", "w", stderr);
        // redirect stdout
        lStdHandle = (intptr_t)GetStdHandle(STD_OUTPUT_HANDLE);
        hConHandle = _open_osfhandle(lStdHandle, _O_TEXT); // convert windows handle to c runtime handle
        fp = _fdopen( hConHandle, "w");
        *stdout = *fp;
        freopen_s(&fp, "CONOUT$", "w", stdout);
        // redirect stdin
        lStdHandle = (intptr_t)GetStdHandle(STD_INPUT_HANDLE);
        hConHandle = _open_osfhandle(lStdHandle, _O_TEXT); // convert windows handle to c runtime handle
        fp = _fdopen( hConHandle, "r");
        *stdin = *fp;
        freopen_s(&fp, "CONIN$", "r", stdin);

        // set console mode for supporting color escape sequences.    
        {
            HANDLE stdOutHandle = GetStdHandle(STD_OUTPUT_HANDLE);
            DWORD stdOutMode;
            GetConsoleMode(stdOutHandle, &stdOutMode);
            // NOTE(Noah): See https://docs.microsoft.com/en-us/windows/console/setconsolemode for 
            // meaning of 0x0004. The macro was not being resolved...
            SetConsoleMode(stdOutHandle, stdOutMode | 0x0004);
        }
        
        PlatformLoggerLog("stdout initialized");
        PlatformLoggerError("testing stderr out");
        //fprintf(stderr, "testing stderr out\n");
        // TODO(Noah): Make this print pull version from a manifest or something...
        PlatformLoggerLog("\"Hello, World!\" from Automata Engine %s", "Alpha v0.1.0");
    }
    #endif

    windowClass.style = CS_VREDRAW | CS_HREDRAW; // Set window to redraw after being resized
    windowClass.lpfnWndProc = Win32WindowProc; // Set callback
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursor(0, IDC_ARROW);
    windowClass.lpszClassName = "Automata Engine";

    // TODO(Noah): Here would be a nice instance for the defer statement.
    classAtom = RegisterClassA(&windowClass); 
    if(classAtom == 0) {
        PlatformLoggerError("Unable to create window class \"%s\"", windowClass.lpszClassName);
        automata_engine::platform::GLOBAL_PROGRAM_RESULT = -1;
        goto WinMainEnd;
    }

    if (GamePreInit != nullptr) {
        GamePreInit(&globalGameMemory);
    } else {
        PlatformLoggerWarn("GamePreInit == nullptr");
    }

    windowHandle = CreateWindowExA(
        0, // dwExStyle
        windowClass.lpszClassName, 
        ae::defaultWindowName,
        (WS_OVERLAPPEDWINDOW | WS_VISIBLE) & 
            ((ae::defaultWinProfile == AUTOMATA_ENGINE_WINPROFILE_NORESIZE) ? 
            (~WS_MAXIMIZEBOX & ~WS_THICKFRAME) : (DWORD)(0xFFFFFFFF)), 
        CW_USEDEFAULT, // init X
        CW_USEDEFAULT, // init Y
        ae::defaultWidth,
        ae::defaultHeight,
        NULL, // parent handle
        NULL, // menu
        instance, 
        NULL // structure to be passed to WM_CREATE message
    );

    // TODO(Noah): Can abstract this type of windows error print code.
    if (windowHandle == NULL) {
        DWORD resultCode = GetLastError();
        char *lpMsgBuf;
        FormatMessage(
            // FORMAT_MESSAGE_FROM_SYSTEM -> search the system message-table resource(s) for the requested message
            // FORMAT_MESSAGE_ALLOCATE_BUFFER -> allocates buffer, places pointer at the address specified by lpBuffer
            // FORMAT_MESSAGE_IGNORE_INSERTS -> Insert sequences in the message definition such as %1 are to be 
            //   ignored and passed through to the output buffer unchanged.
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            resultCode, // dwMessageId
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPTSTR) &lpMsgBuf, // lpBuffer
            0, // no need for nSize with FORMAT_MESSAGE_ALLOCATE_BUFFER
            NULL // not using any insert values such as %1
        );
        // TODO(Noah): Remove newline character at the end of lpMsgBuf
        PlatformLoggerError("Unable to create window: %s", lpMsgBuf);
        LocalFree(lpMsgBuf);
        automata_engine::platform::GLOBAL_PROGRAM_RESULT = -1;
        goto WinMainEnd;
    }

    globalWin32Handle = windowHandle;
    ShowWindow(windowHandle, showCode);
    UpdateWindow(windowHandle);

    // Create the globalBackBuffer
#if defined(CPU_BACKEND)
    {
        game_window_info_t winInfo = automata_engine::platform::getWindowInfo();
        Win32ResizeBackbuffer(&globalBackBuffer, winInfo.width, winInfo.height);
    }
#endif

    // Initialize XAudio2 !!!
    // NOTE(Noah): When we free the xAudio2 object, that frees all subordinate objects.
    IXAudio2MasteringVoice* pMasterVoice = nullptr;
    {
        // Initializes the COM library for use by the calling thread.
        if (FAILED(comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED))) {
            PlatformLoggerError("Unable to initialize COM library (XAudio2)");
            automata_engine::platform::GLOBAL_PROGRAM_RESULT = -1;
            goto WinMainEnd;
        }

        if (FAILED(XAudio2Create(&pXAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR))) {
            PlatformLoggerError("Unable to to create an instance of the XAudio2 engine");
            automata_engine::platform::GLOBAL_PROGRAM_RESULT = -1;
            goto WinMainEnd;
        }

        if (FAILED(pXAudio2->CreateMasteringVoice(&pMasterVoice))) {
            PlatformLoggerError("Unable to create a mastering voice (XAudio2)");
            automata_engine::platform::GLOBAL_PROGRAM_RESULT = -1;
            goto WinMainEnd;
        }
          
        WAVEFORMATEX  waveFormat = {0};

        waveFormat.wFormatTag = WAVE_FORMAT_PCM; // going with 2-channel PCM data.
        waveFormat.nChannels = 2;
        waveFormat.nSamplesPerSec = ENGINE_DESIRED_SAMPLES_PER_SECOND; // 48 kHz
        waveFormat.wBitsPerSample = sizeof(short) * 8; 
        waveFormat.nBlockAlign = waveFormat.nChannels * waveFormat.wBitsPerSample / 8;
        waveFormat.nAvgBytesPerSec = waveFormat.nBlockAlign * waveFormat.nSamplesPerSec;
        waveFormat.cbSize = 0;

        xa2Buffer.Flags = 0;
        xa2Buffer.AudioBytes = 0;
        xa2Buffer.pAudioData = nullptr;
        xa2Buffer.PlayBegin = 0;
        xa2Buffer.PlayLength = 0; // play entire buffer
        xa2Buffer.LoopBegin = 0;
        xa2Buffer.LoopLength = 0; // entire sample should be looped.
        xa2Buffer.LoopCount = XAUDIO2_MAX_LOOP_COUNT;
        xa2Buffer.pContext = NULL;

        // NOTE(Noah): Again, we do not need to be concerned with freeing the source voice
        // as once we free the master xaudio2 object, we are good.
        pXAudio2->CreateSourceVoice(&pSourceVoice, (WAVEFORMATEX*)&waveFormat);
    }

    if (GameInit != nullptr) {
        GameInit(&globalGameMemory);
    } else {
        PlatformLoggerLog("WARN: GameInit == nullptr");
    }

#if defined(GL_BACKEND)
    // ImGUI initialization code :)
    const char* glsl_version = "#version 330";
    {
        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        // ImGuiIO& io = ImGui::GetIO(); (void)io;
        // Setup Dear ImGui style
        ImGui::StyleColorsDark();
        // Setup Platform/Renderer backends
        ImGui_ImplWin32_Init(windowHandle);
        ImGui_ImplOpenGL3_Init(glsl_version);
    }
    // TODO(Noah): Look into what the imGUI functions are going to return on failure!
    isImGuiInitialized = true;
#endif

    // TODO(Noah):
    // add stalling in the CPU model (i.e. the engine does not call game update)
    // as fast as possible. Then make ae::setVsync() control whether this happens, or no.

    while(automata_engine::platform::GLOBAL_RUNNING) {
        // Process windows messages
        {
            MSG message;
            while(PeekMessage(&message, NULL, 0, 0, PM_REMOVE)) {
                switch (message.message) {
                    case WM_QUIT:
                    automata_engine::platform::GLOBAL_RUNNING = false;
                    break;
                    default:
                    TranslateMessage(&message);
                    DispatchMessage(&message);
                    break;
                }
            }
        }

#if !defined(RELEASE) && defined(GL_BACKEND)
        if (isImGuiInitialized) {
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();
        }
#endif

        auto gameUpdateAndRender = automata_engine::bifrost::getCurrentApp();
        if (gameUpdateAndRender != nullptr) {
            gameUpdateAndRender(&globalGameMemory);
        } else {
            PlatformLoggerLog("WARN: gameUpdateAndRender == nullptr");
        }

#if !defined(RELEASE) && defined(GL_BACKEND)
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
#endif

        // Reset for next frame
        globalUserInput.deltaMouseX = 0; globalUserInput.deltaMouseY = 0;

#if defined(GL_BACKEND)
        if (ae::platform::GLOBAL_UPDATE_MODEL == ae::AUTOMATA_ENGINE_UPDATE_MODEL_ATOMIC) {
            glFlush(); // push all buffered commands to GPU
            glFinish(); // block until GPU is complete
        }
        SwapBuffers(gHdc);
#endif

#if defined(CPU_BACKEND)
        // NOTE(Noah): Here we are going to call our custom windows platform layer function that 
        // will write our custom buffer to the screen.
        HDC deviceContext = GetDC(windowHandle);
        game_window_info_t winInfo = automata_engine::platform::getWindowInfo();
        Win32DisplayBufferWindow(deviceContext, winInfo);
        ReleaseDC(windowHandle, deviceContext);
#endif

    }

    // NOTE(Noah): Again, we want a standard naming convention. Labels should be done like func names.
    WinMainEnd:

    // TODO(Noah): Can we leverage our new nc_defer.h to replace this code below?
    {
        // Free XAudio2 resources.
        if (pXAudio2 != nullptr) { pXAudio2->Release(); }
        if (!FAILED(comResult)) { CoUninitialize(); }

#if defined(GL_BACKEND)
        // ImGUI is tightly coupled with the platform.
        if (isImGuiInitialized) {
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
        }
#endif

        if (GameCleanup != nullptr) {
            GameCleanup(&globalGameMemory);
        } else {
            PlatformLoggerLog("WARN: GameCleanup == nullptr");
        }
        
        if (globalGameMemory.data != nullptr) { 
            automata_engine::platform::free(globalGameMemory.data);
        }

        if (windowHandle != NULL) { DestroyWindow(windowHandle); }
        if (classAtom != 0) { UnregisterClassA(windowClass.lpszClassName, instance); }

        // finally, dealloc automata_engine state
        automata_engine::super::close();

        // stall program to allow user to see err.
        printf("Press ENTER exit\n");
        fgetc(stdin);
    }

    return automata_engine::platform::GLOBAL_PROGRAM_RESULT;
}
