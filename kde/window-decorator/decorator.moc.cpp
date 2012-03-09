/****************************************************************************
** KWD::Decorator meta object code from reading C++ file 'decorator.h'
**
** Created: Mon Dec 25 22:08:49 2006
**      by: The Qt MOC ($Id: qt/moc_yacc.cpp   3.3.6   edited Mar 8 17:43 $)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#undef QT_NO_COMPAT
#include "decorator.h"
#include <qmetaobject.h>
#include <qapplication.h>

#include <private/qucomextra_p.h>
#if !defined(Q_MOC_OUTPUT_REVISION) || (Q_MOC_OUTPUT_REVISION != 26)
#error "This file was generated using the moc from 3.3.6. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

const char *KWD::Decorator::className() const
{
    return "KWD::Decorator";
}

QMetaObject *KWD::Decorator::metaObj = 0;
static QMetaObjectCleanUp cleanUp_KWD__Decorator( "KWD::Decorator", &KWD::Decorator::staticMetaObject );

#ifndef QT_NO_TRANSLATION
QString KWD::Decorator::tr( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "KWD::Decorator", s, c, QApplication::DefaultCodec );
    else
	return QString::fromLatin1( s );
}
#ifndef QT_NO_TRANSLATION_UTF8
QString KWD::Decorator::trUtf8( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "KWD::Decorator", s, c, QApplication::UnicodeUTF8 );
    else
	return QString::fromUtf8( s );
}
#endif // QT_NO_TRANSLATION_UTF8

#endif // QT_NO_TRANSLATION

QMetaObject* KWD::Decorator::staticMetaObject()
{
    if ( metaObj )
	return metaObj;
    QMetaObject* parentObject = KApplication::staticMetaObject();
    static const QUMethod slot_0 = {"reconfigure", 0, 0 };
    static const QUParameter param_slot_1[] = {
	{ "id", &static_QUType_ptr, "WId", QUParameter::In }
    };
    static const QUMethod slot_1 = {"handleWindowAdded", 1, param_slot_1 };
    static const QUParameter param_slot_2[] = {
	{ "id", &static_QUType_ptr, "WId", QUParameter::In }
    };
    static const QUMethod slot_2 = {"handleWindowRemoved", 1, param_slot_2 };
    static const QUParameter param_slot_3[] = {
	{ "id", &static_QUType_ptr, "WId", QUParameter::In }
    };
    static const QUMethod slot_3 = {"handleActiveWindowChanged", 1, param_slot_3 };
    static const QUParameter param_slot_4[] = {
	{ "id", &static_QUType_ptr, "WId", QUParameter::In },
	{ "properties", &static_QUType_ptr, "unsigned long", QUParameter::In }
    };
    static const QUMethod slot_4 = {"handleWindowChanged", 2, param_slot_4 };
    static const QUMethod slot_5 = {"processDamage", 0, 0 };
    static const QMetaData slot_tbl[] = {
	{ "reconfigure()", &slot_0, QMetaData::Public },
	{ "handleWindowAdded(WId)", &slot_1, QMetaData::Private },
	{ "handleWindowRemoved(WId)", &slot_2, QMetaData::Private },
	{ "handleActiveWindowChanged(WId)", &slot_3, QMetaData::Private },
	{ "handleWindowChanged(WId,const unsigned long*)", &slot_4, QMetaData::Private },
	{ "processDamage()", &slot_5, QMetaData::Private }
    };
    metaObj = QMetaObject::new_metaobject(
	"KWD::Decorator", parentObject,
	slot_tbl, 6,
	0, 0,
#ifndef QT_NO_PROPERTIES
	0, 0,
	0, 0,
#endif // QT_NO_PROPERTIES
	0, 0 );
    cleanUp_KWD__Decorator.setMetaObject( metaObj );
    return metaObj;
}

void* KWD::Decorator::qt_cast( const char* clname )
{
    if ( !qstrcmp( clname, "KWD::Decorator" ) )
	return this;
    if ( !qstrcmp( clname, "KWinInterface" ) )
	return (KWinInterface*)this;
    return KApplication::qt_cast( clname );
}

bool KWD::Decorator::qt_invoke( int _id, QUObject* _o )
{
    switch ( _id - staticMetaObject()->slotOffset() ) {
    case 0: reconfigure(); break;
    case 1: handleWindowAdded((WId)(*((WId*)static_QUType_ptr.get(_o+1)))); break;
    case 2: handleWindowRemoved((WId)(*((WId*)static_QUType_ptr.get(_o+1)))); break;
    case 3: handleActiveWindowChanged((WId)(*((WId*)static_QUType_ptr.get(_o+1)))); break;
    case 4: handleWindowChanged((WId)(*((WId*)static_QUType_ptr.get(_o+1))),(const unsigned long*)static_QUType_ptr.get(_o+2)); break;
    case 5: processDamage(); break;
    default:
	return KApplication::qt_invoke( _id, _o );
    }
    return TRUE;
}

bool KWD::Decorator::qt_emit( int _id, QUObject* _o )
{
    return KApplication::qt_emit(_id,_o);
}
#ifndef QT_NO_PROPERTIES

bool KWD::Decorator::qt_property( int id, int f, QVariant* v)
{
    return KApplication::qt_property( id, f, v);
}

bool KWD::Decorator::qt_static_property( QObject* , int , int , QVariant* ){ return FALSE; }
#endif // QT_NO_PROPERTIES
