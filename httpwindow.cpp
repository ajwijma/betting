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

    ui->tableWidget->setColumnCount(10);

    m_iTennisHighlightsTimeout = 1;
    m_iTennisMarketsTimeout    = 5;
    timer = new QTimer;
    timer->setInterval(1000);

    connect(ui->pushButtonStart, SIGNAL(clicked()), timer, SLOT(start()));
    connect(timer, SIGNAL(timeout()), this, SLOT(timerAction()));

#ifndef QT_NO_SSL
    connect(&qnam, SIGNAL(sslErrors(QNetworkReply *, QList<QSslError>)), this, SLOT(sslErrors(QNetworkReply *, QList<QSslError>)));
#endif

    setWindowTitle(tr("HTTP"));
}

HttpWindow::~HttpWindow()
{
    delete ui;
}

void HttpWindow::timerAction()
{
    if (--m_iTennisHighlightsTimeout <= 0)
    {
        downloadTennisHighlights();
        m_iTennisHighlightsTimeout = 60;
    }

    if (--m_iTennisMarketsTimeout <= 0)
    {
        downloadTennisMarkets();
        m_iTennisMarketsTimeout = 5;
    }
}

void HttpWindow::downloadTennisHighlights()
{
    replyTennisHighlights = qnam.get(QNetworkRequest(urlTennisHighlights));

    connect(replyTennisHighlights, SIGNAL(finished()),
            this, SLOT(httpTennisHighlightsFinished()));
}

void HttpWindow::downloadTennisMarkets()
{
    int count = 0;

    replies = 0;

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
            replyTennisMarkets[replies] = qnam.get(QNetworkRequest(urlTennisMarkets));
            connect(replyTennisMarkets[replies], SIGNAL(finished()),
                    this, SLOT(httpTennisMarketsFinished()));
            replies++;
            count = 0;
            urlTennisMarkets = urlTennisMarketsBase;
        }
    }

    if (count)
    {
        replyTennisMarkets[replies] = qnam.get(QNetworkRequest(urlTennisMarkets));
        connect(replyTennisMarkets[replies], SIGNAL(finished()),
                this, SLOT(httpTennisMarketsFinished()));

    }
}

void HttpWindow::httpTennisHighlightsFinished()
{
    QJsonDocument   json;
    QJsonParseError error;
    int count = 0;

    json = QJsonDocument::fromJson(replyTennisHighlights->readAll(), &error);

    QFile qq("tennis_highlights.txt");
    qq.open(QIODevice::WriteOnly | QIODevice::Text);
    qq.write(json.toJson());
    qq.close();

    qDebug() << json.toJson();

    QJsonValue value = json.object().value("page").toObject().value("config").toObject().value("marketData");

    QJsonArray array = value.toArray();
    marketIds.clear();
    for (QJsonArray::const_iterator i = array.begin(); i != array.end(); i++)
    {
        QJsonArray a;

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

        marketIds.append(i->toObject().value("marketId").toString());

//      double x = a[0].toObject().value("prices").toObject().value("back").toArray()[0].toObject().value("price").toDouble();
//      double y = a[1].toObject().value("prices").toObject().value("back").toArray()[0].toObject().value("price").toDouble();

        qDebug() << i->toObject().value("eventName").toString();
        if (ui->tableWidget->rowCount() < count + 1)
            ui->tableWidget->insertRow(count);
        ui->tableWidget->setItem(count++, 0, new QTableWidgetItem(i->toObject().value("eventName").toString()));
//      table->insertRow(1);

//        qDebug() << i->toObject().value("eventName").toString() << QString("(%1 / %2, %3 %)").arg(x).arg(y).arg(100.0 * (1/x + 1/y)) << i->toObject().value("marketId").toString();
    }

    qDebug() << marketIds.count();

    ui->tableWidget->resizeColumnsToContents();

//    downloadTennisMarkets();

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

    replyTennisHighlights->deleteLater();
    replyTennisHighlights = NULL;
}

void HttpWindow::httpTennisMarketsFinished()
{
    QJsonDocument   json;
    QJsonParseError error;
    QTableWidgetItem *item;
    int i;
    static int count = 0;

    for (i = 0; i < 16; i++)
    {
        if (replyTennisMarkets[i] && replyTennisMarkets[i]->isFinished())
            break;
    }

//    dataTennisMarkets->append(replyTennisMarkets[i]->readAll());

    json = QJsonDocument::fromJson(replyTennisMarkets[i]->readAll(), &error);
//    dataTennisMarkets->clear();

    QString ww("tennis_markets");
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

        for (row = 0; row < marketIds.count(); row++)
        {
            if (marketIds[row] == marketId)
            {
                break;
            }
        }

        if (row == marketIds.count())
            continue;

        b = a[0].toObject().value("runners").toArray();
        c = b[0].toObject().value("exchange").toObject().value("availableToBack").toArray();
        d = b[0].toObject().value("exchange").toObject().value("availableToLay").toArray();
        e = b[1].toObject().value("exchange").toObject().value("availableToBack").toArray();
        f = b[1].toObject().value("exchange").toObject().value("availableToLay").toArray();

        column = 3;
        for (QJsonArray::const_iterator j = c.begin(); j != c.end(); j++)
        {
            double price, size;

            price = j->toObject().value("price").toDouble();
            size  = j->toObject().value("size").toDouble();

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
                    item->setBackgroundColor(QColor(255, 255, 255));
                else
                    item->setBackgroundColor(QColor(0, 255, 0));

                item->setText(text);
            }

            column--;
        }

        column = 5;
        for (QJsonArray::const_iterator j = e.begin(); j != e.end(); j++)
        {
            double price, size;

            price = j->toObject().value("price").toDouble();
            size  = j->toObject().value("size").toDouble();

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
                    item->setBackgroundColor(QColor(255, 255, 255));
                else
                    item->setBackgroundColor(QColor(0, 255, 0));

                item->setText(text);
            }

            column++;
        }

        column = 9;
        item = ui->tableWidget->item(row, column);
        double p = c[0].toObject().value("price").toDouble();
        double q = e[0].toObject().value("price").toDouble();
        text = QString("%1").arg(100.0 * (1/p + 1/q));
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
                item->setBackgroundColor(QColor(255, 255, 255));
            else
                item->setBackgroundColor(QColor(0, 255, 0));

            item->setText(text);
        }
    }

    ui->tableWidget->resizeColumnsToContents();

    replyTennisMarkets[i]->deleteLater();
    replyTennisMarkets[i] = NULL;
}

#ifndef QT_NO_SSL
void HttpWindow::sslErrors(QNetworkReply *, const QList<QSslError> &errors)
{
    QString errorString;
    foreach (const QSslError &error, errors) {
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
#endif
