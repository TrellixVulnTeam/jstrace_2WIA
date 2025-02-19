// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/certificate_provider/certificate_provider_service_factory.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/values.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider_service.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/common/extensions/api/certificate_provider.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/event_listener_map.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_private_key.h"

namespace chromeos {

namespace {

namespace api_cp = extensions::api::certificate_provider;

class DefaultDelegate : public CertificateProviderService::Delegate,
                        public extensions::ExtensionRegistryObserver {
 public:
  DefaultDelegate(CertificateProviderService* service,
                  extensions::ExtensionRegistry* registry,
                  extensions::EventRouter* event_router);
  ~DefaultDelegate() override;

  // CertificateProviderService::Delegate:
  std::vector<std::string> CertificateProviderExtensions() override;
  void BroadcastCertificateRequest(int request_id) override;
  bool DispatchSignRequestToExtension(
      const std::string& extension_id,
      int request_id,
      net::SSLPrivateKey::Hash hash,
      const scoped_refptr<net::X509Certificate>& certificate,
      const std::string& digest) override;

  // extensions::ExtensionRegistryObserver:
  void OnExtensionUnloaded(
      content::BrowserContext* browser_context,
      const extensions::Extension* extension,
      extensions::UnloadedExtensionInfo::Reason reason) override;

 private:
  CertificateProviderService* const service_;
  extensions::ExtensionRegistry* const registry_;
  extensions::EventRouter* const event_router_;

  DISALLOW_COPY_AND_ASSIGN(DefaultDelegate);
};

DefaultDelegate::DefaultDelegate(CertificateProviderService* service,
                                 extensions::ExtensionRegistry* registry,
                                 extensions::EventRouter* event_router)
    : service_(service), registry_(registry), event_router_(event_router) {
  DCHECK(service_);
  DCHECK(event_router_);
  registry_->AddObserver(this);
}

DefaultDelegate::~DefaultDelegate() {
  registry_->RemoveObserver(this);
}

std::vector<std::string> DefaultDelegate::CertificateProviderExtensions() {
  const std::string event_name(api_cp::OnCertificatesRequested::kEventName);
  std::vector<std::string> ids;
  for (const auto& listener :
       event_router_->listeners().GetEventListenersByName(event_name)) {
    ids.push_back(listener->extension_id());
  }
  return ids;
}

void DefaultDelegate::BroadcastCertificateRequest(int request_id) {
  const std::string event_name(api_cp::OnCertificatesRequested::kEventName);
  std::unique_ptr<base::ListValue> internal_args(new base::ListValue);
  internal_args->AppendInteger(request_id);
  std::unique_ptr<extensions::Event> event(new extensions::Event(
      extensions::events::CERTIFICATEPROVIDER_ON_CERTIFICATES_REQUESTED,
      event_name, std::move(internal_args)));
  event_router_->BroadcastEvent(std::move(event));
}

bool DefaultDelegate::DispatchSignRequestToExtension(
    const std::string& extension_id,
    int request_id,
    net::SSLPrivateKey::Hash hash,
    const scoped_refptr<net::X509Certificate>& certificate,
    const std::string& digest) {
  const std::string event_name(api_cp::OnSignDigestRequested::kEventName);
  if (!event_router_->ExtensionHasEventListener(extension_id, event_name))
    return false;

  api_cp::SignRequest request;
  switch (hash) {
    case net::SSLPrivateKey::Hash::MD5_SHA1:
      request.hash = api_cp::HASH_MD5_SHA1;
      break;
    case net::SSLPrivateKey::Hash::SHA1:
      request.hash = api_cp::HASH_SHA1;
      break;
    case net::SSLPrivateKey::Hash::SHA256:
      request.hash = api_cp::HASH_SHA256;
      break;
    case net::SSLPrivateKey::Hash::SHA384:
      request.hash = api_cp::HASH_SHA384;
      break;
    case net::SSLPrivateKey::Hash::SHA512:
      request.hash = api_cp::HASH_SHA512;
      break;
  }
  request.digest.assign(digest.begin(), digest.end());
  std::string cert_der;
  if (!net::X509Certificate::GetDEREncoded(certificate->os_cert_handle(),
                                           &cert_der)) {
    LOG(ERROR) << "Could not DER encode the certificate.";
    return false;  // Behave as if the extension wasn't registered anymore.
  }
  request.certificate.assign(cert_der.begin(), cert_der.end());

  std::unique_ptr<base::ListValue> internal_args(new base::ListValue);
  internal_args->AppendInteger(request_id);
  internal_args->Append(request.ToValue());

  event_router_->DispatchEventToExtension(
      extension_id,
      base::WrapUnique(new extensions::Event(
          extensions::events::CERTIFICATEPROVIDER_ON_SIGN_DIGEST_REQUESTED,
          event_name, std::move(internal_args))));
  return true;
}

void DefaultDelegate::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionInfo::Reason reason) {
  service_->OnExtensionUnloaded(extension->id());
}

}  // namespace

// static
CertificateProviderService*
CertificateProviderServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<CertificateProviderService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
CertificateProviderServiceFactory*
CertificateProviderServiceFactory::GetInstance() {
  return base::Singleton<CertificateProviderServiceFactory>::get();
}

CertificateProviderServiceFactory::CertificateProviderServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "CertificateProviderService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(extensions::EventRouterFactory::GetInstance());
  DependsOn(extensions::ExtensionRegistryFactory::GetInstance());
}

content::BrowserContext*
CertificateProviderServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

bool CertificateProviderServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

KeyedService* CertificateProviderServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  CertificateProviderService* const service = new CertificateProviderService();
  service->SetDelegate(base::WrapUnique(new DefaultDelegate(
      service,
      extensions::ExtensionRegistryFactory::GetForBrowserContext(context),
      extensions::EventRouterFactory::GetForBrowserContext(context))));
  return service;
}

}  // namespace chromeos
