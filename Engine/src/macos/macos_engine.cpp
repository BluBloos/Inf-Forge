#include <automata_engine.hpp>

// TODO: maybe one day we support multiple backends at once.
// for such cases, in the macOS backend we want to check that ONLY metal is active.
#if defined(AUTOMATA_ENGINE_METAL_BACKEND)

#include <Metal/Metal.hpp>
#include <AppKit/AppKit.hpp>
#include <MetalKit/MetalKit.hpp>

// -------- AE external vars --------

// TODO: are these globals a good design / can we get rid of them?

// TODO: hook.
float ae::platform::lastFrameTime = 1 / 60.0f;
float ae::platform::lastFrameTimeTotal = 1 / 60.0f;
bool ae::platform::_globalRunning = true;
bool ae::platform::_globalVsync = false;
int automata_engine::platform::_globalProgramResult = 0;


// TODO: hook.
ae::update_model_t ae::platform::GLOBAL_UPDATE_MODEL = 
    ae::AUTOMATA_ENGINE_UPDATE_MODEL_ATOMIC; // default.

// -------- AE internal vars --------

// -------- AE platform functions --------

// TODO: implement functions below.

void ae::platform::freeLoadedFile(loaded_file_t file) {}

ae::loaded_file_t ae::platform::readEntireFile(const char *fileName) {
    return {};
}

void automata_engine::platform::fprintf_proxy(int h, const char *fmt, ...) {}

void automata_engine::platform::free(void *data) {
    if (data)
{

}
}

void *automata_engine::platform::alloc(uint32_t bytes) {
    return nullptr;
}

void ae::platform::setVsync(bool b) {
}


// -------- AE platform functions --------


/* REFERENCE COUNTING CONVENTIONS

1. *You own any object returned by methods whose name begins with* `alloc` *,* `new` *,* `copy` *, or* `mutableCopy`. The method returns these objects with `retainCount` equals to `1`.
2. *You can take ownership of an object by calling its* ```retain()``` *method*. 
  A received object is normally guaranteed to remain valid within the method it was received in.
  You use `retain` in two situations:
    (1) In the implementation of an accessor method (a setter) or to take ownership of an object; and
    (2) To prevent an object from being deallocated as a side-effect of some other operation.
3. *When you no longer need it, you must relinquish ownership of an object you own*. You relinquish ownership by calling its `release()` or `autorelease()` method.
4. *You must not relinquish ownership of an object you do not own*.

5. release() works when called on nullptr. safety mechanism.

When an object's `retainCount` reaches `0`, the object is immediately deallocated. It is illegal to call methods on a deallocated object and it may lead to an application crash.
*/

class MyAppDelegate : public NS::ApplicationDelegate
{
    public:
        ~MyAppDelegate();

        virtual void applicationWillFinishLaunching( NS::Notification* pNotification ) override;
        virtual void applicationDidFinishLaunching( NS::Notification* pNotification ) override;
        virtual bool applicationShouldTerminateAfterLastWindowClosed( NS::Application* pSender ) override;

    private:

        NS::Window* m_pWindow;

#if 0 
        MTK::View* _pMtkView;
        MTL::Device* _pDevice;
        MyMTKViewDelegate* _pViewDelegate = nullptr;
#endif
};

MyAppDelegate::~MyAppDelegate()
{
    //_pMtkView->release();
    m_pWindow->release();
    //_pDevice->release();
    //delete _pViewDelegate;
}

// Returns a Boolean value that indicates if the app terminates once the last window closes.
bool MyAppDelegate::applicationShouldTerminateAfterLastWindowClosed( NS::Application* pSender )
{
    return true;
}

// app’s initialization is about to complete.
void MyAppDelegate::applicationWillFinishLaunching( NS::Notification* pNotification )
{
  //  NS::Menu* pMenu = createMenuBar();
    NS::Application* pApp = reinterpret_cast< NS::Application* >( pNotification->object() );
   // pApp->setMainMenu( pMenu );
    
    // The application is an ordinary app that appears in the Dock and may have a user interface.
    pApp->setActivationPolicy( NS::ActivationPolicy::ActivationPolicyRegular );

}

// app’s initialization is complete but it hasn’t received its first event.
void MyAppDelegate::applicationDidFinishLaunching( NS::Notification* pNotification )
{

    // TODO: is there a better default for width/height?
    float width = (ae::defaultWidth == UINT32_MAX) ? 512.0f : ae::defaultWidth;
    float height = (ae::defaultHeight == UINT32_MAX) ? 512.0f : ae::defaultHeight;    
    bool bDefer = false; // defers creating the window device until the window is moved onscreen.
    CGRect frame = (CGRect){ {0.0, 0.0}, {width, height} };

// TODO: handle begin maximized.

    m_pWindow = NS::Window::alloc()->init(
        frame,
        NS::WindowStyleMaskClosable|NS::WindowStyleMaskTitled,
        NS::BackingStoreBuffered, // yeah this is like the double buffering thing, right?
        bDefer );

#if 0
   _pDevice = MTL::CreateSystemDefaultDevice();
    _pMtkView = MTK::View::alloc()->init( frame, _pDevice );
    _pMtkView->setColorPixelFormat( MTL::PixelFormat::PixelFormatBGRA8Unorm_sRGB );
    _pMtkView->setClearColor( MTL::ClearColor::Make( 1.0, 0.0, 0.0, 1.0 ) );

    _pViewDelegate = new MyMTKViewDelegate( _pDevice );
    _pMtkView->setDelegate( _pViewDelegate );
    m_pWindow->setContentView( _pMtkView );
#endif

    const char *windowName=ae::defaultWindowName;
    m_pWindow->setTitle( NS::String::string( windowName, NS::StringEncoding::UTF8StringEncoding ) );

    // sets to be a topmost window and be the thing that takes keybindings to do stuff.
    // TODO: why does this take a sender?
    m_pWindow->makeKeyAndOrderFront( nullptr );

    // TODO: do I need to activate the app explicitly like this? docs said it don't matter.
#if 0
    NS::Application* pApp = reinterpret_cast< NS::Application* >( pNotification->object() );
    pApp->activateIgnoringOtherApps( true );
#endif
}

int main( int argc, char* argv[] )
{
    // this NS::AutoreleasePool is the same thing as the Objective-C API but in a C++ form.
    // maybe the online docs for Obj-C could help us. can't seem to find online C++ docs.
    // maybe the intended usage is to read the Foundation.hpp, which maybe has some docs?
    //
    // this thing will auto release all things once it goes out of scope.
    // no need to call release() on anything.
    NS::AutoreleasePool* pAutoreleasePool = NS::AutoreleasePool::alloc()->init();

    // nothing takes the auto release pool. so there's a lot going on under the hood here.
    // each thread maintains a stack of auto release pools.

    MyAppDelegate del;

    NS::Application* pSharedApplication = NS::Application::sharedApplication();
    // Obj-C is all about messages. so when creating this thing it likely calls an autorelease() member
    // function.

    pSharedApplication->setDelegate( &del );

    pSharedApplication->run();

    pAutoreleasePool->release();

    return 0;
}

#endif // AUTOMATA_ENGINE_METAL_BACKEND