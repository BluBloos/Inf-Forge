#include <automata_engine.hpp>

#if defined(AUTOMATA_ENGINE_DX12_BACKEND)
namespace automata_engine {
namespace DX {

#define COM_RELEASE(comPtr) (comPtr != nullptr) ? comPtr->Release() : 0;

/**
 * @param ppAdapter has AddRef called. Caller of this func must do ->Release()
 * on adapter.
 * @returns nullptr in ppAdapter on failure.
 */
void findHardwareAdapter(IDXGIFactory2 *dxgiFactory,
                         IDXGIAdapter1 **ppAdapter) {
  // TODO(Noah): ensure that our Automata engine can handle when the hardware
  // adapters are changing. see Remarks section of
  // https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgifactory1-enumadapters1
  bool requestHighPerformanceAdapter = true;
  {
    *ppAdapter = nullptr;
    IDXGIAdapter1 *adapter;
    // NOTE(Noah): calling QueryInterface will do AddRef on our object.
    // thus, we must call release on this interface.
    IDXGIFactory6 *factory6;
    defer(COM_RELEASE(factory6));
    if (SUCCEEDED(dxgiFactory->QueryInterface(IID_PPV_ARGS(&factory6)))) {
      for (UINT adapterIndex = 0;
           SUCCEEDED(factory6->EnumAdapterByGpuPreference(
               adapterIndex,
               requestHighPerformanceAdapter == true
                   ? DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE
                   : DXGI_GPU_PREFERENCE_UNSPECIFIED,
               IID_PPV_ARGS(&adapter)));
           ++adapterIndex) {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
          // Don't select the Basic Render Driver adapter.
          // If you want a software adapter, pass in "/warp" on the command
          // line.
          continue;
        }
        // Check to see whether the adapter supports Direct3D 12, but don't
        // create the actual device yet.
        if (SUCCEEDED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0,
                                        _uuidof(ID3D12Device), nullptr))) {
          break;
        }
      }
    }
    if (adapter == nullptr) {
      auto killEnumAdapter = [&]() {
        adapter->Release();
        adapter = nullptr;
      };
      for (UINT adapterIndex = 0;
           SUCCEEDED(dxgiFactory->EnumAdapters1(adapterIndex, &adapter));
           ++adapterIndex) {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
          // Don't select the Basic Render Driver adapter.
          // If you want a software adapter, pass in "/warp" on the command
          // line.
          killEnumAdapter();
          continue;
        }
        // Check to see whether the adapter supports Direct3D 12, but don't
        // create the actual device yet.
        if (SUCCEEDED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0,
                                        _uuidof(ID3D12Device), nullptr))) {
          break;
        } else {
          // TODO(Noah): Probably do something more intelligent here?
          killEnumAdapter();
          // TODO(Noah): Add a warning log to engine support.
          AELoggerLog("WARN: D3D12 hardware adapter is not supporting "
                      "D3D_FEATURE_LEVEL_11_0");
        }
      }
    }
    *ppAdapter = adapter;
    if (*ppAdapter == nullptr) {
      AELoggerError("Unable to find desired D3D12 hardware adapter");
    }
  }
}

// TODO: so, dx12 is the windows graphics API. therefore, if the stuff here
//       is like windows specific, that should be OK. but, maybe we need to
//       think about that??

static HMODULE s_dxcModule = nullptr;

// caller should check if the module was loaded.
static void initDXC() {
  if (s_dxcModule != nullptr)
    return;

  const char *path = "res\\dxcompiler.dll";

  HMODULE hDll = ::LoadLibraryA(path);

  if (!hDll) {
    return;
  }

  s_dxcModule = hDll;
}

void _close() {
  if (s_dxcModule == nullptr)
    return;
  ::FreeLibrary(s_dxcModule);
}

// TODO: we could add args to this I guess. but for now this is not needed.
bool compileShader(const char *filePathIn, const WCHAR *entryPoint,
                   const WCHAR *profile, IDxcBlob **blobOut) {
  // load from disk.
  loaded_file_t lf = ae::platform::readEntireFile(filePathIn);
  if (!lf.contents) {
    AELoggerError("unable to read file: %s", filePathIn);
    return false;
  }
  defer(ae::platform::freeLoadedFile(lf));

  const char *srcCode = (const char *)lf.contents;

  initDXC();
  if (!s_dxcModule) {
    AELoggerError("unable to find dxcompiler.dll");
    return false;
  }

  auto createInst =
      (DxcCreateInstanceProc)::GetProcAddress(s_dxcModule, "DxcCreateInstance");

  IDxcLibrary *pLibrary;
  defer(COM_RELEASE(pLibrary));
  createInst(CLSID_DxcLibrary, IID_PPV_ARGS(&pLibrary));
  if (!pLibrary)
    return false;

  IDxcBlobEncoding *pSource;
  defer(COM_RELEASE(pSource));
  pLibrary->CreateBlobWithEncodingFromPinned((LPBYTE)srcCode, lf.contentSize,
                                             CP_UTF8, &pSource);
  if (!pSource)
    return false;

  IDxcCompiler2 *pCompiler;
  defer(COM_RELEASE(pCompiler));
  createInst(CLSID_DxcCompiler, IID_PPV_ARGS(&pCompiler));
  if (!pCompiler)
    return false;

  IDxcOperationResult *pOpRes;
  defer(COM_RELEASE(pOpRes));
  HRESULT res;

  {
    IDxcBlob *pPDB;
    defer(COM_RELEASE(pPDB));
    LPWSTR debugBlobName = NULL; //[1024];
    defer(CoTaskMemFree(debugBlobName));
    res = pCompiler->CompileWithDebug(
        pSource,
        nullptr, // optional file name
        entryPoint, profile,
        nullptr, // args
        0,       // arg count
        nullptr, // defines
        0,       // define count
        nullptr, // optional interface to handle #include.
        &pOpRes,
        &debugBlobName, // suggested file name for debug blob.
        &pPDB);
  }

  IDxcBlob *pResult = nullptr;
  //    defer(COM_RELEASE(pResult));
  IDxcBlobEncoding *pError = nullptr;
  defer(COM_RELEASE(pError));

  if (pOpRes) {
    pOpRes->GetResult(&pResult);
    pOpRes->GetErrorBuffer(&pError);
  }

  if (pError) {
    IDxcBlobEncoding *pErrorUtf8;
    defer(COM_RELEASE(pErrorUtf8));
    pLibrary->GetBlobAsUtf8(pError, &pErrorUtf8);
    if (pErrorUtf8 && pErrorUtf8->GetBufferSize() > 0) {
      AELoggerError("%s", (const char *)pErrorUtf8->GetBufferPointer());
    }
  }

  if (pResult && pResult->GetBufferSize() > 0) {
    *blobOut = pResult;
    return true;
  }

  return false;
}

ID3D12Resource *AllocUploadBuffer(ID3D12Device *device, UINT64 size,
                                  void **pData) {
  ID3D12Resource *res = nullptr;
  D3D12_HEAP_PROPERTIES hprop = {};
  hprop.Type = D3D12_HEAP_TYPE_UPLOAD;
  D3D12_RESOURCE_DESC resDesc = {};
  resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  resDesc.Width = size;
  resDesc.Height = 1;
  resDesc.DepthOrArraySize = 1;
  resDesc.MipLevels = 1;
  resDesc.SampleDesc.Count = 1;
  resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  (device->CreateCommittedResource(&hprop, D3D12_HEAP_FLAG_NONE, &resDesc,
                                   D3D12_RESOURCE_STATE_GENERIC_READ,
                                   nullptr, // optimized clear.
                                   IID_PPV_ARGS(&res)));

  D3D12_RANGE rr = {}; // empty range, will not be reading this resource.
  if (res)
    res->Map(0, &rr, pData);
  return res;
}

#undef COM_RELEASE
} // namespace DX
}; // namespace automata_engine

#endif
