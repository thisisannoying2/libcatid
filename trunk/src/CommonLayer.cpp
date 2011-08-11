/*
	Copyright (c) 2009-2011 Christopher A. Taylor.  All rights reserved.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:

	* Redistributions of source code must retain the above copyright notice,
	  this list of conditions and the following disclaimer.
	* Redistributions in binary form must reproduce the above copyright notice,
	  this list of conditions and the following disclaimer in the documentation
	  and/or other materials provided with the distribution.
	* Neither the name of LibCat nor the names of its contributors may be used
	  to endorse or promote products derived from this software without
	  specific prior written permission.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
	ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
	LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
	CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
	SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
	INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
	CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
	ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
	POSSIBILITY OF SUCH DAMAGE.
*/

#include <cat/CommonLayer.hpp>
#include <cat/port/SystemInfo.hpp>
#include <cat/time/Clock.hpp>
#include <cat/io/Logging.hpp>
#include <cat/io/Settings.hpp>
#include <cat/threads/RefObjects.hpp>
using namespace cat;


//// CommonLayer Singleton

static CommonLayer common_layer;

CommonLayer *CommonLayer::ref()
{
	return &common_layer;
}


//// CommonLayer

bool CommonLayer::PreWorkerThreads()
{
	return true;
}

bool CommonLayer::OnStartup(IWorkerTLSBuilder *tls_builder, const char *settings_file_name, bool service, const char *service_name)
{
	// Initialize system info
	InitializeSystemInfo();

	// Initialize clock subsystem
	if (!Clock::Initialize())
	{
		FatalStop("Clock subsystem failed to initialize");
	}

	// Initialize logging subsystem with INFO reporting level
	Logging::ref()->Initialize(LVL_INFO);
	if (service) Logging::ref()->EnableServiceMode(service_name);

	// Initialize ref objects subsystem
	if (!RefObjects::ref()->Initialize())
	{
		CAT_FATAL("CommonLayer") << "RefObjects subsystem failed to initialize";
		return false;
	}

	// Initialize disk settings subsystem
	Settings::ref()->readSettingsFromFile(settings_file_name);

	// Read logging subsystem settings
	Logging::ref()->ReadSettings();

	if (!PreWorkerThreads()) return false;

	u32 worker_count_override = Settings::ref()->getInt("WorkerThreads.Count", 0);
	u32 worker_tick_interval = Settings::ref()->getInt("WorkerThreads.TickInterval", 10);

	// Start the Worker threads if requested to by the caller
	if (tls_builder && !WorkerThreads::ref()->Startup(worker_tick_interval, tls_builder, worker_count_override))
	{
		CAT_FATAL("CommonLayer") << "WorkerThreads subsystem failed to initialize";
		return false;
	}

	return true;
}

void CommonLayer::OnShutdown(bool watched_shutdown)
{
	if (!watched_shutdown)
	{
		CAT_WARN("CommonLayer") << "Wait for shutdown expired";
	}

	// Terminate Worker threads
	WorkerThreads::ref()->Shutdown();

	// Shutdown reference objects
	RefObjects::ref()->Shutdown();

	// Write settings to disk
	Settings::ref()->write();

	// Cleanup clock subsystem
	Clock::Shutdown();

	CAT_DEBUG_LEAKS_DUMP();
}