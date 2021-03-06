#include "EventHandler.h"
#include <iostream>
#include <assert.h>
#include <cerrno>

#include <osgViewer/View>
#include <osgUtil/LineSegmentIntersector>
#include <osgUtil/IntersectionVisitor>
#include <osg/Viewport>

#include <QDebug>

#include "Utilities.h"
#include "LineIntersector.h"

EventHandler::EventHandler(GLWidget *widget, RootScene* scene, cher::MOUSE_MODE mode)
    : osgGA::GUIEventHandler()
    , m_glWidget(widget)
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
    if ((cher::maskMouse & m_mode) == cher::MOUSE_CAMERA)
        return false;

    if (! m_scene->getCanvasCurrent())
        return false;

    // key calls like this shoule be processed from GLWidget KeyPressed
    //if (ea.getModKeyMask()&osgGA::GUIEventAdapter::MODKEY_CTRL && ea.getKey() == 'a')
    //    m_scene->getCanvasCurrent()->addStrokesSelectedAll();

    switch (cher::maskMouse & m_mode){
    case cher::MOUSE_PEN:
        switch (m_mode) {
        case cher::PEN_SKETCH:
            this->doSketch(ea, aa);
            break;
        case cher::PEN_ERASE:
            break;
        case cher::PEN_DELETE:
            this->doDeleteEntity(ea, aa);
            break;
        default:
            break;
        }
        break;
    case cher::MOUSE_SELECT:
        switch (cher::maskEntity & m_mode){
        case cher::SELECT_ENTITY:
            switch (cher::maskAction & m_mode) {
            case (cher::maskAction & cher::ENTITY_MOVE):
                this->doEditEntitiesMove(ea, aa);
                break;
            case (cher::maskAction & cher::ENTITY_ROTATE):
                this->doEditEntitiesRotate(ea, aa);
                break;
            case (cher::maskAction & cher::ENTITY_SCALE):
                this->doEditEntitiesScale(ea, aa);
                break;
            default:
                this->doSelectEntity(ea, aa);
                break;
            }
            break;
        case cher::SELECT_CANVAS:
            this->doSelectCanvas<osgUtil::LineSegmentIntersector::Intersection, osgUtil::LineSegmentIntersector>(ea, aa);
            break;
        default:
            break;
        }
        break;
    case cher::MOUSE_CREATE:
        if (m_mode == cher::CREATE_CANVASCLONE){
            this->doCanvasClone(ea, aa);
        }
        else if (m_mode == cher::CREATE_CANVASSEPARATE){
            this->doCanvasSeparate(ea, aa);
        }
        break;
    case cher::MOUSE_CANVAS:
        switch (m_mode){
        case cher::CANVAS_OFFSET:
            this->doEditCanvasOffset(ea, aa);
            break;
        case cher::CANVAS_ROTATE_UPLUS:
            this->doEditCanvasRotate(ea, aa, osg::Vec3f(0,1,0), osg::Vec3f(-1,0,0));
            break;
        case cher::CANVAS_ROTATE_UMINUS:
            this->doEditCanvasRotate(ea, aa, osg::Vec3f(0,1,0), osg::Vec3f(1,0,0));
            break;
        case cher::CANVAS_ROTATE_VPLUS:
            this->doEditCanvasRotate(ea, aa, osg::Vec3f(1,0,0), osg::Vec3f(0,1,0));
            break;
        case cher::CANVAS_ROTATE_VMINUS:
            this->doEditCanvasRotate(ea, aa, osg::Vec3f(1,0,0), osg::Vec3f(0,-1,0));
            break;
        default:
            this->doEditCanvas(ea, aa);
            break;
        }
        break;
    default:
        break;
    }

//    switch (m_mode){
//    case cher::PHOTO_PUSH:
//        this->doEditPhotoPush<osgUtil::LineSegmentIntersector::Intersection, osgUtil::LineSegmentIntersector>(ea, aa);
//        break;
//    default:
//        break;
//    }
    return false;
}

void EventHandler::setMode(cher::MOUSE_MODE mode)
{
    m_mode = mode;

    entity::Canvas* cnv = m_scene->getCanvasCurrent();

    // TODO: move the below content to GLWidget's function
    switch (m_mode){
    case cher::ENTITY_ROTATE:
    case cher::ENTITY_SCALE:
    case cher::ENTITY_FLIPH:
    case cher::ENTITY_FLIPV:
    case cher::PHOTO_PUSH:
    case cher::ENTITY_MOVE:
    case cher::PEN_ERASE:
    case cher::PEN_DELETE:
    case cher::SELECT_ENTITY:
        m_scene->setCanvasesButCurrent(false);
        break;
    default:
        m_scene->setCanvasesButCurrent(true);
        break;
    }

    if (cnv) {
        if ((cher::maskMouse & m_mode) == cher::MOUSE_CANVAS){
            m_scene->getCanvasCurrent()->unselectAll();
            m_scene->getCanvasCurrent()->setModeEdit(true);
        }
        else{
            m_scene->getCanvasCurrent()->setModeEdit(false);
        }
        m_scene->getCanvasCurrent()->updateFrame(m_scene->getCanvasPrevious());
    }
}

cher::MOUSE_MODE EventHandler::getMode() const
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
void EventHandler::doEraseStroke(entity::Stroke* stroke, int first, int last, cher::EVENT event)
{
    /* THIS IS A DEBUG VERSION !!!!!!*/
    if (!stroke){
        qWarning("doEraseStroke: could not obtain stroke");
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
        m_scene->addStroke(u, v, cher::EVENT_PRESSED);
        break;
    case osgGA::GUIEventAdapter::RELEASE:
        if (!this->getRaytraceCanvasIntersection(ea,aa,u,v))
            return;
        m_scene->addStroke(u,v, cher::EVENT_RELEASED);
        break;
    case osgGA::GUIEventAdapter::DRAG:
        if (!this->getRaytraceCanvasIntersection(ea,aa,u,v))
            return;
        m_scene->addStroke(u,v,cher::EVENT_DRAGGED);
        break;
    default:
        break;
    }
}

/* Deletes an entity within a current canvas - stroke or image.
 * If it's left mouse drag-and-drop, then it searches for strokes to delete;
 * If it's right mouse release, then it searches for images to delete.
 */
void EventHandler::doDeleteEntity(const osgGA::GUIEventAdapter &ea, osgGA::GUIActionAdapter &aa)
{
    if (!( (ea.getEventType() == osgGA::GUIEventAdapter::PUSH && ea.getButtonMask()== osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
           || (ea.getEventType() == osgGA::GUIEventAdapter::DRAG && ea.getButtonMask()== osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
           || (ea.getEventType() == osgGA::GUIEventAdapter::RELEASE && ea.getButton()==osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
           || (ea.getEventType() == osgGA::GUIEventAdapter::RELEASE && ea.getButton() == osgGA::GUIEventAdapter::RIGHT_MOUSE_BUTTON)
           ))
        return;

    if (ea.getEventType()==osgGA::GUIEventAdapter::RELEASE && ea.getButton()==osgGA::GUIEventAdapter::RIGHT_MOUSE_BUTTON){
        qDebug("searching for photo to delete");
        osgUtil::LineSegmentIntersector::Intersection result_photo;
        bool inter_photo = this->getIntersection<osgUtil::LineSegmentIntersector::Intersection, osgUtil::LineSegmentIntersector>
                (ea,aa,cher::MASK_CANVAS_IN, result_photo);
        if (!inter_photo) return;
        entity::Photo* photo = this->getPhoto(result_photo);
        if (!photo) return;
        m_scene->editPhotoDelete(photo, m_scene->getCanvasCurrent());
    }
    else{
        /* see if there is a stroke */
        StrokeIntersector::Intersection result_stroke;
        bool inter_stroke = this->getIntersection<StrokeIntersector::Intersection, StrokeIntersector>
                (ea,aa,cher::MASK_CANVAS_IN, result_stroke);
        if (!inter_stroke) return;
        entity::Stroke* stroke = this->getStroke(result_stroke);
        if (!stroke) return;
        m_scene->editStrokeDelete(stroke);
    }
}

void EventHandler::doEditCanvas(const osgGA::GUIEventAdapter &ea, osgGA::GUIActionAdapter &aa)
{
    entity::Canvas* canvas = m_scene->getCanvasCurrent();
    if (!canvas) return;

    /* check if it is offset or rotate or normal mode */
    if (!this->setSubMouseMode<LineIntersector::Intersection, LineIntersector>(
                ea, aa, cher::MOUSE_CANVAS, true))
        return;

    /* when canvas frame is normal, do nothing */
}

void EventHandler::doEditCanvasOffset(const osgGA::GUIEventAdapter &ea, osgGA::GUIActionAdapter &aa)
{
    entity::Canvas* canvas = m_scene->getCanvasCurrent();
    if (!canvas) return;

    /* check if it is offset or rotate or normal mode */
    if (!this->setSubMouseMode<LineIntersector::Intersection, LineIntersector>(
                ea, aa, cher::MOUSE_CANVAS, true))
        return;

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
        m_scene->editCanvasOffset(XC, cher::EVENT_PRESSED);
        break;
    case osgGA::GUIEventAdapter::RELEASE:
        if (!this->getRaytraceNormalProjection(ea,aa,XC))
            return;
        m_scene->editCanvasOffset(XC, cher::EVENT_RELEASED);
        break;
    case osgGA::GUIEventAdapter::DRAG:
        if (!this->getRaytraceNormalProjection(ea,aa,XC))
            return;
        m_scene->editCanvasOffset(XC, cher::EVENT_DRAGGED);
        break;
    default:
        break;
    }
}

void EventHandler::doEditCanvasRotate(const osgGA::GUIEventAdapter &ea, osgGA::GUIActionAdapter &aa, osg::Vec3f alongAxis, osg::Vec3f rotAxis)
{
    entity::Canvas* canvas = m_scene->getCanvasCurrent();
    if (!canvas) return;

    /* check if it is offset or rotate or normal mode */
    if (!this->setSubMouseMode<LineIntersector::Intersection, LineIntersector>(
                ea, aa, cher::MOUSE_CANVAS, true))
        return;

    if (!( (ea.getEventType() == osgGA::GUIEventAdapter::PUSH && ea.getButtonMask()== osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
           || (ea.getEventType() == osgGA::GUIEventAdapter::DRAG && ea.getButtonMask()== osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
           || (ea.getEventType() == osgGA::GUIEventAdapter::RELEASE && ea.getButton()==osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
           ))
        return;

    osg::Vec3f P = osg::Vec3f(0.f,0.f,0.f);
    osg::Vec3f center = m_scene->getCanvasCurrent()->getCenter();
    osg::Matrix M = m_scene->getCanvasCurrent()->getTransform()->getMatrix();

    alongAxis = alongAxis * M - center;
    alongAxis.normalize();
    if (!this->getRaytracePlaneIntersection(ea, aa, alongAxis, P))
        return;

    rotAxis = rotAxis * M - center;
    rotAxis.normalize();

    osg::Vec3f new_axis = P - center;
    new_axis.normalize();

    double atheta = rotAxis * new_axis;
    if (atheta > 1 && atheta < -1) atheta = 1;

    errno = 0;
    double theta = std::acos(atheta); // they are already normalized
    if (errno == EDOM)
        theta = 0;

    /*check for domain errors */
    if (theta < 0 || theta > cher::PI){
        qWarning("doEditCanvasRotate: rotation angle domain error. Fixing.");
        theta = 0;
    }

    /* need to figure out direction of rotation
     * http://stackoverflow.com/questions/11022446/direction-of-shortest-rotation-between-two-vectors */
    osg::Vec3f r = rotAxis ^ new_axis;
    double sign = r * alongAxis;
    theta *= (sign<0? -1 : 1);
    if (std::fabs(theta) > cher::PI){
        qWarning("doEditCanvasRotate: theta is out of range. Fixing.");
        theta = 0;
    }

    osg::Quat rot(theta, alongAxis);

    osg::Vec3f center3d = m_scene->getCanvasCurrent()->getBoundingBoxCenter3D();

    switch (ea.getEventType()){
    case osgGA::GUIEventAdapter::PUSH:
        outLogVec("canvas-rotate pressed, quat", rot.x(), rot.y(), rot.z());
        qDebug() << "canvas-rotate pressed, quat.w " << rot.w();
        m_scene->editCanvasRotate(rot, center3d, cher::EVENT_PRESSED);
        break;
    case osgGA::GUIEventAdapter::RELEASE:
        outLogVec("canvas-rotate released, quat", rot.x(), rot.y(), rot.z());
        qDebug() << "canvas-rotate released, quat.w " << rot.w();
        m_scene->editCanvasRotate(rot, center3d, cher::EVENT_RELEASED);
        break;
    case osgGA::GUIEventAdapter::DRAG:
        m_scene->editCanvasRotate(rot, center3d, cher::EVENT_DRAGGED);
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
        m_scene->editCanvasClone(XC, cher::EVENT_PRESSED);
        break;
    case osgGA::GUIEventAdapter::RELEASE:
        if (!this->getRaytraceNormalProjection(ea,aa,XC))
            this->finishAll();
        m_scene->editCanvasClone(XC, cher::EVENT_RELEASED);
        break;
    case osgGA::GUIEventAdapter::DRAG:
        if (!this->getRaytraceNormalProjection(ea,aa,XC))
            this->finishAll();
        m_scene->editCanvasClone(XC, cher::EVENT_DRAGGED);
        break;
    default:
        break;
    }
}

void EventHandler::doCanvasSeparate(const osgGA::GUIEventAdapter &ea, osgGA::GUIActionAdapter &aa)
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
        m_scene->editCanvasSeparate(XC, cher::EVENT_PRESSED);
        break;
    case osgGA::GUIEventAdapter::RELEASE:
        if (!this->getRaytraceNormalProjection(ea,aa,XC))
            this->finishAll();
        m_scene->editCanvasSeparate(XC, cher::EVENT_RELEASED);
        m_glWidget->setMouseMode(cher::SELECT_ENTITY);
        break;
    case osgGA::GUIEventAdapter::DRAG:
        if (!this->getRaytraceNormalProjection(ea,aa,XC))
            this->finishAll();
        m_scene->editCanvasSeparate(XC, cher::EVENT_DRAGGED);
        break;
    default:
        break;
    }
}

void EventHandler::doEditEntitiesMove(const osgGA::GUIEventAdapter &ea, osgGA::GUIActionAdapter &aa)
{
//    if (!this->setSubSelectionType(ea, aa)) return;
    entity::Canvas* canvas = m_scene->getCanvasCurrent();
    if (!canvas) return;
    if (!this->setSubMouseMode<osgUtil::LineSegmentIntersector::Intersection, osgUtil::LineSegmentIntersector>(
                ea, aa, cher::SELECT_ENTITY, canvas->isEntitiesSelected()))
        return;

    if (!( (ea.getEventType() == osgGA::GUIEventAdapter::PUSH && ea.getButtonMask()== osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
           || (ea.getEventType() == osgGA::GUIEventAdapter::DRAG && ea.getButtonMask()== osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
           || (ea.getEventType() == osgGA::GUIEventAdapter::RELEASE && ea.getButton()==osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
           ))
        return;

    /* if there are no strokes in canvas, return*/
    if (m_scene->getCanvasCurrent()->getEntitiesSelectedSize() == 0){
        qWarning("doEditEntitiesMove: there are no strokes to move");
        return;
    }

    /* if no strokes are selected, return */
    if (m_scene->getCanvasCurrent()->getEntitiesSelectedSize() == 0)
        return;

    /* get mouse ray intersection with canvas plane */
    double u=0, v=0;
    if (!this->getRaytraceCanvasIntersection(ea,aa,u,v))
        return;

    switch (ea.getEventType()){
    case osgGA::GUIEventAdapter::PUSH:
        qDebug("edit strokes move: push button");
        m_scene->editStrokesMove(u, v, cher::EVENT_PRESSED);
        break;
    case osgGA::GUIEventAdapter::RELEASE:
        qDebug("edit strokes move: release button");
        m_scene->editStrokesMove(u, v, cher::EVENT_RELEASED);
        this->finishAll();
        break;
    case osgGA::GUIEventAdapter::DRAG:
        m_scene->editStrokesMove(u,v,cher::EVENT_DRAGGED);
        break;
    default:
        break;
    }
}

void EventHandler::doEditEntitiesScale(const osgGA::GUIEventAdapter &ea, osgGA::GUIActionAdapter &aa)
{
//    if (!this->setSubSelectionType(ea, aa)) return;
    entity::Canvas* canvas = m_scene->getCanvasCurrent();
    if (!canvas) return;
    if (!this->setSubMouseMode<osgUtil::LineSegmentIntersector::Intersection, osgUtil::LineSegmentIntersector>(
                ea, aa, cher::SELECT_ENTITY, canvas->isEntitiesSelected()))
        return;

    if (!( (ea.getEventType() == osgGA::GUIEventAdapter::PUSH && ea.getButtonMask()== osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
           || (ea.getEventType() == osgGA::GUIEventAdapter::DRAG && ea.getButtonMask()== osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
           || (ea.getEventType() == osgGA::GUIEventAdapter::RELEASE && ea.getButton()==osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
           ))
        return;

    /* if there are no strokes in canvas, return*/
    if (m_scene->getCanvasCurrent()->getEntitiesSelectedSize() == 0){
        qWarning("doEditEntitiesMove: there are no strokes to move");
        return;
    }

    /* if no strokes are selected, return */
    if (m_scene->getCanvasCurrent()->getEntitiesSelectedSize() == 0)
        return;

    /* get mouse ray intersection with canvas plane */
    double u=0, v=0;
    if (!this->getRaytraceCanvasIntersection(ea,aa,u,v))
        return;

    switch (ea.getEventType()){
    case osgGA::GUIEventAdapter::PUSH:
        qDebug("edit strokes scale: push button");
        m_scene->editStrokesScale(u, v, cher::EVENT_PRESSED);
        break;
    case osgGA::GUIEventAdapter::RELEASE:
        qDebug("edit strokes scale: release button");
        m_scene->editStrokesScale(u, v, cher::EVENT_RELEASED);
        this->finishAll();
        break;
    case osgGA::GUIEventAdapter::DRAG:
        m_scene->editStrokesScale(u,v,cher::EVENT_DRAGGED);
        break;
    default:
        break;
    }
}

void EventHandler::doEditEntitiesRotate(const osgGA::GUIEventAdapter &ea, osgGA::GUIActionAdapter &aa)
{
//    if (!this->setSubSelectionType(ea, aa)) return;
    entity::Canvas* canvas = m_scene->getCanvasCurrent();
    if (!canvas) return;
    if (!this->setSubMouseMode<osgUtil::LineSegmentIntersector::Intersection, osgUtil::LineSegmentIntersector>(
                ea, aa, cher::SELECT_ENTITY, canvas->isEntitiesSelected()))
        return;

    if (!( (ea.getEventType() == osgGA::GUIEventAdapter::PUSH && ea.getButtonMask()== osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
           || (ea.getEventType() == osgGA::GUIEventAdapter::DRAG && ea.getButtonMask()== osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
           || (ea.getEventType() == osgGA::GUIEventAdapter::RELEASE && ea.getButton()==osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
           ))
        return;


    /* if there are no strokes in canvas, return*/
    if (m_scene->getCanvasCurrent()->getEntitiesSelectedSize() == 0){
        qWarning("doEditEntitiesMove: there are no strokes to move");
        return;
    }

    /* if no strokes are selected, return */
    if (m_scene->getCanvasCurrent()->getEntitiesSelectedSize() == 0)
        return;

    /* get mouse ray intersection with canvas plane */
    double u=0, v=0;
    if (!this->getRaytraceCanvasIntersection(ea,aa,u,v))
        return;

    switch (ea.getEventType()){
    case osgGA::GUIEventAdapter::PUSH:
        qDebug("edit strokes rotate: push button");
        m_scene->editStrokesRotate(u, v, cher::EVENT_PRESSED);
        break;
    case osgGA::GUIEventAdapter::RELEASE:
        qDebug("edit strokes rotate: release button");
        m_scene->editStrokesRotate(u, v, cher::EVENT_RELEASED);
        this->finishAll();
        break;
    case osgGA::GUIEventAdapter::DRAG:
        m_scene->editStrokesRotate(u,v,cher::EVENT_DRAGGED);
        break;
    default:
        break;
    }
}

entity::Stroke *EventHandler::getStroke(const StrokeIntersector::Intersection &result)
{
    return dynamic_cast<entity::Stroke*>(result.drawable.get());
}

entity::Canvas *EventHandler::getCanvas(const osgUtil::LineSegmentIntersector::Intersection &result)
{

    entity::Canvas* canvas = dynamic_cast<entity::Canvas*>(result.nodePath.at(m_scene->getCanvasLevel()));
    if (canvas){
        if (result.drawable.get() != canvas->getGeometryPickable())
            return NULL;
    }
    return canvas;
}

entity::Photo *EventHandler::getPhoto(const osgUtil::LineSegmentIntersector::Intersection &result)
{
    return dynamic_cast<entity::Photo*>(result.drawable.get());
}

template <typename T>
cher::MOUSE_MODE EventHandler::getMouseMode(const T &result, cher::MOUSE_MODE mode_default) const
{
    std::string name = result.drawable->getName();
    if (name == "Pickable")
        return cher::SELECT_ENTITY;
    else if (name == "Center")
        return cher::ENTITY_MOVE;
    else if (name == "AxisU")
        return cher::ENTITY_ROTATE;
    else if (name == "AxisV")
        return cher::ENTITY_ROTATE;
    else if (name == "ScaleUV1" || name == "ScaleUV2" || name == "ScaleUV3" || name == "ScaleUV4")
        return cher::ENTITY_SCALE;
    else if (name == "Normal1" || name == "Normal2")
        return cher::CANVAS_OFFSET;
    else if (name == "RotateX1")
        return cher::CANVAS_ROTATE_VPLUS;
    else if (name == "RotateX2")
        return cher::CANVAS_ROTATE_VMINUS;
    else if (name == "RotateY1")
        return cher::CANVAS_ROTATE_UPLUS;
    else if (name == "RotateY2")
        return cher::CANVAS_ROTATE_UMINUS;
    return mode_default;
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
    if (!Utilities::getRayPlaneIntersection(plane, center, nearPoint, farPoint, P)){
        this->finishAll();
        return false;
    }

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

bool EventHandler::getRaytracePlaneIntersection(const osgGA::GUIEventAdapter &ea, osgGA::GUIActionAdapter &aa, const osg::Vec3f &axis, osg::Vec3f &P)
{
    /* get view-projection-world matrix and its inverse*/
    osg::Matrix VPW, invVPW;
    if (!Utilities::getViewProjectionWorld(ea, aa, VPW, invVPW))
        return false;

    /* get far and near in global 3D coords */
    osg::Vec3f nearPoint, farPoint;
    Utilities::getFarNear(ea.getX(), ea.getY(), invVPW, nearPoint, farPoint);

    /* get intersection point in global 3D coords */
    const osg::Vec3f center = m_scene->getCanvasCurrent()->getCenter();
    const osg::Plane plane(axis, center);
    if (!Utilities::getRayPlaneIntersection(plane, center, nearPoint, farPoint, P)){
        this->finishAll();
        return false;
    }
    return true;
}

/* Defines the mouse mode depending on location of mouse over the canvas frame;
 * Used in entity select, entity move, entity scale, entity rotate, etc.
 * Returns true if no need to exit the parent function, false otherwise
*/
template <typename TResult, typename TIntersector>
bool EventHandler::setSubMouseMode(const osgGA::GUIEventAdapter &ea, osgGA::GUIActionAdapter &aa, cher::MOUSE_MODE modeDefault, bool selected)
{
    bool result = true;
    if (ea.getEventType() == osgGA::GUIEventAdapter::MOVE){
        if (!m_glWidget) return result;
        if (selected){
            TResult result_drawable;
            bool inter_occured = this->getIntersection<TResult, TIntersector>(ea,aa, cher::MASK_CANVASFRAME_IN, result_drawable);

            /* if mouse is hovering over certain drawable, set the corresponding mode */
            if (inter_occured){
                cher::MOUSE_MODE mode = this->getMouseMode<TResult>(result_drawable, modeDefault);
                result = mode == m_mode;
                m_glWidget->setMouseMode(mode);
            }
            /* if not, or if the mouse left the drawable area, make sure it is in entity select mode */
            else{
                result = m_mode == modeDefault;
                m_glWidget->setMouseMode(modeDefault);
            }
            this->setDrawableColorFromMode(result_drawable.drawable.get());
        }
    }
    return result;
}

/* sets colors of canvas frame drawables to colors:
 * gray color when mouse is not hovering anything
 * cyan color over the drawable which get the hovering
*/
void EventHandler::setDrawableColorFromMode(osg::Drawable *draw)
{
    if (!draw) {
        /* when mouse is not hovering over anything
         * set the color to normal (gray) for all the canvas selection frame. */
        m_scene->getCanvasCurrent()->setColor(cher::CANVAS_CLR_CURRENT, cher::CANVAS_CLR_INTERSECTION);
        return;
    }
    osg::Geometry* geom = dynamic_cast<osg::Geometry*>(draw);
    if (!geom) return;
    osg::Vec4Array* colors = static_cast<osg::Vec4Array*>(geom->getColorArray());
    if (colors->size() == 0) return;
    (*colors)[0] = solarized::cyan;
    geom->dirtyDisplayList();
    geom->dirtyBound();
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
    case cher::PEN_SKETCH:
        m_scene->addStroke(0,0, cher::EVENT_OFF);
        break;
    case cher::CANVAS_OFFSET:
        m_scene->editCanvasOffset(osg::Vec3f(0,0,0), cher::EVENT_OFF);
        break;
    case cher::CANVAS_ROTATE_UPLUS:
    case cher::CANVAS_ROTATE_UMINUS:
    case cher::CANVAS_ROTATE_VPLUS:
    case cher::CANVAS_ROTATE_VMINUS:
        m_scene->editCanvasRotate(osg::Quat(0,0,0,1), m_scene->getCanvasCurrent()->getCenter(), cher::EVENT_OFF);
        break;
    case cher::CREATE_CANVASCLONE:
        m_scene->editCanvasClone(osg::Vec3f(0,0,0), cher::EVENT_OFF);
        break;
    case cher::CREATE_CANVASSEPARATE:
        m_scene->editCanvasClone(osg::Vec3f(0,0,0), cher::EVENT_OFF);
        break;
    case cher::ENTITY_MOVE:
        m_scene->editStrokesMove(0,0, cher::EVENT_OFF);
        break;
    case cher::ENTITY_SCALE:
        m_scene->editStrokesScale(0,0, cher::EVENT_OFF);
        break;
    case cher::ENTITY_ROTATE:
        m_scene->editStrokesRotate(0,0, cher::EVENT_OFF);
        break;
    default:
        break;
    }
    m_photo = 0;
}

void EventHandler::doSelectEntity(const osgGA::GUIEventAdapter &ea, osgGA::GUIActionAdapter &aa)
{
    entity::Canvas* canvas = m_scene->getCanvasCurrent();
    if (!canvas) return;
    if (!this->setSubMouseMode<osgUtil::LineSegmentIntersector::Intersection, osgUtil::LineSegmentIntersector>(
                ea, aa, cher::SELECT_ENTITY, canvas->isEntitiesSelected()))
        return;
//    if (!this->setSubSelectionType(ea, aa)) return;

    if (!( (ea.getEventType() == osgGA::GUIEventAdapter::PUSH && ea.getButtonMask()== osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
           || (ea.getEventType() == osgGA::GUIEventAdapter::DRAG && ea.getButtonMask()== osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
           || (ea.getEventType() == osgGA::GUIEventAdapter::RELEASE && ea.getButton()==osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
           ))
        return;

    /* when frame is normal, perform selection of entities */
    if (!(cher::maskAction & m_mode))
    {
        /* if clicked outside of selected area, unselect all */
        if (ea.getEventType() == osgGA::GUIEventAdapter::PUSH)
            canvas->unselectEntities();

        osgUtil::LineSegmentIntersector::Intersection result_photo;
        bool inter_photo = this->getIntersection<osgUtil::LineSegmentIntersector::Intersection, osgUtil::LineSegmentIntersector>
                (ea,aa, cher::MASK_CANVAS_IN, result_photo);
        if (inter_photo){
            entity::Photo* photo = this->getPhoto(result_photo);
            if (photo) canvas->addEntitySelected(photo);
        }

        StrokeIntersector::Intersection result_stroke;
        bool inter_stroke = this->getIntersection<StrokeIntersector::Intersection, StrokeIntersector>
                (ea,aa, cher::MASK_CANVAS_IN, result_stroke);
        if (inter_stroke) {
            entity::Stroke* stroke = this->getStroke(result_stroke);
            if (stroke) canvas->addEntitySelected(stroke);
        }

        /* if some entities were selected, go into edit-frame mode for canvas frame */
        if (ea.getEventType() == osgGA::GUIEventAdapter::RELEASE){
            canvas->updateFrame(m_scene->getCanvasPrevious());
        }
    }
    /* when frame is in edit mode, see what transformation the user wants to do;
     * perform that action;
     * if clicked - search what drawable was clicked onto;
     * If center drawable - switch "move all" to "move center custom",
     * and perfrom the move.
     * If scale drawables - perform scaling action.
     * If rotation axis - perform rotation. */


}

template <typename T1, typename T2>
void EventHandler::doEditPhotoPush(const osgGA::GUIEventAdapter &ea, osgGA::GUIActionAdapter &aa)
{
    if (!( ea.getEventType() == osgGA::GUIEventAdapter::RELEASE && ea.getButton()==osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON ))
        return;

    T1 result;
    bool intersects = this->getIntersection<T1, T2>(ea,aa, cher::MASK_CANVAS_IN, result);
    if (!intersects) return;
    entity::Photo* photo = this->getPhoto(result);
    if (!photo) return;
//    m_scene->editPhotoPush(photo);
}

template <typename T1, typename T2>
void EventHandler::doSelectCanvas(const osgGA::GUIEventAdapter &ea, osgGA::GUIActionAdapter &aa)
{
    if (ea.getEventType()!=osgGA::GUIEventAdapter::RELEASE || ea.getButton()!=osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
        return;

    T1 resultIntersection;
    bool intersected = this->getIntersection<T1, T2>(ea, aa, cher::MASK_CANVAS_IN, resultIntersection);
    if (intersected){
        entity::Canvas* cnv = this->getCanvas(resultIntersection);
        if (cnv){
            qDebug() << "doPickCanvas(): assumed canvas with name: " << QString(cnv->getName().c_str());
            m_scene->setCanvasCurrent(cnv);
        }
        else
            qWarning( "doPickCanvas(): could not dynamic_cast<Canvas*>");
    }
}

template <typename TypeIntersection, typename TypeIntersector>
bool EventHandler::getIntersection(const osgGA::GUIEventAdapter &ea, osgGA::GUIActionAdapter &aa, unsigned int mask, TypeIntersection &resultIntersection)
{
    osgViewer::View* viewer = dynamic_cast<osgViewer::View*>(&aa);
    if (!viewer){
        qWarning("getIntersection(): could not retrieve viewer");
        return false;
    }
    osg::ref_ptr<TypeIntersector> intersector = new TypeIntersector(osgUtil::Intersector::WINDOW, ea.getX(), ea.getY());
    osgUtil::IntersectionVisitor iv(intersector);
    iv.setTraversalMask(mask);
    osg::Camera* cam = viewer->getCamera();
    if (!cam){
        qWarning( "getIntersection(): could not read camera" );
        return false;
    }
    cam->accept(iv);
    if (!intersector->containsIntersections()){
        return false;
    }

    resultIntersection = intersector->getFirstIntersection();
    return true;
}
