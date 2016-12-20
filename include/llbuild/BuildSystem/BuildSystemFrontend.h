//===- BuildSystemFrontend.h ------------------------------------*- C++ -*-===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2015 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See http://swift.org/LICENSE.txt for license information
// See http://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

#ifndef LLBUILD_BUILDSYSTEM_BUILDSYSTEMFRONTEND_H
#define LLBUILD_BUILDSYSTEM_BUILDSYSTEMFRONTEND_H

#include "llbuild/Basic/LLVM.h"
#include "llbuild/BuildSystem/BuildSystem.h"

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"

#include <atomic>
#include <string>
#include <vector>

namespace llvm {

class SourceMgr;

}

namespace llbuild {
namespace basic {

class FileSystem;

}

namespace buildsystem {

class BuildSystemFrontendDelegate;
class BuildSystemInvocation;

/// This provides a standard "frontend" to the build system features, for use in
/// building bespoke build systems that can still take advantage of desirable
/// shared behavior.
///
/// The frontend glues together various parts of the build system functionality
/// to provide:
///   o Support for common command line options.
///   o Support for parallel, persistent builds.
//    o Support for command line diagnostics and status reporting.
class BuildSystemFrontend {
  BuildSystemFrontendDelegate& delegate;
  const BuildSystemInvocation& invocation;

public:
  BuildSystemFrontend(BuildSystemFrontendDelegate& delegate,
                      const BuildSystemInvocation& invocation);

  /// @name Accessors
  /// @{

  BuildSystemFrontendDelegate& getDelegate() { return delegate; }
  const BuildSystemFrontendDelegate& getDelegate() const { return delegate; }

  const BuildSystemInvocation& getInvocation() { return invocation; }
  
  /// @}

  /// @name Client API
  /// @{

  /// Build the named target using the specified invocation parameters.
  ///
  /// \returns True on success, or false if there were errors.
  bool build(StringRef targetToBuild);

  /// @}
};

/// The frontend-specific delegate, which provides some shared behaviors.
class BuildSystemFrontendDelegate : public BuildSystemDelegate {
  friend class BuildSystemFrontend;
  
public:
  /// Handle used to communicate information about a launched process.
  struct ProcessHandle {
    /// Opaque ID.
    uintptr_t id;
  };
  
private:
  void* impl;
  std::atomic<bool> isCancelled_;

  /// Default implementation, cannot be overriden by subclasses.
  virtual void setFileContentsBeingParsed(StringRef buffer) override;

  /// Provides a default error implementation which will delegate to the
  /// provided source manager. Cannot be overriden by subclasses.
  virtual void error(StringRef filename, const Token& at,
                     const Twine& message) override;
  
public:
  /// Create a frontend delegate.
  ///
  /// \param sourceMgr The source manager to use for reporting diagnostics.
  /// \param invocation The invocation parameters.
  /// \param name The name of build system client.
  /// \param version The version of the build system client.
  BuildSystemFrontendDelegate(llvm::SourceMgr& sourceMgr,
                              const BuildSystemInvocation& invocation,
                              StringRef name,
                              uint32_t version);
  virtual ~BuildSystemFrontendDelegate();

  /// Get the file system to use for access.
  virtual basic::FileSystem& getFileSystem() override = 0;

  /// Called by the build system to get a tool definition, must be provided by
  /// subclasses.
  virtual std::unique_ptr<Tool> lookupTool(StringRef name) override = 0;

  /// Provides an appropriate execution queue based on the invocation options.
  virtual std::unique_ptr<BuildExecutionQueue> createExecutionQueue() override;

  /// Provides a default cancellation implementation that will cancel when any
  /// command has failed.
  virtual bool isCancelled() override;

  /// Cancels the current build.
  virtual void cancel();

  /// Reset mutable build state before a new build operation.
  void resetForBuild();
  
  /// Provides a default handler.
  ///
  /// Subclass should call this method if overridden.
  virtual void hadCommandFailure() override;

  /// @name Frontend-specific APIs
  /// @{

  /// Report a non-file specific error message.
  void error(const Twine& message);
  
  /// @}

  /// @name Status Reporting APIs
  ///
  /// The frontend provides default implementations of these methods which
  /// report the status to stdout. Clients should override if they wish to
  /// direct the status elsewhere.
  ///
  /// @{

  /// Called by the build system to report that a declared command's state is
  /// changing.
  virtual void commandStatusChanged(Command*, CommandStatusKind) override;

  /// Called by the build system to report that a declared command is preparing
  /// to run.
  ///
  /// The system guarantees that all such calls will be paired with a
  /// corresponding \see commandFinished() call.
  virtual void commandPreparing(Command*) override;

  /// Called by the build system to allow the delegate to skip a command without
  /// implicitly skipping its dependents.
  ///
  /// WARNING: Clients need to take special care when using this. Skipping
  /// commands without considering their dependencies or dependents can easily
  /// produce an inconsistent build.
  ///
  /// This method is called before the command starts, when the system has
  /// identified that it will eventually need to run (after all of its inputs
  /// have been satisfied).
  ///
  /// The system guarantees that all such calls will be paired with a
  /// corresponding \see commandFinished() call.
  virtual bool shouldCommandStart(Command*) override;

  /// Called by the build system to report that a declared command has started.
  ///
  /// The system guarantees that all such calls will be paired with a
  /// corresponding \see commandFinished() call.
  virtual void commandStarted(Command*) override;

  /// Called by the build system to report a command has completed.
  virtual void commandFinished(Command*) override;

  /// Called when a command's job has been started.
  ///
  /// The system guarantees that any commandStart() call will be paired with
  /// exactly one \see commandFinished() call.
  //
  // FIXME: We may eventually want to allow the individual job to provide some
  // additional context here, for complex commands.
  //
  // FIXME: Design a way to communicate the "lane" here, for use in "super
  // console" like UIs.
  virtual void commandJobStarted(Command*);

  /// Called when a command's job has been finished.
  virtual void commandJobFinished(Command*);

  /// Called when a command's job has started executing an external process.
  ///
  /// The system guarantees that any commandProcessStarted() call will be paired
  /// with exactly one \see commandProcessFinished() call.
  ///
  /// \param handle - A unique handle used in subsequent delegate calls to
  /// identify the process. This handle should only be used to associate
  /// different status calls relating to the same process. It is only guaranteed
  /// to be unique from when it has been provided here to when it has been
  /// provided to the \see commandProcessFinished() call.
  virtual void commandProcessStarted(Command*, ProcessHandle handle);

  /// Called to report an error in the management of a command process.
  ///
  /// \param handle - The process handle.
  /// \param message - The error message.
  //
  // FIXME: Need to move to more structured error handling.
  virtual void commandProcessHadError(Command*, ProcessHandle handle,
                                       const Twine& message);

  /// Called to report a command processes' (merged) standard output and error.
  ///
  /// \param handle - The process handle.
  /// \param data - The process output.
  virtual void commandProcessHadOutput(Command*, ProcessHandle handle,
                                       StringRef data);
  
  /// Called when a command's job has finished executing an external process.
  ///
  /// \param handle - The handle used to identify the process. This handle will
  /// become invalid as soon as the client returns from this API call.
  ///
  /// \param exitStatus - The exit status of the process.
  //
  // FIXME: Need to include additional information on the status here, e.g., the
  // signal status, and the process output (if buffering).
  virtual void commandProcessFinished(Command*, ProcessHandle handle,
                                      int exitStatus);

  /// @}
  
  /// @name Accessors
  /// @{

  BuildSystemFrontend& getFrontend();

  llvm::SourceMgr& getSourceMgr();

  /// Get the number of reported errors.
  unsigned getNumErrors();

  /// Get the number of failed commands.
  unsigned getNumFailedCommands();

  /// @}
};


/// This class wraps the common options which are used by the frontend.
class BuildSystemInvocation {
public:
  /// Whether the command usage should be printed.
  bool showUsage = false;

  /// Whether the command version should be printed.
  bool showVersion = false;

  /// Whether to show verbose output.
  bool showVerboseStatus = false;

  /// Whether to use a serial build.
  bool useSerialBuild = false;
  
  /// The path of the database file to use, if any.
  std::string dbPath = "build.db";

  /// The path of a directory to change into before anything else, if any.
  std::string chdirPath = "";

  /// The path of the build file to use.
  std::string buildFilePath = "build.llbuild";

  /// The path of the build trace output file to use, if any.
  std::string traceFilePath = "";

  /// The positional arguments.
  std::vector<std::string> positionalArgs;

  /// Whether there were any parsing errors.
  bool hadErrors = false;
  
public:
  /// Get the appropriate "usage" text to use for the built in arguments.
  static void getUsage(int optionWidth, raw_ostream& os);
  
  /// Parse the invocation parameters from the given arguments.
  ///
  /// \param sourceMgr The source manager to use for diagnostics.
  void parse(ArrayRef<std::string> args, llvm::SourceMgr& sourceMgr);
};

}
}

#endif
