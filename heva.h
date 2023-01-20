//
// Created by scaled
//

//#define WEBCAM_ENABLE
//#define DEBUG_GEOMETRY
//#define X11_TRANSPARENT_WINDOW
#define COLLIDERS_FROM_MODEL

#pragma once

#include <Urho3D/Engine/Application.h>

namespace Urho3D
{

class Node;
class Scene;
class Sprite;
class Engine;
class Viewport;

}

using namespace Urho3D;

#ifdef X11_TRANSPARENT_WINDOW

class Heva : public Urho3D::Object
{
	URHO3D_OBJECT(Heva, Urho3D::Object);

public:
	/// Construct.
	Heva(Context* context, void* window);

	void Setup();
	void Start();
	void Stop();
	void HandleUpdate();
	void HandleKeyUp(int key);

private:
	Urho3D::Scene* scene_;
  	Urho3D::Engine* engine_;
  	Urho3D::Viewport* viewport_;
	void* window_ = NULL;
};

#else

class Heva : public Application
{
	// Enable type information.
	URHO3D_OBJECT(Heva, Application);

public:
	/// Construct.
	Heva(Context* context);

	void Setup();
	void Start();
	void Stop();

private:
	Urho3D::Scene* scene_;
	Urho3D::Node* cameraNode_;
	void HandleUpdate(StringHash eventType, VariantMap& eventData);
	void HandleKeyUp(StringHash eventType, VariantMap& eventData);
	void HandlePostRenderUpdate(StringHash eventType, VariantMap& eventData);
};

#endif
