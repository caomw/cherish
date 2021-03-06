#include "Photo.h"
#include "Settings.h"

#include <QDebug>
#include <QtGlobal>

#include <osg/Texture2D>
#include <osgDB/ReadFile>
#include <osg/Image>
#include <osg/StateSet>
#include <osg/TexMat>
#include <osg/BlendFunc>
#include <osg/Material>

entity::Photo::Photo()
    : entity::Entity2D()
    , m_texture(new osg::Texture2D)
    , m_center(osg::Vec3f(0.f,0.f,0.f))
    , m_width(0)
    , m_height(0)
    , m_angle(0)
{
    qDebug("New Photo ctor complete");
    this->setName("Photo");
    this->getOrCreateStateSet()->setMode(GL_BLEND, osg::StateAttribute::ON);
    this->getOrCreateStateSet()->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
    this->getOrCreateStateSet()->setAttributeAndModes(new osg::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
}

entity::Photo::Photo(const entity::Photo& photo, const osg::CopyOp& copyop)
    : entity::Entity2D(photo, copyop)
    , m_texture(photo.m_texture)
    , m_center(photo.m_center)
    , m_width(photo.m_width)
    , m_height(photo.m_height)
    , m_angle(photo.m_angle)
{
    qDebug("New Photo ctor by copy complete");
}

void entity::Photo::setTexture(osg::Texture2D* texture)
{
    m_texture = texture;
}

const osg::Texture2D*entity::Photo::getTexture() const
{
    return m_texture.get();
}

void entity::Photo::setWidth(float w)
{
    m_width = w;
    this->updateVertices();
}

float entity::Photo::getWidth() const
{
    return m_width;
}

void entity::Photo::setHeight(float h)
{
    m_height = h;
    this->updateVertices();
}

float entity::Photo::getHeight() const
{
    return m_height;
}

void entity::Photo::setCenter(const osg::Vec3f& c)
{
    m_center = c;
    this->updateVertices();
}

const osg::Vec3f& entity::Photo::getCenter() const
{
    return m_center;
}

void entity::Photo::setAngle(float a)
{
    m_angle = a;
    this->updateVertices();
}

float entity::Photo::getAngle() const
{
    return m_angle;
}

/* width and height represent half size width and height
*/
void entity::Photo::loadImage(const std::string& fname)
{
    qDebug() << "Trying to load image data...";
    osg::Image* image = osgDB::readImageFile(fname);
    if (!image) return;
    qDebug() << "DONE: Image read from file name";
    m_texture->setImage(image);
    qDebug() << "DONE: Texure extracted from image";

    float aspectRatio = static_cast<float>(image->s()) / static_cast<float>(image->t());
    m_width = cher::PHOTO_MINW;
    m_height = m_width / aspectRatio;

    osg::Vec3Array* verts = new osg::Vec3Array(4);
    this->setVertexArray(verts);
    this->updateVertices();
    this->addPrimitiveSet(new osg::DrawArrays(GL_QUADS, 0, 4));

    osg::Vec3Array* normals = new osg::Vec3Array;
    normals->push_back(cher::NORMAL);
    this->setNormalArray(normals);
    this->setNormalBinding(osg::Geometry::BIND_OVERALL);

    osg::Vec2Array* texcoords = new osg::Vec2Array(4);
    this->setTexCoordArray(0, texcoords);
    double x = m_width;
    (*texcoords)[0] = osg::Vec2(0, 0);
    (*texcoords)[1] = osg::Vec2(x, 0);
    (*texcoords)[2] = osg::Vec2(x, x);
    (*texcoords)[3] = osg::Vec2(0, x);

    osg::Vec4Array* colors = new osg::Vec4Array;
    colors->push_back(cher::PHOTO_CLR_REST);
    this->setColorArray(colors, osg::Array::BIND_OVERALL);
}

osg::StateAttribute* entity::Photo::getTextureAsAttribute() const
{
    return dynamic_cast<osg::StateAttribute*>(m_texture.get());
}

void entity::Photo::move(const double u, const double v)
{
    m_center = osg::Vec3f(u, v, 0.f);
    this->updateVertices();
}

void entity::Photo::moveDelta(double du, double dv)
{
    m_center += osg::Vec3f(du,dv,0.f);
    this->updateVertices();
}

/* The angle must be provided in radiants, and it is
 * a incremental angle, not an angle from origin.
*/
void entity::Photo::rotate(double angle)
{
    m_angle += angle;
    this->updateVertices();
}

void entity::Photo::rotate(double theta, osg::Vec3f center)
{
    m_angle += theta;
    m_center = center;
    this->updateVertices();
}

/* When performing a flip, we do not touch vertex coordinates,
 * but rather the sequence of texture coordinates which is
 * mapped to QUAD object.
*/
void entity::Photo::flipH()
{
    osg::Vec2Array* texcoords = static_cast<osg::Vec2Array*>(this->getTexCoordArray(0));
    if (!texcoords){
        qWarning("Could not extract texcoords of Photo");
        return;
    }
    std::swap((*texcoords)[0], (*texcoords)[1]);
    std::swap((*texcoords)[2], (*texcoords)[3]);
    this->dirtyDisplayList();
    this->dirtyBound();
}

void entity::Photo::flipV()
{
    osg::Vec2Array* texcoords = static_cast<osg::Vec2Array*>(this->getTexCoordArray(0));
    if (!texcoords){
        qWarning("Could not extract texcoords of Photo");
        return;
    }
    std::swap((*texcoords)[0], (*texcoords)[3]);
    std::swap((*texcoords)[1], (*texcoords)[2]);
    this->dirtyDisplayList();
    this->dirtyBound();
}

void entity::Photo::scale(double scale, osg::Vec3f center)
{
    m_width *= scale;
    m_height *= scale;
    m_center = center;
    this->updateVertices();
}

void entity::Photo::scale(double scaleX, double scaleY, osg::Vec3f center)
{
    m_width *= scaleX;
    m_height *= scaleY;
    m_center = center;

//    osg::Vec3Array* verts = static_cast<osg::Vec3Array*>(this->getVertexArray());
//    for (unsigned int i=0; i<verts->size(); ++i){
//        osg::Vec3f vi = (*verts)[i] - center;
//        (*verts)[i] = center + osg::Vec3f(scaleX*vi.x(), scaleY*vi.y(), 0);
//    }
//    this->dirtyDisplayList();
//    this->dirtyBound();

    this->updateVertices();
}

void entity::Photo::setColor(const osg::Vec4f &color)
{
    qDebug("photo color resetting");
    osg::Vec4Array* colors = static_cast<osg::Vec4Array*>(this->getColorArray());
    if (color != cher::STROKE_CLR_NORMAL)
        (*colors)[0] = color;
    else
        (*colors)[0] = cher::PHOTO_CLR_REST;
    colors->dirty();
    this->dirtyDisplayList();
    this->dirtyBound();
}

void entity::Photo::setTransparency(float alpha)
{
    alpha = alpha<0? 0.f : alpha;
    alpha = alpha>1? 1.f : alpha;

    osg::Vec4Array* colors = static_cast<osg::Vec4Array*>(this->getColorArray());
    if (colors->size() == 0) return;

    osg::Vec4f color = (*colors)[0];
    if (color.isNaN()) return;
    (*colors)[0] = osg::Vec4f(color.x(), color.y(), color.z(), alpha);
    colors->dirty();
    this->dirtyDisplayList();
    this->dirtyBound();
}

float entity::Photo::getTransparency() const
{
    const osg::Vec4Array* colors = static_cast<const osg::Vec4Array*>(this->getColorArray());
    if (colors->size() == 0) {
        qWarning("photo: color array is not set");
        return 1.f;
    }
    return ((*colors)[0]).a();
//    osg::Material* mat = dynamic_cast< osg::Material* >(this->getOrCreateStateSet()->getAttribute(osg::StateAttribute::MATERIAL));
//    if (!mat) return 1.f;
}

cher::ENTITY_TYPE entity::Photo::getEntityType() const
{
    return cher::ENTITY_PHOTO;
}

/* Since the Photo is represented by Quad, we can use parameters
 * such as width, height, center and angle to control its position
 * in local coordinates (canvas coordinates)
*/
void entity::Photo::updateVertices()
{
    osg::Vec3Array* verts = static_cast<osg::Vec3Array*>(this->getVertexArray());
    if (!verts){
        qWarning("Could not extract vertices of Photo");
        return;
    }

    (*verts)[0] = m_center +  osg::Vec3(-m_width * std::cos(m_angle) + m_height * std::sin(m_angle),
                                        -m_width * std::sin(m_angle) - m_height * std::cos(m_angle), 0);
    (*verts)[1] = m_center +  osg::Vec3(m_width * std::cos(m_angle) + m_height * std::sin(m_angle),
                                        m_width * std::sin(m_angle) - m_height * std::cos(m_angle), 0);
    (*verts)[2] = m_center +  osg::Vec3(m_width * std::cos(m_angle) - m_height * std::sin(m_angle),
                                        m_width * std::sin(m_angle) + m_height * std::cos(m_angle), 0);
    (*verts)[3] = m_center +  osg::Vec3(-m_width * std::cos(m_angle) - m_height * std::sin(m_angle),
                                        -m_width * std::sin(m_angle) + m_height * std::cos(m_angle), 0);
    this->dirtyDisplayList();
    this->dirtyBound();
}

REGISTER_OBJECT_WRAPPER(Photo_Wrapper
                        , new entity::Photo
                        , entity::Photo
                        , "osg::Object osg::Geometry entity::Photo")
{
    ADD_OBJECT_SERIALIZER(Texture, osg::Texture2D, NULL);
    ADD_VEC3F_SERIALIZER(Center, osg::Vec3f());
    ADD_FLOAT_SERIALIZER(Width, 0.f);
    ADD_FLOAT_SERIALIZER(Height, 0.f);
    ADD_FLOAT_SERIALIZER(Angle, 0.f);
}
