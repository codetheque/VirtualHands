/******************************************************************************\
*  Aline Czarnobai															  *
*  Virtual Hands - A Leap Motion Project, Fall 2013							  *
*																			  *
*  This project uses Leap Motion SDK, and JUCE toolkit						  *
\******************************************************************************/

#include "GL/glew.h"
#include <gl/freeglut.h>
#include "../JuceLibraryCode/JuceHeader.h"
#include "Leap.h"
#include "LeapUtilGL.h"
#include <cctype>

class FingerVisualizerWindow;
class OpenGLCanvas;

bool m_useStabelizedPos = false;
float gTipRadius  = 4.0f;

Leap::Vector sphereInitialPos(0.1f, -1.6f, -0.6f);
Leap::Vector spherePos = sphereInitialPos;
Leap::Vector desiredPos = spherePos;

float m_fFrameScale = 0.0075f;;
float sphereRadius = 50 * m_fFrameScale;
float shadowsYPos = sphereInitialPos.y - sphereRadius;
float handY = 0;

Leap::Matrix createTransform(Leap::Vector& forwardVec, Leap::Vector& translation)
{
	//OpenGl Look at
	Leap::Vector z(-forwardVec);
	Leap::Vector y(0,1,0);
	Leap::Vector x(0,0,0);

	x = y.cross(z);
	y = z.cross(x);

	return Leap::Matrix(x, y, z, translation);
}

// To float vector argument passed to GL functions
struct GLColor 
{
	GLColor() : r(1), g(1), b(1), a(1) {}

	GLColor(float _r, float _g, float _b, float _a=1)
		: r(_r), g(_g), b(_b), a(_a)
	{}

	explicit GLColor(const Colour& juceColor) 
		: r(juceColor.getFloatRed()), 
		g(juceColor.getFloatGreen()),
		b(juceColor.getFloatBlue()),
		a(juceColor.getFloatAlpha())
	{}

	operator const GLfloat*() const { return &r; }

	GLfloat r, g, b, a; 
};

//==============================================================================
class FingerVisualizerApplication  : public JUCEApplication
{
public:
	//==============================================================================
	FingerVisualizerApplication()
	{
	}

	~FingerVisualizerApplication()
	{
	}

	//==============================================================================
	void initialise (const String& commandLine);

	void shutdown()
	{ 
	}

	//==============================================================================
	void systemRequestedQuit()
	{
		quit();
	}

	//==============================================================================
	const String getApplicationName()
	{
		return "Leap Motion Project";
	}

	const String getApplicationVersion()
	{
		return ProjectInfo::versionString;
	}

	bool moreThanOneInstanceAllowed()
	{
		return true;
	}

	void anotherInstanceStarted (const String& commandLine)
	{
		(void)commandLine;        
	}

	static Leap::Controller& getController() 
	{
		static Leap::Controller s_controller;

		return s_controller;
	}

private:
	ScopedPointer<FingerVisualizerWindow>  m_pMainWindow; 
};

//==============================================================================
class OpenGLCanvas  : public Component,
	public OpenGLRenderer,
	Leap::Listener
{
public:
	OpenGLCanvas()
		: Component("OpenGLCanvas")
	{
		m_openGLContext.setRenderer (this);
		m_openGLContext.setComponentPaintingEnabled (true);
		m_openGLContext.attachTo (*this);
		setBounds(0, 0, 1024, 768);

		m_fLastUpdateTimeSeconds = Time::highResolutionTicksToSeconds(Time::getHighResolutionTicks());
		m_fLastRenderTimeSeconds = m_fLastUpdateTimeSeconds;

		FingerVisualizerApplication::getController().addListener(*this);

		initColors();

		resetCamera();

		setWantsKeyboardFocus(true);

		m_bPaused = false;
		m_bShowDemo = true;

		m_mtxFrameTransform.origin = Leap::Vector(0.0f, -2.0f, 0.5f);
		m_fPointableRadius = 0.05f;
		m_bShowHelp = false;

		m_strHelp = "ESC - quit\n"
			"h - Toggle help and frame rate display\n"
			"p - Toggle pause\n"
			"Mouse Drag  - Rotate camera\n"
			"Mouse Wheel - Zoom camera\n"
			"Arrow Keys  - Rotate camera\n"
			"Space       - Reset camera";

		m_strPrompt = "Press 'd/D' for demo\n"
			"Press 'h/H' for help";
	}

	~OpenGLCanvas()
	{
		FingerVisualizerApplication::getController().removeListener(*this);
		m_openGLContext.detach();
	}

	void newOpenGLContextCreated()
	{
		glEnable(GL_BLEND);
		glEnable(GL_TEXTURE_2D);
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
		glFrontFace(GL_CCW);

		glEnable(GL_DEPTH_TEST);
		glDepthMask(true);
		glDepthFunc(GL_LESS);

		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
		glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);
		glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
		glEnable(GL_COLOR_MATERIAL);
		glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);
		glShadeModel(GL_SMOOTH);

		glEnable(GL_LIGHTING);

		m_fixedFont = Font("Courier New", 24, Font::plain);
	}

	void openGLContextClosing()
	{
	}

	bool keyPressed(const juce::KeyPress& keyPress)
	{
		int iKeyCode = toupper(keyPress.getKeyCode());

		if (iKeyCode == juce::KeyPress::escapeKey)
		{
			JUCEApplication::quit();
			return true;
		}

		if (iKeyCode == juce::KeyPress::upKey)
		{
			m_camera.RotateOrbit(0, 0, LeapUtil::kfHalfPi * -0.05f);
			return true;
		}

		if (iKeyCode == juce::KeyPress::downKey)
		{
			m_camera.RotateOrbit(0, 0, LeapUtil::kfHalfPi * 0.05f);
			return true;
		}

		if (iKeyCode == juce::KeyPress::leftKey)
		{
			m_camera.RotateOrbit(0, LeapUtil::kfHalfPi * -0.05f, 0);
			return true;
		}

		if (iKeyCode == juce::KeyPress::rightKey)
		{
			m_camera.RotateOrbit(0, LeapUtil::kfHalfPi * 0.05f, 0);
			return true;
		}
		switch(iKeyCode)
		{
		case ' ':
			resetCamera();
			spherePos = sphereInitialPos;
			desiredPos = spherePos;
			break;
		case 'H':
			m_bShowHelp = !m_bShowHelp;
			break;
		case 'D':
			m_bShowDemo = !m_bShowDemo;
			break;
		case 'P':
			m_bPaused = !m_bPaused;
			break;
		case 'M':
			m_useStabelizedPos = !m_useStabelizedPos;
			break;
		default:
			return false;
		}

		return true;
	}

	void mouseDown (const MouseEvent& e)
	{
		m_camera.OnMouseDown(LeapUtil::FromVector2(e.getPosition()));
	}

	void mouseDrag (const MouseEvent& e)
	{
		m_camera.OnMouseMoveOrbit(LeapUtil::FromVector2(e.getPosition()));
		m_openGLContext.triggerRepaint();
	}

	void mouseWheelMove (const MouseEvent& e,
		const MouseWheelDetails& wheel)
	{
		(void)e;
		m_camera.OnMouseWheel(wheel.deltaY);
		m_openGLContext.triggerRepaint();
	}

	void resized()
	{
	}

	void paint(Graphics&)
	{
	}

	void renderOpenGL2D() 
	{
		LeapUtilGL::GLAttribScope attribScope(GL_ENABLE_BIT);

		glDisable(GL_CULL_FACE);

		ScopedPointer<LowLevelGraphicsContext> glRenderer (createOpenGLGraphicsContext (m_openGLContext, getWidth(), getHeight()));

		if (glRenderer != nullptr)
		{
			Graphics g(*glRenderer.get());

			int iMargin   = 10;
			int iFontSize = static_cast<int>(m_fixedFont.getHeight());
			int iLineStep = iFontSize + (iFontSize >> 2);
			int iBaseLine = 20;
			Font origFont = g.getCurrentFont();

			const juce::Rectangle<int>& rectBounds = getBounds();

			if (m_bShowHelp)
			{
				g.setColour(Colours::seagreen);
				g.setFont(static_cast<float>(iFontSize));

				if (!m_bPaused)
				{
					g.drawSingleLineText(m_strUpdateFPS, iMargin, iBaseLine);
				}

				g.drawSingleLineText(m_strRenderFPS, iMargin, iBaseLine + iLineStep);

				//g.setFont(m_fixedFont);
				//g.setColour(Colours::darkorange);

				g.drawMultiLineText(m_strHelp,
					iMargin,
					iBaseLine + iLineStep * 3,
					rectBounds.getWidth() - iMargin*2);
			}	 
			g.setFont(origFont);
			g.setFont(static_cast<float>(iFontSize));

			g.setColour(Colours::hotpink);
			g.drawMultiLineText(m_strPrompt,
				iMargin,
				rectBounds.getBottom() - (iFontSize + iFontSize + iLineStep),
				rectBounds.getWidth()/4);
		}
	}

	//
	// Calculations that should only be done once per leap data frame but may be drawn many times should go here.
	//
	void update(Leap::Frame frame)
	{
		ScopedLock sceneLock(m_renderMutex);

		double curSysTimeSeconds = Time::highResolutionTicksToSeconds(Time::getHighResolutionTicks());

		float deltaTimeSeconds = static_cast<float>(curSysTimeSeconds - m_fLastUpdateTimeSeconds);

		m_fLastUpdateTimeSeconds = curSysTimeSeconds;
		float fUpdateDT = m_avgUpdateDeltaTime.AddSample(deltaTimeSeconds);
		float fUpdateFPS = (fUpdateDT > 0) ? 1.0f/fUpdateDT : 0.0f;
		m_strUpdateFPS = String::formatted("UpdateFPS: %4.2f", fUpdateFPS);
	}

	// Affects model view matrix. (Needs to be inside a glPush/glPop matrix block)
	void setupScene()
	{
		OpenGLHelpers::clear (Colours::skyblue.withAlpha (1.0f));
		m_camera.SetAspectRatio(getWidth() / static_cast<float>(getHeight()));

		m_camera.SetupGLProjection();

		m_camera.ResetGLView();

		// left, high, near - corner light
		LeapUtilGL::GLVector4fv vLight0Position(-3.0f, 3.0f, -3.0f, 1.0f);
		// right, near - side light
		LeapUtilGL::GLVector4fv vLight1Position(3.0f, 0.0f, -1.5f, 1.0f);
		// near - head light
		LeapUtilGL::GLVector4fv vLight2Position(0.0f, 0.0f,  -3.0f, 1.0f);

		///Turns off the depth test every frame when calling paint.
		glEnable(GL_DEPTH_TEST);
		glDepthMask(true);
		glDepthFunc(GL_LESS);

		glEnable(GL_BLEND);
		glEnable(GL_TEXTURE_2D);

		glLightModelfv(GL_LIGHT_MODEL_AMBIENT, GLColor(Colours::darkgrey));

		glLightfv(GL_LIGHT0, GL_POSITION, vLight0Position);
		glLightfv(GL_LIGHT0, GL_DIFFUSE, GLColor(Colour(0.5f, 0.40f, 0.40f, 1.0f)));
		glLightfv(GL_LIGHT0, GL_AMBIENT, GLColor(Colours::black));

		glLightfv(GL_LIGHT1, GL_POSITION, vLight1Position);
		glLightfv(GL_LIGHT1, GL_DIFFUSE, GLColor(Colour(0.0f, 0.0f, 0.25f, 1.0f)));
		glLightfv(GL_LIGHT1, GL_AMBIENT, GLColor(Colours::black));

		glLightfv(GL_LIGHT2, GL_POSITION, vLight2Position);
		glLightfv(GL_LIGHT2, GL_DIFFUSE, GLColor(Colour(0.15f, 0.15f, 0.15f, 1.0f)));
		glLightfv(GL_LIGHT2, GL_AMBIENT, GLColor(Colours::black));

		glEnable(GL_LIGHT0);
		//glEnable(GL_LIGHT1);
		//glEnable(GL_LIGHT2);

		m_camera.SetupGLView();
	}

	void updateDemo(Leap::Frame frame, bool isShadow = false)
	{
		//glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		if (isShadow)
		{
			glEnable (GL_BLEND);
			glColor4f(0,0,0,0.2f);
			glPushMatrix();
			glTranslatef(spherePos.x, shadowsYPos, spherePos.z);
			glScalef(sphereRadius * 1.2f, sphereRadius * 0.001f, sphereRadius * 1.2f);
			LeapUtilGL::drawSphere(LeapUtilGL::kStyle_Solid);
			glPopMatrix();
			glDisable (GL_BLEND);
		}

		glColor3f(0.3f, 0.6f,0.1f);

		glPushMatrix();
		glTranslatef(spherePos.x, spherePos.y, spherePos.z);
		glScalef(sphereRadius, sphereRadius, sphereRadius);
		LeapUtilGL::drawSphere(LeapUtilGL::kStyle_Solid);
		glPopMatrix();

		using namespace Leap;
		if (!frame.hands().isEmpty()) 
		{
			for (int handCount = 0; handCount < frame.hands().count(); ++handCount)
			{
				const Hand hand = frame.hands()[handCount];
				// Check if the hand has any fingers
				const FingerList& fingers = hand.fingers();
				if (!fingers.isEmpty()) 
				{
					for (int fingerCount = 0; fingerCount < fingers.count(); ++fingerCount)
					{
						Finger finger = fingers[fingerCount];

						Leap::Vector stabelizedPos = m_mtxFrameTransform.transformPoint(finger.stabilizedTipPosition() * m_fFrameScale);
						Leap::Vector tipPos = m_mtxFrameTransform.transformPoint(finger.tipPosition() * m_fFrameScale);
						Leap::Vector tipVelocity = finger.tipVelocity();

						float radius = (gTipRadius* m_fFrameScale) + sphereRadius;
						Leap::Vector distance = tipPos - spherePos;
						float length = distance.magnitude();

						//Collision
						if (length <= radius)
						{
							tipVelocity.y = 0;
							spherePos.y = sphereInitialPos.y;
							desiredPos.x = spherePos.x + (tipVelocity.x * m_fFrameScale);
							desiredPos.z = spherePos.z + (tipVelocity.z * m_fFrameScale);
						}
					}
				}
				spherePos = spherePos + (desiredPos - spherePos) * 0.1f;
			}
		}

	}

	// Data should be drawn here but no heavy calculations done.
	// Any major calculations that only need to be updated per leap data frame
	// should be handled in update and cached in members.
	void renderOpenGL()
	{
		{
			MessageManagerLock mm (Thread::getCurrentThread());
			if (! mm.lockWasGained())
				return;
		}

		Leap::Frame frame = m_lastFrame;

		double  curSysTimeSeconds = Time::highResolutionTicksToSeconds(Time::getHighResolutionTicks());
		float   fRenderDT = static_cast<float>(curSysTimeSeconds - m_fLastRenderTimeSeconds);
		fRenderDT = m_avgRenderDeltaTime.AddSample(fRenderDT);
		m_fLastRenderTimeSeconds = curSysTimeSeconds;

		float fRenderFPS = (fRenderDT > 0) ? 1.0f/fRenderDT : 0.0f;

		m_strRenderFPS = String::formatted("RenderFPS: %4.2f", fRenderFPS);

		//now draw the scene over the shadows
		{
			setupScene();
			drawBackground();

			// draw fingers with sphere at the tip.
			drawHands(frame, true);
			drawHands(frame);

			if (m_bShowDemo)
			{
				updateDemo(frame,true);
				updateDemo(frame,false);
			}
		}

		{
			ScopedLock renderLock(m_renderMutex);

			//Draw the text overlay
			renderOpenGL2D();
		}
	}

	void drawBackground()
	{
		glColor3f(0.15f, 0.15f, 0.1f);
		{
			glPushMatrix();
			glTranslatef(0, shadowsYPos + -0.05f, 0);
			glScalef(40,0.01f,40);
			LeapUtilGL::drawBox(LeapUtilGL::kStyle_Solid);	
			glPopMatrix();
		}
	}

	void drawHands(Leap::Frame frame, bool isShadow = false)
	{
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		if (isShadow)
		{
			glEnable (GL_BLEND);
			glColor4f(0,0,0,0.2f);
			glPushMatrix();
			glTranslatef(0, shadowsYPos, 0);
			float maxScale = 4.0f;
			float minScale = 2.0f;

			float maxH = 4;

			float currH = handY / maxH;
			if (currH > maxH)
				currH = maxH;

			float scale = ((maxScale - minScale) * currH) + minScale;

			glScalef(scale, 0.001f, scale);
		}

		using namespace Leap;
		LeapUtilGL::GLAttribScope colorScope(GL_CURRENT_BIT | GL_LINE_BIT);
		if (!frame.hands().isEmpty()) 
		{
			//Get the first hand
			for (int handCount = 0; handCount < frame.hands().count(); ++handCount)
			{
				const Hand hand = frame.hands()[handCount];

				//Find the thumb
				int thumbId = 0;

				/*
				Note that the the leftmost() and rightmost() functions only identify which hand is farthest to the left or to the right. 
				The functions do not identify which hand is the right or the left hand.
				*/
				//Leap does not know which hand is which... lets assume their relative position
				if (frame.hands().count() == 2)
				{
					if (hand.id() == frame.hands().leftmost().id())
						thumbId = hand.fingers().rightmost().id();
					else 
						thumbId = hand.fingers().leftmost().id();
				};

				const Vector normal = hand.palmNormal();
				const Vector direction = hand.direction();
				const float RAD_2_DEG = 57.2957795f;
				//float angle = hand.rotationAngle(frame);
				Leap::Vector handPos = m_mtxFrameTransform.transformPoint(hand.palmPosition()* m_fFrameScale);
				Leap::Vector handDir = hand.direction();
				Leap::Vector wristPos = handPos + (-handDir * (hand.sphereRadius() / 2.0f) * m_fFrameScale);	

				handY = handPos.y;

				// Check if the hand has any fingers
				const FingerList& fingers = hand.fingers();
				if (!fingers.isEmpty()) 
				{
					for (int fingerCount = 0; fingerCount < fingers.count(); ++fingerCount)
					{
						Finger finger = fingers[fingerCount];
						float fingerLength = finger.length();

						Leap::Vector stabelizedPos = finger.stabilizedTipPosition();
						Leap::Vector tipPos;
						if (m_useStabelizedPos)
						{
							tipPos = stabelizedPos;
						}
						else
						{
							tipPos = finger.tipPosition();
						}

						Leap::Vector vStartPos = m_mtxFrameTransform.transformPoint(tipPos * m_fFrameScale);
						Leap::Vector vFingerDir = -m_mtxFrameTransform.transformDirection(finger.direction()); //negative because we want to know the opposite direction to draw the bones
						Leap::Vector vEndRelativePos   = vFingerDir * fingerLength * m_fFrameScale;
						Leap::Vector vLastJointPos;
						{		
							//Finger nail
							if (!isShadow)
							{
								glColor3f(0.0f, 0.2f, 1.0f);

								glPushMatrix();
								//base translation
								glTranslatef(vStartPos.x, vStartPos.y, vStartPos.z);
								glScalef(m_fFrameScale * gTipRadius , m_fFrameScale * gTipRadius, m_fFrameScale * gTipRadius);
								//LeapUtilGL::drawSphere(LeapUtilGL::kStyle_Solid);
								glPopMatrix();
							}

							//Joints
							int numJoints = thumbId == finger.id() ? 2 : 3; //thumb has only 2 joints.
							float splitSize = fingerLength / (float)numJoints;

							//Counts nail as joint and skip it
							//Leap motion does not detect joints, sight, but we are going to fake it by assuming the last joint never moves
							Leap::Vector prevPos = vStartPos;
							for (int i = 0; i < numJoints; ++ i)
							{		
								Leap::Vector desiredDir;
								if (i+1 == numJoints) //last joint
								{
									desiredDir = (wristPos - prevPos).normalized();
								}
								else
								{
									desiredDir = vFingerDir;
								}

								Leap::Vector jointEndPos = (desiredDir * splitSize * m_fFrameScale);
								Leap::Vector jointPos = prevPos + jointEndPos;								

								if (!isShadow)
								{
									glColor3f(0.0f, 0.0f, 0.0f);

									//Finger bone
									glBegin(GL_LINES);
									glVertex3fv(prevPos.toFloatPointer());
									glVertex3fv(jointPos.toFloatPointer());
									glEnd();
								}

								prevPos = jointPos;
								vLastJointPos = jointPos;

								if (!isShadow)
								{
									glColor3f(0.0f, 0.2f, 1.0f);

									//joint
									glPushMatrix();
									glTranslatef(jointPos.x, jointPos.y, jointPos.z);
									glScalef(3.0f * m_fFrameScale, 3.0f * m_fFrameScale,  3.0f * m_fFrameScale);
									LeapUtilGL::drawSphere(LeapUtilGL::kStyle_Solid);
									glPopMatrix();	
								}

								//Joint outline
								if (!isShadow)
								{
									glEnable (GL_BLEND);
									glColor4f(1,0,1,0.5f);
								}

								glPushMatrix();
								Leap::Vector vHalfEndPos = vEndRelativePos * 0.5f;
								Leap::Vector jointOutlinePos = jointPos - (jointEndPos/2); 

								Leap::Matrix model = createTransform(desiredDir, jointOutlinePos);

								//Rotate
								//Finger outline.
								glMultMatrixf(model.toArray4x4());
								//Rotation.
								//Use the hand normal to create a new lookat for the finger and use the direction as z
								glScalef(0.075f, 0.075f, splitSize * m_fFrameScale);
								LeapUtilGL::drawBox(LeapUtilGL::kStyle_Solid);
								glPopMatrix();	

								if (!isShadow)
									glDisable (GL_BLEND);
							}	

							if (!isShadow)
							{
								//note the previous vEndPos was relative to vStartPos not absolute
								//knuckle bone to wrist
								glBegin(GL_LINES);
								glVertex3fv(wristPos.toFloatPointer());
								glVertex3fv(vLastJointPos.toFloatPointer());
								glEnd();
							}
						}
					}
				}

				//HAND
				{
					// Approximation, the hand size is the size of the sphere we can handle minus the size of the biggest finger
					float handSize = 50;//hand.sphereRadius() - hand.fingers().frontmost().length();
					//hand centre
					glPushMatrix();

					glTranslatef(handPos.x, handPos.y, handPos.z);

					glRotatef(direction.yaw() * -RAD_2_DEG, 0, 1, 0);
					glRotatef(direction.pitch() * RAD_2_DEG, 1, 0, 0);
					glRotatef(normal.roll() * RAD_2_DEG, 0, 0, 1);

					glPushMatrix();
					glScalef(m_fFrameScale * 4 , m_fFrameScale * 4, m_fFrameScale * 4);
					LeapUtilGL::drawSphere(LeapUtilGL::kStyle_Solid);
					glPopMatrix();

					if (!isShadow)
					{
						//hand outline
						glEnable (GL_BLEND);
						glColor4f(1,0,1,0.5f);
					}

					glPushMatrix();
					glScalef(handSize * 0.75f * m_fFrameScale, 0.15f, handSize * 0.75f * m_fFrameScale);
					LeapUtilGL::drawSphere(LeapUtilGL::kStyle_Solid);
					glPopMatrix();

					if (!isShadow)
						glDisable (GL_BLEND);

					glPopMatrix();
				}				

				if (!isShadow)
				{
					//wrist
					{
						glColor3f(0.1f, 0.1f, 1.0f);

						//hand.sphereCenter()
						glPushMatrix();
						glTranslatef(wristPos.x, wristPos.y, wristPos.z);

						glScalef(m_fFrameScale * 4 , m_fFrameScale * 4, m_fFrameScale * 4);
						LeapUtilGL::drawSphere(LeapUtilGL::kStyle_Solid);
						glPopMatrix();
					}		

					//wrist to the center of the hand
					{
						glBegin(GL_LINES);
						glVertex3fv(wristPos.toFloatPointer());
						glVertex3fv(handPos.toFloatPointer());
						glEnd();
					}
				}
			}
		}

		if (isShadow)
		{
			glPopMatrix();
			glDisable (GL_BLEND);
		}
	}

	virtual void onInit(const Leap::Controller&) 
	{
	}

	virtual void onConnect(const Leap::Controller&) 
	{
	}

	virtual void onDisconnect(const Leap::Controller&) 
	{
	}

	virtual void onFrame(const Leap::Controller& controller)
	{
		if (!m_bPaused)
		{
			Leap::Frame frame = controller.frame();
			update(frame);
			m_lastFrame = frame;
			m_openGLContext.triggerRepaint();
		}
	}


	void resetCamera()
	{
		m_camera.SetOrbitTarget(Leap::Vector::zero());
		m_camera.SetPOVLookAt(Leap::Vector(0, 6, 10), m_camera.GetOrbitTarget());
	}

	void initColors()
	{
		float fMin      = 0.0f;
		float fMax      = 1.0f;
		float fNumSteps = static_cast<float>(pow(kNumColors, 1.0/3.0));
		float fStepSize = (fMax - fMin)/fNumSteps;
		float fR = fMin, fG = fMin, fB = fMin;

		for (uint32_t i = 0; i < kNumColors; i++)
		{
			m_avColors[i] = Leap::Vector(fR, fG, LeapUtil::Min(fB, fMax));

			fR += fStepSize;

			if (fR > fMax)
			{
				fR = fMin;
				fG += fStepSize;

				if (fG > fMax)
				{
					fG = fMin;
					fB += fStepSize;
				}
			}
		}

		Random rng(0x13491349);

		for (uint32_t i = 0; i < kNumColors; i++)
		{
			uint32_t      uiRandIdx    = i + (rng.nextInt() % (kNumColors - i));
			Leap::Vector  vSwapTemp    = m_avColors[i];

			m_avColors[i]          = m_avColors[uiRandIdx];
			m_avColors[uiRandIdx]  = vSwapTemp;
		}
	}

private:
	OpenGLContext               m_openGLContext;
	LeapUtilGL::CameraGL        m_camera;
	Leap::Frame                 m_lastFrame;
	double                      m_fLastUpdateTimeSeconds;
	double                      m_fLastRenderTimeSeconds;
	Leap::Matrix                m_mtxFrameTransform;
	float                       m_fPointableRadius;
	LeapUtil::RollingAverage<>  m_avgUpdateDeltaTime;
	LeapUtil::RollingAverage<>  m_avgRenderDeltaTime;
	String                      m_strUpdateFPS;
	String                      m_strRenderFPS;
	String                      m_strPrompt;
	String                      m_strHelp;
	Font                        m_fixedFont;
	CriticalSection             m_renderMutex;
	bool                        m_bShowHelp;
	bool                        m_bPaused;
	bool                        m_bShowDemo;

	enum  { kNumColors = 256 };
	Leap::Vector            m_avColors[kNumColors];
};

//==============================================================================
/**
This is the top-level window that we'll pop up. Inside it, we'll create and
show a component from the MainComponent.cpp file
*/
class FingerVisualizerWindow  : public DocumentWindow
{
public:
	//==============================================================================
	FingerVisualizerWindow()
		: DocumentWindow ("Virtual Hands",
		Colours::lightgrey,
		DocumentWindow::allButtons,
		true)
	{
		setContentOwned (new OpenGLCanvas(), true);

		// Centre the window on the screen
		centreWithSize (getWidth(), getHeight());

		// And show it!
		setVisible (true);

		getChildComponent(0)->grabKeyboardFocus();
	}

	~FingerVisualizerWindow()
	{
		// (the content component will be deleted automatically, so no need to do it here)
	}

	//==============================================================================
	void closeButtonPressed()
	{
		// When the user presses the close button, we'll tell the app to quit. This
		JUCEApplication::quit();
	}
};

void FingerVisualizerApplication::initialise (const String& commandLine)
{
	(void) commandLine;
	// Do your application's initialisation code here.
	m_pMainWindow = new FingerVisualizerWindow();
}

//==============================================================================
// This macro generates the main() routine that starts the app.
START_JUCE_APPLICATION(FingerVisualizerApplication)
