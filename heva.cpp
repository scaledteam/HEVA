//
// Created by scaled
//

#include "heva.h"

#include <Urho3D/Core/CoreEvents.h>
#include <Urho3D/Core/ProcessUtils.h>
#include <Urho3D/Engine/Engine.h>
#include <Urho3D/Engine/EngineDefs.h>
#include <Urho3D/IO/FileSystem.h>
#include <Urho3D/Graphics/AnimatedModel.h>
#include <Urho3D/Graphics/Animation.h>
#include <Urho3D/Graphics/AnimationState.h>
#include <Urho3D/Graphics/Camera.h>
#include <Urho3D/Graphics/Graphics.h>
#include <Urho3D/Graphics/Octree.h>
#include <Urho3D/Graphics/Renderer.h>
#include <Urho3D/Graphics/RenderPath.h>
#include <Urho3D/Graphics/Zone.h>
#include <Urho3D/Resource/ResourceCache.h>
#include <Urho3D/Scene/Scene.h>
#ifndef X11_TRANSPARENT_WINDOW
#include <Urho3D/Input/Input.h>
#endif

// Hair
#define HAIR_WITH_END_BONE
#define HAIR_LENGTH 0.05f
#include <Urho3D/Physics/CollisionShape.h>
#include <Urho3D/Physics/PhysicsWorld.h>
#include <Urho3D/Physics/RigidBody.h>
#include <Urho3D/Physics/Constraint.h>

#ifdef DEBUG_GEOMETRY
#include <Urho3D/Graphics/DebugRenderer.h>
#endif

// inih
#ifdef ALL_IN_ONE
#include "./inih/cpp/INIReader.h"
#else
#include "INIReader.h"
#endif

// dlib
//#undef WEBCAM_ENABLE
//#undef VMC_OSC_SENDER
#ifdef WEBCAM_ENABLE
#include "face_recognition.h"
#include <pthread.h>

//std::thread dlib_thread;
pthread_t dlib_thread1;
pthread_t dlib_thread2;
pthread_mutex_t dlib_thread_mutex = PTHREAD_MUTEX_INITIALIZER;
dlib_thread_data* dlib_data;
webcam_settings_t* webcam_settings;
#endif

#ifdef VMC_OSC_SENDER
#include "vmc_osc_sender.h"

pthread_t vmc_osc_sender_thread;
vmc_osc_data* vmc_osc_sender_data;

bool vmc_osc_udp_enabled = false;
std::string vmc_osc_udp_addr = "127.0.0.1";
unsigned int vmc_osc_udp_port = 9000;
#endif

#ifdef X11_TRANSPARENT_WINDOW
Heva::Heva(Context* context, void *window) :
	Urho3D::Object(context), window_(window)
{
}
#else
// Expands to this example's entry-point
URHO3D_DEFINE_APPLICATION_MAIN(Heva)

Heva::Heva(Context* context) :
	Application(context)
{
}
#endif


std::string sceneName = "Default";
bool webcamSync = true;
int graphicsFps = 0;
float model_focalLength = 50.0;

void Heva::Setup()
{
	// Inih
	int graphicsMultisample = 1;
	int graphicsWidth = 550;
	int graphicsHeight = 800;
	bool graphicsVsync = true;
	bool graphicsFullscreen = false;
	bool graphicsBorderless = false;
	bool graphicsSimple = false;
	
	auto* fileSystem = GetSubsystem<FileSystem>();
	INIReader reader((fileSystem->GetProgramDir() + "heva.ini").GetBuffer());
	if (reader.ParseError() < 0) {
		printf("Can't load 'test.ini'\n");
	}
	else {
		printf("Ini loaded.\n");
		
		webcamSync = reader.GetBoolean("webcam", "sync", true);
		
		sceneName = reader.Get("model", "scene", "Default");
		model_focalLength = reader.GetReal("model", "focal_length", 50.0);
		
		graphicsSimple = reader.GetBoolean("graphics", "simple", false);
		graphicsMultisample = reader.GetInteger("graphics", "multisample", 1);
		graphicsWidth = reader.GetInteger("graphics", "width", 550);
		graphicsHeight = reader.GetInteger("graphics", "height", 800);
		graphicsVsync = reader.GetBoolean("graphics", "vsync", true);
		graphicsFps = reader.GetInteger("graphics", "fps", 0);
		graphicsFullscreen = reader.GetBoolean("graphics", "fullscreen", false);
		graphicsBorderless = reader.GetBoolean("graphics", "borderless", false);
		
		#ifdef VMC_OSC_SENDER
		vmc_osc_udp_enabled = reader.GetBoolean("network", "enabled", false);
		vmc_osc_udp_addr = reader.Get("network", "address", "127.0.0.1");
		vmc_osc_udp_port = reader.GetInteger("network", "port", 900);
		#endif
		
		#ifdef WEBCAM_ENABLE
		webcam_settings = (webcam_settings_t*)calloc(1, sizeof(webcam_settings_t));
		webcam_settings->PreferredId = reader.GetInteger("webcam", "preferred_id", -1);
		strcpy(webcam_settings->PreferredName, reader.Get("webcam", "preferred_name", "").c_str());
		strcpy(webcam_settings->Format, reader.Get("webcam", "format", "").c_str());
		
		webcam_settings->Sync = reader.GetBoolean("webcam", "sync", true);
		webcam_settings->SyncType2 = reader.GetBoolean("webcam", "sync_type2", true);
		webcam_settings->MouthIndirect = reader.GetBoolean("webcam", "mouth_indirect", false);
		webcam_settings->Gamma = reader.GetReal("webcam", "gamma", 1.0);
		webcam_settings->Buffer = reader.GetInteger("webcam", "buffer", -1);
		
		webcam_settings->Setup = reader.GetBoolean("webcam", "setup", true);
		webcam_settings->Width = reader.GetInteger("webcam", "width", -1);
		webcam_settings->Height = reader.GetInteger("webcam", "height", -1);
		webcam_settings->Fps = reader.GetInteger("webcam", "fps", -1);
		webcam_settings->LimitTo24or25 = reader.GetBoolean("webcam", "limitTo24or25", false);
		webcam_settings->YUYV = reader.GetBoolean("webcam", "optimised_YUYV_conversion", false);
		
		webcam_settings->EyeSync = reader.GetBoolean("webcam", "eye_sync", true);
		#endif
		
	}
	
	#ifdef ALL_IN_ONE
	strcpy(webcam_settings->shapePredictorPath, (fileSystem->GetProgramDir() + "shape_predictor_68_face_landmarks.dat").GetBuffer());
	#else
	strcpy(webcam_settings->shapePredictorPath, "/usr/share/dlib/shape_predictor_68_face_landmarks.dat");
	#endif
	
	#ifdef X11_TRANSPARENT_WINDOW
	Urho3D::VariantMap engineParameters_;
	#endif
	
	// Modify engine startup parameters
	engineParameters_[EP_LOG_NAME]	 = GetSubsystem<FileSystem>()->GetAppPreferencesDir("urho3d", "logs") + GetTypeName() + ".log";
	engineParameters_[EP_FULL_SCREEN]	= graphicsFullscreen;
	engineParameters_[EP_HEADLESS]		= false;
	engineParameters_[EP_SOUND]		= false;
	engineParameters_[EP_LOG_QUIET]		= true;
	//engineParameters_[EP_WINDOW_RESIZABLE]	= true;
	engineParameters_[EP_WINDOW_TITLE] = "HEVA - High Efficiency VTuber Avatar";
	engineParameters_[EP_WINDOW_WIDTH] = graphicsWidth;
	engineParameters_[EP_WINDOW_HEIGHT] = graphicsHeight;
	engineParameters_[EP_WORKER_THREADS] = false;
	engineParameters_[EP_VSYNC] = graphicsVsync;
	//engineParameters_[EP_TRIPLE_BUFFER] = true;
	//engineParameters_[EP_FLUSH_GPU] = true;
	if (graphicsFps == 0) {
		engineParameters_[EP_FRAME_LIMITER] = false;
	}
	engineParameters_[EP_BORDERLESS] = graphicsBorderless;
	engineParameters_[EP_SHADOWS] = false;
	
	if (graphicsSimple)
		engineParameters_[EP_RENDER_PATH] = "RenderPaths/Simple.xml";
	
	
	engineParameters_[EP_MULTI_SAMPLE] = graphicsMultisample;
	//engineParameters_[EP_FORCE_GL2] = true;
	engineParameters_[EP_HIGH_DPI] = false;
	//engineParameters_[EP_FLUSH_GPU] = true;
	
	#ifdef X11_TRANSPARENT_WINDOW
	engineParameters_[EP_EXTERNAL_WINDOW] = window_;
	#endif

	// Construct a search path to find the resource prefix with two entries:
	// The first entry is an empty path which will be substituted with program/bin directory -- this entry is for binary when it is still in build tree
	// The second and third entries are possible relative paths from the installed program/bin directory to the asset directory -- these entries are for binary when it is in the Urho3D SDK installation location
	//if (!engineParameters_.Contains(EP_RESOURCE_PREFIX_PATHS))
		//engineParameters_[EP_RESOURCE_PREFIX_PATHS] = ";../share/Resources;../share/Urho3D/Resources";
		//engineParameters_[EP_RESOURCE_PREFIX_PATHS] = "Data;CoreData";
	//engineParameters_[EP_RESOURCE_PREFIX_PATHS] = "/home/scaled/git/Urho3D/bin/";
	//engineParameters_[EP_RESOURCE_PREFIX_PATHS] = (Urho3D::String)"CoreData;Data/" + sceneName.c_str() + ";";
	//engineParameters_[EP_RESOURCE_PREFIX_PATHS] = ";CoreData;Data/Default";
	engineParameters_[EP_RESOURCE_PATHS] = (Urho3D::String)"CoreData;Data/" + sceneName.c_str() + ";Data;";
	
	#ifdef X11_TRANSPARENT_WINDOW
	engine_ = new Urho3D::Engine(context_);
	engine_->Initialize(engineParameters_);
	#endif
}


// urho3d
#define ANIM_MULTIPLIER 2
AnimatedModel* modelObject;
AnimationState* animStill;
AnimationState* animLeftRight;
AnimationState* animForwardBackward;
AnimationState* animUpDown;

Urho3D::Node* boneHead;
Urho3D::Node* boneNeck;
Urho3D::Quaternion boneHeadOffset;
Urho3D::Quaternion boneNeckOffset;
Urho3D::Node* boneEyeL;
Urho3D::Node* boneEyeR;
Urho3D::Quaternion boneEyeLOffset;
Urho3D::Quaternion boneEyeROffset;
Urho3D::Bone* boneEyeLBone;
Urho3D::Bone* boneEyeRBone;
Urho3D::Node* boneHips;
float boneLegLength;
float boneLegDistance;
Urho3D::Quaternion boneHipsOffset;
Urho3D::Vector3 boneHipsPosition;
Urho3D::Time* time_;
// Face string hash
StringHash hash_MTH_A;
StringHash hash_MTH_E;
StringHash hash_MTH_U;
StringHash hash_MTH_Fun;
StringHash hash_BRW_Fun;
StringHash hash_BRW_Angry;
StringHash hash_EYE_Angry;
StringHash hash_EYE_Close;
StringHash hash_EYE_Close_L;
StringHash hash_EYE_Close_R;

#ifdef COLLIDERS_FROM_MODEL
void CollidersInitialize(Urho3D::Node* bone)
{
	unsigned int boneNumChildren = bone->GetNumChildren();
	
	if (boneNumChildren == 0)
		return;
	
	if (boneNumChildren == 1 && bone->GetName().ToLower()[0] == 'c' && bone->GetName().ToLower()[1] == 'o')
	{
		//printf("%s\n", bone->GetName());
		Urho3D::Node* boneChild = bone->GetChild(0);
		
		// RigitBody
		RigidBody* colliderBody = bone->CreateComponent<RigidBody>();
		//colliderBody->SetCollisionMask(0);
		//colliderBody->SetCollisionLayerAndMask(0b0100, 0b0000);
		
		// CollisionShape
		CollisionShape* shape = bone->CreateComponent<CollisionShape>();
		//printf("%f\n", modelObject->GetSkeleton().GetBone(bone->GetName())->radius_);
		//shape->SetSphere(boneChild->GetPosition().Length(), boneChild->GetPosition());
		shape->SetSphere(boneChild->GetPosition().Length() * 2);
	}
	if (boneNumChildren > 0)
	{
		for (unsigned int i = 0; i < boneNumChildren; i++)
			CollidersInitialize(bone->GetChild(i));
	}
}
#endif

void SetRotationRecursive(Urho3D::Node* finger, Urho3D::Quaternion rotation) {
	unsigned int finger_index = 0;
	
	finger->SetRotation(finger->GetRotation() * rotation);
	while (finger->GetNumChildren() > 0) {
		finger = finger->GetChild(finger_index);
		finger->SetRotation(finger->GetRotation() * rotation);
	}
}

void HairInitialize(Urho3D::Node* hair, Urho3D::RigidBody* parentRigitBody, float gravity = 9.8f, bool collision = true, float depth = 0, float length = HAIR_LENGTH) {
	// Length and depth computation
	unsigned int index_zero = 0;
	if (hair->GetNumChildren() > 0)
		length = hair->GetChild(index_zero)->GetPosition().Length() * 1;
	#ifdef HAIR_WITH_END_BONE
	else
		return;
	#endif
	
	// RigitBody
	RigidBody* hairBody = hair->CreateComponent<RigidBody>();
	//hairBody->SetCollisionLayerAndMask(0b1000, 0b0100);
	
	hairBody->SetMass(length / HAIR_LENGTH * pow(.2f, depth));

	hairBody->SetGravityOverride(hair->GetWorldRotation().RotationMatrix()*Vector3::UP*gravity);
	hairBody->SetLinearDamping(0.9995f);
	hairBody->SetAngularDamping(1.f);
	//hairBody->SetAngularFactor(Vector3(1, 0, 1));

	hairBody->SetRollingFriction(0.f);
	hairBody->SetFriction(0.f);
	
	//if (!collision)
	//	hairBody->SetCollisionLayerAndMask(0,0);
	
	// CollisionShape
	CollisionShape* shape = hair->CreateComponent<CollisionShape>();
	
	Quaternion inverse_rotation = hair->GetRotation().Inverse();
	shape->SetCapsule(0.01f, length, Vector3::UP*length/2);

	// Constraint
	Constraint* constraint = hair->CreateComponent<Constraint>();
	constraint->SetConstraintType(Urho3D::ConstraintType::CONSTRAINT_CONETWIST);
	if (depth > 0)
		constraint->SetDisableCollision(true);

	constraint->SetOtherBody(parentRigitBody);

	constraint->SetOtherPosition(hair->GetPosition());
	constraint->SetOtherRotation(hair->GetRotation());

	constraint->SetLowLimit(Vector2::ZERO);
	constraint->SetHighLimit(Vector2(15.0f, 15.0f));
	
	if (hair->GetNumChildren() > 0) {
		HairInitialize(hair->GetChild(index_zero), hairBody, gravity, collision, depth + 1, length);
	}
}

void HairInitializeOnBone(Urho3D::Node* base, Urho3D::RigidBody* parentRigitBody) {
	const Urho3D::Vector<Urho3D::SharedPtr<Urho3D::Node>> bonesHead = base->GetChildren();
	
	for (int i = 0; i < bonesHead.Size(); i++) {
		if (bonesHead.At(i)->GetName().ToLower()[0] == 'h') {
    			//using namespace NodeCollision;
			Urho3D::Node* hair = (Urho3D::Node*)(bonesHead.At(i));
	
			// Disable animation
			Bone* tempBone = modelObject->GetSkeleton().GetBone(bonesHead.At(i)->GetName().ToLower());
			if (tempBone)
				tempBone->animated_ = false;
			
			// Initialize hair
			HairInitialize(hair, parentRigitBody, 9.8f, true);
		}
		if (bonesHead.At(i)->GetName().ToLower()[0] == 's') {
    			//using namespace NodeCollision;
			Urho3D::Node* hair = (Urho3D::Node*)(bonesHead.At(i));
	
			// Disable animation
			Bone* tempBone = modelObject->GetSkeleton().GetBone(bonesHead.At(i)->GetName().ToLower());
			if (tempBone)
				tempBone->animated_ = false;
			
			// Initialize hair
			HairInitialize(hair, parentRigitBody, 4.0f, false);
		}
	}
}

Urho3D::Node* cameraNode_;

void Heva::Start()
{
	// Set custom window Title & Icon
	ResourceCache* cache = GetSubsystem<ResourceCache>();
	Graphics* graphics = GetSubsystem<Graphics>();
	Image* icon = cache->GetResource<Image>("heva_icon.png");
	graphics->SetWindowIcon(icon);
	
	#ifndef X11_TRANSPARENT_WINDOW
	// Subscribe to events
	SubscribeToEvent(E_UPDATE, URHO3D_HANDLER(Heva, HandleUpdate));
	SubscribeToEvent(E_KEYUP, URHO3D_HANDLER(Heva, HandleKeyUp));
	#ifdef DEBUG_GEOMETRY
	SubscribeToEvent(E_POSTRENDERUPDATE, URHO3D_HANDLER(Heva, HandlePostRenderUpdate));
	#endif

	// Set the mouse mode
	Input* input = GetSubsystem<Input>();
        input->SetMouseMode(MM_FREE);
	input->SetMouseVisible(true);
	#endif
        
	//// test code
	scene_ = new Scene(context_);
	File loadFile(context_, cache->GetResourceDirs()[1] + "Scenes/Scene.xml", FILE_READ);
	//File loadFile(context_, "/home/scaled/projects/Urho3D/HEVA2/build4/bin/Data/Default/Scene.xml", FILE_READ);
	
        scene_->LoadXML(loadFile);
        
        
	time_ = new Time(context_);
	scene_->CreateComponent<Octree>();

	// Create a scene node for the camera, which we will move around
	cameraNode_ = scene_->CreateChild("Camera");
	Urho3D::Camera* camera_ = cameraNode_->CreateComponent<Camera>();
	camera_->SetNearClip(.1f);
	camera_->SetFarClip(20.f);
	//camera_->SetFov(40.f/16.f*9.f);
	camera_->SetFov(2.0 * 180.0 / 3.1415 * atan(36.0/2.0 / model_focalLength) / 14.5*9.0);
	printf("%f\n", 2.0 * 180.0 / 3.1415 * atan(36.0/2.0 / model_focalLength));

	// Set an initial position for the camera scene node above the plane
	//cameraNode_->SetPosition(Vector3(0.0f, 0.0f, -4.0f));
	Renderer* renderer = GetSubsystem<Renderer>();


	// Set up a viewport to the Renderer subsystem so that the 3D scene can be seen. We need to define the scene and the camera at minimum. Additionally we could configure the viewport screen size and the rendering path (eg. forward / deferred) to use, but now we just use full screen and default render path configured in the engine command line options
	SharedPtr<Viewport> viewport(new Viewport(context_, scene_, cameraNode_->GetComponent<Camera>()));
	renderer->SetViewport(0, viewport);
	renderer->SetDrawShadows(false);
	
	// Chroma key
	/*#ifdef X11_TRANSPARENT_WINDOW
	//renderer->GetDefaultZone()->SetFogColor(Color::TRANSPARENT);
	renderer->GetDefaultZone()->SetFogColor(Color(0, 0, 0, 0));
	#else
	renderer->GetDefaultZone()->SetFogColor(Color::WHITE);
	//renderer->GetDefaultZone()->SetFogColor(Color::MAGENTA);
	#endif*/
	renderer->GetDefaultZone()->SetFogColor(Color(0, 0, 0, 0));
	renderer->GetDefaultZone()->SetAmbientColor(Color::WHITE);
	
	// Character
	//Node* modelNode = scene_->GetChild("Character", true);
	Node* modelNode = scene_->GetChildrenWithComponent(StringHash("AnimatedModel"), true)[0];
	
	modelObject = modelNode->GetComponent<AnimatedModel>();
	
	animLeftRight = modelObject->AddAnimationState(cache->GetResource<Animation>("Models/LeftRight.ani"));
	animLeftRight->SetWeight(1.f);
	animLeftRight->SetTime(.5f);
	
	animForwardBackward = modelObject->AddAnimationState(cache->GetResource<Animation>("Models/ForwardBackward.ani"));
	animForwardBackward->SetWeight(0.5f);
	animForwardBackward->SetTime(.5f);
	
	animUpDown = modelObject->AddAnimationState(cache->GetResource<Animation>("Models/UpDown.ani"));
	animUpDown->SetWeight(0.333f);
	animUpDown->SetTime(.5f);
	
	//animationState->Apply();
	//modelObject->ApplyAttributes();
	modelObject->ApplyAnimation();
	
	boneHips = modelNode->GetChild("hips", true);
	
	// FXAA
	//SharedPtr<RenderPath> effectRenderPath = viewport->GetRenderPath()->Clone();
	//effectRenderPath->Append(cache->GetResource<XMLFile>("PostProcess/FXAA3.xml"));
	//viewport->SetRenderPath(effectRenderPath);
	
	// Neck tracking
	boneNeck = boneHips->GetChild("neck", true);
	boneHead = boneNeck->GetChild("head", true);
	boneHeadOffset = boneHead->GetRotation();
	boneNeckOffset = boneNeck->GetRotation();
	
	Bone* tempBone = modelObject->GetSkeleton().GetBone("head");
	if (tempBone)
		tempBone->animated_ = false;
	tempBone = modelObject->GetSkeleton().GetBone("neck");
	if (tempBone)
		tempBone->animated_ = false;
	
	boneEyeL = boneHead->GetChild("leftEye", true);
	boneEyeR = boneHead->GetChild("rightEye", true);
	boneEyeLOffset = boneEyeL->GetRotation();
	boneEyeROffset = boneEyeR->GetRotation();
	boneEyeLBone = modelObject->GetSkeleton().GetBone("leftEye");
	boneEyeRBone = modelObject->GetSkeleton().GetBone("rightEye");
	
	
	// hair physics
	#ifdef DEBUG_GEOMETRY
	scene_->CreateComponent<DebugRenderer>();
	#endif
	PhysicsWorld* physicsWorld_ = scene_->CreateComponent<PhysicsWorld>();
	
	RigidBody* headRigidBody = boneHead->CreateComponent<RigidBody>();
	
	Urho3D::Node* boneUpperChest = boneHips->GetChild("chest", true);
	//Urho3D::Node* boneUpperChest = boneHips->GetChild("upperChest", true);
	RigidBody* upperChestRigidBody = boneUpperChest->CreateComponent<RigidBody>();
	
	#ifdef COLLIDERS_FROM_MODEL
	CollidersInitialize(boneHips);
	#else
	// Head collision
	CollisionShape* headShape = boneHead->CreateComponent<CollisionShape>();
	//headShape->SetSphere(0.16f, Vector3(0.03, 0.07, 0));
	//headShape->SetSphere(0.16f, Vector3(0.01, 0.06, 0));
	headShape->SetCapsule(0.16f, 0.2f, Vector3(0.02, 0.05, 0));
	headRigidBody->SetLinearDamping(1.f);
	headRigidBody->SetAngularDamping(1.f);
	
	// Chest collistion
	Node* boneChest = boneHips->GetChild("chest", true);
	//Node* boneChest = boneHips->GetChild("upperChest", true);
	RigidBody* chestRigidBody = boneChest->CreateComponent<RigidBody>();
	CollisionShape* chestShape = boneChest->CreateComponent<CollisionShape>();
	chestShape->SetCapsule(0.11469f*1.8, 0.11469f*3, Vector3(-0.012381, -0.01, 0), Quaternion(90, 0, 0));
	chestRigidBody->SetLinearDamping(1.f);
	chestRigidBody->SetAngularDamping(1.f);
	#endif
	
	
	HairInitializeOnBone(boneHead, headRigidBody);
	HairInitializeOnBone(boneUpperChest, upperChestRigidBody);
	
	// Face string hash
	hash_MTH_A = StringHash("MTH_A");
	hash_MTH_E = StringHash("MTH_E");
	hash_MTH_U = StringHash("MTH_U");
	hash_MTH_Fun = StringHash("MTH_Fun");
	hash_BRW_Fun = StringHash("BRW_Fun");
	hash_BRW_Angry = StringHash("BRW_Angry");
	hash_EYE_Angry = StringHash("EYE_Angry");
	hash_EYE_Close = StringHash("EYE_Close");
	hash_EYE_Close_L = StringHash("EYE_Close_L");
	hash_EYE_Close_R = StringHash("EYE_Close_R");
	
	// dlib
	if (graphicsFps > 0) {
		engine_->SetMaxFps(graphicsFps);
	}
	
	#ifdef WEBCAM_ENABLE
	dlib_data = (dlib_thread_data*)calloc(1, sizeof(dlib_thread_data));
	
	pthread_cond_init(&dlib_data->dlib_thread_cond, NULL);
	pthread_cond_init(&dlib_data->dlib_thread2_cond1, NULL);
	pthread_cond_init(&dlib_data->dlib_thread2_cond2, NULL);
	
	dlib_data->dlib_thread_active = 1;
	dlib_data->dlib_thread_ready = 0;
	
	dlib_data->webcam_settings = webcam_settings;
	
	pthread_create(&dlib_thread1, NULL, dlib_thread1_function, dlib_data);
	pthread_create(&dlib_thread2, NULL, dlib_thread2_function, dlib_data);
	#endif
	
	#ifdef VMC_OSC_SENDER
	if (vmc_osc_udp_enabled) {
		vmc_osc_sender_data = (vmc_osc_data*)calloc(1, sizeof(vmc_osc_data));
		vmc_osc_sender_data->udp_addr = vmc_osc_udp_addr.c_str();
		vmc_osc_sender_data->udp_port = vmc_osc_udp_port;
		vmc_osc_sender_data->vmc_osc_sender_thread_active = 1;
		
		pthread_cond_init(&vmc_osc_sender_data->cond, NULL);
		
		pthread_create(&vmc_osc_sender_thread, NULL, vmc_osc_sender_thread_function, vmc_osc_sender_data);
	}
	#endif
}

void Heva::Stop()
{
	#ifdef WEBCAM_ENABLE
	dlib_data->dlib_thread_active = 0;
	pthread_join(dlib_thread1, NULL);
	pthread_join(dlib_thread2, NULL);
	#endif
	
	#ifdef VMC_OSC_SENDER
	if (vmc_osc_udp_enabled) {
		vmc_osc_sender_data->vmc_osc_sender_thread_active = 0;
		pthread_cond_signal(&vmc_osc_sender_data->cond);
		pthread_join(vmc_osc_sender_thread, NULL);
	}
	#endif
	
	#ifndef X11_TRANSPARENT_WINDOW
	engine_->Exit();
	#endif
}


#ifdef X11_TRANSPARENT_WINDOW
#define KEY_ESCAPE 9
#define KEY_1 10
#define KEY_2 11
#define KEY_3 12
#define KEY_4 13
#define KEY_5 14
#define KEY_6 15
#define KEY_7 16
#define KEY_E 26
#define KEY_R 27
#define KEY_N 57
#define KEY_A 38
#define KEY_F 41
#define KEY_U 30

void Heva::HandleKeyUp(int key)
{
#else


void Heva::HandleKeyUp(StringHash eventType, VariantMap& eventData)
{
	using namespace KeyUp;
	int key = eventData[P_KEY].GetI32();

#endif

	// Close console (if open) or exit when ESC is pressed
	if (key == KEY_ESCAPE) {
		#ifndef X11_TRANSPARENT_WINDOW
		engine_->Exit();
		#endif
	}
	
	// Reset adapt strength
	#ifdef WEBCAM_ENABLE
	if (key == KEY_R)
		dlib_data->dlib_thread_signal = DLIB_THREAD_SIGNAL_RESET;
	
	//printf("%d\n", key);
	// Calibrate Emotions
	if (key == KEY_N)
		dlib_data->dlib_thread_signal = DLIB_THREAD_SIGNAL_CALIBRATE_MTH_Neutral;
	if (key == KEY_E)
		dlib_data->dlib_thread_signal = DLIB_THREAD_SIGNAL_CALIBRATE_MTH_E;
	if (key == KEY_A)
		dlib_data->dlib_thread_signal = DLIB_THREAD_SIGNAL_CALIBRATE_MTH_A;
	if (key == KEY_F)
		dlib_data->dlib_thread_signal = DLIB_THREAD_SIGNAL_CALIBRATE_MTH_Fun;
	if (key == KEY_U)
		dlib_data->dlib_thread_signal = DLIB_THREAD_SIGNAL_CALIBRATE_MTH_U;
	#endif
}

#define POINTS_SMOOTHING1 .2
#define POINTS_SMOOTHING .25
#define POINTS_SMOOTHING_DISTANCE_ROT 6
#define POINTS_SMOOTHING_DISTANCE_LOC .1
/*double rotationSmooth1 = 0;
double rotationSmooth2 = 0;
double rotationSmooth3 = 0;
double translationSmooth1 = 0;
double translationSmooth2 = 0;
double translationSmooth3 = 0;*/
Urho3D::Vector3 rotationSmooth;
Urho3D::Vector3 translationSmooth;

#ifdef X11_TRANSPARENT_WINDOW
void Heva::HandleUpdate()
#else
void Heva::HandleUpdate(StringHash eventType, VariantMap& eventData)
#endif
{
	float bodyAngle1 = 0;
	#ifdef WEBCAM_ENABLE
	// Sync with webcam
	if (webcamSync) {
		if (dlib_data->dlib_thread_ready) {
			//float beforeMeasure = time_->GetElapsedTime();
			
			pthread_mutex_lock(&dlib_thread_mutex);
			pthread_cond_wait(&dlib_data->dlib_thread_cond, &dlib_thread_mutex);
			pthread_mutex_unlock(&dlib_thread_mutex);
			
			//printf("%f\n", 1/(time_->GetElapsedTime() - beforeMeasure));
		}
		else {
			time_->Sleep(1000/25);
		}
	}
	
	face_recognition_data* face_data = &dlib_data->face_data;
	
	
	// Body and Head
	rotationSmooth += POINTS_SMOOTHING1 * (Vector3(face_data->rotation1, face_data->rotation2, face_data->rotation3) - rotationSmooth);
	translationSmooth += POINTS_SMOOTHING1 * (Vector3(face_data->translation1, face_data->translation2, face_data->translation3) - translationSmooth);
	
	//Urho3D::Vector3 translation_vector = Vector3(face_data->translation1, face_data->translation2, face_data->translation3);
	//translationSmooth += std::max(0.0, abs(1.0 - 2.0*translation_vector.Length())) * POINTS_SMOOTHING1 * (translation_vector - translationSmooth);
	
	
	double tempTime = translationSmooth.x_ * ANIM_MULTIPLIER;
	animLeftRight->SetTime(.5+.5*tempTime);
	tempTime = translationSmooth.z_ * -ANIM_MULTIPLIER;
	animForwardBackward->SetTime(.5+.5*tempTime);
	tempTime = translationSmooth.y_ * -ANIM_MULTIPLIER;
	animUpDown->SetTime(.5+.5*tempTime);
	
	Urho3D::Quaternion eyesAngle = Quaternion(face_data->rotation1*.5, face_data->rotation3*.4, 0);
	Urho3D::Quaternion headAngle = Quaternion(rotationSmooth.x_*.5, rotationSmooth.z_*.4, -rotationSmooth.y_*.5);
	
	boneNeck->SetRotation(boneNeckOffset * headAngle);
	boneHead->SetRotation(boneHeadOffset * headAngle);
	boneEyeLBone->animated_ = true;
	boneEyeRBone->animated_ = true;
	modelObject->ApplyAnimation();
	boneEyeLOffset = boneEyeL->GetRotation();
	boneEyeROffset = boneEyeR->GetRotation();
	boneEyeL->SetRotation(boneEyeLOffset * eyesAngle);
	boneEyeR->SetRotation(boneEyeROffset * eyesAngle);
	boneEyeLBone->animated_ = false;
	boneEyeRBone->animated_ = false;
	
	// Face
	modelObject->SetMorphWeight(hash_MTH_A, face_data->MTH_A);
	modelObject->SetMorphWeight(hash_MTH_U, face_data->MTH_U);
	modelObject->SetMorphWeight(hash_MTH_Fun, face_data->MTH_Fun);
	modelObject->SetMorphWeight(hash_MTH_E, face_data->MTH_E);
	modelObject->SetMorphWeight(hash_BRW_Fun, face_data->BRW_Fun);
	modelObject->SetMorphWeight(hash_BRW_Angry, face_data->BRW_Angry);
	modelObject->SetMorphWeight(hash_EYE_Angry, face_data->BRW_Angry);
	modelObject->SetMorphWeight(hash_EYE_Close, face_data->EYE_Close);
	modelObject->SetMorphWeight(hash_EYE_Close_L, face_data->EYE_Close_L);
	modelObject->SetMorphWeight(hash_EYE_Close_R, face_data->EYE_Close_R);
	#endif
	
	#ifdef VMC_OSC_SENDER
	if (vmc_osc_udp_enabled) {
		vmc_osc_sender_data->eyesRot = Quaternion(face_data->rotation1*.5, -face_data->rotation3*.5, 0);
		vmc_osc_sender_data->headRot = Quaternion(rotationSmooth.x_, -rotationSmooth.z_, -rotationSmooth.y_);
		vmc_osc_sender_data->headLoc = Vector3(translationSmooth.z_, translationSmooth.y_, -translationSmooth.x_)*.25;
		vmc_osc_sender_data->MTH_A = face_data->MTH_A;
		vmc_osc_sender_data->MTH_U = face_data->MTH_U;
		vmc_osc_sender_data->MTH_Fun = face_data->MTH_Fun;
		vmc_osc_sender_data->MTH_E = face_data->MTH_E;
		vmc_osc_sender_data->BRW_Fun = face_data->BRW_Fun;
		vmc_osc_sender_data->BRW_Angry = face_data->BRW_Angry;
		vmc_osc_sender_data->EYE_Close = face_data->EYE_Close;
		vmc_osc_sender_data->EYE_Close_L = face_data->EYE_Close_L;
		vmc_osc_sender_data->EYE_Close_R = face_data->EYE_Close_R;
		pthread_cond_signal(&vmc_osc_sender_data->cond);
	}
	#endif
	
	#ifdef X11_TRANSPARENT_WINDOW
	#ifdef DEBUG_GEOMETRY
	scene_->GetComponent<PhysicsWorld>()->DrawDebugGeometry(true);
	#endif
	engine_->RunFrame();
	#endif
}

#ifndef X11_TRANSPARENT_WINDOW
#ifdef DEBUG_GEOMETRY
void Heva::HandlePostRenderUpdate(StringHash eventType, VariantMap& eventData)
{
	// If draw debug mode is enabled, draw physics debug geometry. Use depth test to make the result easier to interpret
	scene_->GetComponent<PhysicsWorld>()->DrawDebugGeometry(true);
}
#endif
#endif
