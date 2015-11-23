#include <iostream>

#include "observescenecallback.h"
#include "settings.h"

ObserveSceneCallback::ObserveSceneCallback():
    _text(new osgText::Text),
    _geode(new osg::Geode)
{
    this->setTextProperties(osg::Vec3(-1.f, 0.f, 0.f), 0.1f);
    _geode->addDrawable(_text.get());
}

void ObserveSceneCallback::operator ()(osg::Node *node, osg::NodeVisitor *nv)
{
    std::string content = "Scene structure; ";
    if (_scene.valid()){
        //std::cout << "Scene number of children: " << _scene->getNumChildren() << std::endl;
        for (unsigned int i=0; i<_scene->getNumChildren(); ++i){
            if (_scene->getChild(i)) {
                //std::cout << "ObserveSceneCallback(): child with name: " << _scene->getChild(i)->getName() << std::endl;
                content += _scene->getChild(i)->getName() + "; ";
            }
        }
    }
    if (_text.valid())
        _text->setText(content);
    //traverse(node,nv);
}


void ObserveSceneCallback::setScenePointer(osg::Group *scene){
    _scene = scene;
}

void ObserveSceneCallback::setTextPointer(osgText::Text *text){
    _text = text;
}

void ObserveSceneCallback::setTextProperties(const osg::Vec3 &pos, float size) {
    std::cout << "  Observer->setTextProperties(pos, size)" << std::endl;
    _text->setDataVariance(osg::Object::DYNAMIC);
    _text->setCharacterSize(size);
    _text->setAxisAlignment(osgText::TextBase::XY_PLANE);
    _text->setPosition(pos);
    _text->setText("Scene structure; ");
    _text->setColor(solarized::base0);
}

osg::Geode *ObserveSceneCallback::getTextGeode() const{
    return _geode.get();
}
