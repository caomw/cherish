#include <iostream>

#include <QtWidgets>

#include "DureuApplication.h"
#include "Settings.h"

DureuApplication::DureuApplication(int &argv, char **argc):
    QApplication(argv, argc)
{
    Q_INIT_RESOURCE(Actions);
}

DureuApplication::~DureuApplication(){}

bool DureuApplication::event(QEvent* event){
    if (event->type() == QEvent::TabletEnterProximity || event->type() == QEvent::TabletLeaveProximity) {
            bool active = event->type() == QEvent::TabletEnterProximity? 1 : 0;
            std::cerr << "tablet active: " << active << std::endl;
            emit sendTabletActivity(active);
            return true;
        }
    return QApplication::event(event);
}
