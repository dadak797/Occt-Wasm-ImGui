// Copyright (c) 2019 OPEN CASCADE SAS
//
// This file is part of the examples of the Open CASCADE Technology software library.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE

#include "Common.hpp"
#include "WasmOcctView.hpp"

#include <AIS_Shape.hxx>
#include <AIS_ViewCube.hxx>
#include <Aspect_Handle.hxx>
#include <Aspect_DisplayConnection.hxx>
#include <Image_AlienPixMap.hxx>
#include <Message.hxx>
#include <Message_Messenger.hxx>
#include <Graphic3d_CubeMapPacked.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <Prs3d_DatumAspect.hxx>
#include <Prs3d_ToolCylinder.hxx>
#include <Prs3d_ToolDisk.hxx>
//#include <Wasm_Window.hxx>
#include "GlfwOcctWindow.hpp"
#include <OpenGl_Context.hxx>

#include <BRep_Builder.hxx>
#include <BRepBndLib.hxx>
#include <BRepTools.hxx>
#include <Standard_ArrayStreamBuffer.hxx>

#include <emscripten/bind.h>

#include <iostream>

#include <STEPControl_Reader.hxx>
#include <TDocStd_Document.hxx>
#include <TDF_Label.hxx>
#include <TopLoc_Location.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <TopTools_HSequenceOfShape.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCone.hxx>

#include "AppManager.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>


namespace
{
  //! Auxiliary wrapper for loading model.
  struct ModelAsyncLoader
  {
    std::string Name;
    std::string Path;

    ModelAsyncLoader (const char* theName, const char* thePath)
    : Name (theName), Path (thePath) {}

    //! File data read event.
    static void onDataRead (void* theOpaque, void* theBuffer, int theDataLen)
    {
      const ModelAsyncLoader* aTask = (ModelAsyncLoader* )theOpaque;
      WasmOcctView::openFromMemory (aTask->Name, reinterpret_cast<uintptr_t>(theBuffer), theDataLen, false);
      delete aTask;
    }

    //! File read error event.
    static void onReadFailed (void* theOpaque)
    {
      const ModelAsyncLoader* aTask = (ModelAsyncLoader* )theOpaque;
      Message::DefaultMessenger()->Send (TCollection_AsciiString("Error: unable to load file ") + aTask->Path.c_str(), Message_Fail);
      delete aTask;
    }
  };

  //! Auxiliary wrapper for loading cubemap.
  struct CubemapAsyncLoader
  {
    //! Image file read event.
    static void onImageRead (const char* theFilePath)
    {
      Handle(Graphic3d_CubeMapPacked) aCubemap;
      Handle(Image_AlienPixMap) anImage = new Image_AlienPixMap();
      if (anImage->Load (theFilePath))
      {
        aCubemap = new Graphic3d_CubeMapPacked (anImage);
      }
      WasmOcctView::Instance().View()->SetBackgroundCubeMap (aCubemap, true, false);
      WasmOcctView::Instance().UpdateView();
    }

    //! Image file failed read event.
    static void onImageFailed (const char* theFilePath)
    {
      Message::DefaultMessenger()->Send (TCollection_AsciiString("Error: unable to load image ") + theFilePath, Message_Fail);
    }
  };
}

// Initialize static variable
bool WasmOcctView::m_bShowScale = false;

WasmOcctView& WasmOcctView::Instance()
{
    static WasmOcctView aViewer;
    return aViewer;
}

// ================================================================
// Function : WasmOcctView
// Purpose  :
// ================================================================
WasmOcctView::WasmOcctView()
: myDevicePixelRatio (1.0f),
  myUpdateRequests (0),
    m_SelectionMode(TopAbs_SOLID),
    m_ImGuiContext(nullptr)
{
  addActionHotKeys (Aspect_VKey_NavForward,        Aspect_VKey_W, Aspect_VKey_W | Aspect_VKeyFlags_SHIFT);
  addActionHotKeys (Aspect_VKey_NavBackward ,      Aspect_VKey_S, Aspect_VKey_S | Aspect_VKeyFlags_SHIFT);
  addActionHotKeys (Aspect_VKey_NavSlideLeft,      Aspect_VKey_A, Aspect_VKey_A | Aspect_VKeyFlags_SHIFT);
  addActionHotKeys (Aspect_VKey_NavSlideRight,     Aspect_VKey_D, Aspect_VKey_D | Aspect_VKeyFlags_SHIFT);
  addActionHotKeys (Aspect_VKey_NavRollCCW,        Aspect_VKey_Q, Aspect_VKey_Q | Aspect_VKeyFlags_SHIFT);
  addActionHotKeys (Aspect_VKey_NavRollCW,         Aspect_VKey_E, Aspect_VKey_E | Aspect_VKeyFlags_SHIFT);

  addActionHotKeys (Aspect_VKey_NavSpeedIncrease,  Aspect_VKey_Plus,  Aspect_VKey_Plus  | Aspect_VKeyFlags_SHIFT,
                                                   Aspect_VKey_Equal,
                                                   Aspect_VKey_NumpadAdd, Aspect_VKey_NumpadAdd | Aspect_VKeyFlags_SHIFT);
  addActionHotKeys (Aspect_VKey_NavSpeedDecrease,  Aspect_VKey_Minus, Aspect_VKey_Minus | Aspect_VKeyFlags_SHIFT,
                                                   Aspect_VKey_NumpadSubtract, Aspect_VKey_NumpadSubtract | Aspect_VKeyFlags_SHIFT);

  // arrow keys conflict with browser page scrolling, so better be avoided in non-fullscreen mode
  addActionHotKeys (Aspect_VKey_NavLookUp,         Aspect_VKey_Numpad8); // Aspect_VKey_Up
  addActionHotKeys (Aspect_VKey_NavLookDown,       Aspect_VKey_Numpad2); // Aspect_VKey_Down
  addActionHotKeys (Aspect_VKey_NavLookLeft,       Aspect_VKey_Numpad4); // Aspect_VKey_Left
  addActionHotKeys (Aspect_VKey_NavLookRight,      Aspect_VKey_Numpad6); // Aspect_VKey_Right
  addActionHotKeys (Aspect_VKey_NavSlideLeft,      Aspect_VKey_Numpad1); // Aspect_VKey_Left |Aspect_VKeyFlags_SHIFT
  addActionHotKeys (Aspect_VKey_NavSlideRight,     Aspect_VKey_Numpad3); // Aspect_VKey_Right|Aspect_VKeyFlags_SHIFT
  addActionHotKeys (Aspect_VKey_NavSlideUp,        Aspect_VKey_Numpad9); // Aspect_VKey_Up   |Aspect_VKeyFlags_SHIFT
  addActionHotKeys (Aspect_VKey_NavSlideDown,      Aspect_VKey_Numpad7); // Aspect_VKey_Down |Aspect_VKeyFlags_SHIFT
}

// ================================================================
// Function : ~WasmOcctView
// Purpose  :
// ================================================================
WasmOcctView::~WasmOcctView()
{
    cleanup();
}

// ================================================================
// Function : run
// Purpose  :
// ================================================================
void WasmOcctView::run()
{
  initWindow();
  initViewer();
  initDemoScene();
  if (myView.IsNull())
  {
    return;
  }

  //myView->MustBeResized();
  myView->Redraw();

  // There is no infinite message loop, main() will return from here immediately.
  // Tell that our Module should be left loaded and handle events through callbacks.
  //emscripten_set_main_loop (redrawView, 60, 1);
  //emscripten_set_main_loop (redrawView, -1, 1);
  EM_ASM(Module['noExitRuntime'] = true);
}

void WasmOcctView::cleanup()
{
    // cleanup ImGui
    ImGui_ImplOpenGL3_DestroyFontsTexture();
    ImGui_ImplOpenGL3_DestroyDeviceObjects();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext(m_ImGuiContext);

    myView->Remove();
}

// ================================================================
// Function : initWindow
// Purpose  :
// ================================================================
void WasmOcctView::initWindow()
{
  myDevicePixelRatio = emscripten_get_device_pixel_ratio();
  myCanvasId = THE_CANVAS_ID;
  const char* aTargetId = !myCanvasId.IsEmpty() ? myCanvasId.ToCString() : EMSCRIPTEN_EVENT_TARGET_WINDOW;
  const EM_BOOL toUseCapture = EM_TRUE;
  emscripten_set_resize_callback     (EMSCRIPTEN_EVENT_TARGET_WINDOW, this, toUseCapture, onResizeCallback);

  emscripten_set_mousedown_callback  (aTargetId, this, toUseCapture, onMouseCallback);
  // bind these events to window to track mouse movements outside of canvas
  //emscripten_set_mouseup_callback    (aTargetId, this, toUseCapture, onMouseCallback);
  //emscripten_set_mousemove_callback  (aTargetId, this, toUseCapture, onMouseCallback);
  //emscripten_set_mouseleave_callback (aTargetId, this, toUseCapture, onMouseCallback);
  emscripten_set_mouseup_callback    (EMSCRIPTEN_EVENT_TARGET_WINDOW, this, toUseCapture, onMouseCallback);
  emscripten_set_mousemove_callback  (EMSCRIPTEN_EVENT_TARGET_WINDOW, this, toUseCapture, onMouseCallback);

  emscripten_set_dblclick_callback   (aTargetId, this, toUseCapture, onMouseCallback);
  emscripten_set_click_callback      (aTargetId, this, toUseCapture, onMouseCallback);
  emscripten_set_mouseenter_callback (aTargetId, this, toUseCapture, onMouseCallback);
  emscripten_set_wheel_callback      (aTargetId, this, toUseCapture, onWheelCallback);

  emscripten_set_touchstart_callback (aTargetId, this, toUseCapture, onTouchCallback);
  emscripten_set_touchend_callback   (aTargetId, this, toUseCapture, onTouchCallback);
  emscripten_set_touchmove_callback  (aTargetId, this, toUseCapture, onTouchCallback);
  emscripten_set_touchcancel_callback(aTargetId, this, toUseCapture, onTouchCallback);

  //emscripten_set_keypress_callback   (aTargetId, this, toUseCapture, onKeyCallback);
  emscripten_set_keydown_callback    (aTargetId, this, toUseCapture, onKeyDownCallback);
  emscripten_set_keyup_callback      (aTargetId, this, toUseCapture, onKeyUpCallback);
  //emscripten_set_focus_callback    (aTargetId, this, toUseCapture, onFocusCallback);
  //emscripten_set_focusin_callback  (aTargetId, this, toUseCapture, onFocusCallback);
  emscripten_set_focusout_callback   (aTargetId, this, toUseCapture, onFocusCallback);
}

// ================================================================
// Function : dumpGlInfo
// Purpose  :
// ================================================================
void WasmOcctView::dumpGlInfo (bool theIsBasic)
{
  TColStd_IndexedDataMapOfStringString aGlCapsDict;
  myView->DiagnosticInformation (aGlCapsDict, theIsBasic ? Graphic3d_DiagnosticInfo_Basic : Graphic3d_DiagnosticInfo_Complete);
  if (theIsBasic)
  {
    TCollection_AsciiString aViewport;
    aGlCapsDict.FindFromKey ("Viewport", aViewport);
    aGlCapsDict.Clear();
    aGlCapsDict.Add ("Viewport", aViewport);
  }
  aGlCapsDict.Add ("Display scale", TCollection_AsciiString(myDevicePixelRatio));

  // beautify output
  {
    TCollection_AsciiString* aGlVer   = aGlCapsDict.ChangeSeek ("GLversion");
    TCollection_AsciiString* aGlslVer = aGlCapsDict.ChangeSeek ("GLSLversion");
    if (aGlVer   != NULL
     && aGlslVer != NULL)
    {
      *aGlVer = *aGlVer + " [GLSL: " + *aGlslVer + "]";
      aGlslVer->Clear();
    }
  }

  TCollection_AsciiString anInfo;
  for (TColStd_IndexedDataMapOfStringString::Iterator aValueIter (aGlCapsDict); aValueIter.More(); aValueIter.Next())
  {
    if (!aValueIter.Value().IsEmpty())
    {
      if (!anInfo.IsEmpty())
      {
        anInfo += "\n";
      }
      anInfo += aValueIter.Key() + ": " + aValueIter.Value();
    }
  }

  ::Message::DefaultMessenger()->Send (anInfo, Message_Warning);
}

// ================================================================
// Function : initPixelScaleRatio
// Purpose  :
// ================================================================
void WasmOcctView::initPixelScaleRatio()
{
  SetTouchToleranceScale (myDevicePixelRatio);
  if (!myView.IsNull())
  {
    myView->ChangeRenderingParams().Resolution = (unsigned int )(96.0 * myDevicePixelRatio + 0.5);
  }
  if (!myContext.IsNull())
  {
    myContext->SetPixelTolerance (int(myDevicePixelRatio * 6.0));
    if (!myViewCube.IsNull())
    {
      static const double THE_CUBE_SIZE = 60.0;
      myViewCube->SetSize (myDevicePixelRatio * THE_CUBE_SIZE, false);
      myViewCube->SetBoxFacetExtension (myViewCube->Size() * 0.15);
      myViewCube->SetAxesPadding (myViewCube->Size() * 0.10);
      myViewCube->SetFontHeight  (THE_CUBE_SIZE * 0.16);
      if (myViewCube->HasInteractiveContext())
      {
        myContext->Redisplay (myViewCube, false);
      }
    }
  }
}


void WasmOcctView::s_onWindowResized(GLFWwindow* window, int width, int height)
{
    if (width == 0 || height == 0) return;
    SPDLOG_INFO("framebuffer size changed: ({} x {})", width, height);
    //WasmOcctView* view = reinterpret_cast<WasmOcctView*>(glfwGetWindowUserPointer(window));
    glViewport(0, 0, width, height);
}

void WasmOcctView::s_onMouseButton(GLFWwindow* window, int button, int action, int mods)
{
		ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);
	  WasmOcctView* view = reinterpret_cast<WasmOcctView*>(glfwGetWindowUserPointer(window));
	  SPDLOG_INFO("Mouse Button Event: {}", button);
}

void WasmOcctView::s_onMouseMove(GLFWwindow* window, double thePosX, double thePosY)
{
		ImGui_ImplGlfw_CursorPosCallback(window, thePosX, thePosY);
	  WasmOcctView* view = reinterpret_cast<WasmOcctView*>(glfwGetWindowUserPointer(window));
	  //SPDLOG_INFO("Mouse Move Event");
}


// ================================================================
// Function : initViewer
// Purpose  :
// ================================================================
bool WasmOcctView::initViewer()
{
  // Build with "--preload-file MyFontFile.ttf" option
  // and register font in Font Manager to use custom font(s).
  /*const char* aFontPath = "MyFontFile.ttf";
  if (Handle(Font_SystemFont) aFont = Font_FontMgr::GetInstance()->CheckFont (aFontPath))
  {
    Font_FontMgr::GetInstance()->RegisterFont (aFont, true);
  }
  else
  {
    Message::DefaultMessenger()->Send (TCollection_AsciiString ("Error: font '") + aFontPath + "' is not found", Message_Fail);
  }*/

  Handle(Aspect_DisplayConnection) aDisp;
  Handle(OpenGl_GraphicDriver) aDriver = new OpenGl_GraphicDriver (aDisp, false);
  aDriver->ChangeOptions().buffersNoSwap = true; // swap has no effect in WebGL
  aDriver->ChangeOptions().buffersOpaqueAlpha = true; // avoid unexpected blending of canvas with page background
  if (!aDriver->InitContext())
  {
    Message::DefaultMessenger()->Send (TCollection_AsciiString ("Error: EGL initialization failed"), Message_Fail);
    return false;
  }

  Handle(V3d_Viewer) aViewer = new V3d_Viewer (aDriver);
  aViewer->SetComputedMode (false);
  aViewer->SetDefaultShadingModel (Graphic3d_TypeOfShadingModel_Phong);
  aViewer->SetDefaultLights();
  aViewer->SetLightOn();
  for (V3d_ListOfLight::Iterator aLightIter (aViewer->ActiveLights()); aLightIter.More(); aLightIter.Next())
  {
    const Handle(V3d_Light)& aLight = aLightIter.Value();
    if (aLight->Type() == Graphic3d_TypeOfLightSource_Directional)
    {
      aLight->SetCastShadows (true);
    }
  }

  //Handle(Wasm_Window) aWindow = new Wasm_Window (THE_CANVAS_ID);
  Handle(GlfwOcctWindow) aWindow = new GlfwOcctWindow(THE_CANVAS_ID);  // by skpark
  //aWindow->Size (myWinSizeOld.x(), myWinSizeOld.y());

  glfwSetWindowUserPointer(aWindow->GetGlfwWindow(), this);  // by skpark

  myTextStyle = new Prs3d_TextAspect();
  myTextStyle->SetFont (Font_NOF_ASCII_MONO);
  myTextStyle->SetHeight (12);
  myTextStyle->Aspect()->SetColor (Quantity_NOC_LIGHTGRAY);
  myTextStyle->Aspect()->SetColorSubTitle (Quantity_NOC_BLACK);
  myTextStyle->Aspect()->SetDisplayType (Aspect_TODT_SHADOW);
  myTextStyle->Aspect()->SetTextFontAspect (Font_FA_Bold);
  myTextStyle->Aspect()->SetTextZoomable (false);
  myTextStyle->SetHorizontalJustification (Graphic3d_HTA_LEFT);
  myTextStyle->SetVerticalJustification (Graphic3d_VTA_BOTTOM);

  myView = new V3d_View (aViewer);
  //myView->Camera()->SetProjectionType (Graphic3d_Camera::Projection_Perspective);
  myView->Camera()->SetProjectionType (Graphic3d_Camera::Projection_Orthographic);
  myView->SetImmediateUpdate (false);
  myView->ChangeRenderingParams().IsShadowEnabled = false;
  myView->ChangeRenderingParams().Resolution = (unsigned int )(96.0 * myDevicePixelRatio + 0.5);
  myView->ChangeRenderingParams().ToShowStats = true;
  myView->ChangeRenderingParams().StatsTextAspect = myTextStyle->Aspect();
  myView->ChangeRenderingParams().StatsTextHeight = (int )myTextStyle->Height();
  myView->SetWindow (aWindow);
  myView->SetBgGradientColors(Quantity_Color(255./255., 255./255., 255./255., Quantity_TOC_RGB), 
                              Quantity_Color(107./ 255., 140./ 255., 222./ 255., Quantity_TOC_RGB), Aspect_GFM_VER);

  dumpGlInfo (false);

    m_GLContext = aDriver->GetSharedContext();  // by skpark
    if (!aWindow->IsMapped()) aWindow->Map();  // by skpark
    myView->MustBeResized();  // by skpark

  myContext = new AIS_InteractiveContext (aViewer);
  initPixelScaleRatio();

  // For Solid
  myContext->HighlightStyle(Prs3d_TypeOfHighlight_Dynamic)->SetColor(Quantity_NOC_LIGHTCYAN1);
  myContext->HighlightStyle(Prs3d_TypeOfHighlight_Dynamic)->SetTransparency(0.5f);
  myContext->HighlightStyle(Prs3d_TypeOfHighlight_Dynamic)->SetDisplayMode(AIS_Shaded);
  // For Vertex, Edge, Face
  myContext->HighlightStyle(Prs3d_TypeOfHighlight_LocalDynamic)->SetColor(Quantity_NOC_LIGHTCYAN1);
  myContext->HighlightStyle(Prs3d_TypeOfHighlight_LocalDynamic)->SetTransparency(0.2f);
  myContext->HighlightStyle(Prs3d_TypeOfHighlight_LocalDynamic)->SetDisplayMode(AIS_Shaded);
  // For Selected Solid
  myContext->HighlightStyle(Prs3d_TypeOfHighlight_Selected)->SetColor(Quantity_NOC_DEEPSKYBLUE1);
  myContext->HighlightStyle(Prs3d_TypeOfHighlight_Selected)->SetTransparency(0.2f);
  myContext->HighlightStyle(Prs3d_TypeOfHighlight_Selected)->SetDisplayMode(AIS_Shaded);
  // For Selected Vertex, Edge, Face
  myContext->HighlightStyle(Prs3d_TypeOfHighlight_LocalSelected)->SetColor(Quantity_NOC_INDIANRED);
  myContext->HighlightStyle(Prs3d_TypeOfHighlight_LocalSelected)->SetTransparency(0.2f);
  myContext->HighlightStyle(Prs3d_TypeOfHighlight_LocalSelected)->SetDisplayMode(AIS_Shaded);

  // To Remove Unnecessary Wires On Curved Surface
  myContext->SetIsoNumber(0);

	myContext->DefaultDrawer()->SetFaceBoundaryDraw(Standard_True);
	myContext->SetDisplayMode(AIS_Shaded, Standard_False);

    // window callback
    glfwSetWindowSizeCallback(aWindow->GetGlfwWindow(), WasmOcctView::s_onWindowResized);
    // mouse callback
    glfwSetMouseButtonCallback(aWindow->GetGlfwWindow(), WasmOcctView::s_onMouseButton);
    glfwSetCursorPosCallback(aWindow->GetGlfwWindow(), WasmOcctView::s_onMouseMove);

    // ImGui Initialization
    m_ImGuiContext = ImGui::CreateContext();
    ImGui::SetCurrentContext(m_ImGuiContext);

    Standard_Integer width, height;
    aWindow->Size(width, height);

    ImGuiIO& io = ImGui::GetIO();
    ImGui::StyleColorsLight();
    float fontSize = (float)height / 100.0f + 4.0f;
    SPDLOG_INFO("ImGui Font Size: {}", fontSize);
    io.Fonts->AddFontFromFileTTF("resources/fonts/Consolas.ttf", fontSize);

    ImGui_ImplGlfw_InitForOpenGL(aWindow->GetGlfwWindow(), false);

    ImGui_ImplOpenGL3_Init();
    ImGui_ImplOpenGL3_CreateFontsTexture();
    ImGui_ImplOpenGL3_CreateDeviceObjects();

  return true;
}

// ================================================================
// Function : initDemoScene
// Purpose  :
// ================================================================
void WasmOcctView::initDemoScene()
{
  if (myContext.IsNull())
  {
    return;
  }

  //myView->TriedronDisplay (Aspect_TOTP_LEFT_LOWER, Quantity_NOC_GOLD, 0.08, V3d_WIREFRAME);

  myViewCube = new AIS_ViewCube();
  // presentation parameters
  initPixelScaleRatio();
  myViewCube->SetTransformPersistence (new Graphic3d_TransformPers (Graphic3d_TMF_TriedronPers, Aspect_TOTP_RIGHT_LOWER, Graphic3d_Vec2i(150, 150)));
  myViewCube->Attributes()->SetDatumAspect (new Prs3d_DatumAspect());
  myViewCube->Attributes()->DatumAspect()->SetTextAspect (myTextStyle);
  // animation parameters
  myViewCube->SetViewAnimation (myViewAnimation);
  myViewCube->SetFixedAnimationLoop (false);
  myViewCube->SetAutoStartAnimation (true);
  myContext->Display (myViewCube, false);

  onRedrawView(this);
  // Build with "--preload-file MySampleFile.brep" option to load some shapes here.
}

// ================================================================
// Function : ProcessInput
// Purpose  :
// ================================================================
void WasmOcctView::ProcessInput()
{
  if (!myView.IsNull())
  {
    // Queue onRedrawView()/redrawView callback to redraw canvas after all user input is flushed by browser.
    // Redrawing viewer on every single message would be a pointless waste of resources,
    // as user will see only the last drawn frame due to WebGL implementation details.
    if (++myUpdateRequests == 1)
    {
      emscripten_async_call (onRedrawView, this, 0);
    }
  }
}

// ================================================================
// Function : UpdateView
// Purpose  :
// ================================================================
void WasmOcctView::UpdateView()
{
  if (!myView.IsNull())
  {
    myView->Invalidate();

    // queue next onRedrawView()/redrawView()
    ProcessInput();
  }
}

// ================================================================
// Function : redrawView
// Purpose  :
// ================================================================
void WasmOcctView::redrawView()
{
    if (!myView.IsNull())
    {
        FlushViewEvents(myContext, myView, true); 

        m_GLContext->MakeCurrent();  // by skpark
        //myView->Invalidate();  // by skpark

        ImGui_ImplGlfw_NewFrame();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui::NewFrame();


        if (ImGui::Begin("my first ImGui window")) {
            ImGui::Text("This is first text...");
            if (ImGui::Button("Make Box")) {
                MakeBox(m_Location);
            }
            if (ImGui::Button("Make Cone")) {
                MakeCone(m_Location);
            }
            ImGui::DragFloat3("Box location", m_Location);
            ImGui::Separator();
            if (ImGui::Button("Show Scale")) {
                showScale();
            }
            ImGui::Separator();
            ImGui::Text("View Mode");
            if (ImGui::Button("Perspective")) {
                projectionPerspective();
            }
            if (ImGui::Button("Orthographic")) {
                projectionOrthographic();
            }
            // ImGui::Separator();
            // ImGui::Text("Selection Mode");
            // if (ImGui::Button("Vertex")) {
            //     selectVertexMode();
            // }
            // if (ImGui::Button("Edge")) {
            //     selectEdgeMode();
            // }
            // if (ImGui::Button("Face")) {
            //     selectFaceMode();
            // }
            // if (ImGui::Button("Solid")) {
            //     selectSolidMode();
            // }
        }
        ImGui::End();

        ImGui::ShowDemoWindow();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        m_GLContext->SwapBuffers();  // by skpark       
    }
    glfwPollEvents();
}

// ================================================================
// Function : handleViewRedraw
// Purpose  :
// ================================================================
void WasmOcctView::handleViewRedraw (const Handle(AIS_InteractiveContext)& theCtx,
                                     const Handle(V3d_View)& theView)
{
  myUpdateRequests = 0;
  AIS_ViewController::handleViewRedraw (theCtx, theView);
  setAskNextFrame();
  if (myToAskNextFrame)
  {
    // ask more frames
    ++myUpdateRequests;
    emscripten_async_call(onRedrawView, this, 0);
  }
}

// EM_BOOL WasmOcctView::onResizeCallback(int theEventType, const EmscriptenUiEvent* theEvent, void* theView)
// {
//     int width, height;
//     emscripten_get_canvas_element_size(THE_CANVAS_ID, &width, &height);
//     Handle(GlfwOcctWindow) aWindow = Handle(GlfwOcctWindow)::DownCast(((WasmOcctView*)theView)->Window());
//     GLFWwindow* window = aWindow->GetGlfwWindow();
//     glViewPort(width, height);
//     return ((WasmOcctView*)theView)->onResizeEvent(theEventType, theEvent); 
// }

// ================================================================
// Function : onResizeEvent
// Purpose  :
// ================================================================
EM_BOOL WasmOcctView::onResizeEvent (int theEventType, const EmscriptenUiEvent* theEvent)
{
  (void )theEventType; // EMSCRIPTEN_EVENT_RESIZE or EMSCRIPTEN_EVENT_CANVASRESIZED
  (void )theEvent;
  if (myView.IsNull())
  {
    return EM_FALSE;
  }

  Handle(GlfwOcctWindow) aWindow = Handle(GlfwOcctWindow)::DownCast (myView->Window());
  Graphic3d_Vec2i aWinSizeNew;
  aWindow->DoResize();
  aWindow->Size (aWinSizeNew.x(), aWinSizeNew.y());
  const float aPixelRatio = emscripten_get_device_pixel_ratio();
  if (aWinSizeNew != myWinSizeOld
   || aPixelRatio != myDevicePixelRatio)
  {
    myWinSizeOld = aWinSizeNew;
    if (myDevicePixelRatio != aPixelRatio)
    {
      myDevicePixelRatio = aPixelRatio;
      initPixelScaleRatio();
    }

    myView->MustBeResized();
    myView->Invalidate();
    myView->Redraw();
    dumpGlInfo (true);
  }
  return EM_TRUE;
}

//! Update canvas bounding rectangle.
EM_JS(void, jsUpdateBoundingClientRect, (), {
  Module._myCanvasRect = Module.canvas.getBoundingClientRect();
});

//! Get canvas bounding top.
EM_JS(int, jsGetBoundingClientTop, (), {
  return Math.round(Module._myCanvasRect.top);
});

//! Get canvas bounding left.
EM_JS(int, jsGetBoundingClientLeft, (), {
  return Math.round(Module._myCanvasRect.left);
});

// ================================================================
// Function : onMouseEvent
// Purpose  :
// ================================================================
EM_BOOL WasmOcctView::onMouseEvent (int theEventType, const EmscriptenMouseEvent* theEvent)
{
    ImGuiIO& io = ImGui::GetIO();  
    if (io.WantCaptureMouse) {
        myView->Invalidate();
        return EM_FALSE;
    }

    EmscriptenMouseEvent anEvent = *theEvent;

    if (myView.IsNull())
    {
      return EM_FALSE;
    }

    Handle(GlfwOcctWindow) aWindow = Handle(GlfwOcctWindow)::DownCast(myView->Window());

    if (theEventType == EMSCRIPTEN_EVENT_MOUSEUP) {
        Standard_Integer btnNumber = (Standard_Integer)anEvent.button;

        if (btnNumber == 0)  // L-Button
        {
            Message::DefaultMessenger()->Send(TCollection_AsciiString("L-Button Clicked!"), Message_Info);
        }
        else if (btnNumber == 1)  // Scroll-Button
        {
            Message::DefaultMessenger()->Send(TCollection_AsciiString("Scroll-Button Clicked!"), Message_Info);
        }
        else if (btnNumber == 2)  // R-Button
        {
            Message::DefaultMessenger()->Send(TCollection_AsciiString("R-Button Clicked!"), Message_Info);
        }
    }

    if (theEventType == EMSCRIPTEN_EVENT_MOUSEMOVE
    || theEventType == EMSCRIPTEN_EVENT_MOUSEUP)
    {
        // these events are bound to EMSCRIPTEN_EVENT_TARGET_WINDOW, and coordinates should be converted
        jsUpdateBoundingClientRect();
        EmscriptenMouseEvent anEvent = *theEvent;
        anEvent.targetX -= jsGetBoundingClientLeft();
        anEvent.targetY -= jsGetBoundingClientTop();
        aWindow->ProcessMouseEvent(*this, theEventType, &anEvent);
        return EM_FALSE;
    }

  return aWindow->ProcessMouseEvent(*this, theEventType, theEvent) ? EM_TRUE : EM_FALSE;
}

// ================================================================
// Function : onWheelEvent
// Purpose  :
// ================================================================
EM_BOOL WasmOcctView::onWheelEvent (int theEventType, const EmscriptenWheelEvent* theEvent)
{
  if (myView.IsNull()
   || theEventType != EMSCRIPTEN_EVENT_WHEEL)
  {
    return EM_FALSE;
  }

  Handle(GlfwOcctWindow) aWindow = Handle(GlfwOcctWindow)::DownCast (myView->Window());
  return aWindow->ProcessWheelEvent (*this, theEventType, theEvent) ? EM_TRUE : EM_FALSE;
}

// ================================================================
// Function : onTouchEvent
// Purpose  :
// ================================================================
EM_BOOL WasmOcctView::onTouchEvent (int theEventType, const EmscriptenTouchEvent* theEvent)
{
  if (myView.IsNull())
  {
    return EM_FALSE;
  }

  Handle(GlfwOcctWindow) aWindow = Handle(GlfwOcctWindow)::DownCast (myView->Window());
  return aWindow->ProcessTouchEvent (*this, theEventType, theEvent) ? EM_TRUE : EM_FALSE;
}

// ================================================================
// Function : navigationKeyModifierSwitch
// Purpose  :
// ================================================================
bool WasmOcctView::navigationKeyModifierSwitch (unsigned int theModifOld,
                                                unsigned int theModifNew,
                                                double       theTimeStamp)
{
  bool hasActions = false;
  for (unsigned int aKeyIter = 0; aKeyIter < Aspect_VKey_ModifiersLower; ++aKeyIter)
  {
    if (!myKeys.IsKeyDown (aKeyIter))
    {
      continue;
    }

    Aspect_VKey anActionOld = Aspect_VKey_UNKNOWN, anActionNew = Aspect_VKey_UNKNOWN;
    myNavKeyMap.Find (aKeyIter | theModifOld, anActionOld);
    myNavKeyMap.Find (aKeyIter | theModifNew, anActionNew);
    if (anActionOld == anActionNew)
    {
      continue;
    }

    if (anActionOld != Aspect_VKey_UNKNOWN)
    {
      myKeys.KeyUp (anActionOld, theTimeStamp);
    }
    if (anActionNew != Aspect_VKey_UNKNOWN)
    {
      hasActions = true;
      myKeys.KeyDown (anActionNew, theTimeStamp);
    }
  }
  return hasActions;
}

// ================================================================
// Function : onFocusEvent
// Purpose  :
// ================================================================
EM_BOOL WasmOcctView::onFocusEvent (int theEventType, const EmscriptenFocusEvent* theEvent)
{
  if (myView.IsNull()
   || (theEventType != EMSCRIPTEN_EVENT_FOCUS
    && theEventType != EMSCRIPTEN_EVENT_FOCUSIN // about to receive focus
    && theEventType != EMSCRIPTEN_EVENT_FOCUSOUT))
  {
    return EM_FALSE;
  }

  Handle(GlfwOcctWindow) aWindow = Handle(GlfwOcctWindow)::DownCast (myView->Window());
  return aWindow->ProcessFocusEvent (*this, theEventType, theEvent) ? EM_TRUE : EM_FALSE;
}

// ================================================================
// Function : onKeyDownEvent
// Purpose  :
// ================================================================
EM_BOOL WasmOcctView::onKeyDownEvent (int theEventType, const EmscriptenKeyboardEvent* theEvent)
{
  if (myView.IsNull()
   || theEventType != EMSCRIPTEN_EVENT_KEYDOWN) // EMSCRIPTEN_EVENT_KEYPRESS
  {
    return EM_FALSE;
  }

  Handle(GlfwOcctWindow) aWindow = Handle(GlfwOcctWindow)::DownCast (myView->Window());
  return aWindow->ProcessKeyEvent (*this, theEventType, theEvent) ? EM_TRUE : EM_FALSE;
}

//=======================================================================
//function : KeyDown
//purpose  :
//=======================================================================
void WasmOcctView::KeyDown (Aspect_VKey theKey,
                            double theTime,
                            double thePressure)
{
  const unsigned int aModifOld = myKeys.Modifiers();
  AIS_ViewController::KeyDown (theKey, theTime, thePressure);

  const unsigned int aModifNew = myKeys.Modifiers();
  if (aModifNew != aModifOld
   && navigationKeyModifierSwitch (aModifOld, aModifNew, theTime))
  {
    // modifier key just pressed
  }

  Aspect_VKey anAction = Aspect_VKey_UNKNOWN;
  if (myNavKeyMap.Find (theKey | myKeys.Modifiers(), anAction)
  &&  anAction != Aspect_VKey_UNKNOWN)
  {
    AIS_ViewController::KeyDown (anAction, theTime, thePressure);
  }
}

// ================================================================
// Function : onKeyUpEvent
// Purpose  :
// ================================================================
EM_BOOL WasmOcctView::onKeyUpEvent (int theEventType, const EmscriptenKeyboardEvent* theEvent)
{
  if (myView.IsNull()
   || theEventType != EMSCRIPTEN_EVENT_KEYUP)
  {
    return EM_FALSE;
  }

  Handle(GlfwOcctWindow) aWindow = Handle(GlfwOcctWindow)::DownCast (myView->Window());
  return aWindow->ProcessKeyEvent (*this, theEventType, theEvent) ? EM_TRUE : EM_FALSE;
}

//=======================================================================
//function : KeyUp
//purpose  :
//=======================================================================
void WasmOcctView::KeyUp (Aspect_VKey theKey,
                          double theTime)
{
  const unsigned int aModifOld = myKeys.Modifiers();
  AIS_ViewController::KeyUp (theKey, theTime);

  Aspect_VKey anAction = Aspect_VKey_UNKNOWN;
  if (myNavKeyMap.Find (theKey | myKeys.Modifiers(), anAction)
  &&  anAction != Aspect_VKey_UNKNOWN)
  {
    AIS_ViewController::KeyUp (anAction, theTime);
    processKeyPress (anAction);
  }

  const unsigned int aModifNew = myKeys.Modifiers();
  if (aModifNew != aModifOld
   && navigationKeyModifierSwitch (aModifOld, aModifNew, theTime))
  {
    // modifier key released
  }

  processKeyPress (theKey | aModifNew);
}

//==============================================================================
//function : processKeyPress
//purpose  :
//==============================================================================
bool WasmOcctView::processKeyPress (Aspect_VKey theKey)
{
  switch (theKey)
  {
    case Aspect_VKey_F:
    {
      myView->FitAll (0.01, false);
      UpdateView();
      return true;
    }
  }
  return false;
}

// ================================================================
// Function : setCubemapBackground
// Purpose  :
// ================================================================
void WasmOcctView::setCubemapBackground (const std::string& theImagePath)
{
  if (!theImagePath.empty())
  {
    emscripten_async_wget (theImagePath.c_str(), "/emulated/cubemap.jpg", CubemapAsyncLoader::onImageRead, CubemapAsyncLoader::onImageFailed);
  }
  else
  {
    WasmOcctView::Instance().View()->SetBackgroundCubeMap (Handle(Graphic3d_CubeMapPacked)(), true, false);
    WasmOcctView::Instance().UpdateView();
  }
}

// ================================================================
// Function : fitAllObjects
// Purpose  :
// ================================================================
void WasmOcctView::fitAllObjects (bool theAuto)
{
  WasmOcctView& aViewer = Instance();
  if (theAuto)
  {
    aViewer.FitAllAuto (aViewer.Context(), aViewer.View());
  }
  else
  {
    aViewer.View()->FitAll (0.01, false);
  }
  aViewer.UpdateView();
}

// ================================================================
// Function : removeAllObjects
// Purpose  :
// ================================================================
void WasmOcctView::removeAllObjects()
{
  WasmOcctView& aViewer = Instance();
  for (NCollection_IndexedDataMap<TCollection_AsciiString, Handle(AIS_InteractiveObject)>::Iterator anObjIter (aViewer.myObjects);
       anObjIter.More(); anObjIter.Next())
  {
    aViewer.Context()->Remove (anObjIter.Value(), false);
  }
  aViewer.myObjects.Clear();
  aViewer.UpdateView();
}

// ================================================================
// Function : Perspective
// Purpose  :
// ================================================================
void WasmOcctView::projectionPerspective()
{
    WasmOcctView::Instance().View()->Camera()->SetProjectionType (Graphic3d_Camera::Projection_Perspective);
    WasmOcctView::Instance().UpdateView();
}

// ================================================================
// Function : Orthographic
// Purpose  :
// ================================================================
void WasmOcctView::projectionOrthographic()
{
    WasmOcctView::Instance().View()->Camera()->SetProjectionType (Graphic3d_Camera::Projection_Orthographic);
    WasmOcctView::Instance().UpdateView();
}


// ================================================================
// Function : Select Vertex Mode
// Purpose  :
// ================================================================
void WasmOcctView::selectVertexMode()
{
    AppManager& app = AppManager::GetInstance();
    app.SelectVertexMode();
}


// ================================================================
// Function : Select Edge Mode
// Purpose  :
// ================================================================
void WasmOcctView::selectEdgeMode()
{
    AppManager& app = AppManager::GetInstance();
    app.SelectEdgeMode();
}


// ================================================================
// Function : Select Face Mode
// Purpose  :
// ================================================================
void WasmOcctView::selectFaceMode()
{
    AppManager& app = AppManager::GetInstance();
    app.SelectFaceMode();
    // WasmOcctView& aViewer = Instance();
    // for (NCollection_IndexedDataMap<TCollection_AsciiString, Handle(AIS_InteractiveObject)>::Iterator anObjIter (aViewer.myObjects);
    //      anObjIter.More(); anObjIter.Next())
    // {
    //     aViewer.Context()->Deactivate(anObjIter.Value(), false);
    //     aViewer.Context()->Activate(anObjIter.Value(), AIS_Shape::SelectionMode(TopAbs_FACE));
    // }
}


// ================================================================
// Function : Select Solid Mode
// Purpose  :
// ================================================================
void WasmOcctView::selectSolidMode()
{
    AppManager& app = AppManager::GetInstance();
    app.SelectSolidMode();
    // WasmOcctView& aViewer = Instance();
    // for (NCollection_IndexedDataMap<TCollection_AsciiString, Handle(AIS_InteractiveObject)>::Iterator anObjIter (aViewer.myObjects);
    //      anObjIter.More(); anObjIter.Next())
    // {
    //     aViewer.Context()->Deactivate(anObjIter.Value(), false);
    //     aViewer.Context()->Activate(anObjIter.Value(), AIS_Shape::SelectionMode(TopAbs_SOLID));
    // }
}


// ================================================================
// Function : removeObject
// Purpose  :
// ================================================================
bool WasmOcctView::removeObject (const std::string& theName)
{
  WasmOcctView& aViewer = Instance();
  Handle(AIS_InteractiveObject) anObj;
  if (!theName.empty()
   && !aViewer.myObjects.FindFromKey (theName.c_str(), anObj))
  {
    return false;
  }

  aViewer.Context()->Remove (anObj, false);
  aViewer.myObjects.RemoveKey (theName.c_str());
  aViewer.UpdateView();
  return true;
}

// ================================================================
// Function : eraseObject
// Purpose  :
// ================================================================
bool WasmOcctView::eraseObject (const std::string& theName)
{
  WasmOcctView& aViewer = Instance();
  Handle(AIS_InteractiveObject) anObj;
  if (!theName.empty()
   && !aViewer.myObjects.FindFromKey (theName.c_str(), anObj))
  {
    return false;
  }

  aViewer.Context()->Erase (anObj, false);
  aViewer.UpdateView();
  return true;
}

// ================================================================
// Function : displayObject
// Purpose  :
// ================================================================
bool WasmOcctView::displayObject (const std::string& theName)
{
  WasmOcctView& aViewer = Instance();
  Handle(AIS_InteractiveObject) anObj;
  if (!theName.empty()
   && !aViewer.myObjects.FindFromKey (theName.c_str(), anObj))
  {
    return false;
  }

  aViewer.Context()->Display (anObj, false);
  aViewer.UpdateView();
  return true;
}

// ================================================================
// Function : openFromUrl
// Purpose  :
// ================================================================
void WasmOcctView::openFromUrl (const std::string& theName,
                                const std::string& theModelPath)
{
  ModelAsyncLoader* aTask = new ModelAsyncLoader (theName.c_str(), theModelPath.c_str());
  emscripten_async_wget_data (theModelPath.c_str(), (void* )aTask, ModelAsyncLoader::onDataRead, ModelAsyncLoader::onReadFailed);
}

// ================================================================
// Function : openFromMemory
// Purpose  :
// ================================================================
bool WasmOcctView::openFromMemory (const std::string& theName,
                                   uintptr_t theBuffer, int theDataLen,
                                   bool theToFree)
{
  removeObject (theName);
  char* aBytes = reinterpret_cast<char*>(theBuffer);
  if (aBytes == nullptr
   || theDataLen <= 0)
  {
    return false;
  }

  // Function to check if specified data stream starts with specified header.
  #define dataStartsWithHeader(theData, theHeader) (::strncmp(theData, theHeader, sizeof(theHeader) - 1) == 0)

  if (dataStartsWithHeader(aBytes, "DBRep_DrawableShape"))
  {
    return openBRepFromMemory (theName, theBuffer, theDataLen, theToFree);
  }
  else if (dataStartsWithHeader(aBytes, "glTF"))
  {
    //return openGltfFromMemory (theName, theBuffer, theDataLen, theToFree);
  }
  else if (dataStartsWithHeader(aBytes, "ISO-10303-21"))  // "as1-oc-214-mat.stp" starts with "ISO-10303-21"
  {
    //Message::DefaultMessenger()->Send(TCollection_AsciiString("step file ") + theName.c_str(), Message_Info);
    return openSTEPFromMemory(theName, theBuffer, theDataLen, theToFree);
  }

  if (theToFree)
  {
    free (aBytes);
  }

  Message::SendFail() << "Error: file '" << theName.c_str() << "' has unsupported format";
  return false;
}

// ================================================================
// Function : openSTEPFromMemory
// Purpose  :
// ================================================================
bool WasmOcctView::openSTEPFromMemory (const std::string& theName,
                                       uintptr_t theBuffer, int theDataLen,
                                       bool theToFree)
{
    AppManager& app = AppManager::GetInstance();

    char* aRawData = reinterpret_cast<char*>(theBuffer);
    Standard_ArrayStreamBuffer aStreamBuffer(aRawData, theDataLen);
    std::istream aStream(&aStreamBuffer);

    app.ImportGeometry(theName.c_str(), aStream, GeomFileType::STEP);    
    fitAllObjects(true);

    return true;
  /*
  removeObject (theName);

  WasmOcctView& aViewer = Instance();
  TopoDS_Shape aShape;
  BRep_Builder aBuilder;
  bool isLoaded = false;
  {
    char* aRawData = reinterpret_cast<char*>(theBuffer);
    Standard_ArrayStreamBuffer aStreamBuffer(aRawData, theDataLen);
    std::istream aStream(&aStreamBuffer);
    STEPControl_Reader aReader;
    IFSelect_ReturnStatus status = aReader.ReadStream(theName.c_str(), aStream);
	  if (status != IFSelect_RetDone) {
        return status;
    }

    // Root transfers
    Standard_Integer nbr = aReader.NbRootsForTransfer();
    Standard_Boolean failsonly = Standard_False;
    aReader.PrintCheckTransfer(failsonly, IFSelect_ItemsByEntity);
    for (Standard_Integer n = 1; n <= nbr; n++) {
      aReader.TransferRoot(n);
    }

    // Collecting resulting entities
    Standard_Integer nbs = aReader.NbShapes();
    Handle(TopTools_HSequenceOfShape) aSeqOfShape = new TopTools_HSequenceOfShape();

    if (nbs == 0) {
      Message::DefaultMessenger()->Send("No Shape", Message_Info);
      return IFSelect_RetVoid;
    }
    for (Standard_Integer i = 1; i <= nbs; i++) {
      aShape = aReader.Shape(i);
      aSeqOfShape->Append(aShape);
    }

    // if (!aSeqOfShape.IsNull())
    // {
    //   for (int i = 1; i <= aSeqOfShape->Length(); i++)
    //     WasmOcctView::Instance().Context()->Display(new AIS_Shape(aSeqOfShape->Value(i)), Standard_True);
    // }

    if (theToFree)
    {
      free (aRawData);
    }
    isLoaded = true;
  }
  if (!isLoaded)
  {
    return false;
  }

  Handle(AIS_Shape) aShapePrs = new AIS_Shape(aShape);
  if (!theName.empty())
  {
    aViewer.myObjects.Add (theName.c_str(), aShapePrs);
  }
  aShapePrs->SetMaterial (Graphic3d_NameOfMaterial_Silver);
  aViewer.Context()->Display (aShapePrs, AIS_Shaded, 0, false);
  aViewer.View()->FitAll(0.01, false);
  aViewer.UpdateView();

  Message::DefaultMessenger()->Send (TCollection_AsciiString("Loaded file ") + theName.c_str(), Message_Info);
  Message::DefaultMessenger()->Send (OSD_MemInfo::PrintInfo(), Message_Trace);
  return true;
  */


}

// ================================================================
// Function : openBRepFromMemory
// Purpose  :
// ================================================================
bool WasmOcctView::openBRepFromMemory (const std::string& theName,
                                       uintptr_t theBuffer, int theDataLen,
                                       bool theToFree)
{
  removeObject (theName);

  WasmOcctView& aViewer = Instance();
  TopoDS_Shape aShape;
  BRep_Builder aBuilder;
  bool isLoaded = false;
  {
    char* aRawData = reinterpret_cast<char*>(theBuffer);
    Standard_ArrayStreamBuffer aStreamBuffer (aRawData, theDataLen);
    std::istream aStream (&aStreamBuffer);
    BRepTools::Read (aShape, aStream, aBuilder);
    if (theToFree)
    {
      free (aRawData);
    }
    isLoaded = true;
  }
  if (!isLoaded)
  {
    return false;
  }

  Handle(AIS_Shape) aShapePrs = new AIS_Shape (aShape);
  if (!theName.empty())
  {
    aViewer.myObjects.Add (theName.c_str(), aShapePrs);
  }
  aShapePrs->SetMaterial (Graphic3d_NameOfMaterial_Silver);
  aViewer.Context()->Display (aShapePrs, AIS_Shaded, 0, false);
  aViewer.View()->FitAll (0.01, false);
  aViewer.UpdateView();

  Message::DefaultMessenger()->Send (TCollection_AsciiString("Loaded file ") + theName.c_str(), Message_Info);
  Message::DefaultMessenger()->Send (OSD_MemInfo::PrintInfo(), Message_Trace);
  return true;
}

// ================================================================
// Function : displayGround
// Purpose  :
// ================================================================
void WasmOcctView::displayGround (bool theToShow)
{
  static Handle(AIS_Shape) aGroundPrs = new AIS_Shape (TopoDS_Shape());

  WasmOcctView& aViewer = Instance();
  Bnd_Box aBox;
  if (theToShow)
  {
    aViewer.Context()->Remove (aGroundPrs, false);
    aBox = aViewer.View()->View()->MinMaxValues();
  }
  if (aBox.IsVoid()
  ||  aBox.IsZThin (Precision::Confusion()))
  {
    if (!aGroundPrs.IsNull()
      && aGroundPrs->HasInteractiveContext())
    {
      aViewer.Context()->Remove (aGroundPrs, false);
      aViewer.UpdateView();
    }
    return;
  }

  const gp_XYZ aSize   = aBox.CornerMax().XYZ() - aBox.CornerMin().XYZ();
  const double aRadius = Max (aSize.X(), aSize.Y());
  const double aZValue = aBox.CornerMin().Z() - Min (10.0, aSize.Z() * 0.01);
  const double aZSize  = aRadius * 0.01;
  gp_XYZ aGroundCenter ((aBox.CornerMin().X() + aBox.CornerMax().X()) * 0.5,
                        (aBox.CornerMin().Y() + aBox.CornerMax().Y()) * 0.5,
                         aZValue);

  TopoDS_Compound aGround;
  gp_Trsf aTrsf1, aTrsf2;
  aTrsf1.SetTranslation (gp_Vec (aGroundCenter - gp_XYZ(0.0, 0.0, aZSize)));
  aTrsf2.SetTranslation (gp_Vec (aGroundCenter));
  Prs3d_ToolCylinder aCylTool  (aRadius, aRadius, aZSize, 50, 1);
  Prs3d_ToolDisk     aDiskTool (0.0, aRadius, 50, 1);
  TopoDS_Face aCylFace, aDiskFace1, aDiskFace2;
  BRep_Builder().MakeFace (aCylFace,   aCylTool .CreatePolyTriangulation (aTrsf1));
  BRep_Builder().MakeFace (aDiskFace1, aDiskTool.CreatePolyTriangulation (aTrsf1));
  BRep_Builder().MakeFace (aDiskFace2, aDiskTool.CreatePolyTriangulation (aTrsf2));

  BRep_Builder().MakeCompound (aGround);
  BRep_Builder().Add (aGround, aCylFace);
  BRep_Builder().Add (aGround, aDiskFace1);
  BRep_Builder().Add (aGround, aDiskFace2);

  aGroundPrs->SetShape (aGround);
  aGroundPrs->SetToUpdate();
  aGroundPrs->SetMaterial (Graphic3d_NameOfMaterial_Stone);
  aGroundPrs->SetInfiniteState (false);
  aViewer.Context()->Display (aGroundPrs, AIS_Shaded, -1, false);
  aGroundPrs->SetInfiniteState (true);
  aViewer.UpdateView();
}

void WasmOcctView::MakeBox(const float* location)
{
    Handle(AIS_Shape) box = new AIS_Shape(BRepPrimAPI_MakeBox(50.0, 50.0, 50.0).Shape());
    gp_Trsf trsf;
    gp_Vec trans(location[0], location[1], location[2]);
    trsf.SetTranslation(trans);
    box->SetLocalTransformation(trsf);
    myContext->Display(box, Standard_True);
}

void WasmOcctView::MakeCone(const float* location)
{
    Handle(AIS_Shape) cone = new AIS_Shape(BRepPrimAPI_MakeCone(0.0, 50.0, 50.0).Shape());
    gp_Trsf trsf;
    gp_Vec trans(location[0], location[1], location[2]);
    trsf.SetTranslation(trans);
    cone->SetLocalTransformation(trsf);
    myContext->Display(cone, Standard_True);
}

void WasmOcctView::showScale()
{
    m_bShowScale = !m_bShowScale;

    Graphic3d_GraduatedTrihedron graduated = Graphic3d_GraduatedTrihedron("Arial", Font_FA_Bold, 15, "Arial", Font_FA_Regular, 15);

    if (m_bShowScale) {
        WasmOcctView::Instance().View()->GraduatedTrihedronDisplay(graduated);
        WasmOcctView::Instance().View()->Redraw();
    }
    else {
        WasmOcctView::Instance().View()->GraduatedTrihedronErase();
        WasmOcctView::Instance().View()->Redraw();
    }
}

// Module exports
EMSCRIPTEN_BINDINGS(OccViewerModule) {
  emscripten::function("setCubemapBackground", &WasmOcctView::setCubemapBackground);
  emscripten::function("fitAllObjects",    &WasmOcctView::fitAllObjects);
  emscripten::function("removeAllObjects", &WasmOcctView::removeAllObjects);
  emscripten::function("removeObject",     &WasmOcctView::removeObject);
  emscripten::function("eraseObject",      &WasmOcctView::eraseObject);
  emscripten::function("displayObject",    &WasmOcctView::displayObject);
  emscripten::function("displayGround",    &WasmOcctView::displayGround);
  emscripten::function("openFromUrl",      &WasmOcctView::openFromUrl);
  emscripten::function("openFromMemory",   &WasmOcctView::openFromMemory, emscripten::allow_raw_pointers());
  emscripten::function("openBRepFromMemory", &WasmOcctView::openBRepFromMemory, emscripten::allow_raw_pointers());
  emscripten::function("openSTEPFromMemory", &WasmOcctView::openSTEPFromMemory, emscripten::allow_raw_pointers());
  emscripten::function("projectionPerspective", &WasmOcctView::projectionPerspective);
  emscripten::function("projectionOrthographic", &WasmOcctView::projectionOrthographic);
  emscripten::function("selectVertexMode", &WasmOcctView::selectVertexMode);
  emscripten::function("selectEdgeMode", &WasmOcctView::selectEdgeMode);
  emscripten::function("selectFaceMode", &WasmOcctView::selectFaceMode);
  emscripten::function("selectSolidMode", &WasmOcctView::selectSolidMode);
  emscripten::function("showScale", &WasmOcctView::showScale);
}
