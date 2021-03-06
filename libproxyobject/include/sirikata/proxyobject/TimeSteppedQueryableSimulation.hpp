/*  Sirikata Utilities -- Sirikata Listener Pattern
 *  TimeSteppedQueryableSimulation.hpp
 *
 *  Copyright (c) 2009, Daniel Reiter Horn
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

#ifndef _SIRIKATA_TIME_STEPPED_QUERYABLE_SIMULATION_HPP_
#define _SIRIKATA_TIME_STEPPED_QUERYABLE_SIMULATION_HPP_
#include "TimeSteppedSimulation.hpp"
#include <sirikata/proxyobject/ProxyObject.hpp>


namespace Sirikata {
typedef std::tr1::shared_ptr<ProxyObject> ProxyObjectPtr;
class SpaceObjectReference;


class TimeSteppedQueryableSimulation: public TimeSteppedSimulation {
public:
    TimeSteppedQueryableSimulation(Context* ctx, const Duration& rate, const String& name)
     : TimeSteppedSimulation(ctx, rate, name)
    {
    }

    /**
     * Query the scene to look for the first active simulation object that intersects the ray
     * @param position the starting point for the ray query
     * @param direction the normalized direction which the ray continues at
     * @param maxDistance maximum distance to consider hits
     * @param ignore an object to ignore hits from
     * @param returnDistance is the length down the ray which hits the object
     * @param returnNormal is the normal of the surface which the ray pierces
     * @param returnName is the space object reference of the object that is pierced
     * @returns whether the ray hit anything
     * @note if the ray misses all objects the boolean returns false and all values are unchanged
     */
    virtual bool queryRay(const Vector3d& position,
                          const Vector3f& direction,
                          const double maxDistance,
                          ProxyObjectPtr ignore,
                          double &returnDistance,
                          Vector3f &returnNormal,
                          SpaceObjectReference &returnName)=0;
    virtual Duration desiredTickRate()const=0;
    ///returns true if simulation should continue (false quits app)
    virtual void poll()=0;
    
    /* Do nothing for invoke */
    virtual boost::any invoke(std::vector<boost::any>& params){ return NULL; }
};

}

#endif
