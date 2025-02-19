// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_UPDATE_CLIENT_H_
#define COMPONENTS_UPDATE_CLIENT_UPDATE_CLIENT_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/version.h"

// The UpdateClient class is a facade with a simple interface. The interface
// exposes a few APIs to install a CRX or update a group of CRXs.
//
// The difference between a CRX install and a CRX update is relatively minor.
// The terminology going forward will use the word "update" to cover both
// install and update scenarios, except where details regarding the install
// case are relevant.
//
// Handling an update consists of a series of actions such as sending an update
// check to the server, followed by parsing the server response, identifying
// the CRXs that require an update, downloading the differential update if
// it is available, unpacking and patching the differential update, then
// falling back to trying a similar set of actions using the full update.
// At the end of this process, completion pings are sent to the server,
// as needed, for the CRXs which had updates.
//
// As a general idea, this code handles the action steps needed to update
// a group of components serially, one step at a time. However, concurrent
// execution of calls to UpdateClient::Update is possible, therefore,
// queuing of updates could happen in some cases. More below.
//
// The UpdateClient class features a subject-observer interface to observe
// the CRX state changes during an update.
//
// The threading model for this code assumes that most of the code in the
// public interface runs on a SingleThreadTaskRunner.
// This task runner corresponds to the browser UI thread in many cases. There
// are parts of the installer interface that run on blocking task runners, which
// are usually threads in a thread pool.
//
// Using the UpdateClient is relatively easy. This assumes that the client
// of this code has already implemented the observer interface as needed, and
// can provide an installer, as described below.
//
//    std::unique_ptr<UpdateClient> update_client(UpdateClientFactory(...));
//    update_client->AddObserver(&observer);
//    std::vector<std::string> ids;
//    ids.push_back(...));
//    update_client->Update(ids, base::Bind(...), base::Bind(...));
//
// UpdateClient::Update takes two callbacks as parameters. First callback
// allows the client of this code to provide an instance of CrxComponent
// data structure that specifies additional parameters of the update.
// CrxComponent has a CrxInstaller data member, which must be provided by the
// callers of this class. The second callback indicates that this non-blocking
// call has completed.
//
// There could be several ways of triggering updates for a CRX, user-initiated,
// or timer-based. Since the execution of updates is concurrent, the parameters
// for the update must be provided right before the update is handled.
// Otherwise, the version of the CRX set in the CrxComponent may not be correct.
//
// The UpdateClient public interface includes two functions: Install and
// Update. These functions correspond to installing one CRX immediately as a
// foreground activity (Install), and updating a group of CRXs silently in the
// background (Update). This distinction is important. Background updates are
// queued up and their actions run serially, one at a time, for the purpose of
// conserving local resources such as CPU, network, and I/O.
// On the other hand, installs are never queued up but run concurrently, as
// requested by the user.
//
// The update client introduces a runtime constraint regarding interleaving
// updates and installs. If installs or updates for a given CRX are in progress,
// then installs for the same CRX will fail with a specific error.
//
// Implementation details.
//
// The implementation details below are not relevant to callers of this
// code. However, these design notes are relevant to the owners and maintainers
// of this module.
//
// The design for the update client consists of a number of abstractions
// such as: task, update engine, update context, and action.
// The execution model for these abstractions is simple. They usually expose
// a public, non-blocking Run function, and they invoke a callback when
// the Run function has completed.
//
// A task is the unit of work for the UpdateClient. A task is associated
// with a single call of the Update function. A task represents a group
// of CRXs that are updated together.
//
// The UpdateClient is responsible for the queuing of tasks, if queuing is
// needed.
//
// When the task runs, it calls the update engine to handle the updates for
// the CRXs associated with the task. The UpdateEngine is the abstraction
// responsible for breaking down the update in a set of discrete steps, which
// are implemented as actions, and running the actions.
//
// The UpdateEngine maintains a set of UpdateContext instances. Each of
// these instances maintains the update state for all the CRXs belonging to
// a given task. The UpdateContext contains a queue of CRX ids.
// The UpdateEngine will handle updates for the CRXs in the order they appear
// in the queue, until the queue is empty.
//
// The update state for each CRX is maintained in a container of CrxUpdateItem*.
// As actions run, each action updates the CRX state, represented by one of
// these CrxUpdateItem* instances.
//
// Although the UpdateEngine can and will run update tasks concurrently, the
// actions of a task are run sequentially.
//
// The Action is a polymorphic type. There is some code reuse for convenience,
// implemented as a mixin. The polymorphic behavior of some of the actions
// is achieved using a template method.
//
// State changes of a CRX could generate events, which are observed using a
// subject-observer interface.
//
// The actions chain up. In some sense, the actions implement a state machine,
// as the CRX undergoes a series of state transitions in the process of
// being checked for updates and applying the update.

class ComponentsUI;
class PrefRegistrySimple;

namespace base {
class DictionaryValue;
class FilePath;
}

namespace update_client {

class Configurator;
struct CrxUpdateItem;

enum Error {
  ERROR_UPDATE_INVALID_ARGUMENT = -1,
  ERROR_UPDATE_IN_PROGRESS = 1,
  ERROR_UPDATE_CANCELED = 2,
  ERROR_UPDATE_RETRY_LATER = 3,
};

// Defines an interface for a generic CRX installer.
class CrxInstaller : public base::RefCountedThreadSafe<CrxInstaller> {
 public:
  // Called on the main thread when there was a problem unpacking or
  // verifying the CRX. |error| is a non-zero value which is only meaningful
  // to the caller.
  virtual void OnUpdateError(int error) = 0;

  // Called by the update service when a CRX has been unpacked
  // and it is ready to be installed. |manifest| contains the CRX manifest
  // as a json dictionary.|unpack_path| contains the temporary directory
  // with all the unpacked CRX files.
  // This method may be called from a thread other than the main thread.
  virtual bool Install(const base::DictionaryValue& manifest,
                       const base::FilePath& unpack_path) = 0;

  // Sets |installed_file| to the full path to the installed |file|. |file| is
  // the filename of the file in this CRX. Returns false if this is
  // not possible (the file has been removed or modified, or its current
  // location is unknown). Otherwise, it returns true.
  virtual bool GetInstalledFile(const std::string& file,
                                base::FilePath* installed_file) = 0;

  // Called when a CRX has been unregistered and all versions should
  // be uninstalled from disk. Returns true if uninstallation is supported,
  // and false otherwise.
  virtual bool Uninstall() = 0;

 protected:
  friend class base::RefCountedThreadSafe<CrxInstaller>;

  virtual ~CrxInstaller() {}
};

// A dictionary of installer-specific, arbitrary name-value pairs, which
// may be used in the update checks requests.
using InstallerAttributes = std::map<std::string, std::string>;

// TODO(sorin): this structure will be refactored soon.
struct CrxComponent {
  CrxComponent();
  CrxComponent(const CrxComponent& other);
  ~CrxComponent();

  // SHA256 hash of the CRX's public key.
  std::vector<uint8_t> pk_hash;
  scoped_refptr<CrxInstaller> installer;

  // The current version if the CRX is updated. Otherwise, "0" or "0.0" if
  // the CRX is installed.
  Version version;

  std::string fingerprint;  // Optional.
  std::string name;         // Optional.
  std::vector<std::string> handled_mime_types;

  // Optional.
  // Valid values for the name part of an attribute match
  // ^[-_a-zA-Z0-9]{1,256}$ and valid values the value part of an attribute
  // match ^[-.,;+_=a-zA-Z0-9]{0,256}$ .
  InstallerAttributes installer_attributes;

  // Specifies that the CRX can be background-downloaded in some cases.
  // The default for this value is |true|.
  bool allows_background_download;

  // Specifies that the update checks and pings associated with this component
  // require confidentiality. The default for this value is |true|. As a side
  // note, the confidentiality of the downloads is enforced by the server,
  // which only returns secure download URLs in this case.
  bool requires_network_encryption;

  // True if the component allows enabling or disabling updates by group policy.
  // This member should be set to |false| for data, non-binary components, such
  // as CRLSet, Supervised User Whitelists, STH Set, Origin Trials, and File
  // Type Policies.
  bool supports_group_policy_enable_component_updates;
};

// All methods are safe to call only from the browser's main thread. Once an
// instance of this class is created, the reference to it must be released
// only after the thread pools of the browser process have been destroyed and
// the browser process has gone single-threaded.
class UpdateClient : public base::RefCounted<UpdateClient> {
 public:
  using CrxDataCallback =
      base::Callback<void(const std::vector<std::string>& ids,
                          std::vector<CrxComponent>* components)>;
  using CompletionCallback = base::Callback<void(int error)>;

  // Defines an interface to observe the UpdateClient. It provides
  // notifications when state changes occur for the service itself or for the
  // registered CRXs.
  class Observer {
   public:
    enum class Events {
      // Sent before the update client does an update check.
      COMPONENT_CHECKING_FOR_UPDATES,

      // Sent when there is a new version of a registered CRX. After
      // the notification is sent the CRX will be downloaded unless the
      // update client inserts a
      COMPONENT_UPDATE_FOUND,

      // Sent when a CRX is in the update queue but it can't be acted on
      // right away, because the update client spaces out CRX updates due to a
      // throttling policy.
      COMPONENT_WAIT,

      // Sent after the new CRX has been downloaded but before the install
      // or the upgrade is attempted.
      COMPONENT_UPDATE_READY,

      // Sent when a CRX has been successfully updated.
      COMPONENT_UPDATED,

      // Sent when a CRX has not been updated following an update check:
      // either there was no update available, or the update failed.
      COMPONENT_NOT_UPDATED,

      // Sent when CRX bytes are being downloaded.
      COMPONENT_UPDATE_DOWNLOADING,
    };

    virtual ~Observer() {}

    // Called by the update client when a state change happens.
    // If an |id| is specified, then the event is fired on behalf of the
    // specific CRX. The implementors of this interface are
    // expected to filter the relevant events based on the id of the CRX.
    virtual void OnEvent(Events event, const std::string& id) = 0;
  };

  // Adds an observer for this class. An observer should not be added more
  // than once. The caller retains the ownership of the observer object.
  virtual void AddObserver(Observer* observer) = 0;

  // Removes an observer. It is safe for an observer to be removed while
  // the observers are being notified.
  virtual void RemoveObserver(Observer* observer) = 0;

  // Installs the specified CRX. Calls back on |completion_callback| after the
  // update has been handled. The |error| parameter of the |completion_callback|
  // contains an error code in the case of a run-time error, or 0 if the
  // install has been handled successfully. Overlapping calls of this function
  // are executed concurrently, as long as the id parameter is different,
  // meaning that installs of different components are parallelized.
  // The |Install| function is intended to be used for foreground installs of
  // one CRX. These cases are usually associated with on-demand install
  // scenarios, which are triggered by user actions. Installs are never
  // queued up.
  virtual void Install(const std::string& id,
                       const CrxDataCallback& crx_data_callback,
                       const CompletionCallback& completion_callback) = 0;

  // Updates the specified CRXs. Calls back on |crx_data_callback| before the
  // update is attempted to give the caller the opportunity to provide the
  // instances of CrxComponent to be used for this update. The |Update| function
  // is intended to be used for background updates of several CRXs. Overlapping
  // calls to this function result in a queuing behavior, and the execution
  // of each call is serialized. In addition, updates are always queued up when
  // installs are running.
  virtual void Update(const std::vector<std::string>& ids,
                      const CrxDataCallback& crx_data_callback,
                      const CompletionCallback& completion_callback) = 0;

  // Sends an uninstall ping for the CRX identified by |id| and |version|. The
  // |reason| parameter is defined by the caller. The current implementation of
  // this function only sends a best-effort, fire-and-forget ping. It has no
  // other side effects regarding installs or updates done through an instance
  // of this class.
  virtual void SendUninstallPing(const std::string& id,
                                 const Version& version,
                                 int reason) = 0;

  // Returns status details about a CRX update. The function returns true in
  // case of success and false in case of errors, such as |id| was
  // invalid or not known.
  virtual bool GetCrxUpdateState(const std::string& id,
                                 CrxUpdateItem* update_item) const = 0;

  // Returns true if the |id| is found in any running task.
  virtual bool IsUpdating(const std::string& id) const = 0;

  // Cancels the queued updates and makes a best effort to stop updates in
  // progress as soon as possible. Some updates may not be stopped, in which
  // case, the updates will run to completion. Calling this function has no
  // effect if updates are not currently executed or queued up.
  virtual void Stop() = 0;

 protected:
  friend class base::RefCounted<UpdateClient>;

  virtual ~UpdateClient() {}
};

// Creates an instance of the update client.
scoped_refptr<UpdateClient> UpdateClientFactory(
    const scoped_refptr<Configurator>& config);

// This must be called prior to the construction of any Configurator that
// contains a PrefService.
void RegisterPrefs(PrefRegistrySimple* registry);

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_UPDATE_CLIENT_H_
