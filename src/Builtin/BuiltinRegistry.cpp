/// \file BuiltinRegistry.cpp
/// \brief 内置函数注册表实现。

#include "yuan/Builtin/BuiltinRegistry.h"
#include <cassert>

namespace yuan {

// 前向声明所有内置函数处理器的创建函数
std::unique_ptr<BuiltinHandler> createImportBuiltin();
std::unique_ptr<BuiltinHandler> createSizeofBuiltin();
std::unique_ptr<BuiltinHandler> createAlignofBuiltin();
std::unique_ptr<BuiltinHandler> createTypeofBuiltin();
std::unique_ptr<BuiltinHandler> createPlatformOsBuiltin();
std::unique_ptr<BuiltinHandler> createPlatformArchBuiltin();
std::unique_ptr<BuiltinHandler> createPlatformPointerBitsBuiltin();
std::unique_ptr<BuiltinHandler> createPanicBuiltin();
std::unique_ptr<BuiltinHandler> createAssertBuiltin();
std::unique_ptr<BuiltinHandler> createFileBuiltin();
std::unique_ptr<BuiltinHandler> createLineBuiltin();
std::unique_ptr<BuiltinHandler> createColumnBuiltin();
std::unique_ptr<BuiltinHandler> createFuncBuiltin();
std::unique_ptr<BuiltinHandler> createPrintBuiltin();
std::unique_ptr<BuiltinHandler> createFormatBuiltin();
std::unique_ptr<BuiltinHandler> createAllocBuiltin();
std::unique_ptr<BuiltinHandler> createReallocBuiltin();
std::unique_ptr<BuiltinHandler> createFreeBuiltin();
std::unique_ptr<BuiltinHandler> createMemcpyBuiltin();
std::unique_ptr<BuiltinHandler> createMemmoveBuiltin();
std::unique_ptr<BuiltinHandler> createMemsetBuiltin();
std::unique_ptr<BuiltinHandler> createStrFromPartsBuiltin();
std::unique_ptr<BuiltinHandler> createSliceBuiltin();
std::unique_ptr<BuiltinHandler> createAsyncSchedulerCreateBuiltin();
std::unique_ptr<BuiltinHandler> createAsyncSchedulerDestroyBuiltin();
std::unique_ptr<BuiltinHandler> createAsyncSchedulerSetCurrentBuiltin();
std::unique_ptr<BuiltinHandler> createAsyncSchedulerCurrentBuiltin();
std::unique_ptr<BuiltinHandler> createAsyncSchedulerRunOneBuiltin();
std::unique_ptr<BuiltinHandler> createAsyncSchedulerRunUntilIdleBuiltin();
std::unique_ptr<BuiltinHandler> createAsyncPromiseCreateBuiltin();
std::unique_ptr<BuiltinHandler> createAsyncPromiseRetainBuiltin();
std::unique_ptr<BuiltinHandler> createAsyncPromiseReleaseBuiltin();
std::unique_ptr<BuiltinHandler> createAsyncPromiseStatusBuiltin();
std::unique_ptr<BuiltinHandler> createAsyncPromiseValueBuiltin();
std::unique_ptr<BuiltinHandler> createAsyncPromiseErrorBuiltin();
std::unique_ptr<BuiltinHandler> createAsyncPromiseResolveBuiltin();
std::unique_ptr<BuiltinHandler> createAsyncPromiseRejectBuiltin();
std::unique_ptr<BuiltinHandler> createAsyncPromiseAwaitBuiltin();
std::unique_ptr<BuiltinHandler> createAsyncStepBuiltin();
std::unique_ptr<BuiltinHandler> createAsyncStepCountBuiltin();
std::unique_ptr<BuiltinHandler> createOsTimeUnixNanosBuiltin();
std::unique_ptr<BuiltinHandler> createOsSleepNanosBuiltin();
std::unique_ptr<BuiltinHandler> createOsYieldBuiltin();
std::unique_ptr<BuiltinHandler> createOsThreadSpawnBuiltin();
std::unique_ptr<BuiltinHandler> createOsThreadIsFinishedBuiltin();
std::unique_ptr<BuiltinHandler> createOsThreadJoinBuiltin();
std::unique_ptr<BuiltinHandler> createOsReadFileBuiltin();
std::unique_ptr<BuiltinHandler> createOsWriteFileBuiltin();
std::unique_ptr<BuiltinHandler> createOsExistsBuiltin();
std::unique_ptr<BuiltinHandler> createOsIsFileBuiltin();
std::unique_ptr<BuiltinHandler> createOsIsDirBuiltin();
std::unique_ptr<BuiltinHandler> createOsCreateDirBuiltin();
std::unique_ptr<BuiltinHandler> createOsCreateDirAllBuiltin();
std::unique_ptr<BuiltinHandler> createOsRemoveDirBuiltin();
std::unique_ptr<BuiltinHandler> createOsRemoveFileBuiltin();
std::unique_ptr<BuiltinHandler> createOsReadDirOpenBuiltin();
std::unique_ptr<BuiltinHandler> createOsReadDirNextBuiltin();
std::unique_ptr<BuiltinHandler> createOsReadDirEntryPathBuiltin();
std::unique_ptr<BuiltinHandler> createOsReadDirEntryNameBuiltin();
std::unique_ptr<BuiltinHandler> createOsReadDirEntryIsFileBuiltin();
std::unique_ptr<BuiltinHandler> createOsReadDirEntryIsDirBuiltin();
std::unique_ptr<BuiltinHandler> createOsReadDirCloseBuiltin();
std::unique_ptr<BuiltinHandler> createOsStdinReadLineBuiltin();
std::unique_ptr<BuiltinHandler> createOsHttpGetStatusBuiltin();
std::unique_ptr<BuiltinHandler> createOsHttpGetBodyBuiltin();
std::unique_ptr<BuiltinHandler> createOsHttpPostStatusBuiltin();
std::unique_ptr<BuiltinHandler> createOsHttpPostBodyBuiltin();
std::unique_ptr<BuiltinHandler> createFfiOpenBuiltin();
std::unique_ptr<BuiltinHandler> createFfiOpenSelfBuiltin();
std::unique_ptr<BuiltinHandler> createFfiSymBuiltin();
std::unique_ptr<BuiltinHandler> createFfiCloseBuiltin();
std::unique_ptr<BuiltinHandler> createFfiLastErrorBuiltin();
std::unique_ptr<BuiltinHandler> createFfiCStrLenBuiltin();
std::unique_ptr<BuiltinHandler> createFfiCall0Builtin();
std::unique_ptr<BuiltinHandler> createFfiCall1Builtin();
std::unique_ptr<BuiltinHandler> createFfiCall2Builtin();
std::unique_ptr<BuiltinHandler> createFfiCall3Builtin();
std::unique_ptr<BuiltinHandler> createFfiCall4Builtin();
std::unique_ptr<BuiltinHandler> createFfiCall5Builtin();
std::unique_ptr<BuiltinHandler> createFfiCall6Builtin();

BuiltinRegistry& BuiltinRegistry::instance() {
    static BuiltinRegistry registry;
    return registry;
}

BuiltinRegistry::BuiltinRegistry() {
    registerAllBuiltins();
}

BuiltinRegistry::~BuiltinRegistry() = default;

void BuiltinRegistry::registerHandler(std::unique_ptr<BuiltinHandler> handler) {
    assert(handler && "Handler cannot be null");
    
    std::string name(handler->getName());
    BuiltinKind kind = handler->getKind();
    
    // 检查是否已注册
    assert(NameToHandler.find(name) == NameToHandler.end() &&
           "Handler with this name already registered");
    assert(KindToHandler.find(kind) == KindToHandler.end() &&
           "Handler with this kind already registered");
    
    // 保存原始指针用于 KindToHandler
    BuiltinHandler* rawPtr = handler.get();
    
    // 注册到两个映射表
    NameToHandler[name] = std::move(handler);
    KindToHandler[kind] = rawPtr;
}

BuiltinHandler* BuiltinRegistry::getHandler(const std::string& name) const {
    auto it = NameToHandler.find(name);
    if (it != NameToHandler.end()) {
        return it->second.get();
    }
    return nullptr;
}

BuiltinHandler* BuiltinRegistry::getHandler(BuiltinKind kind) const {
    auto it = KindToHandler.find(kind);
    if (it != KindToHandler.end()) {
        return it->second;
    }
    return nullptr;
}

bool BuiltinRegistry::isBuiltin(const std::string& name) const {
    return NameToHandler.find(name) != NameToHandler.end();
}

std::vector<std::string> BuiltinRegistry::getAllBuiltinNames() const {
    std::vector<std::string> names;
    names.reserve(NameToHandler.size());
    for (const auto& pair : NameToHandler) {
        names.push_back(pair.first);
    }
    return names;
}

void BuiltinRegistry::registerAllBuiltins() {
    // 注册所有内置函数
    registerHandler(createImportBuiltin());
    registerHandler(createSizeofBuiltin());
    registerHandler(createAlignofBuiltin());
    registerHandler(createTypeofBuiltin());
    registerHandler(createPlatformOsBuiltin());
    registerHandler(createPlatformArchBuiltin());
    registerHandler(createPlatformPointerBitsBuiltin());
    registerHandler(createPanicBuiltin());
    registerHandler(createAssertBuiltin());
    registerHandler(createFileBuiltin());
    registerHandler(createLineBuiltin());
    registerHandler(createColumnBuiltin());
    registerHandler(createFuncBuiltin());
    registerHandler(createPrintBuiltin());
    registerHandler(createFormatBuiltin());
    registerHandler(createAllocBuiltin());
    registerHandler(createReallocBuiltin());
    registerHandler(createFreeBuiltin());
    registerHandler(createMemcpyBuiltin());
    registerHandler(createMemmoveBuiltin());
    registerHandler(createMemsetBuiltin());
    registerHandler(createStrFromPartsBuiltin());
    registerHandler(createSliceBuiltin());
    registerHandler(createAsyncSchedulerCreateBuiltin());
    registerHandler(createAsyncSchedulerDestroyBuiltin());
    registerHandler(createAsyncSchedulerSetCurrentBuiltin());
    registerHandler(createAsyncSchedulerCurrentBuiltin());
    registerHandler(createAsyncSchedulerRunOneBuiltin());
    registerHandler(createAsyncSchedulerRunUntilIdleBuiltin());
    registerHandler(createAsyncPromiseCreateBuiltin());
    registerHandler(createAsyncPromiseRetainBuiltin());
    registerHandler(createAsyncPromiseReleaseBuiltin());
    registerHandler(createAsyncPromiseStatusBuiltin());
    registerHandler(createAsyncPromiseValueBuiltin());
    registerHandler(createAsyncPromiseErrorBuiltin());
    registerHandler(createAsyncPromiseResolveBuiltin());
    registerHandler(createAsyncPromiseRejectBuiltin());
    registerHandler(createAsyncPromiseAwaitBuiltin());
    registerHandler(createAsyncStepBuiltin());
    registerHandler(createAsyncStepCountBuiltin());
    registerHandler(createOsTimeUnixNanosBuiltin());
    registerHandler(createOsSleepNanosBuiltin());
    registerHandler(createOsYieldBuiltin());
    registerHandler(createOsThreadSpawnBuiltin());
    registerHandler(createOsThreadIsFinishedBuiltin());
    registerHandler(createOsThreadJoinBuiltin());
    registerHandler(createOsReadFileBuiltin());
    registerHandler(createOsWriteFileBuiltin());
    registerHandler(createOsExistsBuiltin());
    registerHandler(createOsIsFileBuiltin());
    registerHandler(createOsIsDirBuiltin());
    registerHandler(createOsCreateDirBuiltin());
    registerHandler(createOsCreateDirAllBuiltin());
    registerHandler(createOsRemoveDirBuiltin());
    registerHandler(createOsRemoveFileBuiltin());
    registerHandler(createOsReadDirOpenBuiltin());
    registerHandler(createOsReadDirNextBuiltin());
    registerHandler(createOsReadDirEntryPathBuiltin());
    registerHandler(createOsReadDirEntryNameBuiltin());
    registerHandler(createOsReadDirEntryIsFileBuiltin());
    registerHandler(createOsReadDirEntryIsDirBuiltin());
    registerHandler(createOsReadDirCloseBuiltin());
    registerHandler(createOsStdinReadLineBuiltin());
    registerHandler(createOsHttpGetStatusBuiltin());
    registerHandler(createOsHttpGetBodyBuiltin());
    registerHandler(createOsHttpPostStatusBuiltin());
    registerHandler(createOsHttpPostBodyBuiltin());
    registerHandler(createFfiOpenBuiltin());
    registerHandler(createFfiOpenSelfBuiltin());
    registerHandler(createFfiSymBuiltin());
    registerHandler(createFfiCloseBuiltin());
    registerHandler(createFfiLastErrorBuiltin());
    registerHandler(createFfiCStrLenBuiltin());
    registerHandler(createFfiCall0Builtin());
    registerHandler(createFfiCall1Builtin());
    registerHandler(createFfiCall2Builtin());
    registerHandler(createFfiCall3Builtin());
    registerHandler(createFfiCall4Builtin());
    registerHandler(createFfiCall5Builtin());
    registerHandler(createFfiCall6Builtin());
}

} // namespace yuan
