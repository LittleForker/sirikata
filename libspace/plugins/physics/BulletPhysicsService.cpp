/*  Sirikata
 *  BulletPhysicsService.cpp
 *
 *  Copyright (c) 2011, Ewen Cheslack-Postava, Rahul Sheth
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

#include "BulletPhysicsService.hpp"
#include <sirikata/core/trace/Trace.hpp>

#include "Protocol_Loc.pbj.hpp"

namespace Sirikata {
#ifdef _WIN32
	//FIXME: is this the right thing? DRH
typedef float float_t;
#endif

BulletPhysicsService::BulletPhysicsService(SpaceContext* ctx, LocationUpdatePolicy* update_policy)
 : LocationService(ctx, update_policy)
{
	broadphase = new btDbvtBroadphase();

	collisionConfiguration = new btDefaultCollisionConfiguration();
	dispatcher = new btCollisionDispatcher(collisionConfiguration);

	solver = new btSequentialImpulseConstraintSolver;

	dynamicsWorld = new btDiscreteDynamicsWorld(dispatcher, broadphase, solver, collisionConfiguration);

	dynamicsWorld->setGravity(btVector3(0,-1,0));

	//this stuff is from the beginner program, for a ground plane
	/*btCollisionShape* groundShape = new btStaticPlaneShape(btVector3(0,1,0),1);
	//btCollisionShape* fallShape = new btSphereShape(1);

	btDefaultMotionState* groundMotionState = new btDefaultMotionState(btTransform(btQuaternion(0,0,0,1), btVector3(0,-50,0)));

	btRigidBody::btRigidBodyConstructionInfo groundRigidBodyCI(0, groundMotionState, groundShape, btVector3(0,0,0));
	btRigidBody* groundRigidBody = new btRigidBody(groundRigidBodyCI);

	dynamicsWorld->addRigidBody(groundRigidBody);*/

	mTimer.start();

	mModelsSystem = ModelsSystemFactory::getSingleton().getConstructor("any")("");

    mTransferMediator = &(Transfer::TransferMediator::getSingleton());

    static char x = '1';
    mTransferPool = mTransferMediator->registerClient("BulletPhysics_"+x);
    x++;

	firstCube = true;

	//change this if you need more debugging output
	printDebugInfo = false;

	printf("[BulletPhysicsService] Service Loaded\n");
}

BulletPhysicsService::~BulletPhysicsService(){
	delete dynamicsWorld;
	delete solver;
	delete dispatcher;
	delete collisionConfiguration;
	delete broadphase;
	delete mModelsSystem;

	printf("[BulletPhysicsService] Service Unloaded\n");
}

bool BulletPhysicsService::contains(const UUID& uuid) const {
    return (mLocations.find(uuid) != mLocations.end());
}

LocationService::TrackingType BulletPhysicsService::type(const UUID& uuid) const {
    LocationMap::const_iterator it = mLocations.find(uuid);
    if (it == mLocations.end())
        return NotTracking;
    if (it->second.aggregate)
        return Aggregate;
    if (it->second.local)
        return Local;
    return Replica;
}


void BulletPhysicsService::service() {
	//get the time elapsed between calls to this service and
	//move the simulation forward by that amount
	Duration delTime = mTimer.elapsed();
	float simForwardTime = delTime.toMilliseconds() / 1000.0f;

    dynamicsWorld->stepSimulation(simForwardTime, 10);
	mTimer.start();

    for(unsigned int i = 0; i < physicsUpdates.size(); i++) {
		LocationMap::iterator it = mLocations.find(physicsUpdates[i]);
		if(it != mLocations.end()) {
			notifyLocalLocationUpdated( physicsUpdates[i], it->second.aggregate, it->second.location );
		}
	}
	physicsUpdates.empty();

    mUpdatePolicy->service();
}

TimedMotionVector3f BulletPhysicsService::location(const UUID& uuid) {
    LocationMap::iterator it = mLocations.find(uuid);
    assert(it != mLocations.end());


    LocationInfo locinfo = it->second;
    return locinfo.location;
}

Vector3f BulletPhysicsService::currentPosition(const UUID& uuid) {
    TimedMotionVector3f loc = location(uuid);
    Vector3f returner = loc.extrapolate(mContext->simTime()).position();
    return returner;
}

TimedMotionQuaternion BulletPhysicsService::orientation(const UUID& uuid) {
    LocationMap::iterator it = mLocations.find(uuid);
    assert(it != mLocations.end());

    LocationInfo locinfo = it->second;
    return locinfo.orientation;
}

Quaternion BulletPhysicsService::currentOrientation(const UUID& uuid) {
    TimedMotionQuaternion orient = orientation(uuid);
    return orient.extrapolate(mContext->simTime()).position();
}

BoundingSphere3f BulletPhysicsService::bounds(const UUID& uuid) {
    LocationMap::iterator it = mLocations.find(uuid);
    assert(it != mLocations.end());

    LocationInfo locinfo = it->second;
    return locinfo.bounds;
}

const String& BulletPhysicsService::mesh(const UUID& uuid) {
    LocationMap::iterator it = mLocations.find(uuid);
    assert(it != mLocations.end());

    const LocationInfo& locinfo = it->second;
    return locinfo.mesh;
}

void BulletPhysicsService::getMesh(const std::string meshURI, const UUID uuid) {

	meshLoaded = false;
	getMetadata(meshURI, uuid);
	//FIXME: Busy waiting is bad.
	//Waiting for meshes to be retrieved from CDN
	while(!meshLoaded) {};

    std::string t = retrievedMesh->uri;
	printf("[BulletPhysicsService] This mesh was retrieved from the CDN: %s\n", &t[0]);
}

void BulletPhysicsService::getMetadata(const std::string meshURI, const UUID uuid) {
	Transfer::TransferRequestPtr req(
                                       new Transfer::MetadataRequest( Transfer::URI(meshURI), 1.0, std::tr1::bind(
                                       &BulletPhysicsService::metadataFinished, this, uuid, meshURI,
                                       std::tr1::placeholders::_1, std::tr1::placeholders::_2)));

    mTransferPool->addRequest(req);
}

void BulletPhysicsService::metadataFinished(const UUID uuid, std::string meshName,
                                          std::tr1::shared_ptr<Transfer::MetadataRequest> request,
                                          std::tr1::shared_ptr<Transfer::RemoteFileMetadata> response) {
	if (response != NULL) {
    const Transfer::RemoteFileMetadata metadata = *response;

    Transfer::TransferRequestPtr req(new Transfer::ChunkRequest(response->getURI(), metadata,
                                               response->getChunkList().front(), 1.0,
                                             std::tr1::bind(&BulletPhysicsService::chunkFinished, this, uuid, meshName,
                                                              std::tr1::placeholders::_1,
                                                              std::tr1::placeholders::_2) ) );

    mTransferPool->addRequest(req);
  }
  else {
    Transfer::TransferRequestPtr req(new Transfer::MetadataRequest( Transfer::URI(meshName), 1.0, std::tr1::bind(
                                       &BulletPhysicsService::metadataFinished, this, uuid, meshName,
                                       std::tr1::placeholders::_1, std::tr1::placeholders::_2)));

    mTransferPool->addRequest(req);
  }
}

void BulletPhysicsService::chunkFinished(const UUID uuid, std::string meshName,
                                       std::tr1::shared_ptr<Transfer::ChunkRequest> request,
                                       std::tr1::shared_ptr<const Transfer::DenseData> response)
{
	if (response != NULL) {
        retrievedMesh = mModelsSystem->load(request->getURI(), request->getMetadata().getFingerprint(), response);
        meshLoaded = true;
    }
    else {
     Transfer::TransferRequestPtr req(
                                       new Transfer::MetadataRequest( Transfer::URI(meshName), 1.0, std::tr1::bind(
                                       &BulletPhysicsService::metadataFinished, this, uuid, meshName,
                                       std::tr1::placeholders::_1, std::tr1::placeholders::_2)));

     mTransferPool->addRequest(req);
	}
}
void BulletPhysicsService::addLocalObject(const UUID& uuid, const TimedMotionVector3f& loc, const TimedMotionQuaternion& orient, const BoundingSphere3f& bnds, const String& msh) {
    LocationMap::iterator it = mLocations.find(uuid);

    // Add or update the information to the cache
    if (it == mLocations.end()) {
        mLocations[uuid] = LocationInfo();
        it = mLocations.find(uuid);
    } else {
        // It was already in there as a replica, notify its removal
        assert(it->second.local == false);
        CONTEXT_SPACETRACE(serverObjectEvent, 0, mContext->id(), uuid, false, TimedMotionVector3f()); // FIXME remote server ID
        notifyReplicaObjectRemoved(uuid);
    }

    LocationInfo& locinfo = it->second;
    locinfo.location = loc;
    locinfo.orientation = orient;
    locinfo.bounds = bnds;
    locinfo.mesh = msh;
    locinfo.local = true;
    locinfo.aggregate = false;

    // FIXME: we might want to verify that location(uuid) and bounds(uuid) are
    // reasonable compared to the loc and bounds passed in

    // Add to the list of local objects
    CONTEXT_SPACETRACE(serverObjectEvent, mContext->id(), mContext->id(), uuid, true, loc);
    notifyLocalObjectAdded(uuid, false, location(uuid), orientation(uuid), bounds(uuid), mesh(uuid));

    /*BEGIN: Bullet Physics Code*/

    //We need to store the pointers to the bullet objects we create to delete them later
    PhysicsPointerMap::iterator it2 = BulletPhysicsPointers.find(uuid);
	if(it2 == BulletPhysicsPointers.end()) {
		BulletPhysicsPointers[uuid] = BulletPhysicsPointerData();
		it2 = BulletPhysicsPointers.find(uuid);
	}
    BulletPhysicsPointerData& newObjData = it2->second;

    //FIXME: Physics details need to be passed to us and/or read from CDN, not set manually!

    //setting mass to 1 for no good reason
    btScalar mass = 1;

    bulletObjTreatment& objTreatment = newObjData.objTreatment;
    bulletObjBBox& objBBox = newObjData.objBBox;

    //FIXME
    //BEGIN list of string comparisons to set appropriate flags
    if(strcmp(&msh[0], "meerkat:///ewencp/lights.dae") == 0) {
		objTreatment = BULLET_OBJECT_TREATMENT_IGNORE;
	}
	if(strcmp(&msh[0], "meerkat:///danielrh/Poisonbird.dae") == 0) {
		objTreatment = BULLET_OBJECT_TREATMENT_IGNORE;
	}
	//FIXME this doesn't seem to work, need to ignore "stringless" meshes
	if(strcmp(&msh[0], "") == 0) {
		objTreatment = BULLET_OBJECT_TREATMENT_IGNORE;
	}
	if(strcmp(&msh[0], "meerkat:///danielrh/ground.DAE") == 0) {
		objTreatment = BULLET_OBJECT_TREATMENT_STATIC;
		objBBox = BULLET_OBJECT_BOUNDS_PER_TRIANGLE;
	}
	if(strcmp(&msh[0], "meerkat:///rbsheth/Plane.dae") == 0) {
		objTreatment = BULLET_OBJECT_TREATMENT_STATIC;
		objBBox = BULLET_OBJECT_BOUNDS_PER_TRIANGLE;
	}
	if(strcmp(&msh[0], "meerkat:///rbsheth/UnitCube2.dae") == 0) {
		//firstCube is for having different behavior for different instances of the same mesh
		if(firstCube) {
			objTreatment = BULLET_OBJECT_TREATMENT_STATIC;
			objBBox = BULLET_OBJECT_BOUNDS_ENTIRE_OBJECT;
			firstCube = false;
		}
		else {
			objTreatment = BULLET_OBJECT_TREATMENT_DYNAMIC;
			objBBox = BULLET_OBJECT_BOUNDS_ENTIRE_OBJECT;
		}
	}
	if(strcmp(&msh[0], "meerkat:///rbsheth/UnitCube.dae") == 0) {
		//firstCube is for having different behavior for different instances of the same mesh
		if(firstCube) {
			objTreatment = BULLET_OBJECT_TREATMENT_STATIC;
			objBBox = BULLET_OBJECT_BOUNDS_PER_TRIANGLE;
			firstCube = false;
		}
		else {
			objTreatment = BULLET_OBJECT_TREATMENT_DYNAMIC;
			objBBox = BULLET_OBJECT_BOUNDS_ENTIRE_OBJECT;
		}
	}
    if(strcmp(&msh[0], "meerkat://danielrh/roof_zinc.DAE") == 0) {
		objTreatment = BULLET_OBJECT_TREATMENT_DYNAMIC;
		objBBox = BULLET_OBJECT_BOUNDS_PER_TRIANGLE;
	}
	if(strcmp(&msh[0], "meerkat:///rbsheth/Sphere.dae") == 0) {
		objTreatment = BULLET_OBJECT_TREATMENT_DYNAMIC;
		objBBox = BULLET_OBJECT_BOUNDS_SPHERE;
	}
	if(strcmp(&msh[0], "meerkat:///rbsheth/UnitSphere.dae") == 0) {
		objTreatment = BULLET_OBJECT_TREATMENT_DYNAMIC;
		objBBox = BULLET_OBJECT_BOUNDS_SPHERE;
	}
	if(strcmp(&msh[0], "meerkat:///danielrh/Insect-Bird.dae") == 0) {
		objTreatment = BULLET_OBJECT_TREATMENT_DYNAMIC;
		objBBox = BULLET_OBJECT_BOUNDS_SPHERE;
	}
	if(strcmp(&msh[0], "meerkat:///danielrh/Seagull.dae") == 0) {
		objTreatment = BULLET_OBJECT_TREATMENT_DYNAMIC;
		objBBox = BULLET_OBJECT_BOUNDS_SPHERE;
	}
    //END list of string comparisons to set appropriate flags

    //objTreatment enum defined in header file
    //using if/elseif here to avoid switch/case compiler complaints (initializing variables in a case)
    if(objTreatment == BULLET_OBJECT_TREATMENT_IGNORE) {
		printf("[BulletPhysicsService] This mesh will not be added to the bullet world: %s\n", &msh[0]);
		return;
	}
	else if(objTreatment == BULLET_OBJECT_TREATMENT_STATIC) {
		printf("[BulletPhysicsService] This mesh will not move: %s\n", &msh[0]);
		//this is a variable in loc structure that sets the item to be static
		locinfo.isFixed = true;
		//if the mass is 0, Bullet treats the object as static
		mass = 0;
	}
	else if(objTreatment == BULLET_OBJECT_TREATMENT_DYNAMIC) {
		printf("[BulletPhysicsService] This mesh will move: %s\n", &msh[0]);
		locinfo.isFixed = false;
		mass = 1;
	}
	else {
		printf("[BulletPhysicsService] Error in objTreatment initialization!\n");
	}

	//initialize the world transformation
	Matrix4x4f worldTransformation = Matrix4x4f::identity();
    //get the position and orientation
    Vector3f objPosition = loc.position();
    Quaternion objOrient = orient.position();

    /***Let's now find the bounding box for the entire object, which is needed for re-scaling purposes.
	* Supposedly the system scales every mesh down to a unit sphere and then scales up by the scale factor
	* from the scene file. We try to emulate this behavior here, but this should really be on the CDN side
	* (we retrieve the precomputed bounding box as well as the mesh) ***/
    BoundingBox3f3f bbox = BoundingBox3f3f::null();
	double mesh_rad = 0;
	//getting the mesh
	getMesh(msh, uuid);
	Meshdata::GeometryInstanceIterator geoIter = retrievedMesh->getGeometryInstanceIterator();
	uint32 indexInstance; Matrix4x4f transformInstance;
	//loop through the instances, expand the bounding box and find the radius
	while(geoIter.next(&indexInstance, &transformInstance)) {
		GeometryInstance* geoInst = &(retrievedMesh->instances[indexInstance]);
		BoundingBox3f3f inst_bnds;
		double rad = 0;
		geoInst->computeTransformedBounds(retrievedMesh, transformInstance, &inst_bnds, &rad);
		if (bbox == BoundingBox3f3f::null())
			bbox = inst_bnds;
		else
			bbox.mergeIn(inst_bnds);
		mesh_rad = std::max(mesh_rad, rad);
	}
	if(printDebugInfo) {
		std::cout << "bbox: " << bbox << std::endl;
	}
	Vector3f diff = bbox.max() - bbox.min();

    //objBBox enum defined in header file
    //using if/elseif here to avoid switch/case compiler complaints (initializing variables in a case)
    if(objBBox == BULLET_OBJECT_BOUNDS_ENTIRE_OBJECT) {
		double scalingFactor = bnds.radius()/mesh_rad;
		if(printDebugInfo) {
			printf("objposition: %f, %f, %f\n", objPosition.x, objPosition.y, objPosition.z);
			printf("bbox half extents: %f, %f, %f\n", fabs(diff.x/2)*scalingFactor, fabs(diff.y/2)*scalingFactor, fabs(diff.z/2)*scalingFactor);
		}
		newObjData.objShape = new btBoxShape(btVector3(fabs((diff.x/2)*scalingFactor), fabs((diff.y/2)*scalingFactor), fabs((diff.z/2)*scalingFactor)));
	}
	//do NOT attempt to collide two btBvhTriangleMeshShapes, it will not work
	else if(objBBox == BULLET_OBJECT_BOUNDS_PER_TRIANGLE) {
		//we found the bounding box and radius, so let's scale the mesh down by the radius and up by the scaling factor from the scene file (bnds.radius())
		worldTransformation = worldTransformation * Matrix4x4f::scale((float) bnds.radius()/mesh_rad);
		//reset the instance iterator for a second round
		geoIter = retrievedMesh->getGeometryInstanceIterator();
		//we need to pass the triangles to Bullet
		btTriangleMesh * meshToConstruct = new btTriangleMesh(false, false);
		//loop through the instances again, applying the new transformations to vertices and adding them to the Bullet mesh
		while(geoIter.next(&indexInstance, &transformInstance)) {
			transformInstance = transformInstance * worldTransformation;
			GeometryInstance* geoInst = &(retrievedMesh->instances[indexInstance]);

			unsigned int geoIndx = geoInst->geometryIndex;
			SubMeshGeometry* subGeom = &(retrievedMesh->geometry[geoIndx]);
			unsigned int numOfPrimitives = subGeom->primitives.size();
			std::vector<int> gIndices;
			std::vector<Vector3f> gVertices;
			for(unsigned int i = 0; i < numOfPrimitives; i++) {
				//create bullet triangle array from our data structure
				Vector3f transformedVertex;
				if(printDebugInfo) {
					printf("subgeom indices: ");
				}
				for(unsigned int j=0; j < subGeom->primitives[i].indices.size(); j++) {
					gIndices.push_back((int)(subGeom->primitives[i].indices[j]));
					if(printDebugInfo) {
						printf("%d, ", (int)(subGeom->primitives[i].indices[j]));
					}
				}
				if(printDebugInfo) {
					printf("\ngIndices size: %d\n", (int) gIndices.size());
				}
				for(unsigned int j=0; j < subGeom->positions.size(); j++) {
					//printf("preTransform Vertex: %f, %f, %f\n", subGeom->positions[j].x, subGeom->positions[j].y, subGeom->positions[j].z);
					transformedVertex = transformInstance * subGeom->positions[j];
					//printf("Transformed Vertex: %f, %f, %f\n", transformedVertex.x, transformedVertex.y, transformedVertex.z);
					gVertices.push_back(transformedVertex);
				}
				//TODO: check memleak, check divisible by 3
				/*printf("btTriangleIndexVertexArray:\n");
				printf("argument 1: %d\n", (int) (gIndices.size())/3);
				printf("argument 3: %d\n", (int) 3*sizeof(int));
				printf("argument 4: %d\n", gVertices.size());
				printf("argument 6: %d\n", (int) sizeof(btVector3));
				btTriangleIndexVertexArray* indexVertexArrays = new btTriangleIndexVertexArray((int) gIndices.size()/3, &gIndices[0], (int) 3*sizeof(int), gVertices.size(), (btScalar *) &gVertices[0].x, (int) sizeof(btVector3));*/
			}
			for(unsigned int j=0; j < gIndices.size(); j+=3) {
				//printf("triangle %d: %d, %d, %d\n", j/3, j, j+1, j+2);
				//printf("triangle %d:\n",  j/3);
				//printf("vertex 1: %f, %f, %f\n", gVertices[gIndices[j]].x, gVertices[gIndices[j]].y, gVertices[gIndices[j]].z);
				//printf("vertex 2: %f, %f, %f\n", gVertices[gIndices[j+1]].x, gVertices[gIndices[j+1]].y, gVertices[gIndices[j+1]].z);
				//printf("vertex 3: %f, %f, %f\n\n", gVertices[gIndices[j+2]].x, gVertices[gIndices[j+2]].y, gVertices[gIndices[j+2]].z);
				meshToConstruct->addTriangle(
                                    btVector3( gVertices[gIndices[j]].x, gVertices[gIndices[j]].y, gVertices[gIndices[j]].z ),
                                    btVector3( gVertices[gIndices[j+1]].x, gVertices[gIndices[j+1]].y, gVertices[gIndices[j+1]].z ),
                                    btVector3( gVertices[gIndices[j+2]].x, gVertices[gIndices[j+2]].y, gVertices[gIndices[j+2]].z )
                                );
			}
			Vector3f bMin = bbox.min();
			/*objPosition.x = objPosition.x + bMin.x;
			objPosition.y = objPosition.y + bMin.y;
			objPosition.z = objPosition.z - bMin.z;*/
		}
		if(printDebugInfo) {
			std::cout << "total bounds: " << bbox << std::endl;
			printf("bounds radius: %f\n", mesh_rad);
			printf("Num of triangles in mesh: %d\n", meshToConstruct->getNumTriangles());
		}
		//btVector3 aabbMin(-1000,-1000,-1000),aabbMax(1000,1000,1000);
		newObjData.objShape  = new btBvhTriangleMeshShape(meshToConstruct,true);
	}
	//FIXME bug somewhere else? bnds.radius()/mesh_rad should be the correct radius, but it is not...
	else if(objBBox == BULLET_OBJECT_BOUNDS_SPHERE) {

		newObjData.objShape = new btSphereShape(bnds.radius());
		if(printDebugInfo) {
			printf("objposition: %f, %f, %f\n", objPosition.x, objPosition.y, objPosition.z);
			printf("sphere radius: %f\n", bnds.radius());
		}
	}
	else {
		printf("[BulletPhysicsService] Error in objBBox initialization!\n");
	}
    //register the motion state (callbacks) for Bullet
    newObjData.objMotionState = new SirikataMotionState(btTransform(btQuaternion(objOrient.x,objOrient.y,objOrient.z,objOrient.w),btVector3(objPosition.x,objPosition.y,objPosition.z)), &(it->second), uuid, this);

    //set a placeholder for the inertial vector
	btVector3 objInertia(0,0,0);
	//calculate the inertia
	newObjData.objShape->calculateLocalInertia(mass,objInertia);
	//make a constructionInfo object
	btRigidBody::btRigidBodyConstructionInfo objRigidBodyCI(mass, newObjData.objMotionState, newObjData.objShape, objInertia);
    //CREATE: make the rigid body
    newObjData.objRigidBody = new btRigidBody(objRigidBodyCI);
    //newObjData.objRigidBody->setRestitution(0.5);
    //set initial velocity
    Vector3f objVelocity = loc.velocity();
    //newObjData.objRigidBody->setLinearVelocity(btVector3(objVelocity.x, objVelocity.y, objVelocity.z));
    //add to the dynamics world
    dynamicsWorld->addRigidBody(newObjData.objRigidBody);

    /*END: Bullet Physics Code*/
}

void BulletPhysicsService::removeLocalObject(const UUID& uuid) {
    // Remove from mLocations, but save the cached state
    assert( mLocations.find(uuid) != mLocations.end() );
    assert( mLocations[uuid].local == true );
    assert( mLocations[uuid].aggregate == false );

    mLocations.erase(uuid);

    // Remove from the list of local objects
    CONTEXT_SPACETRACE(serverObjectEvent, mContext->id(), mContext->id(), uuid, false, TimedMotionVector3f());
    notifyLocalObjectRemoved(uuid, false);

    // FIXME we might want to give a grace period where we generate a replica if one isn't already there,
    // instead of immediately removing all traces of the object.
    // However, this needs to be handled carefully, prefer updates from another server, and expire
    // automatically.

    /*BEGIN: Bullet Physics Code*/
    if(BulletPhysicsPointers.find(uuid) != BulletPhysicsPointers.end()) {
		return;
	}

    PhysicsPointerMap::iterator it = BulletPhysicsPointers.find(uuid);
    BulletPhysicsPointerData * objPointers = &(it->second);

    dynamicsWorld->removeRigidBody(objPointers->objRigidBody);

    delete objPointers->objShape;
	delete objPointers->objMotionState;
	delete objPointers->objRigidBody;

    BulletPhysicsPointers.erase(uuid);

    /*END: Bullet Physics Code*/
}

void BulletPhysicsService::addLocalAggregateObject(const UUID& uuid, const TimedMotionVector3f& loc, const TimedMotionQuaternion& orient, const BoundingSphere3f& bnds, const String& msh) {
    // Aggregates get randomly assigned IDs -- if there's a conflict either we
    // got a true conflict (incredibly unlikely) or somebody (prox/query
    // handler) screwed up.
    assert(mLocations.find(uuid) == mLocations.end());

    mLocations[uuid] = LocationInfo();
    LocationMap::iterator it = mLocations.find(uuid);

    LocationInfo& locinfo = it->second;
    locinfo.location = loc;
    locinfo.orientation = orient;
    locinfo.bounds = bnds;
    locinfo.mesh = msh;
    locinfo.local = true;
    locinfo.aggregate = true;

    // Add to the list of local objects
    notifyLocalObjectAdded(uuid, true, location(uuid), orientation(uuid), bounds(uuid), mesh(uuid));
}

void BulletPhysicsService::removeLocalAggregateObject(const UUID& uuid) {
    // Remove from mLocations, but save the cached state
    assert( mLocations.find(uuid) != mLocations.end() );
    assert( mLocations[uuid].local == true );
    assert( mLocations[uuid].aggregate == true );
    mLocations.erase(uuid);

    notifyLocalObjectRemoved(uuid, true);
}

void BulletPhysicsService::updateLocalAggregateLocation(const UUID& uuid, const TimedMotionVector3f& newval) {
    LocationMap::iterator loc_it = mLocations.find(uuid);
    assert(loc_it != mLocations.end());
    assert(loc_it->second.aggregate == true);
    loc_it->second.location = newval;
    notifyLocalLocationUpdated( uuid, true, newval );
}
void BulletPhysicsService::updateLocalAggregateOrientation(const UUID& uuid, const TimedMotionQuaternion& newval) {
    LocationMap::iterator loc_it = mLocations.find(uuid);
    assert(loc_it != mLocations.end());
    assert(loc_it->second.aggregate == true);
    loc_it->second.orientation = newval;
    notifyLocalOrientationUpdated( uuid, true, newval );
}
void BulletPhysicsService::updateLocalAggregateBounds(const UUID& uuid, const BoundingSphere3f& newval) {
    LocationMap::iterator loc_it = mLocations.find(uuid);
    assert(loc_it != mLocations.end());
    assert(loc_it->second.aggregate == true);
    loc_it->second.bounds = newval;
    notifyLocalBoundsUpdated( uuid, true, newval );
}
void BulletPhysicsService::updateLocalAggregateMesh(const UUID& uuid, const String& newval) {
    LocationMap::iterator loc_it = mLocations.find(uuid);
    assert(loc_it != mLocations.end());
    assert(loc_it->second.aggregate == true);
    loc_it->second.mesh = newval;
    notifyLocalMeshUpdated( uuid, true, newval );
}

void BulletPhysicsService::addReplicaObject(const Time& t, const UUID& uuid, const TimedMotionVector3f& loc, const TimedMotionQuaternion& orient, const BoundingSphere3f& bnds, const String& msh) {
    // FIXME we should do checks on timestamps to decide which setting is "more" sane
    LocationMap::iterator it = mLocations.find(uuid);

    if (it != mLocations.end()) {
        // It already exists. If its local, ignore the update. If its another replica, somethings out of sync, but perform the update anyway
        LocationInfo& locinfo = it->second;
        if (!locinfo.local) {
            locinfo.location = loc;
            locinfo.orientation = orient;
            locinfo.bounds = bnds;
            locinfo.mesh = msh;
            //local = false
            // FIXME should we notify location and bounds updated info?
        }
        // else ignore
    }
    else {
        // Its a new replica, just insert it
        LocationInfo locinfo;
        locinfo.location = loc;
        locinfo.orientation = orient;
        locinfo.bounds = bnds;
        locinfo.mesh = msh;
        locinfo.local = false;
        locinfo.aggregate = false;
        mLocations[uuid] = locinfo;

        // We only run this notification when the object actually is new
        CONTEXT_SPACETRACE(serverObjectEvent, 0, mContext->id(), uuid, true, loc); // FIXME add remote server ID
        notifyReplicaObjectAdded(uuid, location(uuid), orientation(uuid), bounds(uuid), mesh(uuid));
    }
}

void BulletPhysicsService::removeReplicaObject(const Time& t, const UUID& uuid) {
    // FIXME we should maintain some time information and check t against it to make sure this is sane

    LocationMap::iterator it = mLocations.find(uuid);
    if (it == mLocations.end())
        return;

    // If the object is marked as local, this is out of date information.  Just ignore it.
    LocationInfo& locinfo = it->second;
    if (locinfo.local)
        return;

    // Otherwise, remove and notify
    mLocations.erase(uuid);
    CONTEXT_SPACETRACE(serverObjectEvent, 0, mContext->id(), uuid, false, TimedMotionVector3f()); // FIXME add remote server ID
    notifyReplicaObjectRemoved(uuid);
}


void BulletPhysicsService::receiveMessage(Message* msg) {
    assert(msg->dest_port() == SERVER_PORT_LOCATION);
    Sirikata::Protocol::Loc::BulkLocationUpdate contents;
    bool parsed = parsePBJMessage(&contents, msg->payload());

    if (parsed) {
        for(int32 idx = 0; idx < contents.update_size(); idx++) {
            Sirikata::Protocol::Loc::LocationUpdate update = contents.update(idx);

            // Its possible we'll get an out of date update. We only use this update
            // if (a) we have this object marked as a replica object and (b) we don't
            // have this object marked as a local object
            if (type(update.object()) != Replica)
                continue;

            LocationMap::iterator loc_it = mLocations.find( update.object() );
            // We can safely make this assertion right now because space server
            // to space server loc and prox are on the same reliable channel. If
            // this goes away then a) we can't make this assertion and b) we
            // need an OrphanLocUpdateManager to save updates where this
            // condition is false so they can be applied once the prox update
            // arrives.
            assert(loc_it != mLocations.end());

            if (update.has_location()) {
                TimedMotionVector3f newloc(
                    update.location().t(),
                    MotionVector3f( update.location().position(), update.location().velocity() )
                );
                loc_it->second.location = newloc;
                notifyReplicaLocationUpdated( update.object(), newloc );

                CONTEXT_SPACETRACE(serverLoc, msg->source_server(), mContext->id(), update.object(), newloc );
            }

            if (update.has_orientation()) {
                TimedMotionQuaternion neworient(
                    update.orientation().t(),
                    MotionQuaternion( update.orientation().position(), update.orientation().velocity() )
                );
                loc_it->second.orientation = neworient;
                notifyReplicaOrientationUpdated( update.object(), neworient );
            }

            if (update.has_bounds()) {
                BoundingSphere3f newbounds = update.bounds();
                loc_it->second.bounds = newbounds;
                notifyReplicaBoundsUpdated( update.object(), newbounds );
            }

            if (update.has_mesh()) {
                String newmesh = update.mesh();
                loc_it->second.mesh = newmesh;
                notifyReplicaMeshUpdated( update.object(), newmesh );
            }
        }
    }

    delete msg;
}

void BulletPhysicsService::locationUpdate(UUID source, void* buffer, uint32 length) {
    Sirikata::Protocol::Loc::Container loc_container;
    bool parse_success = loc_container.ParseFromString( String((char*) buffer, length) );
    if (!parse_success) {
        LOG_INVALID_MESSAGE_BUFFER(standardloc, error, ((char*)buffer), length);
        return;
    }

    if (loc_container.has_update_request()) {
        Sirikata::Protocol::Loc::LocationUpdateRequest request = loc_container.update_request();

        TrackingType obj_type = type(source);
        if (obj_type == Local) {
            LocationMap::iterator loc_it = mLocations.find( source );
            assert(loc_it != mLocations.end());

            if (request.has_location()) {
                TimedMotionVector3f newloc(
                    request.location().t(),
                    MotionVector3f( request.location().position(), request.location().velocity() )
                );
                loc_it->second.location = newloc;
                notifyLocalLocationUpdated( source, loc_it->second.aggregate, newloc );

                CONTEXT_SPACETRACE(serverLoc, mContext->id(), mContext->id(), source, newloc );
            }

            if (request.has_bounds()) {
                BoundingSphere3f newbounds = request.bounds();
                loc_it->second.bounds = newbounds;
                notifyLocalBoundsUpdated( source, loc_it->second.aggregate, newbounds );
            }

            if (request.has_mesh()) {
                String newmesh = request.mesh();
                loc_it->second.mesh = newmesh;
                notifyLocalMeshUpdated( source, loc_it->second.aggregate, newmesh );
            }

            if (request.has_orientation()) {
                TimedMotionQuaternion neworient(
                    request.orientation().t(),
                    MotionQuaternion( request.orientation().position(), request.orientation().velocity() )
                );
                loc_it->second.orientation = neworient;
                notifyLocalOrientationUpdated( source, loc_it->second.aggregate, neworient );
            }

        }
        else {
            // Warn about update to non-local object
        }
    }
}



} // namespace Sirikata
