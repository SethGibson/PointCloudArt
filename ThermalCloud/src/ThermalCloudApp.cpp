#ifdef _DEBUG
#pragma comment(lib, "DSAPI32.dbg.lib")
#else
#pragma comment(lib, "DSAPI32.lib")
#endif

#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/gl/Batch.h"
#include "cinder/gl/GlslProg.h"
#include "cinder/Rand.h"
#include "cinder/Camera.h"
#include "cinder/MayaCamUI.h"
#include "CiDSAPI.h"

using namespace ci;
using namespace ci::app;
using namespace std;
using namespace CinderDS;

static int S_MAX_S = 100000;
static int S_STEP = 40;
class ThermalCloudApp : public App
{
public:
	void setup() override;
	void mouseDown( MouseEvent event ) override;
	void mouseDrag(MouseEvent event) override;
	void keyDown(KeyEvent event) override;
	void update() override;
	void draw() override;
	void cleanup() override;

	struct CloudPoint
	{
		vec3	PPos;
		vec2	PUV;
		float	PSize;

		CloudPoint(vec3 pPos, vec2 pUv, float pSize) : PPos(pPos), PUV(pUv), PSize(pSize){}
	};
	struct Particle
	{
		vec2 POrigin;
		float PSize;
		vec2 PSize_R;
		float PVel;
		float PMod;

		Particle(vec2 pOrigin, vec2 pSize, float pVel, float pMod) :POrigin(pOrigin), PSize_R(pSize), PVel(pVel), PMod(pMod)
		{
		}
	};

private:
	void reset();

	bool mUpdate;

	vector<Particle>	mParticles;
	vector<CloudPoint>	mPositions;
	int mNumToAdd;

	gl::BatchRef		mCloudBatch;
	gl::VboRef			mCloudData;
	gl::VboMeshRef		mCloudMesh;
	geom::BufferLayout	mCloudAttribs;
	gl::GlslProgRef		mCloudShader;

	CinderDSRef			mCinderDS;
	gl::Texture2dRef	mTexRgb;

	CameraPersp			mCamera;
	MayaCamUI			mMayaCam;
	mat4				mObjectRotation;


};


void ThermalCloudApp::setup()
{
	mUpdate = true;
	getWindow()->setSize(1280, 720);
	
	mParticles.push_back	
	(
		Particle(vec2(randInt(0, 479), 360), vec2(randFloat(5,10), randFloat(1,4)), randFloat(0.1f,1.f), randFloat(0.001f,0.1f))
	);

	mNumToAdd = S_MAX_S - 1;
	
	mPositions.push_back(CloudPoint(vec3(0), vec2(0), 1.0f));
	
	mCloudData = gl::Vbo::create(GL_ARRAY_BUFFER, mPositions, GL_DYNAMIC_DRAW);
	mCloudAttribs.append(geom::CUSTOM_0, 3, sizeof(CloudPoint), offsetof(CloudPoint,PPos), 1);
	mCloudAttribs.append(geom::CUSTOM_1, 2, sizeof(CloudPoint), offsetof(CloudPoint, PUV), 1);
	mCloudAttribs.append(geom::CUSTOM_2, 1, sizeof(CloudPoint), offsetof(CloudPoint, PSize), 1);

	mCloudMesh = gl::VboMesh::create(geom::Sphere());
	mCloudShader = gl::GlslProg::create(loadAsset("cloud_vert.glsl"), loadAsset("cloud_frag.glsl"));
	mCloudMesh->appendVbo(mCloudAttribs, mCloudData);
	
	mCloudBatch = gl::Batch::create(mCloudMesh, mCloudShader, { { geom::CUSTOM_0, "iPosition" }, { geom::CUSTOM_1, "iUV" }, { geom::CUSTOM_2, "iSize" } });
	mCloudBatch->getGlslProg()->uniform("mTexRgb", 0);

	mCinderDS = CinderDSAPI::create();
	mCinderDS->init();
	mCinderDS->initDepth(FrameSize::DEPTHSD, 60);
	mCinderDS->initRgb(FrameSize::RGBVGA, 60);
	mCinderDS->start();

	mTexRgb = gl::Texture2d::create(640, 480, gl::Texture2d::Format().internalFormat(GL_RGB8));

	mCamera.setPerspective(45.0f, getWindowAspectRatio(), 100.0f, 3000.0f);
	mCamera.lookAt(vec3(0), vec3(0, 0, 1), vec3(0, -1, 0));
	mCamera.setCenterOfInterestPoint(vec3(0, 0, 750));
	mMayaCam.setCurrentCam(mCamera);

	gl::enableDepthRead();
	gl::enableDepthWrite();
}


void ThermalCloudApp::reset()
{
	mPositions.clear();
	mParticles.clear();
	mCloudData->bufferData(480*360*sizeof(vec3), nullptr, GL_DYNAMIC_DRAW);
	mCloudMesh = gl::VboMesh::create(geom::Sphere());
	mCloudMesh->appendVbo(mCloudAttribs, mCloudData);
	mCloudBatch->replaceVboMesh(mCloudMesh);
}


void ThermalCloudApp::mouseDown( MouseEvent event )
{
	mMayaCam.mouseDown(event.getPos());
}

void ThermalCloudApp::mouseDrag(MouseEvent event)
{
	mMayaCam.mouseDrag(event.getPos(), event.isLeftDown(), false, event.isRightDown());
}

void ThermalCloudApp::keyDown(KeyEvent event)
{
	if (event.getCode() == KeyEvent::KEY_SPACE)
		reset();
}

void ThermalCloudApp::update()
{
	if (mUpdate)
	{
		//update particles first
		for (auto pit = mParticles.begin(); pit != mParticles.end();)
		{
			if (pit->POrigin.y <= 0)
			{
				pit = mParticles.erase(pit);
				mNumToAdd += 1;
			}
			else
			{
				pit->POrigin.y -= pit->PVel;
				pit->PSize = lerp(pit->PSize_R.y, pit->PSize_R.x, pit->POrigin.y / 480.0f);

				++pit;
			}
		}
		if (mNumToAdd > 0)
		{
			for (int p = 0; p < S_STEP; ++p)
			{
				mParticles.push_back(Particle(vec2(randInt(0, 479), 360), vec2(randFloat(5, 10), randFloat(1, 4)), randFloat(1.f, 3.f), randFloat(0.01f, 0.25f)));
				mNumToAdd -= 1;
			}
		}

		//update mesh
		mCinderDS->update();
		mPositions.clear();
		Channel16u cDepth = mCinderDS->getDepthFrame();
		mTexRgb->update(mCinderDS->getRgbFrame());

		for (auto p : mParticles)
		{
			float cX = p.POrigin.x;
			float cY = p.POrigin.y;

			float cVal = (float)cDepth.getValue(vec2((int)cX, (int)cY));
			if (cVal > 100 && cVal < 1500)
			{
				vec3 cPos = mCinderDS->getZCameraSpacePoint(vec3(cX, cY, cVal));
				vec2 cUV = mCinderDS->getColorSpaceCoordsFromZCamera(cPos);
				mPositions.push_back(CloudPoint(cPos, cUV, p.PSize));
			}
		}

		mCloudData->bufferData(mPositions.size()*sizeof(vec3), mPositions.data(), GL_DYNAMIC_DRAW);
		mCloudMesh = gl::VboMesh::create(geom::Sphere());
		mCloudMesh->appendVbo(mCloudAttribs, mCloudData);
		mCloudBatch->replaceVboMesh(mCloudMesh);
	}

	mObjectRotation *= rotate(0.04f, normalize(vec3(0.0f, 1, 750)));
}

void ThermalCloudApp::draw()
{
	gl::clear( Color( 0.1f, 0.25f, 0.15f ) ); 
	gl::color(Color::white());
	gl::setMatrices(mMayaCam.getCamera());
	gl::pushMatrices();
	gl::ScopedTextureBind cRgb(mTexRgb);
	mCloudBatch->getGlslProg()->uniform("ViewDirection", vec3(0, -1, 0));
	mCloudBatch->drawInstanced(mPositions.size());
	gl::popMatrices();
}

void ThermalCloudApp::cleanup()
{
	mCinderDS->stop();
}

CINDER_APP( ThermalCloudApp, RendererGl )
