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
#include "./inih-r53/cpp/INIReader.h"
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
pthread_t dlib_thread;
pthread_mutex_t dlib_thread_mutex = PTHREAD_MUTEX_INITIALIZER;
dlib_thread_data* dlib_data;
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


std::string modelName = "Default";
bool femaleLegs = false;
bool webcamSync = true;

void Heva::Setup()
{
	// Inih
	int graphicsMultisample = 1;
	int graphicsWidth = 550;
	int graphicsHeight = 800;
	bool graphicsVsync = true;
	
	INIReader reader(HEVA_CONFIG_PATH);
	if (reader.ParseError() < 0) {
		printf("Can't load 'test.ini'\n");
	}
	else {
		printf("Ini loaded.\n");
		
		webcamSync = reader.GetBoolean("webcam", "sync", true);
		
		modelName = reader.Get("model", "name", "Default");
		femaleLegs = reader.GetBoolean("model", "femalelegs", false);
		
		graphicsMultisample = reader.GetInteger("graphics", "multisample", 1);
		graphicsWidth = reader.GetInteger("graphics", "width", 550);
		graphicsHeight = reader.GetInteger("graphics", "height", 800);
		graphicsVsync = reader.GetBoolean("graphics", "vsync", true);
		
		#ifdef VMC_OSC_SENDER
		vmc_osc_udp_enabled = reader.GetBoolean("network", "enabled", false);
		vmc_osc_udp_addr = reader.Get("network", "address", "127.0.0.1");
		vmc_osc_udp_port = reader.GetInteger("network", "port", 900);
		#endif
	}
	
	#ifdef X11_TRANSPARENT_WINDOW
	Urho3D::VariantMap engineParameters_;
	#endif
	
	// Modify engine startup parameters
	engineParameters_[EP_LOG_NAME]	 = GetSubsystem<FileSystem>()->GetAppPreferencesDir("urho3d", "logs") + GetTypeName() + ".log";
	engineParameters_[EP_FULL_SCREEN]	= false;
	engineParameters_[EP_HEADLESS]		= false;
	engineParameters_[EP_SOUND]		= false;
	engineParameters_[EP_LOG_QUIET]		= true;
	//engineParameters_[EP_WINDOW_RESIZABLE]	= true;
	engineParameters_[EP_WINDOW_TITLE] = "HEVA - High Efficiency VTuber Avatar";
	engineParameters_[EP_WINDOW_WIDTH] = graphicsWidth;
	engineParameters_[EP_WINDOW_HEIGHT] = graphicsHeight;
	engineParameters_[EP_WORKER_THREADS] = false;
	engineParameters_[EP_VSYNC] = graphicsVsync;
	engineParameters_[EP_FRAME_LIMITER] = false;
	
	engineParameters_[EP_MULTI_SAMPLE] = graphicsMultisample;
	
	#ifdef X11_TRANSPARENT_WINDOW
	engineParameters_[EP_EXTERNAL_WINDOW] = window_;
	#endif

	// Construct a search path to find the resource prefix with two entries:
	// The first entry is an empty path which will be substituted with program/bin directory -- this entry is for binary when it is still in build tree
	// The second and third entries are possible relative paths from the installed program/bin directory to the asset directory -- these entries are for binary when it is in the Urho3D SDK installation location
	if (!engineParameters_.Contains(EP_RESOURCE_PREFIX_PATHS))
		engineParameters_[EP_RESOURCE_PREFIX_PATHS] = ";../share/Resources;../share/Urho3D/Resources";
	
	#ifdef X11_TRANSPARENT_WINDOW
	engine_ = new Urho3D::Engine(context_);
	engine_->Initialize(engineParameters_);
	#endif
}


// urho3d
AnimatedModel* modelObject;
Urho3D::Node* boneHead;
Urho3D::Node* boneNeck;
Urho3D::Node* boneHips;
Urho3D::Node* boneArm_L;
Urho3D::Node* boneArm_R;
Urho3D::Node* boneLeg_L;
Urho3D::Node* boneLeg_R;
Urho3D::Node* boneLeg_L_2;
Urho3D::Node* boneLeg_R_2;
Urho3D::Node* boneLeg_L_3;
Urho3D::Node* boneLeg_R_3;
float boneLegLength;
float boneLegDistance;
Urho3D::Node* boneUpperChest;
Urho3D::Quaternion boneHeadOffset;
Urho3D::Quaternion boneNeckOffset;
Urho3D::Quaternion boneHipsOffset;
Urho3D::Quaternion boneUpperChestOffset;
Urho3D::Vector3 boneHipsPosition;
Urho3D::Quaternion headAngle;
Urho3D::Quaternion headAngleOffset;
Urho3D::Quaternion boneArm_L_Offset;
Urho3D::Quaternion boneArm_R_Offset;
Urho3D::Quaternion boneLeg_L_Offset;
Urho3D::Quaternion boneLeg_R_Offset;
Urho3D::Quaternion boneLeg_L_2_LocalRot;
Urho3D::Quaternion boneLeg_R_2_LocalRot;
Urho3D::Quaternion boneLeg_L_3_Offset;
Urho3D::Quaternion boneLeg_R_3_Offset;
Urho3D::Time* time_;
// Face string hash
StringHash hash_MTH_A;
StringHash hash_MTH_E;
StringHash hash_MTH_U;
StringHash hash_MTH_Fun;
StringHash hash_BRW_Fun;
StringHash hash_BRW_Angry;
StringHash hash_EYE_Angry;
StringHash hash_EYE_Close_L;
StringHash hash_EYE_Close_R;

#ifdef COLLIDERS_FROM_MODEL
void CollidersInitialize(Urho3D::Node* bone)
{
	unsigned int boneNumChildren = bone->GetNumChildren();
	
	if (bone->GetName()[0] == 'C' && boneNumChildren == 1)
	{
		//printf("HUI!\n");
		Urho3D::Node* boneChild = bone->GetChild((unsigned int)0);
		
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
	else
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

void HairInitialize(Urho3D::Node* hair, Urho3D::RigidBody* parentRigitBody, float depth = 0, float length = HAIR_LENGTH) {
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

	hairBody->SetGravityOverride(hair->GetWorldRotation().RotationMatrix()*Vector3::UP*9.8f);
	hairBody->SetLinearDamping(0.9995f);
	hairBody->SetAngularDamping(1.f);
	//hairBody->SetAngularFactor(Vector3(1, 0, 1));

	hairBody->SetRollingFriction(0.f);
	hairBody->SetFriction(0.f);
	

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
		HairInitialize(hair->GetChild(index_zero), hairBody, depth + 1, length);
	}
}

void Heva::Start()
{
	// Set custom window Title & Icon
	ResourceCache* cache = GetSubsystem<ResourceCache>();
	Graphics* graphics = GetSubsystem<Graphics>();
	Image* icon = cache->GetResource<Image>("Textures/heva_icon.png");
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
	time_ = new Time(context_);
	scene_->CreateComponent<Octree>();

	// Create a scene node for the camera, which we will move around
	cameraNode_ = scene_->CreateChild("Camera");
	Urho3D::Camera* camera_ = cameraNode_->CreateComponent<Camera>();
	camera_->SetNearClip(.1f);
	camera_->SetFarClip(10.f);
	camera_->SetFov(30.f);
	//cameraNode_->SetPosition(Vector3(0.0f, 0.4f, -1.3f));
	cameraNode_->SetPosition(Vector3(0.0f, 0.5f, -1.3f));
	cameraNode_->SetRotation(Quaternion(5.0f, 0.0f, 0.0f));

	// Set an initial position for the camera scene node above the plane
	//cameraNode_->SetPosition(Vector3(0.0f, 0.0f, 0.0f));
	Renderer* renderer = GetSubsystem<Renderer>();


	// Set up a viewport to the Renderer subsystem so that the 3D scene can be seen. We need to define the scene and the camera at minimum. Additionally we could configure the viewport screen size and the rendering path (eg. forward / deferred) to use, but now we just use full screen and default render path configured in the engine command line options
	SharedPtr<Viewport> viewport(new Viewport(context_, scene_, cameraNode_->GetComponent<Camera>()));
	renderer->SetViewport(0, viewport);
	renderer->SetDrawShadows(false);
	
	// Chroma key
	#ifdef X11_TRANSPARENT_WINDOW
	//renderer->GetDefaultZone()->SetFogColor(Color::TRANSPARENT);
	renderer->GetDefaultZone()->SetFogColor(Color(0, 0, 0, 0));
	#else
	renderer->GetDefaultZone()->SetFogColor(Color::GREEN);
	//renderer->GetDefaultZone()->SetFogColor(Color::MAGENTA);
	#endif
	renderer->GetDefaultZone()->SetAmbientColor(Color::WHITE);
	
	// FXAA
	//viewport->SetRenderPath(cache->GetResource<XMLFile>("RenderPaths/ForwardOutline.xml"));
	/*SharedPtr<RenderPath> effectRenderPath = viewport->GetRenderPath()->Clone();
	effectRenderPath->Append(cache->GetResource<XMLFile>("PostProcess/FXAA3.xml"));
	viewport->SetRenderPath(effectRenderPath);*/
	
	// Character
	Node* modelNode = scene_->CreateChild("Character");
	modelNode->SetRotation(Quaternion(0.0f, 90.0f, 0.0f));

	modelObject = modelNode->CreateComponent<AnimatedModel>();
	modelObject->SetModel(cache->GetResource<Model>(("Models/" + modelName + ".mdl").c_str()));
	modelObject->ApplyMaterialList(("Models/" + modelName + ".txt").c_str());
	
	boneHips = modelNode->GetChild("J_Bip_C_Hips", true);
	boneHips->SetPosition(Vector3::ZERO);
	boneHipsOffset = boneHips->GetRotation();
	
	
	// Nice pose
	if (femaleLegs) {
		Urho3D::Node* tempNode;
		
		// Spine
		// x = -x
		tempNode = boneHips->GetChild("J_Bip_C_Spine", true);
		tempNode->SetRotation(tempNode->GetRotation() * Quaternion(-1, 0, 0));
		
		tempNode = tempNode->GetChild("J_Bip_C_Chest", true);
		tempNode->SetRotation(tempNode->GetRotation() * Quaternion(-1, 0, 0));
		
		tempNode = tempNode->GetChild("J_Bip_C_Neck", true);
		tempNode->SetRotation(tempNode->GetRotation() * Quaternion(1, 0, 0));
		
		tempNode = tempNode->GetChild("J_Bip_C_Head", true);
		tempNode->SetRotation(tempNode->GetRotation() * Quaternion(1, 0, 0));
		
		// Right Arm
		// x = -x
		// y = -y
		// z =  z
		tempNode = boneHips->GetChild("J_Bip_R_UpperArm", true);
		tempNode->SetRotation(tempNode->GetRotation() * Quaternion(-73.7, 6.1, 13.6));
		
		tempNode = boneHips->GetChild("J_Bip_R_LowerArm", true);
		tempNode->SetRotation(tempNode->GetRotation() * Quaternion(6.0, -10.8, -21.0));
		
		tempNode = boneHips->GetChild("J_Bip_R_Hand", true);
		tempNode->SetRotation(tempNode->GetRotation() * Quaternion(20.0, 10.6, -1.1));
		
		tempNode = boneHips->GetChild("J_Bip_R_Index1", true);
		tempNode->SetRotation(tempNode->GetRotation() * Quaternion(3, 0, 0));
		
		tempNode = boneHips->GetChild("J_Bip_R_Index2", true);
		tempNode->SetRotation(tempNode->GetRotation() * Quaternion(-4, 0, 0));
		
		tempNode = boneHips->GetChild("J_Bip_R_Middle1", true);
		tempNode->SetRotation(tempNode->GetRotation() * Quaternion(-4, 0, 0));
		
		tempNode = boneHips->GetChild("J_Bip_R_Middle2", true);
		tempNode->SetRotation(tempNode->GetRotation() * Quaternion(-4, 0, 0));
		
		tempNode = boneHips->GetChild("J_Bip_R_Ring1", true);
		tempNode->SetRotation(tempNode->GetRotation() * Quaternion(-18, 0, 0));
		
		tempNode = boneHips->GetChild("J_Bip_R_Ring2", true);
		tempNode->SetRotation(tempNode->GetRotation() * Quaternion(-4, 0, 0));
		
		tempNode = boneHips->GetChild("J_Bip_R_Little1", true);
		tempNode->SetRotation(tempNode->GetRotation() * Quaternion(-24, 0, 0));
		
		tempNode = boneHips->GetChild("J_Bip_R_Little2", true);
		tempNode->SetRotation(tempNode->GetRotation() * Quaternion(-11, 0, 0));
		
		tempNode = boneHips->GetChild("J_Bip_R_Thumb1", true);
		tempNode->SetRotation(tempNode->GetRotation() * Quaternion(-15.5, 3.0, 16.6));
		
		tempNode = boneHips->GetChild("J_Bip_R_Thumb2", true);
		tempNode->SetRotation(tempNode->GetRotation() * Quaternion(0, 0, 14));
		
		tempNode = boneHips->GetChild("J_Bip_R_Thumb3", true);
		tempNode->SetRotation(tempNode->GetRotation() * Quaternion(0, 0, 14));
		
		// Left Arm
		// x = -x
		// y =  y
		// z =  z
		tempNode = boneHips->GetChild("J_Bip_L_UpperArm", true);
		tempNode->SetRotation(tempNode->GetRotation() * Quaternion(75.6, 0.0, 21.4));
		
		tempNode = boneHips->GetChild("J_Bip_L_LowerArm", true);
		tempNode->SetRotation(tempNode->GetRotation() * Quaternion(0.0, 20.0, -15.0));
		
		tempNode = boneHips->GetChild("J_Bip_L_Hand", true);
		tempNode->SetRotation(tempNode->GetRotation() * Quaternion(-16.3, -15.0, -4.7));
		
		tempNode = boneHips->GetChild("J_Bip_L_Index1", true);
		tempNode->SetRotation(tempNode->GetRotation() * Quaternion(16, 0, 0));
		
		tempNode = boneHips->GetChild("J_Bip_L_Middle1", true);
		tempNode->SetRotation(tempNode->GetRotation() * Quaternion(10, 0, 0));
		
		tempNode = boneHips->GetChild("J_Bip_L_Ring1", true);
		tempNode->SetRotation(tempNode->GetRotation() * Quaternion(17, 0, 0));
		
		tempNode = boneHips->GetChild("J_Bip_L_Little1", true);
		tempNode->SetRotation(tempNode->GetRotation() * Quaternion(18, 0, 0));
		
		SetRotationRecursive(boneHips->GetChild("J_Bip_L_Index2", true),  Quaternion(43, 0, 0));
		SetRotationRecursive(boneHips->GetChild("J_Bip_L_Middle2", true),  Quaternion(43, 0, 0));
		SetRotationRecursive(boneHips->GetChild("J_Bip_L_Ring2", true),  Quaternion(43, 0, 0));
		SetRotationRecursive(boneHips->GetChild("J_Bip_L_Little2", true),  Quaternion(43, 0, 0));
		
		tempNode = boneHips->GetChild("J_Bip_L_Thumb1", true);
		tempNode->SetRotation(tempNode->GetRotation() * Quaternion(0, 16.3, 9));
		
		tempNode = boneHips->GetChild("J_Bip_L_Thumb2", true);
		tempNode->SetRotation(tempNode->GetRotation() * Quaternion(0, 0, 23));
		
		tempNode = boneHips->GetChild("J_Bip_L_Thumb3", true);
		tempNode->SetRotation(tempNode->GetRotation() * Quaternion(0, 0, 27));
		
		// Legs
		// y = -y
		// z = -z
		tempNode = boneHips->GetChild("J_Bip_R_UpperLeg", true);
		tempNode->SetRotation(tempNode->GetRotation() * Quaternion(0, 16.5, 7.4));
		
		tempNode = boneHips->GetChild("J_Bip_R_LowerLeg", true);
		tempNode->SetRotation(tempNode->GetRotation() * Quaternion(0, 0, 15.8));
		
		tempNode = boneHips->GetChild("J_Bip_R_Foot", true);
		tempNode->SetRotation(tempNode->GetRotation() * Quaternion(0, 0, -7.8));
		
		// y = y
		// z = -z
		tempNode = boneHips->GetChild("J_Bip_L_UpperLeg", true);
		tempNode->SetRotation(tempNode->GetRotation() * Quaternion(0, -17.3, 7.4));
		
		tempNode = boneHips->GetChild("J_Bip_L_LowerLeg", true);
		tempNode->SetRotation(tempNode->GetRotation() * Quaternion(0, 0, 23.8));
		
		tempNode = boneHips->GetChild("J_Bip_L_Foot", true);
		tempNode->SetRotation(tempNode->GetRotation() * Quaternion(0, 0, -14.7));
	}
	else {
		// Arms
		boneArm_L = boneHips->GetChild("J_Bip_L_UpperArm", true);
		boneArm_L->SetRotation(Quaternion(65.0f, 0.0f, 0.0f));
		boneArm_R = boneHips->GetChild("J_Bip_R_UpperArm", true);
		boneArm_R->SetRotation(Quaternion(-65.0f, 0.0f, 0.0f));
		boneArm_L_Offset = boneArm_L->GetRotation();
		boneArm_R_Offset = boneArm_R->GetRotation();
		
		
		// Hands
		SetRotationRecursive(boneHips->GetChild("J_Bip_R_Index1", true),  Quaternion(-7, 0, 0));
		SetRotationRecursive(boneHips->GetChild("J_Bip_L_Index1", true),  Quaternion(7, 0, 0));
		SetRotationRecursive(boneHips->GetChild("J_Bip_R_Middle1", true), Quaternion(-7, 0, 0));
		SetRotationRecursive(boneHips->GetChild("J_Bip_L_Middle1", true), Quaternion(7, 0, 0));
		SetRotationRecursive(boneHips->GetChild("J_Bip_R_Ring1", true),   Quaternion(-7, 0, 0));
		SetRotationRecursive(boneHips->GetChild("J_Bip_L_Ring1", true),   Quaternion(7, 0, 0));
		SetRotationRecursive(boneHips->GetChild("J_Bip_R_Little1", true), Quaternion(-7, 0, 0));
		SetRotationRecursive(boneHips->GetChild("J_Bip_L_Little1", true), Quaternion(7, 0, 0));
		SetRotationRecursive(boneHips->GetChild("J_Bip_R_Thumb1", true),  Quaternion(0, 0, 15));
		SetRotationRecursive(boneHips->GetChild("J_Bip_L_Thumb1", true),  Quaternion(0, 0, 15));
	}
	
	// Neck tracking
	boneNeck = boneHips->GetChild("J_Bip_C_Neck", true);
	boneHead = boneNeck->GetChild("J_Bip_C_Head", true);
	boneHeadOffset = boneHead->GetRotation();
	boneNeckOffset = boneNeck->GetRotation();
	
	boneUpperChest = boneHips->GetChild("J_Bip_C_Chest", true);
	//boneUpperChest = boneHips->GetChild("J_Bip_C_UpperChest", true);
	boneUpperChestOffset = boneUpperChest->GetRotation();
	
	
	// Arms and Legs Tracking
	boneArm_L = boneHips->GetChild("J_Bip_L_LowerArm", true);
	boneArm_R = boneHips->GetChild("J_Bip_R_LowerArm", true);
	boneArm_L_Offset = boneArm_L->GetRotation();
	boneArm_R_Offset = boneArm_R->GetRotation();
	
	boneLeg_L = boneHips->GetChild("J_Bip_L_UpperLeg", true);
	boneLeg_R = boneHips->GetChild("J_Bip_R_UpperLeg", true);
	boneLeg_L_2 = boneLeg_L->GetChild("J_Bip_L_LowerLeg", true);
	boneLeg_R_2 = boneLeg_R->GetChild("J_Bip_R_LowerLeg", true);
	boneLeg_L_3 = boneLeg_L_2->GetChild("J_Bip_L_Foot", true);
	boneLeg_R_3 = boneLeg_R_2->GetChild("J_Bip_R_Foot", true);
	boneLeg_L_Offset = boneLeg_L->GetWorldRotation();
	boneLeg_R_Offset = boneLeg_R->GetWorldRotation();
	boneLeg_L_2_LocalRot = boneLeg_L_2->GetRotation();
	boneLeg_R_2_LocalRot = boneLeg_R_2->GetRotation();
	boneLeg_L_3_Offset = boneLeg_L_3->GetWorldRotation();
	boneLeg_R_3_Offset = boneLeg_R_3->GetWorldRotation();
	
	boneLegLength = 0.5 * (
		boneLeg_L->GetWorldPosition() - boneLeg_L_2->GetWorldPosition()
		+
		boneLeg_L_2->GetWorldPosition() - boneLeg_L_3->GetWorldPosition()).Length();
	
	boneLegDistance = 0.5 * (boneLeg_L->GetPosition() - boneLeg_R->GetPosition()).Length();
	
	// hair physics
	#ifdef DEBUG_GEOMETRY
	scene_->CreateComponent<DebugRenderer>();
	#endif
	PhysicsWorld* physicsWorld_ = scene_->CreateComponent<PhysicsWorld>();
	
	RigidBody* headRigidBody = boneHead->CreateComponent<RigidBody>();
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
	Node* boneChest = boneHips->GetChild("J_Bip_C_UpperChest", true);
	RigidBody* chestRigidBody = boneChest->CreateComponent<RigidBody>();
	CollisionShape* chestShape = boneChest->CreateComponent<CollisionShape>();
	chestShape->SetCapsule(0.11469f*1.8, 0.11469f*3, Vector3(-0.012381, -0.01, 0), Quaternion(90, 0, 0));
	chestRigidBody->SetLinearDamping(1.f);
	chestRigidBody->SetAngularDamping(1.f);
	#endif
	
	const Urho3D::Vector<Urho3D::SharedPtr<Urho3D::Node>> bonesHead = boneHead->GetChildren();
	for (int i = 0; i < bonesHead.Size(); i++) {
		if (bonesHead.At(i)->GetName()[0] == 'H') {
    			//using namespace NodeCollision;
			Urho3D::Node* hair = (Urho3D::Node*)(bonesHead.At(i));
			
			HairInitialize(hair, headRigidBody);
		}
	}
	
	// Face string hash
	hash_MTH_A = StringHash("MTH_A");
	hash_MTH_E = StringHash("MTH_E");
	hash_MTH_U = StringHash("MTH_U");
	hash_MTH_Fun = StringHash("MTH_Fun");
	hash_BRW_Fun = StringHash("BRW_Fun");
	hash_BRW_Angry = StringHash("BRW_Angry");
	hash_EYE_Angry = StringHash("EYE_Angry");
	hash_EYE_Close_L = StringHash("EYE_Close_L");
	hash_EYE_Close_R = StringHash("EYE_Close_R");
	
	// dlib
	//engine_->SetMaxFps(24);
	
	#ifdef WEBCAM_ENABLE
	dlib_data = (dlib_thread_data*)calloc(1, sizeof(dlib_thread_data));
	
	//dlib_data->dlib_thread_cond = PTHREAD_COND_INITIALIZER;
	//dlib_data->dlib_thread_cond = (pthread_cond_t*)malloc(sizeof(pthread_cond_t));
	//pthread_cond_init(dlib_data->dlib_thread_cond, NULL);
	
	pthread_cond_init(&dlib_data->dlib_thread_cond, NULL);
	
	dlib_data->dlib_thread_active = 1;
	dlib_data->dlib_thread_ready = 0;
	
	pthread_create(&dlib_thread, NULL, dlib_thread_function, dlib_data);
	#endif
	
	#ifdef VMC_OSC_SENDER
	if (vmc_osc_udp_enabled) {
		vmc_osc_sender_data = (vmc_osc_data*)calloc(1, sizeof(vmc_osc_data));
		vmc_osc_sender_data->udp_addr = vmc_osc_udp_addr.c_str();
		vmc_osc_sender_data->udp_port = vmc_osc_udp_port;
		vmc_osc_sender_data->vmc_osc_sender_thread_active = 1;
		
		//dlib_data->dlib_thread_cond = PTHREAD_COND_INITIALIZER;
		//vmc_osc_sender_data->cond = (pthread_cond_t*)malloc(sizeof(pthread_cond_t));
		//pthread_cond_init(vmc_osc_sender_data->cond, NULL);
		pthread_cond_init(&vmc_osc_sender_data->cond, NULL);
		
		pthread_create(&vmc_osc_sender_thread, NULL, vmc_osc_sender_thread_function, vmc_osc_sender_data);
	}
	#endif
}

void Heva::Stop()
{
	#ifdef WEBCAM_ENABLE
	dlib_data->dlib_thread_active = 0;
	//dlib_thread.join();
	pthread_join(dlib_thread, NULL);
	#endif
	
	#ifdef VMC_OSC_SENDER
	if (vmc_osc_udp_enabled) {
		vmc_osc_sender_data->vmc_osc_sender_thread_active = 0;
		pthread_cond_signal(&vmc_osc_sender_data->cond);
		//vmc_osc_sender_thread.join();
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
#define KEY_E 26
#define KEY_R 27

void Heva::HandleKeyUp(int key)
{
#else

void Heva::HandleKeyUp(StringHash eventType, VariantMap& eventData)
{
	using namespace KeyUp;
	int key = eventData[P_KEY].GetInt();

#endif

	// Close console (if open) or exit when ESC is pressed
	if (key == KEY_ESCAPE) {
		#ifndef X11_TRANSPARENT_WINDOW
		engine_->Exit();
		#endif
	}
	if (key == KEY_1) {
		//cameraNode_->SetPosition(Vector3(0.0f, 0.5f, -1.3f));
		
		//printf("%f\n", boneHead->GetWorldPosition().y_);
		// boneHead pose = 0.443506
		
		//cameraNode_->SetPosition(Vector3(0.0f, 0.5f + boneHead->GetWorldPosition().y_ - 0.443506, -1.3f));
		
		//printf("%f\n", boneHead->GetWorldPosition().y_ - boneLeg_R_3->GetWorldPosition().y_);
		
		cameraNode_->SetPosition(Vector3(0.0f, 0.5f + boneHead->GetWorldPosition().y_ - 0.443506, -1.3f / 1.208164 * (boneHead->GetWorldPosition().y_ - boneLeg_R_3->GetWorldPosition().y_) ));
		
		cameraNode_->SetRotation(Quaternion(5.0f, 0.0f, 0.0f));
		Graphics* graphics = GetSubsystem<Graphics>();
		graphics->SetMode(550, 800);
		headAngleOffset = Quaternion();
	}
	if (key == KEY_2) {
		cameraNode_->SetPosition(Vector3(
			0.0f, 
			(0.5f + boneHead->GetWorldPosition().y_ - 0.443506) * 1.15, 
			-1.3f / 1.208164 * (boneHead->GetWorldPosition().y_ - boneLeg_R_3->GetWorldPosition().y_) * 1.2 
		));
		
		cameraNode_->SetRotation(Quaternion(5.0f, 0.0f, 0.0f));
		Graphics* graphics = GetSubsystem<Graphics>();
		graphics->SetMode(550, 800);
		headAngleOffset = Quaternion();
	}
	if (key == KEY_3) {
		cameraNode_->SetPosition(Vector3(0, 0.2, -3.0));
		cameraNode_->SetRotation(Quaternion(5.0f, 0.0f, 0.0f));
		Graphics* graphics = GetSubsystem<Graphics>();
		graphics->SetMode(550, 1280);
		headAngleOffset = Quaternion(0, 0, -3);
	}
	if (key == KEY_4) {
		cameraNode_->SetPosition(Vector3(0, 0.2, -3.0));
		cameraNode_->SetRotation(Quaternion(5.0f, 0.0f, 0.0f));
		Graphics* graphics = GetSubsystem<Graphics>();
		graphics->SetMode(650, 1440);
		headAngleOffset = Quaternion(0, 0, -3);
	}
	if (key == KEY_5 || key == KEY_6) {
		Quaternion rotationOffset = Quaternion(0, 12 * (key == KEY_6 ? 1 : -1), 0);
		cameraNode_->SetPosition(Matrix3x4(Vector3::ZERO, rotationOffset, 1) * Vector3(0.0f, 0.5f, -1.3f));
		cameraNode_->SetRotation(rotationOffset * Quaternion(5.0f, 0.0f, 0.0f));
	}
	// Reset adapt strength
	#ifdef WEBCAM_ENABLE
	if (key == KEY_R) {
		dlib_data->dlib_thread_signal = DLIB_THREAD_SIGNAL_RESET;
	}
	#endif
}


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
			time_->Sleep(1000/30);
		}
	}
	
	face_recognition_data* face_data = &dlib_data->face_data;
	
	// Body and Head
	headAngle = Quaternion(face_data->rotation2*.5, face_data->rotation3*.4, face_data->rotation1*.5) * headAngleOffset;
	bodyAngle1 = -face_data->translation1*.4 *180/3.1415;
	Urho3D::Quaternion bodyAngle = Quaternion(
		bodyAngle1, 
		face_data->rotation3*.2, 
		face_data->translation3*.5 *180/3.1415
	);
	
	boneHead->SetRotation(boneHeadOffset*headAngle * bodyAngle*(1/bodyAngle.LengthSquared()));
	boneHips->SetRotation(boneHipsOffset*bodyAngle);
	
	boneHipsPosition = Vector3(0, face_data->translation2*.2, -face_data->rotation3*.0005);
	
	// Face
	modelObject->SetMorphWeight(hash_MTH_A, face_data->MTH_A);
	modelObject->SetMorphWeight(hash_MTH_U, face_data->MTH_U);
	modelObject->SetMorphWeight(hash_MTH_Fun, face_data->MTH_Fun);
	modelObject->SetMorphWeight(hash_MTH_E, face_data->MTH_E);
	modelObject->SetMorphWeight(hash_BRW_Fun, face_data->BRW_Fun);
	modelObject->SetMorphWeight(hash_BRW_Angry, face_data->BRW_Angry);
	modelObject->SetMorphWeight(hash_EYE_Angry, face_data->BRW_Angry);
	modelObject->SetMorphWeight(hash_EYE_Close_L, face_data->EYE_Close_L);
	modelObject->SetMorphWeight(hash_EYE_Close_R, face_data->EYE_Close_R);
	#endif
	
	// Idle
	float angleSin = sin(time_->GetElapsedTime() * 2);
	boneHips->SetPosition(boneHipsPosition + Vector3(0, angleSin * .002, 0));
	Quaternion angleMain = Quaternion(0, 0, angleSin * .65);
	Quaternion angleReverse = Quaternion(0, 0, angleSin * -.65);
	
	boneUpperChest->SetRotation(boneUpperChestOffset * angleMain);
	boneNeck->SetRotation(boneNeckOffset * headAngle * angleReverse);
	
	// Arms and Legs Tracking
	if (femaleLegs) {
		float angleLegs_L = std::fmax(-10.f, std::fmin(10.f, 5*Urho3D::Asin((boneHips->GetPosition().y_ + boneLegDistance * Urho3D::Sin(bodyAngle1)) / boneLegLength)));
		float angleLegs_R = std::fmax(-10.f, std::fmin(10.f, 5*Urho3D::Asin((boneHips->GetPosition().y_ - boneLegDistance * Urho3D::Sin(bodyAngle1)) / boneLegLength)));
		
		boneArm_L->SetRotation(boneArm_L_Offset * angleMain * Quaternion(0, 0, angleLegs_L * .5));
		boneArm_R->SetRotation(boneArm_R_Offset * angleMain * Quaternion(0, 0, angleLegs_R * .5));
		
		boneLeg_L->SetWorldRotation(boneLeg_L_Offset);
		boneLeg_R->SetWorldRotation(boneLeg_R_Offset);
		boneLeg_L->SetRotation(boneLeg_L->GetRotation() * Quaternion(0, 0, angleLegs_L));
		boneLeg_R->SetRotation(boneLeg_R->GetRotation() * Quaternion(0, 0, angleLegs_R));
		boneLeg_L_2->SetRotation(boneLeg_L_2_LocalRot * Quaternion(0, 0, -angleLegs_L*1.7));
		boneLeg_R_2->SetRotation(boneLeg_R_2_LocalRot * Quaternion(0, 0, -angleLegs_R*1.7));
		boneLeg_L_3->SetWorldRotation(boneLeg_L_3_Offset);
		boneLeg_R_3->SetWorldRotation(boneLeg_R_3_Offset);
		
		
		#ifdef VMC_OSC_SENDER
		if (vmc_osc_udp_enabled) {
			vmc_osc_sender_data->boneLeg_L = Quaternion(0, 0, angleLegs_L);
			vmc_osc_sender_data->boneLeg_R = Quaternion(0, 0, angleLegs_R);
			vmc_osc_sender_data->boneLeg_L_2 = Quaternion(0, 0, -angleLegs_L*1.7);
			vmc_osc_sender_data->boneLeg_R_2 = Quaternion(0, 0, -angleLegs_R*1.7);
		}
		#endif
	}
	else {
		boneLeg_L->SetWorldRotation(boneLeg_L_Offset);
		boneLeg_R->SetWorldRotation(boneLeg_R_Offset);
	}
	
	#ifdef VMC_OSC_SENDER
	if (vmc_osc_udp_enabled) {
		/*vmc_osc_sender_data->headRot = boneHead->GetRotation() * boneHeadOffset.Inverse();
		vmc_osc_sender_data->neckRot = boneNeck->GetRotation() * boneNeckOffset.Inverse();
		vmc_osc_sender_data->hipsRot = boneHips->GetRotation() * boneHipsOffset.Inverse();*/
		/*vmc_osc_sender_data->headRot = boneHead->GetRotation() * boneHeadOffset*(1/boneHeadOffset.LengthSquared());
		vmc_osc_sender_data->neckRot = boneNeck->GetRotation() * boneNeckOffset*(1/boneNeckOffset.LengthSquared());
		vmc_osc_sender_data->hipsRot = boneHips->GetRotation() * boneHipsOffset*(1/boneHipsOffset.LengthSquared());*/
		vmc_osc_sender_data->headRot = headAngle;
		vmc_osc_sender_data->neckRot = headAngle;
		vmc_osc_sender_data->hipsRot = bodyAngle;
		vmc_osc_sender_data->hipsLoc = boneHips->GetPosition();
		vmc_osc_sender_data->MTH_A = face_data->MTH_A;
		vmc_osc_sender_data->MTH_U = face_data->MTH_U;
		vmc_osc_sender_data->MTH_Fun = face_data->MTH_Fun;
		vmc_osc_sender_data->MTH_E = face_data->MTH_E;
		vmc_osc_sender_data->BRW_Fun = face_data->BRW_Fun;
		vmc_osc_sender_data->BRW_Angry = face_data->BRW_Angry;
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
