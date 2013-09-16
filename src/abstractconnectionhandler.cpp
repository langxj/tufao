#include "priv/abstractconnectionhandler_p.h"

#include "httpserverrequest.h"
#include <QtNetwork/QTcpSocket>
#include <QtCore/QThread>
#include "headers.h"

namespace Tufao {

AbstractConnectionHandler::Priv::Priv() : timeout(120000){}
AbstractConnectionHandler::Priv::~Priv(){}


AbstractConnectionHandler::AbstractConnectionHandler(AbstractConnectionHandler::Priv *priv, QObject *parent)
    : QObject(parent),priv(priv)
{

}

AbstractConnectionHandler::AbstractConnectionHandler(QObject *parent) : AbstractConnectionHandler(new Priv(),parent){}

AbstractConnectionHandler::~AbstractConnectionHandler()
{
    delete priv;
}

void AbstractConnectionHandler::setTimeout(int msecs)
{
    priv->timeout = msecs;
}

int AbstractConnectionHandler::timeout() const
{
    return priv->timeout;
}

void AbstractConnectionHandler::handleConnection(QAbstractSocket *connection)
{
    HttpServerRequest *handle = new HttpServerRequest(*connection,this);
    connection->setParent(handle);

    if (priv->timeout)
        handle->setTimeout(priv->timeout);

    connect(handle, &HttpServerRequest::ready,
            this,   &AbstractConnectionHandler::onRequestReady);
    connect(handle, &HttpServerRequest::upgrade, this, &AbstractConnectionHandler::onUpgrade);
    connect(connection, &QAbstractSocket::disconnected,
            handle, &QObject::deleteLater);
    connect(connection, &QAbstractSocket::disconnected,
            connection, &QObject::deleteLater);
}

void AbstractConnectionHandler::onRequestReady()
{
    HttpServerRequest *request = qobject_cast<HttpServerRequest *>(sender());
    Q_ASSERT(request);

    QAbstractSocket &socket = request->socket();
    HttpServerResponse *response
            = new HttpServerResponse(socket, request->responseOptions(), request);

    connect(&socket, &QAbstractSocket::disconnected,
            response, &QObject::deleteLater);
    connect(response, &HttpServerResponse::finished,
            response, &QObject::deleteLater);

    //in case the timout was changed in the last request handler
    //and the connection was reused
    if (priv->timeout)
        handle->setTimeout(priv->timeout);

    if (request->headers().contains("Expect", "100-continue"))
        checkContinue(*request, *response);
    else
        emit requestReady(*request, *response);
}

void AbstractConnectionHandler::onUpgrade()
{
    HttpServerRequest* req = qobject_cast<HttpServerRequest*>(sender());
    if(req){
        this->handleUpgrade(*req);

        //make shure the socket is useable after the upgrade
        QAbstractSocket& socket = req->socket();
        if(socket.parent() == req){
            //socket will be automatically deleted when disconnected was emitted
            socket.setParent(0);
        }

        delete req;
    }
}

} // namespace Tufao
