#ifndef UTILITIES_H
#define UTILITIES_H

#include <vector>

#include <osgGA/GUIEventAdapter>
#include <osgViewer/View>
#include <osg/Camera>
#include <osg/Plane>

#include "Canvas.h"
#include "Stroke.h"

class Utilities
{
public:
    static bool areStrokesProjectable(const std::vector<entity::Stroke *> &strokes, entity::Canvas *source, entity::Canvas *target, osg::Camera* camera)
    {
        if (!target || !source || strokes.empty() || !camera) {
            std::cerr << "push strokes: one of the pointers is NULL " << std::endl;
            return false;
        }
        osg::Matrix M = source->getTransform()->getMatrix();
        osg::Matrix m = target->getTransform()->getMatrix();
        osg::Matrix invM;
        if (!invM.invert(m)){
            std::cerr << "push strokes: could not invert model matrix" << std::endl;
            return false;
        }
        const osg::Plane plane = target->getPlane();
        const osg::Vec3 center = target->getCenter();

        /* get camera matrices and other data */
        osg::Vec3f eye, c, u;
        camera->getViewMatrixAsLookAt(eye, c, u);
        osg::Matrix VPW = camera->getViewMatrix() * camera->getProjectionMatrix() * camera->getViewport()->computeWindowMatrix();
        osg::Matrix invVPW;
        if (!invVPW.invert(VPW)){
            std::cerr << "areStrokesProjectable(): could not invert View-projection-world matrix for ray casting" << std::endl;
            return false;
        }

        std::vector<osg::Vec3f> ray(2);

        for (unsigned int i=0; i<strokes.size(); ++i){
            entity::Stroke* s0 = strokes.at(i);
            osg::Vec3Array* verts = static_cast<osg::Vec3Array*>(s0->getVertexArray());
            for (unsigned int j=0; j<verts->size(); ++j){
                osg::Vec3 p = (*verts)[j];
                osg::Vec3 P = p * M;
                osg::Vec3 dir = P - eye;
                osg::Vec3f screen = P * VPW;
                osg::Vec3f nearPoint = osg::Vec3f(screen.x(), screen.y(), 0.f) * invVPW;
                osg::Vec3f farPoint = osg::Vec3f(screen.x(), screen.y(), 1.f) * invVPW;
                ray[0] = nearPoint;
                ray[1] = farPoint;

                if (plane.intersect(ray)){
                    outErrMsg("push strokes: some point projections do not intersect the target canvas plane."
                              "To resolve, change the camera view");
                    return false;
                }

                if (! plane.dotProductNormal(dir)){ // denominator
                    std::cerr << "push strokes: one of the points of projected stroke forms parallel projection to the canvas plane."
                                 "To resolve, change camera view." << std::endl;
                    return false;
                }
                if (! plane.dotProductNormal(center-P)){
                    std::cerr << "push strokes: plane contains the projected stroke or its part, so no single intersection can be defined."
                                 "To resolve, change camera view." << std::endl;
                    return false;
                }
                double len = plane.dotProductNormal(center-P) / plane.dotProductNormal(dir);
                osg::Vec3 P_ = dir * len + P;
                osg::Vec3 p_ = P_ * invM;
                if (std::fabs(p_.z())>dureu::EPSILON){
                    std::cerr << "push strokes: error while projecting point from global 3D to local 3D, z-coordinate is not zero." << std::endl;
                    outLogVec("P_", P_.x(), P_.y(), P_.z());
                    outLogVec("p_", p_.x(), p_.y(), p_.z());
                    return false;
                }
            }
        }
        outLogMsg("strokes are projectable");
        return true;
    }

    static bool getViewProjectionWorld(const osgGA::GUIEventAdapter& ea,
                                       osgGA::GUIActionAdapter& aa,
                                       osg::Matrix& VPW,
                                       osg::Matrix& invVPW)
    {
        osgViewer::View* viewer = dynamic_cast<osgViewer::View*>(&aa);
        if (!viewer){
            outErrMsg("getVPW: could not dynamic_cast to View*");
            return false;
        }

        osg::Camera* camera = viewer->getCamera();
        if (!camera){
            outErrMsg("getVPW: could not obtain camera");
            return false;
        }

        if (!camera->getViewport()){
            outErrMsg("getVPW: could not obtain viewport");
            return false;
        }

        VPW = camera->getViewMatrix()
                * camera->getProjectionMatrix()
                * camera->getViewport()->computeWindowMatrix();

        if (!invVPW.invert(VPW)){
            outErrMsg("getVPW: could not invert VPW matrix");
            return false;
        }

        return true;
    }

    static void getFarNear(double x, double y,
                           const osg::Matrix& invVPW,
                           osg::Vec3f& near, osg::Vec3f& far)
    {
        near = osg::Vec3f(x, y, 0.f) * invVPW;
        far = osg::Vec3f(x, y, 1.f) * invVPW;
    }

    static bool getRayPlaneIntersection(const osg::Plane& plane,
                                        const osg::Vec3f& center,
                                        const osg::Vec3f& nearPoint,
                                        const osg::Vec3f& farPoint,
                                        osg::Vec3f& P)
    {
        if (!plane.valid()){
            outErrMsg("rayPlaneIntersection: plane is not valid");
            return false;
        }

        std::vector<osg::Vec3f> ray(2);
        ray[0] = nearPoint;
        ray[1] = farPoint;
        if (plane.intersect(ray)) { // 1 or -1 means no intersection
            outErrMsg("rayPlaneIntersection: not intersection with ray");
            return false;
        }

        osg::Vec3f dir = farPoint-nearPoint;
        if (!plane.dotProductNormal(dir)){
            outErrMsg("rayPlaneIntersection: projected ray is almost parallel to plane. "
                      "Change view point.");
            return false;
        }

        if (! plane.dotProductNormal(center-nearPoint)){
            outErrMsg("rayPlaneIntersection: plane contains the line. "
                      "Change view point");
            return false;
        }

        double len = plane.dotProductNormal(center-nearPoint) / plane.dotProductNormal(dir);
        P = dir * len + nearPoint;

        return true;
    }

    static bool getModel(entity::Canvas* canvas,
                         osg::Matrix& M,
                         osg::Matrix& invM)
    {
        if (!canvas){
            outErrMsg("getModel: canvas is NULL");
            return false;
        }

        M = canvas->getTransform()->getMatrix();

        if (!invM.invert(M)){
            outErrMsg("getModel: could not invert M");
            return false;
        }

        return true;
    }

    static bool getLocalFromGlobal(const osg::Vec3f& P,
                                   const osg::Matrix& invM,
                                   osg::Vec3f& p)
    {
        p = P * invM;
        if (std::fabs(p.z())>dureu::EPSILON){
            outErrMsg("getLocalFromGlobal: local point's z-coordinate is not zero");
            return false;
        }

        return true;
    }

    /* algorithm for distance between skew lines:
     * http://www2.washjeff.edu/users/mwoltermann/Dorrie/69.pdf
     * For two points P1 and P2 on skew lines;
     * and d - the direction vector from P1 to P2;
     * u1 and u2 - unit direction vectors for the lines
     */
    static bool getSkewLinesProjection(const osg::Vec3f& center,
                                       const osg::Vec3f& farPoint,
                                       const osg::Vec3f& nearPoint,
                                       const osg::Vec3f& normal,
                                       osg::Vec3f& X1)
    {
        osg::Vec3f P1 = center;
        osg::Vec3f P2 = nearPoint;
        osg::Vec3f d = P2 - P1;
        osg::Vec3f u1 = normal;
        u1.normalize();
        osg::Vec3f u2 = farPoint - nearPoint;
        u2.normalize();
        osg::Vec3f u3 = u1^u2;

        if (std::fabs(u3.x())<=dureu::EPSILON && std::fabs(u3.y())<=dureu::EPSILON && std::fabs(u3.z())<=dureu::EPSILON){
            outErrMsg("getSkewLinesProjection: cast ray and normal are almost parallel."
                      "Switch view point.");
            return false;
        }

        // X1 and X2 are the closest points on lines
        // we want to find X1 (u1 corresponds to normal)
        // solving the linear equation in r1 and r2: Xi = Pi + ri*ui
        // we are only interested in X1 so we only solve for r1.
        float a1 = u1*u1, b1 = u1*u2, c1 = u1*d;
        float a2 = u1*u2, b2 = u2*u2, c2 = u2*d;
        if (!(std::fabs(b1) > dureu::EPSILON)){
            outErrMsg("getSkewLinesProjection: denominator is zero");
            return false;
        }
        if (!(a2!=-1 && a2!=1)){
            outErrMsg("getSkewLinesProjection: lines are parallel");
            return false;
        }

        double r1 = (c2 - b2*c1/b1)/(a2-b2*a1/b1);
        X1 = P1 + u1*r1;

        return true;

    }

};

#endif // UTILITIES_H
