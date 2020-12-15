template<typename T, glm::precision P>
FPSCamera<T, P>::FPSCamera(T fovy, T aspect, T nnear, T nfar) : mWorld(), mMovementSpeed(1), mMouseSensitivity(1), mFov(fovy), mAspect(aspect), mNear(nnear), mFar(nfar), mProjection(), mProjectionInverse(), mRotation(glm::tvec2<T, P>(0.0f)), mMousePosition(glm::tvec2<T, P>(0.0f))
{
	SetProjection(fovy, aspect, nnear, nfar);
}

template<typename T, glm::precision P>
FPSCamera<T, P>::~FPSCamera()
{
}

template<typename T, glm::precision P>
void FPSCamera<T, P>::SetProjection(T fovy, T aspect, T nnear, T nfar)
{
	mFov = fovy;
	mAspect = aspect;
	mNear = nnear;
	mFar = nfar;
	mProjection = glm::perspective(fovy, aspect, nnear, nfar);
	mProjectionInverse = glm::inverse(mProjection);
}


template<typename T, glm::precision P>
void FPSCamera<T, P>::SetFov(T fovy)
{
	SetProjection(fovy, mAspect, mNear, mFar);
}

template<typename T, glm::precision P>
T FPSCamera<T, P>::GetFov()
{
	return mFov;
}

template<typename T, glm::precision P>
void FPSCamera<T, P>::SetAspect(T a)
{
	SetProjection(mFov, a, mNear, mFar);
}

template<typename T, glm::precision P>
T FPSCamera<T, P>::GetAspect()
{
	return mAspect;
}


template<typename T, glm::precision P>
void FPSCamera<T, P>::Update(std::chrono::microseconds deltaTime, InputHandler &ih, bool ignoreKeyEvents, bool ignoreMouseEvents)
{
	glm::tvec2<T, P> newMousePosition = glm::tvec2<T, P>(ih.GetMousePosition().x, ih.GetMousePosition().y);
	glm::tvec2<T, P> mouse_diff = newMousePosition - mMousePosition;
	mouse_diff.y = -mouse_diff.y;
	mMousePosition = newMousePosition;
	mouse_diff *= mMouseSensitivity;

	if (!ih.IsMouseCapturedByUI() && !ignoreMouseEvents && (ih.GetMouseState(GLFW_MOUSE_BUTTON_LEFT) & PRESSED)) {
		mRotation.x -= mouse_diff.x;
		mRotation.y += mouse_diff.y;
		mWorld.SetRotateX(mRotation.y);
		mWorld.RotateY(mRotation.x);
	}

	T movementModifier = ((ih.GetKeycodeState(GLFW_MOD_SHIFT) & PRESSED)) ? 0.25f : ((ih.GetKeycodeState(GLFW_MOD_CONTROL) & PRESSED)) ? 4.0f : 1.0f;
	auto const deltaTime_s = std::chrono::duration<T>(deltaTime);
	T movement = movementModifier * deltaTime_s.count() * mMovementSpeed;

	T move = 0.0f, strafe = 0.0f, levitate = 0.0f;
	if (!ih.IsKeyboardCapturedByUI() && !ignoreKeyEvents) {
		if ((ih.GetKeycodeState(GLFW_KEY_W) & PRESSED)) move += movement;
		if ((ih.GetKeycodeState(GLFW_KEY_S) & PRESSED)) move -= movement;
		if ((ih.GetKeycodeState(GLFW_KEY_A) & PRESSED)) strafe -= movement;
		if ((ih.GetKeycodeState(GLFW_KEY_D) & PRESSED)) strafe += movement;
		if ((ih.GetKeycodeState(GLFW_KEY_Q) & PRESSED)) levitate -= movement;
		if ((ih.GetKeycodeState(GLFW_KEY_E) & PRESSED)) levitate += movement;
	}

	mWorld.Translate(mWorld.GetFront() * move);
	mWorld.Translate(mWorld.GetRight() * strafe);
	mWorld.Translate(mWorld.GetUp() * levitate);
}

template<typename T, glm::precision P>
glm::tmat4x4<T, P> FPSCamera<T, P>::GetViewToWorldMatrix()
{
	return mWorld.GetMatrix();
}

template<typename T, glm::precision P>
glm::tmat4x4<T, P> FPSCamera<T, P>::GetWorldToViewMatrix()
{
	return mWorld.GetMatrixInverse();
}

template<typename T, glm::precision P>
glm::tmat4x4<T, P> FPSCamera<T, P>::GetClipToWorldMatrix()
{
	return GetViewToWorldMatrix() * mProjectionInverse;
}

template<typename T, glm::precision P>
glm::tmat4x4<T, P> FPSCamera<T, P>::GetWorldToClipMatrix()
{
	return mProjection * GetWorldToViewMatrix();
}

template<typename T, glm::precision P>
glm::tmat4x4<T, P> FPSCamera<T, P>::GetClipToViewMatrix()
{
	return mProjectionInverse;
}

template<typename T, glm::precision P>
glm::tmat4x4<T, P> FPSCamera<T, P>::GetViewToClipMatrix()
{
	return mProjection;
}

template<typename T, glm::precision P>
glm::tvec3<T, P> FPSCamera<T, P>::GetClipToWorld(glm::tvec3<T, P> xyw)
{
	glm::tvec4<T, P> vv = glm::tvec4<T, P>(GetClipToView(xyw), static_cast<T>(1));
	glm::tvec3<T, P> wv = mWorld.GetMatrix() * vv;
	return wv;
}

template<typename T, glm::precision P>
glm::tvec3<T, P> FPSCamera<T, P>::GetClipToView(glm::tvec3<T, P> xyw)
{
	return xyw * glm::tvec3<T, P>(mProjectionInverse[0][0], mProjectionInverse[1][1], static_cast<T>(-1));
}


// new 
template<typename T, glm::precision P>
glm::tvec3<T, P> FPSCamera<T, P>::GetMouseRay(T s, T t)
{
	T theta = mFov;
	T h = glm::tan(theta / 2);
	T viewport_height = 2.0 * h;
	T viewport_width = mAspect * viewport_height;

	glm::tvec3<T, P> forward = glm::normalize(mWorld.GetFront()) * mNear;

	glm::tvec3<T, P> horizontal = mNear * viewport_width * glm::normalize(mWorld.GetRight());

	glm::tvec3<T, P> vertical = mNear * viewport_height * glm::normalize(mWorld.GetDown());

	// from camera origin to world position of camera == ray direction
	return glm::normalize(forward + s * horizontal + t * vertical);
}