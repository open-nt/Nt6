// MIT License - see LICENSE
#include <QtNetwork/qauthenticator.h>

/*!
    \class QAuthenticator
    \brief The QAuthenticator class provides an authentication object.

    A QAuthenticator carries the credentials for a server that asks for them. QNetworkAccessManager hands one to
    the slots of its \l{QNetworkAccessManager::authenticationRequired()}{authenticationRequired()} signal with the
    realm of the server filled in; the slot fills in the user name and the password, and the request is sent
    again with them:

    \code
    connect(manager, &QNetworkAccessManager::authenticationRequired, this,
            [](QNetworkReply *, QAuthenticator *authenticator) {
                authenticator->setUser("admin");
                authenticator->setPassword(cameraPassword());
            });
    \endcode

    options() has what the server said about the scheme, such as its \c "realm" and, for Digest, its \c "nonce".

    \differences
    The Basic and Digest (MD5) schemes are supported; there is no NTLM, Negotiate or Kerberos, and no proxy
    authentication.
    \enddifferences

    \sa QNetworkAccessManager
*/

/*!
    \fn QAuthenticator::QAuthenticator()

    Constructs an empty authenticator.
*/

/*!
    \fn QAuthenticator::QAuthenticator(const QAuthenticator &other)

    Constructs a copy of \a other.
*/

/*!
    \fn QAuthenticator &QAuthenticator::operator=(const QAuthenticator &other)

    Makes the authenticator a copy of \a other and returns it.
*/

/*!
    \fn QAuthenticator::~QAuthenticator()

    Destroys the authenticator.
*/

/*!
    Returns \c true if the authenticator has the same user, password, realm and options as \a other.
*/
bool QAuthenticator::operator==(const QAuthenticator &other) const {
    return m_user == other.m_user && m_password == other.m_password && m_realm == other.m_realm &&
           m_options == other.m_options;
}

/*!
    \fn bool QAuthenticator::operator!=(const QAuthenticator &other) const

    Returns \c true if the authenticator differs from \a other.
*/

/*!
    \fn QString QAuthenticator::user() const

    Returns the user name.

    \sa setUser()
*/

/*!
    \fn void QAuthenticator::setUser(const QString &user)

    Sets the user name to \a user.

    \sa user()
*/

/*!
    \fn QString QAuthenticator::password() const

    Returns the password.

    \sa setPassword()
*/

/*!
    \fn void QAuthenticator::setPassword(const QString &password)

    Sets the password to \a password.

    \sa password()
*/

/*!
    \fn QString QAuthenticator::realm() const

    Returns the realm that the server named in its challenge.

    \sa setRealm()
*/

/*!
    \fn void QAuthenticator::setRealm(const QString &realm)

    Sets the realm to \a realm.

    \sa realm()
*/

/*!
    \fn QVariant QAuthenticator::option(const QString &opt) const

    Returns the value of the option \a opt, or an invalid QVariant if it is not set.

    \sa options(), setOption()
*/

/*!
    \fn QVariantHash QAuthenticator::options() const

    Returns all options of the authenticator: what the server said in its challenge.

    \sa option()
*/

/*!
    \fn void QAuthenticator::setOption(const QString &opt, const QVariant &value)

    Sets the option \a opt to \a value.

    \sa option()
*/

/*!
    Returns \c true if the authenticator has no user, password, realm or options.
*/
bool QAuthenticator::isNull() const {
    return m_user.isEmpty() && m_password.isEmpty() && m_realm.isEmpty() && m_options.isEmpty();
}
