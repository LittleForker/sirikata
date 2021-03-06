/*  Sirikata
 *  ComputeBoundsFilter.cpp
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

#include "ComputeBoundsFilter.hpp"

namespace Sirikata {
namespace Mesh {

ComputeBoundsFilter::ComputeBoundsFilter(const String& args) {
}

FilterDataPtr ComputeBoundsFilter::apply(FilterDataPtr input) {
    // The bounding box is just the bounding box of all the component geometry
    // instances.  These were already computed, post-transform, for each
    // instance, so this is a simple computation.

    BoundingBox3f3f bbox = BoundingBox3f3f::null();
    for(FilterData::const_iterator mesh_it = input->begin(); mesh_it != input->end(); mesh_it++) {
        MeshdataPtr mesh = *mesh_it;

        Meshdata::GeometryInstanceIterator geoinst_it = mesh->getGeometryInstanceIterator();
        uint32 geoinst_idx;
        Matrix4x4f pos_xform;
        while( geoinst_it.next(&geoinst_idx, &pos_xform) ) {
            BoundingBox3f3f inst_bnds = mesh->instances[geoinst_idx].computeTransformedBounds(mesh, pos_xform);
            if (bbox == BoundingBox3f3f::null())
                bbox = inst_bnds;
            else
                bbox.mergeIn(inst_bnds);
        }
    }
    std::cout << bbox << std::endl;
    return input;
}

} // namespace Mesh
} // namespace Sirikata
