// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/SourceLocation.h"
#include "bindings/core/v8/V8CacheOptions.h"
#include "bindings/core/v8/V8GCController.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/workers/WorkerBackingThread.h"
#include "core/workers/WorkerClients.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerLoaderProxy.h"
#include "core/workers/WorkerReportingProxy.h"
#include "core/workers/WorkerThread.h"
#include "core/workers/WorkerThreadLifecycleObserver.h"
#include "core/workers/WorkerThreadStartupData.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/WaitableEvent.h"
#include "platform/WebThreadSupportingGC.h"
#include "platform/heap/Handle.h"
#include "platform/network/ContentSecurityPolicyParsers.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/WebAddressSpace.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "wtf/CurrentTime.h"
#include "wtf/Forward.h"
#include "wtf/PtrUtil.h"
#include "wtf/Vector.h"
#include <memory>
#include <v8.h>

namespace blink {

class MockWorkerLoaderProxyProvider : public WorkerLoaderProxyProvider {
public:
    MockWorkerLoaderProxyProvider() { }
    ~MockWorkerLoaderProxyProvider() override { }

    void postTaskToLoader(const WebTraceLocation&, std::unique_ptr<ExecutionContextTask>) override
    {
        NOTIMPLEMENTED();
    }

    bool postTaskToWorkerGlobalScope(const WebTraceLocation&, std::unique_ptr<ExecutionContextTask>) override
    {
        NOTIMPLEMENTED();
        return false;
    }
};

class MockWorkerReportingProxy : public WorkerReportingProxy {
public:
    MockWorkerReportingProxy() { }
    ~MockWorkerReportingProxy() override { }

    MOCK_METHOD3(reportExceptionMock, void(const String& errorMessage, SourceLocation*, int exceptionId));
    void reportException(const String& errorMessage, std::unique_ptr<SourceLocation> location, int exceptionId)
    {
        reportExceptionMock(errorMessage, location.get(), exceptionId);
    }
    MOCK_METHOD4(reportConsoleMessage, void(MessageSource, MessageLevel, const String& message, SourceLocation*));
    MOCK_METHOD1(postMessageToPageInspector, void(const String&));
    MOCK_METHOD1(didEvaluateWorkerScript, void(bool success));
    MOCK_METHOD1(workerGlobalScopeStarted, void(WorkerOrWorkletGlobalScope*));
    MOCK_METHOD0(workerGlobalScopeClosed, void());
    MOCK_METHOD0(workerThreadTerminated, void());
    MOCK_METHOD0(willDestroyWorkerGlobalScope, void());
};

class MockWorkerThreadLifecycleObserver final : public GarbageCollectedFinalized<MockWorkerThreadLifecycleObserver>, public WorkerThreadLifecycleObserver {
    USING_GARBAGE_COLLECTED_MIXIN(MockWorkerThreadLifecycleObserver);
    WTF_MAKE_NONCOPYABLE(MockWorkerThreadLifecycleObserver);
public:
    explicit MockWorkerThreadLifecycleObserver(WorkerThreadLifecycleContext* context)
        : WorkerThreadLifecycleObserver(context) { }

    MOCK_METHOD0(contextDestroyed, void());
};

class WorkerThreadForTest : public WorkerThread {
public:
    WorkerThreadForTest(
        WorkerLoaderProxyProvider* mockWorkerLoaderProxyProvider,
        WorkerReportingProxy& mockWorkerReportingProxy)
        : WorkerThread(WorkerLoaderProxy::create(mockWorkerLoaderProxyProvider), mockWorkerReportingProxy)
        , m_workerBackingThread(WorkerBackingThread::createForTest("Test thread"))
        , m_scriptLoadedEvent(wrapUnique(new WaitableEvent()))
    {
    }

    ~WorkerThreadForTest() override { }

    WorkerBackingThread& workerBackingThread() override { return *m_workerBackingThread; }
    ConsoleMessageStorage* consoleMessageStorage() final { return toWorkerGlobalScope(globalScope())->consoleMessageStorage(); }

    WorkerOrWorkletGlobalScope* createWorkerGlobalScope(std::unique_ptr<WorkerThreadStartupData>) override;

    void waitUntilScriptLoaded()
    {
        m_scriptLoadedEvent->wait();
    }

    void scriptLoaded()
    {
        m_scriptLoadedEvent->signal();
    }

    void startWithSourceCode(SecurityOrigin* securityOrigin, const String& source)
    {
        std::unique_ptr<Vector<CSPHeaderAndType>> headers = wrapUnique(new Vector<CSPHeaderAndType>());
        CSPHeaderAndType headerAndType("contentSecurityPolicy", ContentSecurityPolicyHeaderTypeReport);
        headers->append(headerAndType);

        WorkerClients* clients = nullptr;

        start(WorkerThreadStartupData::create(
            KURL(ParsedURLString, "http://fake.url/"),
            "fake user agent",
            source,
            nullptr,
            DontPauseWorkerGlobalScopeOnStart,
            headers.get(),
            "",
            securityOrigin,
            clients,
            WebAddressSpaceLocal,
            nullptr,
            nullptr,
            V8CacheOptionsDefault));
    }

    void waitForInit()
    {
        std::unique_ptr<WaitableEvent> completionEvent = wrapUnique(new WaitableEvent());
        workerBackingThread().backingThread().postTask(BLINK_FROM_HERE, crossThreadBind(&WaitableEvent::signal, crossThreadUnretained(completionEvent.get())));
        completionEvent->wait();
    }

private:
    std::unique_ptr<WorkerBackingThread> m_workerBackingThread;
    std::unique_ptr<WaitableEvent> m_scriptLoadedEvent;
};

class FakeWorkerGlobalScope : public WorkerGlobalScope {
public:
    FakeWorkerGlobalScope(const KURL& url, const String& userAgent, WorkerThreadForTest* thread, std::unique_ptr<SecurityOrigin::PrivilegeData> starterOriginPrivilegeData, WorkerClients* workerClients)
        : WorkerGlobalScope(url, userAgent, thread, monotonicallyIncreasingTime(), std::move(starterOriginPrivilegeData), workerClients)
        , m_thread(thread)
    {
    }

    ~FakeWorkerGlobalScope() override
    {
    }

    void scriptLoaded(size_t, size_t) override
    {
        m_thread->scriptLoaded();
    }

    // EventTarget
    const AtomicString& interfaceName() const override
    {
        return EventTargetNames::DedicatedWorkerGlobalScope;
    }

    void exceptionThrown(ErrorEvent*) override
    {
    }

private:
    WorkerThreadForTest* m_thread;
};

inline WorkerOrWorkletGlobalScope* WorkerThreadForTest::createWorkerGlobalScope(std::unique_ptr<WorkerThreadStartupData> startupData)
{
    return new FakeWorkerGlobalScope(startupData->m_scriptURL, startupData->m_userAgent, this, std::move(startupData->m_starterOriginPrivilegeData), std::move(startupData->m_workerClients));
}

} // namespace blink
