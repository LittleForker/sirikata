/*  Sirikata
 *  SpaceContext.cpp
 *
 *  Copyright (c) 2009, Ewen Cheslack-Postava
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

#include <sirikata/space/SpaceContext.hpp>
#include <sirikata/core/network/IOStrandImpl.hpp>

#include <sirikata/core/network/SSTImpl.hpp>

namespace Sirikata {

SpaceContext::SpaceContext(ServerID _id, Network::IOService* ios, Network::IOStrand* strand, const Time& epoch, Trace::Trace* _trace, const Duration& duration)
 : Context("Space", ios, strand, _trace, epoch, duration),
   mID(_id),
   mServerRouter(NULL),
   mServerDispatcher(NULL),
   mSpaceTrace( new SpaceTrace(_trace) )
{
}

SpaceContext::~SpaceContext() {
    mObjectStreams.clear();
}

void SpaceContext::newStream(int err, SSTStreamPtr s) {
    typedef Connection<SpaceObjectReference> SSTConnection;
    typedef SSTConnection::Ptr SSTConnectionPtr;

    SSTConnectionPtr conn = s->connection().lock();
    assert(conn);
    SpaceObjectReference sourceObject = conn->remoteEndPoint().endPoint;

    if (mObjectStreams.find(sourceObject.object()) != mObjectStreams.end()) {
        std::cout << "A stream already exists from source object " << sourceObject.toString() << "\n";fflush(stdout);

        SSTConnectionPtr sstConnection = mObjectStreams[sourceObject.object()]->connection().lock();
        assert(sstConnection);
        sstConnection->close(false);
    }

    mObjectStreams[sourceObject.object()] = s;
}

} // namespace Sirikata
