/*  Sirikata
 *  Breakpad.cpp
 *
 *  Copyright (c) 2011, Ewen Cheslack-Postava
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of Sirikata nor the names of its contributors may
 *    be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sirikata/core/util/Standard.hh>
#include <sirikata/core/service/Breakpad.hpp>

#include <sirikata/core/util/Platform.hpp>

#ifdef HAVE_BREAKPAD
#if SIRIKATA_PLATFORM == PLATFORM_WINDOWS
#include <client/windows/handler/exception_handler.h>
#elif SIRIKATA_PLATFORM == PLATFORM_LINUX
#include <client/linux/handler/exception_handler.h>
#endif
#endif // HAVE_BREAKPAD

namespace Sirikata {
namespace Breakpad {

#ifdef HAVE_BREAKPAD

// Each implementation of ExceptionHandler and the setup are different enough
// that these are worth just completely separating. Each just needs to setup the
// exception handler. Currently, all should set it up to save minidumps to the
// current directory.

#if SIRIKATA_PLATFORM  == PLATFORM_WINDOWS
namespace {

static google_breakpad::ExceptionHandler* breakpad_handler = NULL;

bool finishedDump(const wchar_t* dump_path,
    const wchar_t* minidump_id,
    void* context,
    EXCEPTION_POINTERS* exinfo,
    MDRawAssertionInfo* assertion,
    bool succeeded) {
    printf("Finished breakpad dump at %s/%s.dmp: success %d\n", dump_path, minidump_id, succeeded ? 1 : -1);

    return succeeded;
}
}

void init() {
    if (breakpad_handler != NULL) return;

    // This is needed for CRT to not show dialog for invalid param
    // failures and instead let the code handle it.
    _CrtSetReportMode(_CRT_ASSERT, 0);

    using namespace google_breakpad;
    breakpad_handler = new ExceptionHandler(L".\\",
        NULL,
        finishedDump,
        NULL,
        ExceptionHandler::HANDLER_ALL,
        MiniDumpNormal,
        NULL,
        NULL);
}

#elif SIRIKATA_PLATFORM == PLATFORM_LINUX

namespace {

static google_breakpad::ExceptionHandler* breakpad_handler = NULL;

bool finishedDump(const char* dump_path,
    const char* minidump_id,
    void* context,
    bool succeeded) {
    printf("Finished breakpad dump at %s/%s.dmp: success %d\n", dump_path, minidump_id, succeeded ? 1 : -1);

    return succeeded;
}
}

void init() {
    if (breakpad_handler != NULL) return;

    using namespace google_breakpad;
    breakpad_handler = new ExceptionHandler("./", NULL, finishedDump, NULL, true);
}

#elif SIRIKATA_PLATFORM == PLATFORM_MAC
// No mac support currently
void init() {
}

#endif

#else //def HAVE_BREAKPAD
// Dummy implementation
void init() {
}
#endif

} // namespace Breakpad
} // namespace Sirikata
