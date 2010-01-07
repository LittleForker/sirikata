/*  Sirikata Graphical Object Host
 *  MeshEntity.cpp
 *
 *  Copyright (c) 2009, Patrick Reiter Horn
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
#include <proxyobject/Platform.hpp>
#include "MeshEntity.hpp"
#include <util/AtomicTypes.hpp>
#include "OgreHeaders.hpp"
#include <OgreMeshManager.h>
#include <OgreResourceGroupManager.h>
#include <OgreSubEntity.h>
#include <OgreEntity.h>
#include "resourceManager/GraphicsResourceManager.hpp"
#include "WebView.hpp"
#include <util/Sha256.hpp>
#include <transfer/TransferManager.hpp>

namespace Sirikata {
namespace Graphics {

MeshEntity::MeshEntity(OgreSystem *scene,
                       const std::tr1::shared_ptr<ProxyMeshObject> &pmo,
                       const std::string &id)
        : Entity(scene,
                 pmo,
                 id.length()?id:ogreMeshName(pmo->getObjectReference()),
                 NULL)
{
    getProxy().MeshProvider::addListener(this);
    Meru::GraphicsResourceManager* grm = Meru::GraphicsResourceManager::getSingletonPtr();
    mResource = std::tr1::dynamic_pointer_cast<Meru::GraphicsResourceEntity>
        (grm->getResourceEntity(pmo->getObjectReference(), this));
    unloadMesh();
}

std::string MeshEntity::ogreMeshName(const SpaceObjectReference&ref) {
    return "Mesh:"+ref.toString();
}
std::string MeshEntity::ogreMovableName()const{
    return ogreMeshName(id());
}
MeshEntity::~MeshEntity() {
    mResource->entityDestroyed();
    Ogre::Entity * toDestroy=getOgreEntity();
    init(NULL);
    if (toDestroy) {
        getScene()->getSceneManager()->destroyEntity(toDestroy);
    }
    getProxy().MeshProvider::removeListener(this);

}

class ReplaceTexture {
	const String &mFrom;
	const String &mTo;
public:
	ReplaceTexture(const String &from, const String &to)
		: mFrom(from),
		  mTo(to) {
	}
	bool operator()(Ogre::TextureUnitState *tus) {
		if (tus->getTextureName() == mFrom) {
			tus->setTextureName(mTo);
		}
		return false;
	}
};

class ShouldReplaceTexture {
	const MeshEntity::TextureBindingsMap &mFrom;
public:
	ShouldReplaceTexture(const MeshEntity::TextureBindingsMap &from)
		: mFrom(from) {
	}
	bool operator()(Ogre::TextureUnitState *tus) {
		if (mFrom.find(tus->getTextureName()) != mFrom.end()) {
			return true;
		}
		return false;
	}
};

template <class Functor>
bool forEachTexture(const Ogre::MaterialPtr &material, Functor func) {
	int numTechniques = material->getNumTechniques();
	for (int whichTechnique = 0; whichTechnique < numTechniques; whichTechnique++) {
		Ogre::Technique *tech = material->getTechnique(whichTechnique);
		int numPasses = tech->getNumPasses();
		for (int whichPass = 0; whichPass < numPasses; whichPass++) {
			Ogre::Pass *pass = tech->getPass(whichPass);
			int numTUS = pass->getNumTextureUnitStates();
			for (int whichTUS = 0; whichTUS < numTUS; whichTUS++) {
				Ogre::TextureUnitState *tus = pass->getTextureUnitState(whichTUS);
				if (func(tus)) {
					return true;
				}
			}
		}
	}
	return false;
}

WebView *MeshEntity::getWebView(int whichSubEnt) {
	Ogre::Entity *ent = getOgreEntity();
	if (!ent) {
		return NULL;
	}
	if (whichSubEnt >= (int)(ent->getNumSubEntities()) || whichSubEnt < 0) {
		return NULL;
	}
	ReplacedMaterialMap::iterator iter = mReplacedMaterials.find(whichSubEnt);
	if (iter != mReplacedMaterials.end()) {
		WebView *wv = WebViewManager::getSingleton().getWebView(iter->second.first);
		return wv;
	}
	return NULL;
}

void MeshEntity::fixTextures() {
	Ogre::Entity *ent = getOgreEntity();
	if (!ent) {
		return;
	}
	SILOG(ogre,debug,"Fixing texture for "<<id());
	int numSubEntities = ent->getNumSubEntities();
	for (ReplacedMaterialMap::iterator iter = mReplacedMaterials.begin();
			iter != mReplacedMaterials.end();
			++iter) {
		int whichSubEntity = iter->first;
		if (whichSubEntity >= numSubEntities) {
			SILOG(ogre,fatal,"Original material map not cleared when mesh changed. which = " << whichSubEntity << " num = " << numSubEntities);
			continue;
		}
		Ogre::SubEntity *subEnt = ent->getSubEntity(whichSubEntity);
		Ogre::MaterialPtr origMaterial = iter->second.second;
		subEnt->setMaterial(origMaterial);
		SILOG(ogre,debug,"Resetting a material "<<id());
	}
	mReplacedMaterials.clear();
	for (int whichSubEntity = 0; whichSubEntity < numSubEntities; whichSubEntity++) {
		Ogre::SubEntity *subEnt = ent->getSubEntity(whichSubEntity);
		Ogre::MaterialPtr material = subEnt->getMaterial();
		if (forEachTexture(material, ShouldReplaceTexture(mTextureBindings))) {
			SILOG(ogre,debug,"Replacing a material "<<id());
			Ogre::MaterialPtr newMaterial = material->clone(material->getName()+id().toString(), false, Ogre::String());
			String newTexture;
			for (TextureBindingsMap::const_iterator iter = mTextureBindings.begin();
					iter != mTextureBindings.end();
					++iter) {
				SILOG(ogre,debug,"Replacing a texture "<<id()<<" : "<<iter->first<<" -> "<<iter->second);
				forEachTexture(newMaterial, ReplaceTexture(iter->first, iter->second));
				newTexture = iter->second;
			}
			mReplacedMaterials.insert(
				ReplacedMaterialMap::value_type(whichSubEntity, std::pair<String, Ogre::MaterialPtr>(newTexture, newMaterial)));
			subEnt->setMaterial(newMaterial);
		}
	}
}

void MeshEntity::bindTexture(const std::string &textureName, const SpaceObjectReference &objId) {
	mTextureBindings[textureName] = objId.toString();
	fixTextures();
}
void MeshEntity::unbindTexture(const std::string &textureName) {
	TextureBindingsMap::iterator iter = mTextureBindings.find(textureName);
	if (iter != mTextureBindings.end()) {
		mTextureBindings.erase(iter);
	}
	fixTextures();
}

void MeshEntity::loadMesh(const String& meshname)
{
    Ogre::Entity * oldMeshObj=getOgreEntity();
    mReplacedMaterials.clear();

    /** FIXME we need a better way of generating unique id's. We should
     *  be able to use just the uuid, but its not enough since we want
     *  to be able to create a replacement entity before destroying the
     *  original.  Also, this is definitely thread safe.
     */
    static AtomicValue<int32> idx ((int32)0);
    Ogre::Entity* new_entity;
    try {
      try {
        new_entity = getScene()->getSceneManager()->createEntity(
            ogreMovableName(), meshname);
      } catch (Ogre::InvalidParametersException &) {
        SILOG(ogre,error,"Got invalid parameters");
        throw;
      }
    } catch (...) {
        SILOG(ogre,error,"Failed to load mesh "<<getProxy().getMesh()<< " (id "<<id()<<")!");
        new_entity = getScene()->getSceneManager()->createEntity(ogreMovableName(),Ogre::SceneManager::PT_CUBE);
        /*
        init(NULL);
        if (oldMeshObj) {
            getScene()->getSceneManager()->destroyEntity(oldMeshObj);
        }
        return;
        */
    }
    SILOG(ogre,debug,"Bounding box: " << new_entity->getBoundingBox());
    if (false) { //programOptions[OPTION_ENABLE_TEXTURES].as<bool>() == false) {
        new_entity->setMaterialName("BaseWhiteTexture");
    }
    unsigned int num_subentities=new_entity->getNumSubEntities();
    SHA256 random_seed=SHA256::computeDigest(id().toRawHexData());
    const SHA256::Digest &digest=random_seed.rawData();
    Ogre::Vector4 parallax_steps(getScene()->mParallaxSteps->as<float>(),getScene()->mParallaxShadowSteps->as<int>(),0.0,1.0);
    unsigned short av=digest[0]+256*(unsigned short)digest[1];
    unsigned short bv=digest[2]+256*(unsigned short)digest[3];
    unsigned short cv=digest[4]+256*(unsigned short)digest[5];
    unsigned short dv=digest[6]+256*(unsigned short)digest[7];
    unsigned short ev=digest[8]+256*(unsigned short)digest[9];
    unsigned short fv=digest[10]+256*(unsigned short)digest[11];
    Ogre::Vector4 random_values((float)av/65535.0f,(float)bv/65535.0f,(float)cv/65535.0f,1.0);
    random_values.x=random_values.x*.0+1.0;//fixme: make this per object
    random_values.y=random_values.y*.0+1.0;
    random_values.z=random_values.z*.0+1.0;
    for (unsigned int subent=0;subent<num_subentities;++subent) {
        new_entity->getSubEntity(subent)->setCustomParameter(0,random_values);
        new_entity->getSubEntity(subent)->setCustomParameter(1,parallax_steps);
    }

    init(new_entity);
    if (oldMeshObj) {
        getScene()->getSceneManager()->destroyEntity(oldMeshObj);
    }
    fixTextures();
}

void MeshEntity::unloadMesh() {
    Ogre::Entity * meshObj=getOgreEntity();
    //init(getScene()->getSceneManager()->createEntity(ogreMovableName(), Ogre::SceneManager::PT_CUBE));
    init(NULL);
    if (meshObj) {
        getScene()->getSceneManager()->destroyEntity(meshObj);
    }
    mReplacedMaterials.clear();
}

void MeshEntity::setSelected(bool selected) {
    Entity::setSelected(selected);
    WebView *webView = WebViewManager::getSingleton().getWebView(id().toString());
    if (selected) {
        WebViewManager::getSingleton().focusWebView(webView);
    } else if (WebViewManager::getSingleton().getFocusedWebView() == webView) {
        WebViewManager::getSingleton().deFocusAllWebViews();
    }
}

/////////////////////////////////////////////////////////////////////
// overrides from MeshListener
// MCB: integrate these with the MeshObject model class

void MeshEntity::onSetMesh ( URI const& meshFile )
{
    // MCB: responsibility to load model meshes must move to MeshObject plugin
    /// hack to support collada mesh -- eventually this should be smarter
    String fn=meshFile.filename();
    bool is_collada=false;
    if (fn.rfind(".dae")==fn.size()-4) is_collada=true;
    if (is_collada) {
        Meru::GraphicsResourceManager* grm = Meru::GraphicsResourceManager::getSingletonPtr ();
        Meru::SharedResourcePtr newModelPtr = grm->getResourceAsset ( meshFile, Meru::GraphicsResource::MODEL );
        mResource->setMeshResource ( newModelPtr );
    }
    else {
        Meru::GraphicsResourceManager* grm = Meru::GraphicsResourceManager::getSingletonPtr ();
        Meru::SharedResourcePtr newMeshPtr = grm->getResourceAsset ( meshFile, Meru::GraphicsResource::MESH );
        mResource->setMeshResource ( newMeshPtr );
    }
}

Vector3f fixUp(int up, Vector3f v) {
    if (up==3) return Vector3f(v[0],v[2], -v[1]);
    else if (up==2) return v;
    std::cerr << "ERROR: X up? You gotta be frakkin' kiddin'\n";
    assert(false);
}

Task::EventResponse MeshEntity::downloadFinished(Task::EventPtr evbase, Meshdata& md) {
    Transfer::DownloadEventPtr ev = std::tr1::static_pointer_cast<Transfer::DownloadEvent> (evbase);
    /*
    std::cout << "dbm debug MeshEntity::downloadFinished for: " << md.uri
            << " status: " << (int)ev->getStatus()
            << " == " << (int)Transfer::TransferManager::SUCCESS
            << " texture: " << md.texture
            << " fingerprint: " << ev->fingerprint()
            << " length: " << (int)ev->data().length() 
            << std::endl;
    */
    SHA256 sha = SHA256::computeDigest(md.uri);    /// rest of system uses hash
    String hash = sha.convertToHexString();
    int up = md.up_axis;
    int vertcount = md.positions.size();
    int normcount = md.normals.size();
    int indexcount = md.position_indices.size();
    /*
    std::cout << "dbm debug vertex data:\n";
    for (int i=0; i<vertcount; i++) {
        std::cout << "  dbm debug vertex (ogre): " << md.positions[i] << std::endl;
    }
    std::cout << "dbm debug normal data:\n";
    for (int i=0; i<normcount; i++) {
        std::cout << "  dbm debug normal (ogre): " << md.normals[i] << std::endl;
    }
    */
    Ogre::MeshManager& mm = Ogre::MeshManager::getSingleton();
    /// FIXME: set bounds, bounding radius here
    Ogre::ManualObject mo(hash);
    mo.clear();

    /// create a material & give it the texture
    Ogre::MaterialManager& matm = Ogre::MaterialManager::getSingleton();
    Ogre::MaterialPtr mat = matm.getByName("baseogremat");
    std::string matname = hash + "_texture";
    mat = mat->clone(matname);

    /// FIXME: only support one texture!
    mat->getTechnique(0)->getPass(0)->createTextureUnitState("Cache/" + ev->fingerprint().convertToHexString(), 0);
    mo.begin(matname);
//    std::cout << "dbm debug triangle data:\n";
    float tu, tv;
    for (int i=0; i<indexcount; i++) {
//        std::cout << "  dbm debug triangle (ogre): ";
        int j = md.position_indices[i];
        Vector3f v = fixUp(up, md.positions[j]);
        mo.position(v[0], v[1], v[2]);
//        std::cout << " pos: " << v;
        j = md.normal_indices[i];
        v = fixUp(up, md.normals[j]);
//        std::cout << " norm: " << v;
        mo.normal(v[0], v[1], v[2]);
        mo.colour(1.0,1.0,1.0,1.0);
        if (md.texUVs.size()==0) {
            /// bogus texture for textureless models
            if (i%3==0) {
                tu=0.0;
                tv=0.0;
            }
            if (i%3==1) {
                tu=0.0;
                tv=1.0;
            }
            if (i%3==2) {
                tu=1.0;
                tv=1.0;
            }
        }
        else {
            Sirikata::Vector2f uv = md.texUVs[i];
//            std::cout << " tex: " << uv;
            tu=uv[0];
            tv=1.0-uv[1];           //  why you gotta be like that?
        }
        mo.textureCoord(tu, tv);
//        std::cout << std::endl;
    }
    mo.end();
    mo.setVisible(true);
    Ogre::MeshPtr mp = mo.convertToMesh(hash, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
    bool check = mm.resourceExists(hash);
    loadMesh(hash);                     /// this is here because we removed  mResource->loaded(true, mEpoch) in  ModelLoadTask::doRun
    return Task::EventResponse::del();
}

void MeshEntity::onMeshParsed (String const& uri, Meshdata& md) {
    if (md.texture=="") {
        md.texture="BumpyMetal.jpg";
    }
    /// attempt download of texture FIXME: only covers single texture case
    String texURI=uri.substr(0, uri.rfind("/")+1) + md.texture;
    mScene->mTransferManager->download(Transfer::URI(texURI), std::tr1::bind(&MeshEntity::downloadFinished,
                                       this,_1,md), Transfer::Range(true));
}

void MeshEntity::onSetScale ( Vector3f const& scale )
{
    mSceneNode->setScale ( toOgre ( scale ) );
}

void MeshEntity::onSetPhysical ( PhysicalParameters const& pp )
{

}

} // namespace Graphics
} // namespace Sirikata