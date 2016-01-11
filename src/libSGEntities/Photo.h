/* Photo
 * for more info on textures, see OpenSceneGraph beginner's guide,
 * Chapter 6 "The basis of texture mapping"
 */

#ifndef PHOTO
#define PHOTO

#include <osg/Geometry>
#include <osg/ref_ptr>
#include <osg/Texture2D>
#include <osgDB/ObjectWrapper>

namespace entity {
class Photo: public osg::Geometry{
public:
    Photo();
    Photo(const Photo& photo, const osg::CopyOp& copyop = osg::CopyOp::SHALLOW_COPY);
    //Photo(const std::string& fname);

    META_Node(entity, Photo)

    void setTexture(osg::Texture2D* texture);
    const osg::Texture2D* getTexture() const;

    void setWidth(float w);
    float getWidth() const;

    void setHeight(float h);
    float getHeight() const;

    void setCenter(const osg::Vec3f& c);
    const osg::Vec3f& getCenter() const;

    void loadImage(const std::string& fname);
    osg::StateAttribute* getTextureAsAttribute() const;

    void setFrameColor(const osg::Vec4 color);
    void setModeEdit(bool edit);
    void move(const double u, const double v);
private:
    osg::ref_ptr<osg::Texture2D> m_texture;
    osg::Vec3f m_center;
    float m_width, m_height;
};
}

#endif // PHOTO
