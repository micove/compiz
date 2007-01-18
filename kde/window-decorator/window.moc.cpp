/****************************************************************************
** KWD::Window meta object code from reading C++ file 'window.h'
**
** Created: Mon Dec 25 22:08:51 2006
**      by: The Qt MOC ($Id: qt/moc_yacc.cpp   3.3.6   edited Mar 8 17:43 $)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#undef QT_NO_COMPAT
#include "window.h"
#include <qmetaobject.h>
#include <qapplication.h>

#include <private/qucomextra_p.h>
#if !defined(Q_MOC_OUTPUT_REVISION) || (Q_MOC_OUTPUT_REVISION != 26)
#error "This file was generated using the moc from 3.3.6. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

const char *KWD::Window::className() const
{
    return "KWD::Window";
}

QMetaObject *KWD::Window::metaObj = 0;
static QMetaObjectCleanUp cleanUp_KWD__Window( "KWD::Window", &KWD::Window::staticMetaObject );

#ifndef QT_NO_TRANSLATION
QString KWD::Window::tr( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "KWD::Window", s, c, QApplication::DefaultCodec );
    else
	return QString::fromLatin1( s );
}
#ifndef QT_NO_TRANSLATION_UTF8
QString KWD::Window::trUtf8( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "KWD::Window", s, c, QApplication::UnicodeUTF8 );
    else
	return QString::fromUtf8( s );
}
#endif // QT_NO_TRANSLATION_UTF8

#endif // QT_NO_TRANSLATION

QMetaObject* KWD::Window::staticMetaObject()
{
    if ( metaObj )
	return metaObj;
    QMetaObject* parentObject = QWidget::staticMetaObject();
    static const QUParameter param_slot_0[] = {
	{ "id", &static_QUType_int, 0, QUParameter::In }
    };
    static const QUMethod slot_0 = {"handlePopupActivated", 1, param_slot_0 };
    static const QUParameter param_slot_1[] = {
	{ "id", &static_QUType_int, 0, QUParameter::In }
    };
    static const QUMethod slot_1 = {"handleDesktopPopupActivated", 1, param_slot_1 };
    static const QUMethod slot_2 = {"handlePopupAboutToShow", 0, 0 };
    static const QUMethod slot_3 = {"handleProcessKillerExited", 0, 0 };
    static const QMetaData slot_tbl[] = {
	{ "handlePopupActivated(int)", &slot_0, QMetaData::Private },
	{ "handleDesktopPopupActivated(int)", &slot_1, QMetaData::Private },
	{ "handlePopupAboutToShow()", &slot_2, QMetaData::Private },
	{ "handleProcessKillerExited()", &slot_3, QMetaData::Private }
    };
    metaObj = QMetaObject::new_metaobject(
	"KWD::Window", parentObject,
	slot_tbl, 4,
	0, 0,
#ifndef QT_NO_PROPERTIES
	0, 0,
	0, 0,
#endif // QT_NO_PROPERTIES
	0, 0 );
    cleanUp_KWD__Window.setMetaObject( metaObj );
    return metaObj;
}

void* KWD::Window::qt_cast( const char* clname )
{
    if ( !qstrcmp( clname, "KWD::Window" ) )
	return this;
    if ( !qstrcmp( clname, "KDecorationBridge" ) )
	return (KDecorationBridge*)this;
    return QWidget::qt_cast( clname );
}

bool KWD::Window::qt_invoke( int _id, QUObject* _o )
{
    switch ( _id - staticMetaObject()->slotOffset() ) {
    case 0: handlePopupActivated((int)static_QUType_int.get(_o+1)); break;
    case 1: handleDesktopPopupActivated((int)static_QUType_int.get(_o+1)); break;
    case 2: handlePopupAboutToShow(); break;
    case 3: handleProcessKillerExited(); break;
    default:
	return QWidget::qt_invoke( _id, _o );
    }
    return TRUE;
}

bool KWD::Window::qt_emit( int _id, QUObject* _o )
{
    return QWidget::qt_emit(_id,_o);
}
#ifndef QT_NO_PROPERTIES

bool KWD::Window::qt_property( int id, int f, QVariant* v)
{
    return QWidget::qt_property( id, f, v);
}

bool KWD::Window::qt_static_property( QObject* , int , int , QVariant* ){ return FALSE; }
#endif // QT_NO_PROPERTIES
