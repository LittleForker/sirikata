/*  Sirikata
 *  DelegatePort.cpp
 *
 *  Copyright (c) 2010, Ewen Cheslack-Postava
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
#include <sirikata/core/odp/DelegateService.hpp>
#include <sirikata/core/odp/DelegatePort.hpp>

namespace Sirikata {
namespace ODP {

DelegatePort::DelegatePort(DelegateService* parent, const Endpoint& ep, SendFunction send_func)
 : mParent(parent),
   mEndpoint(ep),
   mSendFunc(send_func),
   mFromHandlers(),
   mInvalidated(false)
{
}

DelegatePort::~DelegatePort() {
    // Get it out of the parent map to ensure we won't get any more messages
    if (!mInvalidated)
        mParent->deallocatePort(this);
}

void DelegatePort::invalidate() {
    // Marke and remove from parent
    mInvalidated = true;
    mParent->deallocatePort(this);
}

const Endpoint& DelegatePort::endpoint() const {
    return mEndpoint;
}

bool DelegatePort::send(const Endpoint& to, MemoryReference payload) {
    if (mInvalidated) return false;
    return mSendFunc(to, payload);
}

void DelegatePort::receiveFrom(const Endpoint& from, const MessageHandler& cb) {
    mFromHandlers[from] = cb;
}


bool DelegatePort::deliver(const ODP::Endpoint& src, const ODP::Endpoint& dst, MemoryReference data) const {
    if (mInvalidated) return false;

    // See ODP::Port documentation for details on this ordering.
    if (tryDeliver(src, src, dst, data)) return true;
    if (tryDeliver(Endpoint(src.space(), ObjectReference::any(), src.port()), src, dst, data)) return true;
    if (tryDeliver(Endpoint(SpaceID::any(), ObjectReference::any(), src.port()), src, dst, data)) return true;
    if (tryDeliver(Endpoint(src.space(), src.object(), PortID::any()), src, dst, data)) return true;
    if (tryDeliver(Endpoint(src.space(), ObjectReference::any(), PortID::any()), src, dst, data)) return true;
    if (tryDeliver(Endpoint(SpaceID::any(), ObjectReference::any(), PortID::any()), src, dst, data)) return true;

    return false;
}


bool DelegatePort::tryDeliver(const Endpoint& src_match_ep, const Endpoint& src_real_ep, const Endpoint& dst, MemoryReference data) const {
    ReceiveFromHandlers::const_iterator rit = mFromHandlers.find(src_match_ep);

    if (rit == mFromHandlers.end())
        return false;

    const MessageHandler& handler = rit->second;

    handler(src_real_ep, dst, data);

    return true;
}

} // namespace ODP
} // namespace Sirikata
