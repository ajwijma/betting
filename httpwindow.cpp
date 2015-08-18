/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtWidgets>
#include <QtNetwork>

#include "httpwindow.h"
#include "ui_dialog.h"

HttpWindow::HttpWindow(QWidget *parent) : QDialog(parent), ui(new Ui::Dialog)
{
    ui->setupUi(this);

    urlTennisHighlights  = "https://www.betfair.com/exchange/tennis?modules=tennishighlights&container=false&isAjax=true&eventTypeId=undefined&alt=json";
    urlTennisMarketsBase = "https://uk-api.betfair.com/www/sports/exchange/readonly/v1.0/bymarket?currencyCode=EUR&alt=json&locale=en_GB&types=MARKET_STATE%2CMARKET_RATES%2CMARKET_DESCRIPTION%2CEVENT%2CRUNNER_DESCRIPTION%2CRUNNER_STATE%2CRUNNER_EXCHANGE_PRICES_BEST%2CRUNNER_METADATA&marketIds=";

    ui->tableWidget->setColumnCount(12);
//  ui->tableWidget->setColumnHidden(0, true);
//  ui->tableWidget->setColumnHidden(1, true);

    m_iTennisHighlightsTimeout = 1;
    m_iTennisMarketsTimeout    = 6;
    betfairTimer = new QTimer;
    betfairTimer->setInterval(1000);

    pokerStarsTimer = new QTimer;
    pokerStarsTimer->setInterval(10000);

    colorTimer = new QTimer;
    colorTimer->start(100);

    connect(betfairTimer, SIGNAL(timeout()), this, SLOT(onBetfairTimeout()));
    connect(pokerStarsTimer, SIGNAL(timeout()), this, SLOT(onPokerStarsTimeout()));
    connect(colorTimer, SIGNAL(timeout()), this, SLOT(onColorTimeout()));

    connect(ui->pushButtonBetfair, SIGNAL(clicked()), betfairTimer, SLOT(start()));
    connect(ui->pushButtonPokerStars, SIGNAL(clicked()), this, SLOT(pokerStars()));

    connect(&qnam, SIGNAL(sslErrors(QNetworkReply *, QList<QSslError>)), this, SLOT(sslErrors(QNetworkReply *, QList<QSslError>)));
}

HttpWindow::~HttpWindow()
{
    delete ui;
}

void HttpWindow::onBetfairTimeout()
{
    if (--m_iTennisMarketsTimeout <= 0)
    {
        downloadTennisMarkets();
        m_iTennisMarketsTimeout = 5;
    }

    if (--m_iTennisHighlightsTimeout <= 0)
    {
        downloadTennisHighlights();
        m_iTennisHighlightsTimeout = 60;
    }
}

void HttpWindow::downloadTennisHighlights()
{
    replyTennisHighlights = qnam.get(QNetworkRequest(urlTennisHighlights));

    connect(replyTennisHighlights, SIGNAL(finished()), this, SLOT(httpBetfairTennisHighlightsFinished()));
}

void HttpWindow::downloadTennisMarkets()
{
    int count = 0;

    urlTennisMarkets = urlTennisMarketsBase;
    for (QList<QString>::iterator i = marketIds.begin(); i != marketIds.end(); i++)
    {
        if (count == 0)
        {
            urlTennisMarkets.append(*i);
        }
        else
        {
            urlTennisMarkets.append("%2C");
            urlTennisMarkets.append(*i);
        }

        if (++count >= 20)
        {
            replyTennisMarkets.append(qnam.get(QNetworkRequest(urlTennisMarkets)));
            connect(replyTennisMarkets.last(), SIGNAL(finished()), this, SLOT(httpBetfairTennisMarketsFinished()));

            urlTennisMarkets = urlTennisMarketsBase;
            count = 0;
        }
    }

    if (count)
    {
        replyTennisMarkets.append(qnam.get(QNetworkRequest(urlTennisMarkets)));
        connect(replyTennisMarkets.last(), SIGNAL(finished()), this, SLOT(httpBetfairTennisMarketsFinished()));

    }
}

void HttpWindow::httpBetfairTennisHighlightsFinished()
{
    QJsonDocument   json;
    QJsonParseError error;
    int             added = 0, deleted = 0;

    json = QJsonDocument::fromJson(replyTennisHighlights->readAll(), &error);

    QFile qq("betfair_tennis_highlights.txt");
    qq.open(QIODevice::WriteOnly | QIODevice::Text);
    qq.write(json.toJson());
    qq.close();

//  qDebug() << json.toJson();

    QJsonValue value = json.object().value("page").toObject().value("config").toObject().value("marketData");

    QJsonArray array = value.toArray();
    marketIds.clear();
    for (QJsonArray::const_iterator i = array.begin(); i != array.end(); i++)
    {
        QJsonArray a;
        QString    marketId;
        int        row;

        if (i->toObject().value("marketId").isUndefined())
        {
            qDebug() << "No marketId";
            continue;
        }
        if (i->toObject().value("eventName").isUndefined())
        {
            qDebug() << "No eventName";
            continue;
        }

        if (i->toObject().value("runners").isUndefined())
        {
            qDebug() << "No runners";
            continue;
        }

        a = i->toObject().value("runners").toArray();

        if (a[0].toObject().value("prices").isUndefined())
        {
            qDebug() << "No prices 0";
            continue;
        }
        if (a[1].toObject().value("prices").isUndefined())
        {
            qDebug() << "No prices 1";
            continue;
        }

        qDebug() << QString::number(i->toObject().value("marketTime").toDouble(), 'f', 0) << i->toObject().value("eventName").toString();

        marketId = i->toObject().value("marketId").toString();
        marketIds.append(marketId);
        for (row = 0; row < ui->tableWidget->rowCount(); row++)
        {
            if (ui->tableWidget->item(row, 0)->text() == marketId)
                break;
        }
        if (row < ui->tableWidget->rowCount())
        {
            if (QString::number(i->toObject().value("marketTime").toDouble(), 'f', 0) != ui->tableWidget->item(row, 1)->text())
                    qDebug() << "marketTime has changed...";
            continue;
        }
        else
        {
            ui->tableWidget->insertRow(row);
            ui->tableWidget->setItem(row, 0, new QTableWidgetItem(i->toObject().value("marketId").toString()));
            ui->tableWidget->setItem(row, 1, new QTableWidgetItem(QString::number(i->toObject().value("marketTime").toDouble(), 'f', 0)));
            ui->tableWidget->setItem(row, 2, new QTableWidgetItem(i->toObject().value("eventName").toString()));
            ui->tableWidget->item(row, 2)->setBackgroundColor(QColor(0, 255, 0));
            added++;
        }
    }

    for (int row = 0; row < ui->tableWidget->rowCount(); row++)
    {
        int i;
        for (i = 0; i < marketIds.count(); i++)
        {
            if (marketIds[i] == ui->tableWidget->item(row, 0)->text())
                break;
        }
        if (i >= marketIds.count())
        {
            ui->tableWidget->item(row, 2)->setBackgroundColor(QColor(255, 0, 0));
            deleted++;
        }
    }

    qDebug() << marketIds.count() << ui->tableWidget->rowCount() << added << deleted;

    ui->tableWidget->sortByColumn(1, Qt::AscendingOrder);
    ui->tableWidget->resizeColumnsToContents();

    QTimer::singleShot(0, this, SLOT(resizeWindow()));

/*
    QVariant redirectionTarget = replyTennisHighlights->attribute(QNetworkRequest::RedirectionTargetAttribute);
    if (replyTennisHighlights->error())
    {
        QMessageBox::information(this, tr("HTTP"),
                                 tr("Download failed: %1.")
                                 .arg(replyTennisHighlights->errorString()));
    }
    else if (!redirectionTarget.isNull())
    {
        QUrl newUrl = url.resolved(redirectionTarget.toUrl());
        if (QMessageBox::question(this, tr("HTTP"),
                                  tr("Redirect to %1 ?").arg(newUrl.toString()),
                                  QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
            url = newUrl;
            replyTennisHighlights->deleteLater();
//            startRequest(url);
            return;
        }
    }
    else
    {
    }
*/
    replyTennisHighlights->deleteLater();
    replyTennisHighlights = NULL;
}

void HttpWindow::resizeWindow()
{
//  qDebug() << size() << sizeHint();

    if (sizeHint() != size())
    {
//      resize(sizeHint());
        adjustSize();
    }
}

void HttpWindow::httpBetfairTennisMarketsFinished()
{
    QJsonDocument    json;
    QJsonParseError  error;
    QTableWidgetItem *item;
    QNetworkReply    *reply = NULL;
    static int       count  = 0;

    for (QList<QNetworkReply *>::iterator i = replyTennisMarkets.begin(); i != replyTennisMarkets.end(); i++)
    {
        if ((*i)->isFinished())
        {
            reply = *i;
            replyTennisMarkets.erase(i);
            break;
        }
    }

    if (reply == NULL)
    {
        qDebug() << "We hebben een probleem...";
        return;
    }

    json = QJsonDocument::fromJson(reply->readAll(), &error);

    QString ww("betfair_tennis_markets");
    ww.append(QString("_%1").arg(count++));
    ww.append(".txt");
    QFile qq(ww);
    qq.open(QIODevice::WriteOnly | QIODevice::Text);
    qq.write(json.toJson());
    qq.close();

    QJsonValue value = json.object().value("eventTypes").toArray()[0].toObject().value("eventNodes");
    QJsonArray array = value.toArray();

    for (QJsonArray::const_iterator i = array.begin(); i != array.end(); i++)
    {
        QJsonArray a, b, c, d, e, f;
        QString marketId, text;
        int row, column;

        a = i->toObject().value("marketNodes").toArray();
        marketId = a[0].toObject().value("marketId").toString();

        for (row = 0; row < ui->tableWidget->rowCount(); row++)
        {
            if (ui->tableWidget->item(row, 0)->text() == marketId)
            {
                break;
            }
        }
        if (row == ui->tableWidget->rowCount())
            continue;

        b = a[0].toObject().value("runners").toArray();
        c = b[0].toObject().value("exchange").toObject().value("availableToBack").toArray();
        d = b[0].toObject().value("exchange").toObject().value("availableToLay").toArray();
        e = b[1].toObject().value("exchange").toObject().value("availableToBack").toArray();
        f = b[1].toObject().value("exchange").toObject().value("availableToLay").toArray();

        column = 5;
        for (QJsonArray::const_iterator j = c.begin(); j != c.end(); j++)
        {
            double price; //, size;

            price = j->toObject().value("price").toDouble();
//          size  = j->toObject().value("size").toDouble();

            item = ui->tableWidget->item(row, column);

            if (item == NULL)
            {
                item = new QTableWidgetItem;
                item->setText(QString("%1").arg(price));
                item->setBackgroundColor(QColor(0, 255, 0));
                ui->tableWidget->setItem(row, column, item);
            }
            else
            {
                text = QString("%1").arg(price);

                if (text == item->text())
                {
                    // item->setBackgroundColor(QColor(255, 255, 255));
                }
                else
                {
                    item->setBackgroundColor(QColor(0, 255, 0));
                }

                item->setText(text);
            }

            column--;
        }

        column = 7;
        for (QJsonArray::const_iterator j = e.begin(); j != e.end(); j++)
        {
            double price; //, size;

            price = j->toObject().value("price").toDouble();
//          size  = j->toObject().value("size").toDouble();

            item = ui->tableWidget->item(row, column);

            if (item == NULL)
            {
                item = new QTableWidgetItem;
                item->setText(QString("%1").arg(price));
                item->setBackgroundColor(QColor(0, 255, 0));
                ui->tableWidget->setItem(row, column, item);
            }
            else
            {
                text = QString("%1").arg(price);

                if (text == item->text())
                {
                    // item->setBackgroundColor(QColor(255, 255, 255));
                }
                else
                {
                    item->setBackgroundColor(QColor(0, 255, 0));
                }

                item->setText(text);
            }

            column++;
        }

        column = 11;
        item = ui->tableWidget->item(row, column);
        if (c.size() && e.size())
        {
            double p = c[0].toObject().value("price").toDouble();
            double q = e[0].toObject().value("price").toDouble();
            text = QString::number(100.0 / p + 100.0 / q, 'f', 2);
            if (item == NULL)
            {
                item = new QTableWidgetItem;
                item->setText(text);
                item->setBackgroundColor(QColor(0, 255, 0));
                ui->tableWidget->setItem(row, column, item);
            }
            else
            {
                if (text == item->text())
                {
                    // item->setBackgroundColor(QColor(255, 255, 255));
                }
                else
                {
                    item->setBackgroundColor(QColor(0, 255, 0));
                }

                item->setText(text);
            }
        }
    }

    ui->tableWidget->resizeColumnsToContents();
    QTimer::singleShot(0, this, SLOT(resizeWindow()));

    reply->deleteLater();
}

void HttpWindow::sslErrors(QNetworkReply *, const QList<QSslError> &errors)
{
    QString errorString;
    foreach (const QSslError &error, errors)
    {
        if (!errorString.isEmpty())
            errorString += ", ";
        errorString += error.errorString();
    }

    if (QMessageBox::warning(this, tr("HTTP"),
                             tr("One or more SSL errors has occurred: %1").arg(errorString),
                             QMessageBox::Ignore | QMessageBox::Abort) == QMessageBox::Ignore)
    {
        replyTennisHighlights->ignoreSslErrors();
    }
}

void HttpWindow::pokerStars()
{
    connect(&webSocket, SIGNAL(connected()), this, SLOT(onConnected()));
    connect(&webSocket, SIGNAL(sslErrors(const QList<QSslError> &)), this, SLOT(onSslErrors(const QList<QSslError> &)));
    connect(&webSocket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(onError()));

    webSocket.open(QUrl(QStringLiteral("wss://sports.pokerstars.eu/websocket")));


}

void HttpWindow::onError()
{
    qDebug() << "onError";
}

void HttpWindow::onConnected()
{
    qDebug() << "WebSocket connected" << webSocket.readBufferSize();
    connect(&webSocket, SIGNAL(textMessageReceived(const QString &)), this, SLOT(onTextMessageReceived(const QString &)));
    connect(&webSocket, SIGNAL(textFrameReceived(const QString &, bool)), this, SLOT(onTextFrameReceived(const QString &, bool)));
    webSocket.sendTextMessage(QStringLiteral("{\"PublicLoginRequest\":{\"application\":\"web-sportsbook\",\"locale\":\"en-gb\",\"channel\":\"INTERNET\",\"apiVersion\":2,\"reqId\":0}}"));

    pokerStarsTimer->start();

    urlPokerStarsRootLadder   = "https://sports.pokerstars.eu/sportsbook/v1/api/getRootLadder";
    replyPokerStarsRootLadder = qnam.get(QNetworkRequest(urlPokerStarsRootLadder));

    connect(replyPokerStarsRootLadder, SIGNAL(finished()), this, SLOT(httpPokerStarsRootLadderFinished()));
}

void HttpWindow::onTextMessageReceived(const QString &message)
{
    QJsonDocument   json;
    QJsonParseError error;
    QJsonValue      value;

//  qDebug() << "Message received:" << message.count() << message;

    json = QJsonDocument::fromJson(message.toUtf8(), &error);
    if (error.error)
        qDebug() << error.error << error.errorString();

//  qDebug() << endl << json << endl;

    value = json.object().value("PushMsg");
    if (value.isUndefined())
    {
        value = json.object().value("SubscribeResponse");
        if (value.isUndefined())
            return;

        value = value.toObject().value("match");
        if (value.isUndefined())
            return;
        if (value.toArray().size() == 0)
            return;

        value = value.toArray()[0].toObject().value("eventTradingState");

        QTimer::singleShot(0, this, SLOT(resizeWindow()));
    }
    else
    {
        value = value.toObject().value("eventTradingState");
    }

    if (value.isUndefined())
        return;

    if (value.toObject().value("prices").isUndefined())
        return;
    if (value.toObject().value("prices").toObject().value("market").isUndefined())
        return;
    if (value.toObject().value("prices").toObject().value("market").toArray()[0].toObject().value("channel").isUndefined())
        return;
    if (value.toObject().value("prices").toObject().value("market").toArray()[0].toObject().value("channel").toArray().size() == 0)
        return;
    if (value.toObject().value("prices").toObject().value("market").toArray()[0].toObject().value("channel").toArray()[0].toObject().value("selection").isUndefined())
        return;
    if (value.toObject().value("prices").toObject().value("market").toArray()[0].toObject().value("channel").toArray()[0].toObject().value("selection").toArray().size() < 2)
        return;
    if (value.toObject().value("prices").toObject().value("market").toArray()[0].toObject().value("channel").toArray()[0].toObject().value("selection").toArray()[0].toObject().value("rootIdx").isUndefined())
        return;

    if (value.isObject())
    {
        QString          eventId, text;
        int              row;
        QJsonArray       selection;
        int              idxA, idxB;
        double           priceA, priceB;
        QTableWidgetItem *item;
        static int       count = 0;

        QString ww("pokerstars_event_trading_state");
        ww.append(QString("_%1").arg(count++));
        ww.append(".txt");
        QFile qq(ww);
        qq.open(QIODevice::WriteOnly | QIODevice::Text);
        qq.write(json.toJson());
        qq.close();

        eventId = QString::number(value.toObject().value("id").toDouble(), 'f', 0);

        for (row = 0; row < ui->tableWidget->rowCount(); row++)
        {
            if (ui->tableWidget->item(row, 0)->text() == eventId)
                break;
        }
        if (row == ui->tableWidget->rowCount())
        {
            qDebug() << "Hmm, eventId waar we niet om gevraagd hebben...";
            return;
        }

        selection = value.toObject().value("prices").toObject().value("market").toArray()[0].toObject().value("channel").toArray()[0].toObject().value("selection").toArray();
//      qDebug() << selection.count();
        if (selection[0].toObject().value("type").toString() == "playerA")
        {
            idxA = (int)selection[0].toObject().value("rootIdx").toDouble();
            idxB = (int)selection[1].toObject().value("rootIdx").toDouble();
        }
        else
        {
            idxA = (int)selection[1].toObject().value("rootIdx").toDouble();
            idxB = (int)selection[0].toObject().value("rootIdx").toDouble();
        }
        priceA = rootLadder[idxA];
        priceB = rootLadder[idxB];

        text = QString::number(priceA, 'f', 2);
        item = ui->tableWidget->item(row, 5);
        if (item == 0)
        {
            item = new QTableWidgetItem;
            ui->tableWidget->setItem(row, 5, item);
        }
        if (text != item->text())
            item->setBackgroundColor(QColor(0, 255, 0));
        item->setText(text);

        text = QString::number(priceB, 'f', 2);
        item = ui->tableWidget->item(row, 7);
        if (item == 0)
        {
            item = new QTableWidgetItem;
            ui->tableWidget->setItem(row, 7, item);
            item->setBackgroundColor(QColor(0, 255, 0));
        }
        if (text != item->text())
            item->setBackgroundColor(QColor(0, 255, 0));
        item->setText(text);

        text = QString::number(100.0 / priceA + 100.0 / priceB, 'f', 2);
        item = ui->tableWidget->item(row, 11);
        if (item == 0)
        {
            item = new QTableWidgetItem;
            ui->tableWidget->setItem(row, 11, item);
            item->setBackgroundColor(QColor(0, 255, 0));
        }
        if (text != item->text())
            item->setBackgroundColor(QColor(0, 255, 0));
        item->setText(text);
    }

    ui->tableWidget->resizeColumnsToContents();
}

void HttpWindow::onTextFrameReceived(const QString &frame, bool last)
{
//  qDebug() << "last:" << last << "frame:" << frame.count() << frame << endl;
}

void HttpWindow::onPokerStarsTimeout()
{
    webSocket.sendTextMessage("{KeepAlive:{reqId:1}}");
}

void HttpWindow::onSslErrors(const QList<QSslError> &errors)
{
    Q_UNUSED(errors);

    qDebug() << "onSslErrors";

    // WARNING: Never ignore SSL errors in production code.
    // The proper way to handle self-signed certificates is to add a custom root
    // to the CA store.

    webSocket.ignoreSslErrors();
}

void HttpWindow::httpPokerStarsRootLadderFinished()
{
    QJsonDocument   json;
    QJsonParseError error;
    QJsonArray      array;

    json = QJsonDocument::fromJson(replyPokerStarsRootLadder->readAll(), &error);

    replyPokerStarsRootLadder->deleteLater();
    replyPokerStarsRootLadder = NULL;

    QFile qq("pokerstars_root_ladder.txt");
    qq.open(QIODevice::WriteOnly | QIODevice::Text);
    qq.write(json.toJson());
    qq.close();

    if (json.object().value("PriceAdjustmentDetailsResponse").isUndefined())
        return;
    if (json.object().value("PriceAdjustmentDetailsResponse").toObject().value("rootLadder").isUndefined())
        return;

    array = json.object().value("PriceAdjustmentDetailsResponse").toObject().value("rootLadder").toArray();

    rootLadder.clear();
    rootLadder.reserve(array.size());
    for (int i = 0; i < array.size(); i++)
    {
        rootLadder.append(0.0);
    }
    for (int i = 0; i < array.size(); i++)
    {
        int    idx     = (int)array[i].toObject().value("rootIndex").toDouble();
        double decimal = array[i].toObject().value("decimal").toString().toDouble();

        qDebug() << idx << decimal;

        rootLadder[idx] = decimal;
    }

    urlPokerStarsTennisHighlights   = "https://sports.pokerstars.eu/sportsbook/v1/api/getSportSchedule?sport=TENNIS&marketTypes=AB&days=0%2C1%2C2%2C3%2C4&embedComps=false&maxPrematch=20&maxInplay=20&topupInplay=true&channelId=6&locale=en-gb";
    replyPokerStarsTennisHighlights = qnam.get(QNetworkRequest(urlPokerStarsTennisHighlights));

    connect(replyPokerStarsTennisHighlights, SIGNAL(finished()), this, SLOT(httpPokerStarsTennisHighlightsFinished()));
}

void HttpWindow::httpPokerStarsTennisHighlightsFinished()
{
    QJsonDocument   json;
    QJsonParseError error;
    QJsonObject     object;
    QJsonArray      inPlay, preMatch;
    int             added = 0, deleted = 0;

    json = QJsonDocument::fromJson(replyPokerStarsTennisHighlights->readAll(), &error);

    QFile qq("pokerstars_tennis_higlights.txt");
    qq.open(QIODevice::WriteOnly | QIODevice::Text);
    qq.write(json.toJson());
    qq.close();

    object = json.object().value("Sport").toObject();
    inPlay = object.value("inplay").toObject().value("event").toArray();
    preMatch = object.value("prematch").toObject().value("event").toArray();

    eventIds.clear();
    for (QJsonArray::const_iterator i = inPlay.begin(); i != inPlay.end(); i++)
    {
        QString eventId;
        int     row;

        qDebug() << QString::number(i->toObject().value("eventTime").toDouble(), 'f', 0) << i->toObject().value("name").toString();

        eventId = QString::number(i->toObject().value("id").toDouble(), 'f', 0);
        eventIds.append(eventId);

        for (row = 0; row < ui->tableWidget->rowCount(); row++)
        {
            if (ui->tableWidget->item(row, 0)->text() == eventId)
                break;
        }
        if (row < ui->tableWidget->rowCount())
        {
            if (QString::number(i->toObject().value("eventTime").toDouble(), 'f', 0) != ui->tableWidget->item(row, 1)->text())
                qDebug() << "eventTime has changed...";
            continue;
        }
        else
        {
            ui->tableWidget->insertRow(row);
            ui->tableWidget->setItem(row, 0, new QTableWidgetItem(QString::number(i->toObject().value("id").toDouble(), 'f', 0)));
            ui->tableWidget->setItem(row, 1, new QTableWidgetItem(QString::number(i->toObject().value("eventTime").toDouble(), 'f', 0)));
            ui->tableWidget->setItem(row, 2, new QTableWidgetItem(i->toObject().value("name").toString()));
            added++;
        }
    }

    for (QJsonArray::const_iterator i = preMatch.begin(); i != preMatch.end(); i++)
    {
        QString eventId;
        int     row;

        qDebug() << QString::number(i->toObject().value("eventTime").toDouble(), 'f', 0) << i->toObject().value("name").toString();

        eventId = QString::number(i->toObject().value("id").toDouble(), 'f', 0);
        eventIds.append(eventId);

        for (row = 0; row < ui->tableWidget->rowCount(); row++)
        {
            if (ui->tableWidget->item(row, 0)->text() == eventId)
                break;
        }
        if (row < ui->tableWidget->rowCount())
        {
            if (QString::number(i->toObject().value("eventTime").toDouble(), 'f', 0) != ui->tableWidget->item(row, 1)->text())
                qDebug() << "eventTime has changed...";
            continue;
        }
        else
        {
            ui->tableWidget->insertRow(row);
            ui->tableWidget->setItem(row, 0, new QTableWidgetItem(QString::number(i->toObject().value("id").toDouble(), 'f', 0)));
            ui->tableWidget->setItem(row, 1, new QTableWidgetItem(QString::number(i->toObject().value("eventTime").toDouble(), 'f', 0)));
            ui->tableWidget->setItem(row, 2, new QTableWidgetItem(i->toObject().value("name").toString()));
            added++;
        }
    }

    for (int row = 0; row < ui->tableWidget->rowCount(); row++)
    {
        int i;

        for (i = 0; i < eventIds.count(); i++)
        {
            if (eventIds[i] == ui->tableWidget->item(row, 0)->text())
                break;
        }
        if (i >= eventIds.count())
        {
            ui->tableWidget->item(row, 2)->setBackgroundColor(QColor(255, 0, 0));
            deleted++;
        }
    }

    qDebug() << eventIds.count() << ui->tableWidget->rowCount() << added << deleted;

    ui->tableWidget->sortByColumn(1, Qt::AscendingOrder);
    ui->tableWidget->resizeColumnsToContents();

    QTimer::singleShot(0, this, SLOT(resizeWindow()));

    replyPokerStarsTennisHighlights->deleteLater();
    replyPokerStarsTennisHighlights = NULL;

    QString subscribeA = "{\"UpdateSubcriptions\":{\"snapshotResponse\":true,\"toAdd\":[{\"name\":\"eventSummaries\",\"ids\":\"";
    QString subscribeB = "\"},{\"name\":\"marketTypes\",\"ids\":\"AB\"},{\"name\":\"schedule\",\"ids\":\"tennis\"}]}}";

    for (int i = 0; i < eventIds.count(); i++)
    {
        QString subscribeC = subscribeA + eventIds[i] + subscribeB;

        webSocket.sendTextMessage(subscribeC);
    }
}

void HttpWindow::onColorTimeout()
{
    for (int row = 0; row < ui->tableWidget->rowCount(); row++)
    {
        int r = row;
        for (int col = 0; col < ui->tableWidget->columnCount(); col++)
        {
            QColor backgroundColor;

            if (row != r)
                continue;

            if (ui->tableWidget->item(row, col))
                backgroundColor = ui->tableWidget->item(row, col)->backgroundColor();
            else
                continue;

            if (backgroundColor.blue() != 255)
            {
                if (backgroundColor.red() != backgroundColor.blue() && backgroundColor.red() >= 200)
                {
                    if (backgroundColor.red() == 200)
                    {
                        ui->tableWidget->removeRow(row);
                        row--;
                        continue;
                    }
                    else
                    {
                        ui->tableWidget->item(row, col)->setBackgroundColor(QColor(backgroundColor.red() - 1, 0, 0));
                    }
                }
                else
                {
                    ui->tableWidget->item(row, col)->setBackgroundColor(QColor(backgroundColor.red() + 5, 255, backgroundColor.blue() + 5));
                }

            }
        }
    }
}
