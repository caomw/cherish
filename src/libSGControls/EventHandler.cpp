#include "EventHandler.h"
#include <iostream>
#include <assert.h>
#include <osgViewer/View>
#include <osgUtil/LineSegmentIntersector>
#include <osgUtil/IntersectionVisitor>
#include <osg/Viewport>

#include "Utilities.h"

EventHandler::EventHandler(RootScene* scene, dureu::MOUSE_MODE mode)
    : osgGA::GUIEventHandler()
    , m_mode(mode)
    , m_scene(scene)
    , m_photo(0)
{
}

// handle() has to be re-defined, for more info, check
// OpenSceneGraph beginner's guide or
// OpenSceneGraph 3.0 Cookbook
// and search for custom event handler examples
bool EventHandler::handle(const osgGA::GUIEventAdapter &ea, osgGA::GUIActionAdapter &aa)
{    
    /* if it's mouse navigation mode, don't process event
     * it will be processed by Manipulator */

    if (m_mode == dureu::MOUSE_ORBIT || m_mode == dureu::MOUSE_PAN ||
            m_mode == dureu::MOUSE_ZOOM || m_mode == dureu::MOUSE_FIXEDVIEW)
        return false;

    if (! m_scene->getCanvasCurrent())
        return false;

    if (ea.getModKeyMask()&osgGA::GUIEventAdapter::MODKEY_CTRL && ea.getKey() == 'a')
        m_scene->getCanvasCurrent()->addStrokesSelectedAll();

    switch (m_mode){
    case dureu::MOUSE_SELECT:
        if (ea.getModKeyMask()&osgGA::GUIEventAdapter::MODKEY_CTRL)
            this->doSelectStroke<StrokeIntersector::Intersection, StrokeIntersector>(ea, aa);
        else
            this->doSelectEntity<osgUtil::LineSegmentIntersector::Intersection, osgUtil::LineSegmentIntersector>(ea, aa);
        break;
    case dureu::MOUSE_SKETCH:
        this->doSketch(ea, aa);
        break;
    case dureu::MOUSE_DELETE:
        if (ea.getModKeyMask()&osgGA::GUIEventAdapter::MODKEY_CTRL)
            this->doDeleteEntity(ea, aa);
        else
            this->doDeleteCanvas<osgUtil::LineSegmentIntersector::Intersection, osgUtil::LineSegmentIntersector>(ea, aa);
        break;
    case dureu::MOUSE_CANVAS_OFFSET:
        this->doEditCanvasOffset(ea, aa);
        break;
    case dureu::MOUSE_CANVAS_ROTATE:
        this->doEditCanvasRotate(ea, aa);
        break;
    case dureu::MOUSE_CANVAS_CLONE:
        this->doCanvasClone(ea, aa);
        break;
    case dureu::MOUSE_PHOTO_FLIPH:
        this->doEditPhotoFlip<osgUtil::LineSegmentIntersector::Intersection, osgUtil::LineSegmentIntersector>(ea, aa, true);
        break;
    case dureu::MOUSE_PHOTO_FLIPV:
        this->doEditPhotoFlip<osgUtil::LineSegmentIntersector::Intersection, osgUtil::LineSegmentIntersector>(ea, aa, false);
        break;
    case dureu::MOUSE_ENTITY_MOVE:
        if (m_scene->getCanvasCurrent()->getStrokesSelectedSize() > 0)
            this->doEditStrokesMove(ea, aa);
        else
            this->doEditPhotoMove<osgUtil::LineSegmentIntersector::Intersection, osgUtil::LineSegmentIntersector>(ea, aa);
        break;
    case dureu::MOUSE_ENTITY_SCALE:
        if (m_scene->getCanvasCurrent()->getStrokesSelectedSize() > 0)
            this->doEditStrokesScale(ea, aa);
        else
            this->doEditPhotoScale<osgUtil::LineSegmentIntersector::Intersection, osgUtil::LineSegmentIntersector>(ea, aa);
        break;
    case dureu::MOUSE_ENTITY_ROTATE:
        if (m_scene->getCanvasCurrent()->getStrokesSelectedSize() > 0)
            this->doEditStrokesRotate(ea, aa);
        else
            this->doEditPhotoRotate<osgUtil::LineSegmentIntersector::Intersection, osgUtil::LineSegmentIntersector>(ea, aa);
        break;
    default:
        break;
    }
    return false;
}

void EventHandler::setMode(dureu::MOUSE_MODE mode)
{
    m_mode = mode;
    switch (m_mode){
    case dureu::MOUSE_ENTITY_ROTATE:
    case dureu::MOUSE_ENTITY_SCALE:
    case dureu::MOUSE_PHOTO_FLIPH:
    case dureu::MOUSE_PHOTO_FLIPV:
    case dureu::MOUSE_ENTITY_MOVE:
        m_scene->setCanvasesButCurrent(false);
        break;
    default:
        m_scene->setCanvasesButCurrent(true);
        break;
    }
}

dureu::MOUSE_MODE EventHandler::getMode() const
{
    return m_mode;
}

/* Algorithm:
 * Get closest line segment out of Stroke
 * Pass that line segment to split stroke
 * That line segment will be deleted from stroke, and,
 * depending on its location inside the stroke, it will either
 * split the stroke, or it will erase one of its ends.
 * When stroke is split, need to see if both substrokes are long
 * enough to continue to exist.
*/
void EventHandler::doEraseStroke(entity::Stroke* stroke, int first, int last, dureu::EVENT event)
{
    /* THIS IS A DEBUG VERSION !!!!!!*/
    if (!stroke){
        outErrMsg("doEraseStroke: could not obtain stroke");
        return;
    }
    m_scene->eraseStroke(stroke, first, last, event);
}

void EventHandler::doSketch(const osgGA::GUIEventAdapter &ea, osgGA::GUIActionAdapter &aa)
{
    if (!( (ea.getEventType() == osgGA::GUIEventAdapter::PUSH && ea.getButtonMask()== osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
           || (ea.getEventType() == osgGA::GUIEventAdapter::DRAG && ea.getButtonMask()== osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
           || (ea.getEventType() == osgGA::GUIEventAdapter::RELEASE && ea.getButton()==osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
           ))
        return;

    double u=0, v=0;
    switch (ea.getEventType()){
    case osgGA::GUIEventAdapter::PUSH:
        if (!this->getRaytraceCanvasIntersection(ea,aa,u,v))
            return;
        m_scene->addStroke(u, v, dureu::EVENT_PRESSED);
        break;
    case osgGA::GUIEventAdapter::RELEASE:
        if (!this->getRaytraceCanvasIntersection(ea,aa,u,v))
            return;
        m_scene->addStroke(u,v, dureu::EVENT_RELEASED);
        break;
    case osgGA::GUIEventAdapter::DRAG:
        if (!this->getRaytraceCanvasIntersection(ea,aa,u,v))
            return;
        m_scene->addStroke(u,v,dureu::EVENT_DRAGGED);
        break;
    default:
        break;
    }
}

void EventHandler::doDeleteEntity(const osgGA::GUIEventAdapter &ea, osgGA::GUIActionAdapter &aa)
{
    if (ea.getEventType()!=osgGA::GUIEventAdapter::RELEASE || ea.getButton()!=osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
        return;

    /* see if there is a stroke or photo selected */
    osgUtil::LineSegmentIntersector::Intersection* result_photo = new osgUtil::LineSegmentIntersector::Intersection;
    StrokeIntersector::Intersection* result_stroke = new StrokeIntersector::Intersection;
    bool inter_photo = this->getLineIntersection<osgUtil::LineSegmentIntersector::Intersection, osgUtil::LineSegmentIntersector>
            (ea,aa, *result_photo);
    bool inter_stroke = this->getLineIntersection<StrokeIntersector::Intersection, StrokeIntersector>
            (ea,aa, *result_stroke);
    if (!inter_photo && !inter_stroke) {return;}
    if (inter_photo){
        outLogMsg("About to delete photo");
        entity::Photo* photo = this->getPhoto(*result_photo);
        if (!photo){
            outErrMsg("doDeleteEntity: could not obtain photo");
            return;
        }
        m_scene->editPhotoDelete(photo);
    }
    else{
        outLogMsg("About to delete stroke");
        entity::Stroke* stroke = this->getStroke(*result_stroke);
        if (!stroke){
            std::cerr << "doDeleteEntity(): could not dynamic_cast<Stroke*>" << std::endl;
            return;
        }
        m_scene->editStrokeDelete(stroke);
    }
}

template <typename T1, typename T2>
void EventHandler::doDeleteCanvas(const osgGA::GUIEventAdapter &ea, osgGA::GUIActionAdapter &aa)
{
    if (ea.getEventType()!=osgGA::GUIEventAdapter::RELEASE || ea.getButton()!=osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
        return;
    outLogMsg("doDeleteCanvas called");

    T1* result = new T1;
    bool intersected = this->getLineIntersection<T1, T2>(ea,aa, *result);
    if (!intersected)
        return;

    entity::Canvas* cnv = this->getCanvas(*result);
    if (!cnv){
        std::cerr << "doDeleteCanvas(): could not dynamic_cast<Canvas*>" << std::endl;
        return;
    }
    m_scene->editCanvasDelete(cnv);
}

void EventHandler::doEditCanvasOffset(const osgGA::GUIEventAdapter &ea, osgGA::GUIActionAdapter &aa)
{
    if (!( (ea.getEventType() == osgGA::GUIEventAdapter::PUSH && ea.getButtonMask()== osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
           || (ea.getEventType() == osgGA::GUIEventAdapter::DRAG && ea.getButtonMask()== osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
           || (ea.getEventType() == osgGA::GUIEventAdapter::RELEASE && ea.getButton()==osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
           ))
        return;

    osg::Vec3f XC = osg::Vec3f(0.f,0.f,0.f);
    switch (ea.getEventType()){
    case osgGA::GUIEventAdapter::PUSH:
        if (!this->getRaytraceNormalProjection(ea,aa,XC))
            return;
        m_scene->editCanvasOffset(XC, dureu::EVENT_PRESSED);
        break;
    case osgGA::GUIEventAdapter::RELEASE:
        if (!this->getRaytraceNormalProjection(ea,aa,XC))
            return;
        m_scene->editCanvasOffset(XC, dureu::EVENT_RELEASED);
        break;
    case osgGA::GUIEventAdapter::DRAG:
        if (!this->getRaytraceNormalProjection(ea,aa,XC))
            return;
        m_scene->editCanvasOffset(XC, dureu::EVENT_DRAGGED);
        break;
    default:
        break;
    }
}

void EventHandler::doEditCanvasRotate(const osgGA::GUIEventAdapter &ea, osgGA::GUIActionAdapter &aa)
{
    if (!( (ea.getEventType() == osgGA::GUIEventAdapter::PUSH && ea.getButtonMask()== osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
           || (ea.getEventType() == osgGA::GUIEventAdapter::DRAG && ea.getButtonMask()== osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
           || (ea.getEventType() == osgGA::GUIEventAdapter::RELEASE && ea.getButton()==osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
           ))
        return;

    //osg::Vec3f XC = osg::Vec3f(0.f,0.f,0.f);
    osg::Vec3f center = m_scene->getCanvasCurrent()->getCenter();
    osg::Vec3f rotaxis = osg::Vec3f(0.f, 1.f, 0.f) * m_scene->getCanvasCurrent()->getTransform()->getMatrix() - center;
    rotaxis.normalize();
    osg::Quat rot(dureu::PI/48, rotaxis);

    switch (ea.getEventType()){
    case osgGA::GUIEventAdapter::PUSH:
        //if (!this->getRaytraceNormalProjection(ea,aa,XC)) return;
        m_scene->editCanvasRotate(rot, dureu::EVENT_PRESSED);
        outLogVec("rotaxis", rotaxis.x(), rotaxis.y(), rotaxis.z());
        break;
    case osgGA::GUIEventAdapter::RELEASE:
        //if (!this->getRaytraceNormalProjection(ea,aa,XC)) return;
        m_scene->editCanvasRotate(rot, dureu::EVENT_RELEASED);
        outLogVec("rotaxis", rotaxis.x(), rotaxis.y(), rotaxis.z());
        break;
    case osgGA::GUIEventAdapter::DRAG:
        //if (!this->getRaytraceNormalProjection(ea,aa,XC))return;
        m_scene->editCanvasRotate(rot, dureu::EVENT_DRAGGED);
        break;
    default:
        break;
    }
}

void EventHandler::doCanvasClone(const osgGA::GUIEventAdapter &ea, osgGA::GUIActionAdapter &aa)
{
    if (!( (ea.getEventType() == osgGA::GUIEventAdapter::PUSH && ea.getButtonMask()== osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
           || (ea.getEventType() == osgGA::GUIEventAdapter::DRAG && ea.getButtonMask()== osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
           || (ea.getEventType() == osgGA::GUIEventAdapter::RELEASE && ea.getButton()==osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
           ))
        return;

    osg::Vec3f XC = osg::Vec3f(0.f,0.f,0.f);
    switch (ea.getEventType()){
    case osgGA::GUIEventAdapter::PUSH:
        if (!this->getRaytraceNormalProjection(ea,aa,XC)) return;
        m_scene->editCanvasClone(XC, dureu::EVENT_PRESSED);
        break;
    case osgGA::GUIEventAdapter::RELEASE:
        if (!this->getRaytraceNormalProjection(ea,aa,XC))
            this->finishAll();
        m_scene->editCanvasClone(XC, dureu::EVENT_RELEASED);
        break;
    case osgGA::GUIEventAdapter::DRAG:
        if (!this->getRaytraceNormalProjection(ea,aa,XC))
            this->finishAll();
        m_scene->editCanvasClone(XC, dureu::EVENT_DRAGGED);
        break;
    default:
        break;
    }
}

void EventHandler::doEditStrokesMove(const osgGA::GUIEventAdapter &ea, osgGA::GUIActionAdapter &aa)
{
    if (!( (ea.getEventType() == osgGA::GUIEventAdapter::PUSH && ea.getButtonMask()== osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
           || (ea.getEventType() == osgGA::GUIEventAdapter::DRAG && ea.getButtonMask()== osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
           || (ea.getEventType() == osgGA::GUIEventAdapter::RELEASE && ea.getButton()==osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
           ))
        return;

    /* if there are no strokes in canvas, return*/
    if (m_scene->getCanvasCurrent()->getStrokesSelectedSize() == 0){
        outErrMsg("doEditStrokesMove: there are no strokes to move");
        return;
    }

    /* if no strokes are selected, return */
    if (m_scene->getCanvasCurrent()->getStrokesSelectedSize() == 0)
        return;

    /* get mouse ray intersection with canvas plane */
    double u=0, v=0;
    if (!this->getRaytraceCanvasIntersection(ea,aa,u,v))
        return;

    switch (ea.getEventType()){
    case osgGA::GUIEventAdapter::PUSH:
        std::cout << "edit strokes move: push button" << std::endl;
        m_scene->editStrokesMove(u, v, dureu::EVENT_PRESSED);
        break;
    case osgGA::GUIEventAdapter::RELEASE:
        std::cout << "edit strokes move: release button" << std::endl;
        m_scene->editStrokesMove(u, v, dureu::EVENT_RELEASED);
        this->finishAll();
        break;
    case osgGA::GUIEventAdapter::DRAG:
        m_scene->editStrokesMove(u,v,dureu::EVENT_DRAGGED);
        break;
    default:
        break;
    }
}

void EventHandler::doEditStrokesScale(const osgGA::GUIEventAdapter &ea, osgGA::GUIActionAdapter &aa)
{
    if (!( (ea.getEventType() == osgGA::GUIEventAdapter::PUSH && ea.getButtonMask()== osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
           || (ea.getEventType() == osgGA::GUIEventAdapter::DRAG && ea.getButtonMask()== osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
           || (ea.getEventType() == osgGA::GUIEventAdapter::RELEASE && ea.getButton()==osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
           ))
        return;

    /* if there are no strokes in canvas, return*/
    if (m_scene->getCanvasCurrent()->getStrokesSelectedSize() == 0){
        outErrMsg("doEditStrokesMove: there are no strokes to move");
        return;
    }

    /* if no strokes are selected, return */
    if (m_scene->getCanvasCurrent()->getStrokesSelectedSize() == 0)
        return;

    /* get mouse ray intersection with canvas plane */
    double u=0, v=0;
    if (!this->getRaytraceCanvasIntersection(ea,aa,u,v))
        return;

    switch (ea.getEventType()){
    case osgGA::GUIEventAdapter::PUSH:
        std::cout << "edit strokes scale: push button" << std::endl;
        m_scene->editStrokesScale(u, v, dureu::EVENT_PRESSED);
        break;
    case osgGA::GUIEventAdapter::RELEASE:
        std::cout << "edit strokes scale: release button" << std::endl;
        m_scene->editStrokesScale(u, v, dureu::EVENT_RELEASED);
        this->finishAll();
        break;
    case osgGA::GUIEventAdapter::DRAG:
        m_scene->editStrokesScale(u,v,dureu::EVENT_DRAGGED);
        break;
    default:
        break;
    }
}

void EventHandler::doEditStrokesRotate(const osgGA::GUIEventAdapter &ea, osgGA::GUIActionAdapter &aa)
{
    if (!( (ea.getEventType() == osgGA::GUIEventAdapter::PUSH && ea.getButtonMask()== osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
           || (ea.getEventType() == osgGA::GUIEventAdapter::DRAG && ea.getButtonMask()== osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
           || (ea.getEventType() == osgGA::GUIEventAdapter::RELEASE && ea.getButton()==osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
           ))
        return;

    /* if there are no strokes in canvas, return*/
    if (m_scene->getCanvasCurrent()->getStrokesSelectedSize() == 0){
        outErrMsg("doEditStrokesMove: there are no strokes to move");
        return;
    }

    /* if no strokes are selected, return */
    if (m_scene->getCanvasCurrent()->getStrokesSelectedSize() == 0)
        return;

    /* get mouse ray intersection with canvas plane */
    double u=0, v=0;
    if (!this->getRaytraceCanvasIntersection(ea,aa,u,v))
        return;

    switch (ea.getEventType()){
    case osgGA::GUIEventAdapter::PUSH:
        std::cout << "edit strokes rotate: push button" << std::endl;
        m_scene->editStrokesRotate(u, v, dureu::EVENT_PRESSED);
        break;
    case osgGA::GUIEventAdapter::RELEASE:
        std::cout << "edit strokes rotate: release button" << std::endl;
        m_scene->editStrokesRotate(u, v, dureu::EVENT_RELEASED);
        this->finishAll();
        break;
    case osgGA::GUIEventAdapter::DRAG:
        m_scene->editStrokesRotate(u,v,dureu::EVENT_DRAGGED);
        break;
    default:
        break;
    }
}

/* Pick photo, obtain canvas that contains that photo;
 * Make that canvas current;
 * Change photo coordinates to [x, y];
 * Change photo colors when pushed (edit color) and released (no color)
*/
template <typename T1, typename T2>
void EventHandler::doEditPhotoMove(const osgGA::GUIEventAdapter &ea, osgGA::GUIActionAdapter &aa)
{
    if (!( (ea.getEventType() == osgGA::GUIEventAdapter::PUSH && ea.getButtonMask()== osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
           || (ea.getEventType() == osgGA::GUIEventAdapter::DRAG && ea.getButtonMask()== osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
           || (ea.getEventType() == osgGA::GUIEventAdapter::RELEASE && ea.getButton()==osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
           ))
        return;

    T1* result = new T1;
    bool intersects = this->getLineIntersection<T1, T2>(ea,aa, *result);

    /* pick photo to track */
    if (ea.getEventType() == osgGA::GUIEventAdapter::PUSH){
        if (!intersects)
            return;
        m_photo = this->getPhoto(*result);
    }

    if (!m_photo){
        std::cerr << "doEditPhotoMove(): no selected photo" << std::endl;
        return;
    }

    double u=0, v=0;
    if (!this->getRaytraceCanvasIntersection(ea,aa,u,v))
        return;

    switch (ea.getEventType()){
    case osgGA::GUIEventAdapter::PUSH:
        std::cout << "edit photo move: push button" << std::endl;
        m_scene->editPhotoMove(m_photo.get(), u, v, dureu::EVENT_PRESSED);
        break;
    case osgGA::GUIEventAdapter::RELEASE:
        std::cout << "edit photo move: release button" << std::endl;
        m_scene->editPhotoMove(m_photo.get(), u, v, dureu::EVENT_RELEASED);
        this->finishAll();
        break;
    case osgGA::GUIEventAdapter::DRAG:
        m_scene->editPhotoMove(m_photo.get(), u, v, dureu::EVENT_DRAGGED);
        break;
    default:
        break;
    }
}

template <typename T1, typename T2>
void EventHandler::doEditPhotoFlip(const osgGA::GUIEventAdapter &ea, osgGA::GUIActionAdapter &aa, bool horizontal)
{
    if (ea.getEventType()!=osgGA::GUIEventAdapter::RELEASE || ea.getButton()!=osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
        return;

    T1* result = new T1;
    if (!this->getLineIntersection<T1, T2>(ea,aa, *result))
        return;

    entity::Photo* photo = getPhoto(*result);
    if (!photo){
        std::cerr << "doEditPhotoMove(): could not dynamic_cast<Photo*>" << std::endl;
        return;
    }

    m_scene->editPhotoFlip(photo, horizontal);
}

entity::Stroke *EventHandler::getStroke(const StrokeIntersector::Intersection &result)
{
    return dynamic_cast<entity::Stroke*>(result.drawable.get());
}

entity::Canvas *EventHandler::getCanvas(const osgUtil::LineSegmentIntersector::Intersection &result){
    if (result.drawable->getName() != "Pickable")
        return NULL;
    return dynamic_cast<entity::Canvas*>(result.nodePath.at(m_scene->getCanvasLevel()));
}

entity::Photo *EventHandler::getPhoto(const osgUtil::LineSegmentIntersector::Intersection &result)
{
    return dynamic_cast<entity::Photo*>(result.drawable.get());
}

template <typename T1, typename T2>
bool EventHandler::getLineIntersection(const osgGA::GUIEventAdapter &ea,
                                        osgGA::GUIActionAdapter &aa,
                                        T1& result)
{
    osgViewer::View* viewer = dynamic_cast<osgViewer::View*>(&aa);
    if (!viewer){
        outErrMsg("getLineIntersection(): could not retrieve viewer");
        return false;
    }
    osg::ref_ptr<T2> intersector = new T2(osgUtil::Intersector::WINDOW, ea.getX(), ea.getY());

    osgUtil::IntersectionVisitor iv(intersector);
    iv.setTraversalMask(~0x1);
    osg::Camera* cam = viewer->getCamera();
    if (!cam){
        std::cerr << "getLineIntersection(): could not read camera" << std::endl;
        return false;
    }
    cam->accept(iv);
    if (!intersector->containsIntersections()){
        outLogMsg("getLineIntersection(): no intersections found");
        return false;
    }
    result = *(intersector->getIntersections().begin());
    return true;
}

/* Algorithm:
 * use ray-tracking techinique
 * calcualte near and far point in global 3D
 * intersect that segment with plane of canvas - 3D intersection point
 * extract local 3D coords so that to create a stroke (or apprent that point to a current stroke)
 */
bool EventHandler::getRaytraceCanvasIntersection(const osgGA::GUIEventAdapter& ea, osgGA::GUIActionAdapter& aa,
                                                 double &u, double &v)
{
    /* get view-projection-world matrix and its inverse*/
    osg::Matrix VPW, invVPW;
    if (!Utilities::getViewProjectionWorld(ea, aa, VPW, invVPW))
        return false;

    /* get far and near in global 3D coords */
    osg::Vec3f nearPoint, farPoint;
    Utilities::getFarNear(ea.getX(), ea.getY(), invVPW, nearPoint, farPoint);

    /* get intersection point in global 3D coords */
    osg::Vec3f P;
    const osg::Plane plane = m_scene->getCanvasCurrent()->getPlane();
    const osg::Vec3f center = m_scene->getCanvasCurrent()->getCenter();
    if (!Utilities::getRayPlaneIntersection(plane, center, nearPoint, farPoint, P))
        this->finishAll();

    /* get model matrix and its inverse */
    osg::Matrix M, invM;
    if (!Utilities::getModel(m_scene->getCanvasCurrent(), M, invM))
        return false;

    /* obtain intersection in local 2D point */
    osg::Vec3f p;
    if (!Utilities::getLocalFromGlobal(P, invM, p))
        return false;

    u=p.x();
    v=p.y();
    return true;
}

/* Algorithm:
 * Cast the ray into 3D space
 * Make sure the ray is not parallel to the normal
 * The new offset point will be located on the projected point
 * between the ray and canvas normal.
 * Ray and normal are skew lines in 3d space, so we only need
 * to extract the projection point of the ray into the normal.
*/
bool EventHandler::getRaytraceNormalProjection(const osgGA::GUIEventAdapter &ea, osgGA::GUIActionAdapter &aa, osg::Vec3f& XC)
{
    osg::Matrix VPW, invVPW;
    if (!Utilities::getViewProjectionWorld(ea, aa, VPW, invVPW))
        return false;

    osg::Vec3f nearPoint, farPoint;
    Utilities::getFarNear(ea.getX(), ea.getY(), invVPW, nearPoint, farPoint);

    osg::Vec3f C = m_scene->getCanvasCurrent()->getCenter();
    osg::Vec3f N = m_scene->getCanvasCurrent()->getNormal();

    osg::Vec3f X1;
    if (!Utilities::getSkewLinesProjection(C, farPoint, nearPoint, N, X1)){
        this->finishAll();
        return false;
    }
    XC = X1 - C;
    return true;
}

/* If any entity is in edit mode,
 * finish the edit command.
 * For example, if photo was in edit mode, set it to normal mode
 * This function should be called only when an intersector fails to
 * find an intersection. It can happen when, for example, the mouse goes
 * off-plane mode, or the mouse movement is too fast and the photo is not
 * tracked under the cursor.
 */
void EventHandler::finishAll()
{
    switch (m_mode)
    {
    case dureu::MOUSE_SKETCH:
        m_scene->addStroke(0,0, dureu::EVENT_OFF);
        break;
    case dureu::MOUSE_CANVAS_OFFSET:
        m_scene->editCanvasOffset(osg::Vec3f(0,0,0), dureu::EVENT_OFF);
        break;
    case dureu::MOUSE_CANVAS_ROTATE:
        m_scene->editCanvasOffset(osg::Vec3f(0,0,0), dureu::EVENT_OFF);
        break;
    case dureu::MOUSE_CANVAS_CLONE:
        m_scene->editCanvasClone(osg::Vec3f(0,0,0), dureu::EVENT_OFF);
        break;
    case dureu::MOUSE_ENTITY_MOVE:
        m_scene->editPhotoMove(0,0,0, dureu::EVENT_OFF);
        m_scene->editStrokesMove(0,0, dureu::EVENT_OFF);
        break;
    case dureu::MOUSE_ENTITY_SCALE:
        m_scene->editPhotoScale(0,0,0, dureu::EVENT_OFF);
        m_scene->editStrokesScale(0,0, dureu::EVENT_OFF);
        break;
    case dureu::MOUSE_ENTITY_ROTATE:
        m_scene->editPhotoRotate(0,0,0, dureu::EVENT_OFF);
        m_scene->editStrokesRotate(0,0, dureu::EVENT_OFF);
        break;
    default:
        break;
    }
    m_photo = 0;
}

template <typename T1, typename T2>
void EventHandler::doEditPhotoScale(const osgGA::GUIEventAdapter &ea, osgGA::GUIActionAdapter &aa)
{
    if (!( (ea.getEventType() == osgGA::GUIEventAdapter::PUSH && ea.getButtonMask()== osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
           || (ea.getEventType() == osgGA::GUIEventAdapter::DRAG && ea.getButtonMask()== osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
           || (ea.getEventType() == osgGA::GUIEventAdapter::RELEASE && ea.getButton()==osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
           ))
        return;

    T1* result = new T1;
    bool intersects = this->getLineIntersection<T1, T2>(ea,aa, *result);

    /* pick photo to track */
    if (ea.getEventType() == osgGA::GUIEventAdapter::PUSH){
        if (!intersects)
            return;
        m_photo = this->getPhoto(*result);
    }

    if (!m_photo){
        std::cerr << "doEditPhotoScale(): no selected photo" << std::endl;
        return;
    }

    double u=0, v=0;
    if (!this->getRaytraceCanvasIntersection(ea,aa,u,v))
        return;

    switch (ea.getEventType()){
    case osgGA::GUIEventAdapter::PUSH:
        std::cout << "scale: push button" << std::endl;
        m_scene->editPhotoScale(m_photo.get(), u, v, dureu::EVENT_PRESSED);
        break;
    case osgGA::GUIEventAdapter::RELEASE:
        std::cout << "scale: release button" << std::endl;
        m_scene->editPhotoScale(m_photo.get(), u, v, dureu::EVENT_RELEASED);
        this->finishAll();
        break;
    case osgGA::GUIEventAdapter::DRAG:
        m_scene->editPhotoScale(m_photo.get(), u, v, dureu::EVENT_DRAGGED);
        break;
    default:
        break;
    }
}

template <typename T1, typename T2>
void EventHandler::doEditPhotoRotate(const osgGA::GUIEventAdapter &ea, osgGA::GUIActionAdapter &aa)
{
    if (!( (ea.getEventType() == osgGA::GUIEventAdapter::PUSH && ea.getButtonMask()== osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
           || (ea.getEventType() == osgGA::GUIEventAdapter::DRAG && ea.getButtonMask()== osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
           || (ea.getEventType() == osgGA::GUIEventAdapter::RELEASE && ea.getButton()==osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
           ))
        return;

    T1* result = new T1;
    bool intersects = this->getLineIntersection<T1, T2>(ea,aa, *result);

    /* pick photo to track */
    if (ea.getEventType() == osgGA::GUIEventAdapter::PUSH){
        if (!intersects)
            return;
        m_photo = this->getPhoto(*result);
    }

    double u=0, v=0;
    if (!this->getRaytraceCanvasIntersection(ea,aa,u,v))
        return;

    switch (ea.getEventType()){
    case osgGA::GUIEventAdapter::PUSH:
        std::cout << "scale: push button" << std::endl;
        m_scene->editPhotoRotate(m_photo.get(), u, v, dureu::EVENT_PRESSED);
        break;
    case osgGA::GUIEventAdapter::RELEASE:
        std::cout << "scale: release button" << std::endl;
        m_scene->editPhotoRotate(m_photo.get(), u, v, dureu::EVENT_RELEASED);
        this->finishAll();
        break;
    case osgGA::GUIEventAdapter::DRAG:
        m_scene->editPhotoRotate(m_photo.get(), u, v, dureu::EVENT_DRAGGED);
        break;
    default:
        break;
    }

}

template <typename T1, typename T2>
void EventHandler::doSelectStroke(const osgGA::GUIEventAdapter &ea, osgGA::GUIActionAdapter &aa)
{
    if (ea.getEventType() == osgGA::GUIEventAdapter::KEYDOWN)
        m_scene->getCanvasCurrent()->resetStrokesSelected();

    if (ea.getEventType()!=osgGA::GUIEventAdapter::RELEASE || ea.getButton()!=osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
        return;

    outLogMsg("Attempting to select a stroke");
    T1* result = new T1;
    bool intersected = this->getLineIntersection<T1, T2>(ea,aa, *result);
    if (!intersected){
        outLogMsg("No strokes intersection found");
        return;
    }

    entity::Stroke* stroke = this->getStroke(*result);
    if (!stroke){
        std::cerr << "doSelectStroke(): could not dynamic_cast<Stroke*>" << std::endl;
        return;
    }
    m_scene->getCanvasCurrent()->addStrokesSelected(stroke);
}

template <typename T1, typename T2>
void EventHandler::doSelectEntity(const osgGA::GUIEventAdapter &ea, osgGA::GUIActionAdapter &aa)
{
    if (ea.getEventType()!=osgGA::GUIEventAdapter::RELEASE || ea.getButton()!=osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
        return;

    T1* result = new T1;
    bool intersected = this->getLineIntersection<T1, T2>(ea,aa, *result);
    if (!intersected)
        return;

    entity::Canvas* cnv = this->getCanvas(*result);
    if (!cnv){
        std::cerr << "doPickCanvas(): could not dynamic_cast<Canvas*>" << std::endl;
        return;
    }
    std::cout << "doPickCanvas(): assumed canvas with name: " << cnv->getName() << std::endl;
    m_scene->setCanvasCurrent(cnv);
}
