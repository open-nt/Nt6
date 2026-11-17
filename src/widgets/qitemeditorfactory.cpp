// MIT License - see LICENSE
#include <QtWidgets/qitemeditorfactory.h>

#include <QtCore/qmetatype.h>
#include <QtWidgets/qcombobox.h>
#include <QtWidgets/qdatetimeedit.h>
#include <QtWidgets/qlineedit.h>
#include <QtWidgets/qspinbox.h>

#include <limits>
#include <memory>
#include <set>

namespace {

bool isInteger(int type) {
    return type == QMetaType::Int || type == QMetaType::UInt || type == QMetaType::Short || type == QMetaType::UShort ||
           type == QMetaType::Long || type == QMetaType::ULong || type == QMetaType::LongLong ||
           type == QMetaType::ULongLong || type == QMetaType::Char || type == QMetaType::SChar ||
           type == QMetaType::UChar;
}

std::unique_ptr<QItemEditorFactory> &builtInFactory() {
    static std::unique_ptr<QItemEditorFactory> factory;
    return factory;
}

QItemEditorFactory *&customFactory() {
    static QItemEditorFactory *factory = nullptr;
    return factory;
}

} // namespace

/*!
    \class QItemEditorCreatorBase
    \inmodule NtWidgets
    \since 6.0
    \brief The QItemEditorCreatorBase class is the base of the classes that make editors for a QItemEditorFactory.

    \differences
    None.
    \enddifferences

    \sa QItemEditorCreator, QStandardItemEditorCreator
*/

/*!
    \fn QItemEditorCreatorBase::~QItemEditorCreatorBase()

    Destroys the creator.
*/

/*!
    \fn QWidget *QItemEditorCreatorBase::createWidget(QWidget *parent) const

    Returns a new editor, a child of \a parent.
*/

/*!
    \fn QByteArray QItemEditorCreatorBase::valuePropertyName() const

    Returns the name of the property of the editor that holds the value.
*/

/*!
    \class QItemEditorCreator
    \inmodule NtWidgets
    \since 6.0
    \brief The QItemEditorCreator class makes editors of the widget class \c T, with a property for the value.

    \code
    auto *factory = new QItemEditorFactory;
    factory->registerEditor(QMetaType::QColor, new QItemEditorCreator<ColorPicker>("color"));
    delegate->setItemEditorFactory(factory);
    \endcode

    \differences
    None.
    \enddifferences
*/

/*!
    \fn template <class T> QItemEditorCreator<T>::QItemEditorCreator(const QByteArray &valuePropertyName)

    Constructs a creator of editors whose value is the property \a valuePropertyName.
*/

/*!
    \fn template <class T> QWidget *QItemEditorCreator<T>::createWidget(QWidget *parent) const

    Returns a new \c T, a child of \a parent.
*/

/*!
    \fn template <class T> QByteArray QItemEditorCreator<T>::valuePropertyName() const

    Returns the name of the property that holds the value.
*/

/*!
    \class QStandardItemEditorCreator
    \inmodule NtWidgets
    \since 6.0
    \brief The QStandardItemEditorCreator class makes editors of the widget class \c T, whose user property holds
    the value.

    \differences
    None.
    \enddifferences
*/

/*!
    \fn template <class T> QStandardItemEditorCreator<T>::QStandardItemEditorCreator()

    Constructs a creator of editors whose value is their user property.
*/

/*!
    \fn template <class T> QWidget *QStandardItemEditorCreator<T>::createWidget(QWidget *parent) const

    Returns a new \c T, a child of \a parent.
*/

/*!
    \fn template <class T> QByteArray QStandardItemEditorCreator<T>::valuePropertyName() const

    Returns the name of the user property of \c T.
*/

/*!
    \class QItemEditorFactory
    \inmodule NtWidgets
    \since 6.0
    \brief The QItemEditorFactory class makes the editors that a delegate uses, by the type of the value.

    QStyledItemDelegate asks its factory, or defaultFactory(), for an editor for the type of the value of the item.
    The factory makes the editors registered with registerEditor() and otherwise its own: a combo box for \c bool,
    spin boxes for numbers, date and time edits, and a line edit for anything else.

    \differences
    None.
    \enddifferences

    \sa QStyledItemDelegate::setItemEditorFactory()
*/

/*!
    Constructs a factory without registered editors; it makes the built-in ones.
*/
QItemEditorFactory::QItemEditorFactory() = default;

/*!
    Destroys the factory and its creators.
*/
QItemEditorFactory::~QItemEditorFactory() {
    std::set<QItemEditorCreatorBase *> creators;
    for (QItemEditorCreatorBase *creator : std::as_const(m_creators))
        creators.insert(creator);
    for (QItemEditorCreatorBase *creator : creators)
        delete creator;
}

/*!
    Returns a new editor for values of the type \a userType, a child of \a parent.
*/
QWidget *QItemEditorFactory::createEditor(int userType, QWidget *parent) const {
    if (QItemEditorCreatorBase *creator = m_creators.value(userType))
        return creator->createWidget(parent);
    if (userType == QMetaType::Bool) {
        auto *box = new QComboBox(parent);
        box->addItem(QStringLiteral("False"));
        box->addItem(QStringLiteral("True"));
        return box;
    }
    if (isInteger(userType)) {
        auto *box = new QSpinBox(parent);
        const bool isUnsigned = userType == QMetaType::UInt || userType == QMetaType::UShort ||
                                userType == QMetaType::ULong || userType == QMetaType::ULongLong;
        box->setRange(isUnsigned ? 0 : std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
        return box;
    }
    if (userType == QMetaType::Double || userType == QMetaType::Float) {
        auto *box = new QDoubleSpinBox(parent);
        box->setDecimals(6);
        box->setRange(-std::numeric_limits<double>::max(), std::numeric_limits<double>::max());
        return box;
    }
    if (userType == QMetaType::QDate)
        return new QDateEdit(parent);
    if (userType == QMetaType::QTime)
        return new QTimeEdit(parent);
    if (userType == QMetaType::QDateTime)
        return new QDateTimeEdit(parent);
    auto *edit = new QLineEdit(parent);
    edit->setFrame(false);
    return edit;
}

/*!
    Returns the name of the property of the editor for \a userType that holds the value.
*/
QByteArray QItemEditorFactory::valuePropertyName(int userType) const {
    if (QItemEditorCreatorBase *creator = m_creators.value(userType))
        return creator->valuePropertyName();
    if (userType == QMetaType::Bool)
        return "currentIndex";
    if (isInteger(userType) || userType == QMetaType::Double || userType == QMetaType::Float)
        return "value";
    if (userType == QMetaType::QDate)
        return "date";
    if (userType == QMetaType::QTime)
        return "time";
    if (userType == QMetaType::QDateTime)
        return "dateTime";
    return "text";
}

/*!
    Makes \a creator make the editors for \a userType, in place of the one there was. The factory takes ownership of
    \a creator.
*/
void QItemEditorFactory::registerEditor(int userType, QItemEditorCreatorBase *creator) {
    QItemEditorCreatorBase *old = m_creators.value(userType);
    m_creators.insert(userType, creator);
    if (old && old != creator) {
        bool used = false;
        for (QItemEditorCreatorBase *c : std::as_const(m_creators))
            used = used || c == old;
        if (!used)
            delete old;
    }
}

/*!
    Returns the factory of the delegates that have none of their own: the one setDefaultFactory() set, or a factory
    of the built-in editors.
*/
const QItemEditorFactory *QItemEditorFactory::defaultFactory() {
    if (customFactory())
        return customFactory();
    if (!builtInFactory())
        builtInFactory() = std::make_unique<QItemEditorFactory>();
    return builtInFactory().get();
}

/*!
    Makes \a factory the default factory, and deletes the one that was set before. Nt takes ownership of it.
*/
void QItemEditorFactory::setDefaultFactory(QItemEditorFactory *factory) {
    if (factory == customFactory())
        return;
    delete customFactory();
    customFactory() = factory;
}
