#include <automata_engine.hpp>

// TODO: can we make this stuff not windows specific?
#define NOMINMAX
#include <Windows.h>

#include <dxcapi.h>  // stuff needed for DXC compiler.

namespace automata_engine {
    namespace HLSL {

#define COM_RELEASE(comPtr) (comPtr != nullptr) ? comPtr->Release() : 0;

        static HMODULE s_dxcModule = nullptr;

        // caller should check if the module was loaded.
        static void initDXC()
        {
            if (s_dxcModule != nullptr) return;

            const char *path = "res\\dxcompiler.dll";

            HMODULE hDll = ::LoadLibraryA(path);

            if (!hDll) { return; }

            s_dxcModule = hDll;
        }

        void _close()
        {
            if (s_dxcModule == nullptr) return;
            ::FreeLibrary(s_dxcModule);
        }

        // TODO: we could add args to this I guess. but for now this is not needed.
        bool compileBlobFromFile(
            const char *filePathIn, const WCHAR *entryPoint, const WCHAR *profile, IDxcBlob **blobOut, bool emitSpirv)
        {
            // load from disk.
            loaded_file_t lf = ae::EM->pfn.readEntireFile(filePathIn);
            if (!lf.contents) {
                AELoggerError("unable to read file: %s", filePathIn);
                return false;
            }
            defer(ae::EM->pfn.freeLoadedFile(lf));

            const char *srcCode = (const char *)lf.contents;

            initDXC();
            if (!s_dxcModule) {
                AELoggerError("unable to find dxcompiler.dll");
                return false;
            }

            std::vector<LPCWSTR>   args;
            std::vector<DxcDefine> defs;  // = {{L"VULKAN", L"1"}};

            if (emitSpirv) {
                args.push_back(L"-spirv");
                args.push_back(L"-T");
                args.push_back(profile);

                args.push_back(L"-E");
                args.push_back(entryPoint);

                // TODO: maybe have the user request if they want raytracing or not.
                args.push_back(L"-fspv-target-env=vulkan1.2");
                args.push_back(L"-fspv-extension=SPV_KHR_ray_tracing");
                args.push_back(L"-fspv-extension=SPV_KHR_ray_query");  // TODO: not sure why this is needed?

                // TODO: technically, spirv is not a VK specific thing??
                defs.push_back({L"VULKAN", L"1"});
            }

            auto createInst = (DxcCreateInstanceProc)::GetProcAddress(s_dxcModule, "DxcCreateInstance");

            IDxcLibrary *pLibrary;
            defer(COM_RELEASE(pLibrary));
            createInst(CLSID_DxcLibrary, IID_PPV_ARGS(&pLibrary));
            if (!pLibrary) return false;

            IDxcBlobEncoding *pSource;
            defer(COM_RELEASE(pSource));
            pLibrary->CreateBlobWithEncodingFromPinned((LPBYTE)srcCode, lf.contentSize, CP_UTF8, &pSource);
            if (!pSource) return false;

            IDxcCompiler2 *pCompiler;
            defer(COM_RELEASE(pCompiler));
            createInst(CLSID_DxcCompiler, IID_PPV_ARGS(&pCompiler));
            if (!pCompiler) return false;

            IDxcOperationResult *pOpRes;
            defer(COM_RELEASE(pOpRes));
            HRESULT res;

            {
                IDxcBlob *pPDB;
                defer(COM_RELEASE(pPDB));
                LPWSTR debugBlobName = NULL;  //[1024];
                defer(CoTaskMemFree(debugBlobName));
                res = pCompiler->CompileWithDebug(pSource,
                    nullptr,  // optional file name
                    entryPoint,
                    profile,
                    args.data(),
                    (UINT32)args.size(),
                    defs.data(),  // defines
                    defs.size(),  // define count
                    nullptr,      // optional interface to handle #include.
                    &pOpRes,
                    &debugBlobName,  // suggested file name for debug blob.
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

#undef COM_RELEASE
    }  // namespace HLSL
};     // namespace automata_engine
