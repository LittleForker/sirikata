
#ifndef _CBR_SERVER_HPP_
#define _CBR_SERVER_HPP_

#include "Utility.hpp"
#include "SpaceContext.hpp"

#include "ObjectHostConnectionManager.hpp"
#include "TimeProfiler.hpp"

namespace CBR
{
class Forwarder;

class LocationService;
class Proximity;

class CoordinateSegmentation;
class ObjectSegmentation;

// FIXME these are only passed to the forwarder...
class ServerMessageQueue;
class ObjectMessageQueue;

class LoadMonitor;

class ObjectConnection;
class ObjectHostConnectionManager;

class ServerIDMap;

  /** Handles all the basic services provided for objects by a server,
   *  including routing and message delivery, proximity services, and
   *  object -> server mapping.  This is a singleton for each simulated
   *  server.  Other servers are referenced by their ServerID.
   */
class Server : public MessageRecipient
  {
  public:
      Server(SpaceContext* ctx, Forwarder* forwarder, LocationService* loc_service, CoordinateSegmentation* cseg, Proximity* prox, ObjectMessageQueue* omq, ServerMessageQueue* smq, LoadMonitor* lm, ObjectSegmentation* oseg, ServerIDMap* sidmap);
    ~Server();

      void service();

    ServerID lookup(const Vector3f&);
    ServerID lookup(const UUID&);

      virtual void receiveMessage(Message* msg);
private:
    // Methods for periodic servicing
    void serviceProximity();
    void serviceObjectHostNetwork();
    void checkObjectMigrations();

    // Finds the ObjectConnection associated with the given object, returns NULL if the object isn't available.
    ObjectConnection* getObjectConnection(const UUID& object_id) const;

    // Callback which handles messages from object hosts -- mostly just does sanity checking
    // before using the forwarder to do routing.
    void handleObjectHostMessage(const ObjectHostConnectionManager::ConnectionID& conn_id, CBR::Protocol::Object::ObjectMessage* msg);

    // Handle Session messages from an object
    void handleSessionMessage(const ObjectHostConnectionManager::ConnectionID& oh_conn_id, const CBR::Protocol::Object::ObjectMessage& msg);
    // Handle Connect message from object
    void handleConnect(const ObjectHostConnectionManager::ConnectionID& oh_conn_id, const CBR::Protocol::Object::ObjectMessage& container, const CBR::Protocol::Session::Connect& connect_msg);
    // Handle Migrate message from object
    void handleMigrate(const ObjectHostConnectionManager::ConnectionID& oh_conn_id, const CBR::Protocol::Object::ObjectMessage& container, const CBR::Protocol::Session::Connect& migrate_msg);

    // Performs actual migration after all the necessary information is available.
    void handleMigration(const UUID& obj_id);

    //finally deletes any object connections to obj_id
    void killObjectConnection(const UUID& obj_id);

    SpaceContext* mContext;
    LocationService* mLocationService;
    CoordinateSegmentation* mCSeg;
    Proximity* mProximity;
    ObjectSegmentation* mOSeg;
    Forwarder* mForwarder;
      LoadMonitor* mLoadMonitor;

    ObjectHostConnectionManager* mObjectHostConnectionManager;

    typedef std::map<UUID, ObjectConnection*> ObjectConnectionMap;

    ObjectConnectionMap mObjects;
    ObjectConnectionMap mObjectsAwaitingMigration;

    typedef std::map<UUID, CBR::Protocol::Migration::MigrationMessage*> ObjectMigrationMap;
      ObjectMigrationMap mObjectMigrations;

    typedef std::set<ObjectConnection*> ObjectConnectionSet;
    ObjectConnectionSet mClosingConnections; // Connections that are closing but need to finish delivering some messages

    ObjectConnectionMap mMigratingConnections;//bftm add


      TimeProfiler mProfiler;
}; // class Server

} // namespace CBR

#endif //_CBR_SERVER_HPP_
