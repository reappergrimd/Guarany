#include "poolbrowser.h"
#include "ui_poolbrowser.h"
#include "main.h"
#include "wallet.h"
#include "base58.h"
#include "clientmodel.h"
#include "bitcoinrpc.h"

#include <sstream>
#include <string>

#include <QDesktopServices>
#include <QString>

using namespace json_spirit;

//Coinbase API
const QString apiCoinbasePrice = "https://coinbase.com/api/v1/currencies/exchange_rates";

/*//Bleutrade API
const QString apiBleutradeMarketSummary = "https://bleutrade.com/api/v2/public/getmarketsummaries"; 
const QString apiBleutradeTrades = "https://bleutrade.com/api/v2/public/getmarkethistory?market=BTC_NMB&count=50";
const QString apiBleutradeOrders = "https://bleutrade.com/api/v2/public/getorderbook?market=BTC_NMB&type=all&depth=50"; 
*/

const QString apiBleutradeMarketSummary = "https://bittrex.com/api/v1.1/public/getmarketsummaries"; 
const QString apiBleutradeTrades = "https://bittrex.com/api/v1.1/public/getmarkethistory?market=BTC-DOGE&count=50";
const QString apiBleutradeOrders = "https://bittrex.com/api/v1.1/public/getorderbook?market=BTC-DOGE&type=both&depth=50"; 


//Common Globals
int mode=1;
double _dScPriceLast = 0;
double _dBtcPriceCurrent = 0;
double _dBtcPriceLast = 0;

//Bleutrade Globals
BleutradeMarketSummary* _bleutradeMarketSummary = new BleutradeMarketSummary();
BleutradeTrades* _bleutradeTrades = new BleutradeTrades();
BleutradeOrders* _bleutradeOrders = new BleutradeOrders();

PoolBrowser::PoolBrowser(QWidget* parent) : QWidget(parent), ui(new Ui::PoolBrowser)
{
    //TODO: Complete multi-threading so we don't have to call this as a primer
    getRequest(apiCoinbasePrice);

    ui->setupUi(this);
    setFixedSize(400, 420);

    ui->qCustomPlotBleutradeTrades->addGraph();
    ui->qCustomPlotBleutradeTrades->setBackground(QBrush(QColor("#edf1f7")));

    ui->qCustomPlotBleutradeOrderDepth->addGraph();
    ui->qCustomPlotBleutradeOrderDepth->addGraph();
    ui->qCustomPlotBleutradeOrderDepth->setBackground(QBrush(QColor("#edf1f7")));


    QObject::connect(&m_nam, SIGNAL(finished(QNetworkReply*)), this, SLOT(parseNetworkResponse(QNetworkReply*)), Qt::AutoConnection);

    //One time primer
    pollAPIs();
}

void PoolBrowser::on_btnConvertSilkoin_clicked()
{
    double silkcoinQty = ui->txtConvertSilkcoinQty->text().toDouble();
    double totalBtc = _bleutradeMarketSummary->getLastCurrent(double()) * silkcoinQty;
    double totalUsd = totalBtc * _dBtcPriceCurrent;

    ui->lblConvertSilkcoinResults->setText("$" + QString::number(totalUsd, 'f', 2) +
                                           "  /  B"+ QString::number(totalBtc, 'f', 8));

}
void PoolBrowser::on_btnUpdateMarketData_clicked()
{
    pollAPIs();
}

void PoolBrowser::openBleutrade()
{
    QDesktopServices::openUrl(QUrl("https://www.bittrex.com/Market/Index?MarketName=BTC-NMB"));
}
void PoolBrowser::openPoloniex()
{
    QDesktopServices::openUrl(QUrl("https://poloniex.com/exchange/btc_nmb"));
}

void PoolBrowser::pollAPIs()
{
    ui->iconOverviewUpdateWait->setVisible(true);

    getRequest(apiCoinbasePrice);

    getRequest(apiBleutradeMarketSummary);
    getRequest(apiBleutradeTrades);
    getRequest(apiBleutradeOrders);
}

void PoolBrowser::processOverview()
{
}

void PoolBrowser::getRequest(const QString &urlString)
{
    QUrl url (urlString);
    QNetworkRequest req (url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json; charset=utf-8");
    m_nam.get(req);
}

void PoolBrowser::parseNetworkResponse(QNetworkReply* response)
{
    QUrl apiCall = response->url();

    if (response->error() != QNetworkReply::NoError) {
        //Communication error has occurred
        emit networkError(response->error());
        return;
    }

    if (apiCall == apiCoinbasePrice) { coinbasePrice(response); }
    else if (apiCall == apiBleutradeMarketSummary) { bleutradeMarketSummary(response); }
    else if (apiCall == apiBleutradeTrades) { bleutradeTrades(response); }
    else if (apiCall == apiBleutradeOrders) { bleutradeOrders(response); }
    else { }  //Sould NEVER get here unless something went completely awry

    if (_bleutradeMarketSummary->getLastPrev(double()) > 0)
    {
        ui->iconOverviewUpdateWait->setVisible(false);
    }

    processOverview();

    response->deleteLater();
}

/*************************************************************************************
 * Method: PoolBrowser::coinbasePrice
 * Parameter(s): QNetworkReply* response
 *
 * Unauthenticated resource that returns BTC to fiat (and vice versus) exchange rates in various currencies.
 * It has keys for both btc_to_xxx and xxx_to_btc so you can convert either way.
 * The key always contains downcase representations of the currency ISO.
 * Note that some small numbers may use E notation such as 2.8e-05.
 *
 * Response: {"btc_to_pgk":"28.152994","btc_to_gyd":"2743.906541","btc_to_mmk":"11611.550858", ... ,"brl_to_btc":"0.037652"}
 *************************************************************************************/
void PoolBrowser::coinbasePrice(QNetworkReply* response)
{
    mValue jsonResponse = new mValue();
    QString apiResponse = response->readAll();

    //Make sure the response is valid
    if(!read_string(apiResponse.toStdString(), jsonResponse)) { return; }

    mObject jsonObject = jsonResponse.get_obj();

    _dBtcPriceCurrent =  QString::fromStdString(getPairValue(jsonObject, "btc_to_usd").get_str()).toDouble();

    _dBtcPriceLast = _dBtcPriceCurrent;
    _dScPriceLast = _dBtcPriceCurrent * _bleutradeMarketSummary->getLastCurrent(double());
}

/*************************************************************************************
 * Method: PoolBrowser::bleutradeMarketSummary
 * Parameter(s): QNetworkReply* response
 *
 * Used to get the last 24 hour summary of all active exchanges
 *
 * Parameter(s): None
 * Response:
 * {
 * 	"success" : true,
 * 	"message" : "",
 * 	"result" : [{
 * 	        "MarketName" : "BTC-LTC",
 * 	        "High" : 0.02590000,
 * 	        "Low" : 0.02400000,
 * 	        "Volume" : 114.84340665,
 * 	        "Last" : 0.02480000,
 * 	        "BaseVolume" : 2.85028800,
 * 	        "TimeStamp" : "2014-04-19T20:49:23.483"
 *         }, {
 * 	        "MarketName" : "BTC-WC",
 * 	        "High" : 0.00002456,
 * 	        "Low" : 0.00001352,
 * 	        "Volume" : 4574426.27271220,
 * 	        "Last" : 0.00002006,
 * 	        "BaseVolume" : 82.96629666,
 * 	        "TimeStamp" : "2014-04-19T20:49:50.053"
 *         }
 * 	]
 * }
 *************************************************************************************/
void PoolBrowser::bleutradeMarketSummary(QNetworkReply* response)
{
    QString apiResponse = response->readAll();

    apiResponse = apiResponse.replace("{\"success\":true,\"message\":\"\",\"result\":[", "").replace("]}","").replace("},{", "}{");

    QStringList qslApiResponse = apiResponse.split("{", QString::SkipEmptyParts);

    for(int i = 0; i < qslApiResponse.count(); i++){
        mValue jsonResponse = new mValue();

        //Fix missing leading brace caused by split string, otherwise it will not be recognized an an mObject
        qslApiResponse[i].replace("\"MarketName", "{\"MarketName");

        //json_spirit does not handle null so make it "null"
        qslApiResponse[i].replace("null", "\"null\"");

        //Make sure the response is valid
        if(read_string(qslApiResponse[i].toStdString(), jsonResponse)) {
            mObject jsonObject = jsonResponse.get_obj();

            if (getPairValue(jsonObject, "MarketName").get_str() == "BTC_NMB") {
                try {
                    _bleutradeMarketSummary->setHighCurrent(getPairValue(jsonObject, "High").get_real());
                    _bleutradeMarketSummary->setLowCurrent(getPairValue(jsonObject, "Low").get_real());
                    _bleutradeMarketSummary->setVolumeCurrent(getPairValue(jsonObject, "Volume").get_real());
                    _bleutradeMarketSummary->setLastCurrent(getPairValue(jsonObject, "Last").get_real());
                    _bleutradeMarketSummary->setBaseVolumeCurrent(getPairValue(jsonObject, "BaseVolume").get_real());
                    _bleutradeMarketSummary->setTimeStamp(getPairValue(jsonObject, "TimeStamp").get_str());
                    _bleutradeMarketSummary->setBidCurrent(getPairValue(jsonObject, "Bid").get_real());
                    _bleutradeMarketSummary->setAskCurrent(getPairValue(jsonObject, "Ask").get_real());
                    _bleutradeMarketSummary->setPrevDayCurrent(getPairValue(jsonObject, "PrevDay").get_real());
                }
                catch (std::exception) {} //API did not return all needed data so skip processing market summary

                break;
            }
        }
    }

    updateLabel(ui->lblBleutradeHighBtc,
                _bleutradeMarketSummary->getHighCurrent(double()),
                _bleutradeMarketSummary->getHighPrev(double()),
                QString("B"),
                8);

    updateLabel(ui->lblBleutradeLowBtc,
                _bleutradeMarketSummary->getLowCurrent(double()),
                _bleutradeMarketSummary->getLowPrev(double()),
                QString("B"),
                8);

    updateLabel(ui->lblBleutradeCloseBtc,
                _bleutradeMarketSummary->getPrevDayCurrent(double()),
                _bleutradeMarketSummary->getPrevDayPrev(double()),
                QString("B"),
                8);

    double changeCurrent = (_bleutradeMarketSummary->getLastCurrent(double()) - _bleutradeMarketSummary->getPrevDayCurrent(double())) / _bleutradeMarketSummary->getPrevDayCurrent(double()) * 100;
    double changeLast  = (_bleutradeMarketSummary->getLastPrev(double()) - _bleutradeMarketSummary->getPrevDayCurrent(double())) / _bleutradeMarketSummary->getPrevDayCurrent(double()) * 100;

    QString changeDirection = _bleutradeMarketSummary->getLastCurrent(double()) > _bleutradeMarketSummary->getPrevDayCurrent(double())
            ? QString("+") : _bleutradeMarketSummary->getLastCurrent(double()) < _bleutradeMarketSummary->getPrevDayCurrent(double())
            ? QString("") : QString("");

    updateLabel(ui->lblBleutradeChangePerc,
                changeCurrent,
                changeLast,
                changeDirection,
                QString("%"),
                2);

    updateLabel(ui->lblBleutradeVolumeUsd,
                _bleutradeMarketSummary->getBaseVolumeCurrent(double()) * _dBtcPriceCurrent,
                _bleutradeMarketSummary->getBaseVolumePrev(double()) * _dBtcPriceCurrent,
                QString(""),
                2);

    updateLabel(ui->lblBleutradeVolumeSc,
                _bleutradeMarketSummary->getVolumeCurrent(double()),
                _bleutradeMarketSummary->getVolumePrev(double()),
                QString(""),
                4);

    updateLabel(ui->lblBleutradeVolumeBtc,
                _bleutradeMarketSummary->getBaseVolumeCurrent(double()),
                _bleutradeMarketSummary->getBaseVolumePrev(double()),
                QString(""),
                4);

    updateLabel(ui->lblBleutradeLastBtc,
                _bleutradeMarketSummary->getLastCurrent(double()),
                _bleutradeMarketSummary->getLastPrev(double()),
                QString("B"),

                8);

    updateLabel(ui->lblBleutradeLastUsd,
                _bleutradeMarketSummary->getLastCurrent(double()) * _dBtcPriceCurrent,
                _bleutradeMarketSummary->getLastPrev(double()) * _dBtcPriceCurrent,
                QString("$"),
                8);

    updateLabel(ui->lblBleutradeAskBtc,
                _bleutradeMarketSummary->getAskCurrent(double()),
                _bleutradeMarketSummary->getAskPrev(double()),
                QString("B"),
                8);

    updateLabel(ui->lblBleutradeAskUsd,
                _bleutradeMarketSummary->getAskCurrent(double()) * _dBtcPriceCurrent,
                _bleutradeMarketSummary->getAskPrev(double()) * _dBtcPriceCurrent,
                QString("$"),
                8);

    updateLabel(ui->lblBleutradeBidBtc,
                _bleutradeMarketSummary->getBidCurrent(double()),
                _bleutradeMarketSummary->getBidPrev(double()),
                QString("B"),
                8);

    updateLabel(ui->lblBleutradeBidUsd,
                _bleutradeMarketSummary->getBidCurrent(double()) * _dBtcPriceCurrent,
                _bleutradeMarketSummary->getBidPrev(double()) * _dBtcPriceCurrent,
                QString("$"),
                8);

    _bleutradeMarketSummary->setAskPrev(_bleutradeMarketSummary->getAskCurrent(double()));
    _bleutradeMarketSummary->setBaseVolumePrev(_bleutradeMarketSummary->getBaseVolumeCurrent(double()));
    _bleutradeMarketSummary->setBidPrev(_bleutradeMarketSummary->getBidCurrent(double()));
    _bleutradeMarketSummary->setHighPrev(_bleutradeMarketSummary->getHighCurrent(double()));
    _bleutradeMarketSummary->setLowPrev(_bleutradeMarketSummary->getLowCurrent(double()));
    _bleutradeMarketSummary->setPrevDayPrev(_bleutradeMarketSummary->getPrevDayCurrent(double()));
    _bleutradeMarketSummary->setLastPrev(_bleutradeMarketSummary->getLastCurrent(double()));
    _bleutradeMarketSummary->setVolumePrev(_bleutradeMarketSummary->getVolumeCurrent(double()));

    _dScPriceLast = _dBtcPriceCurrent * _bleutradeMarketSummary->getLastCurrent(double());
}
/*************************************************************************************
 * Method: PoolBrowser::bleutradeTrades
 * Parameter(s): QNetworkReply* response
 *
 * Used to retrieve the latest trades that have occurred for a specific market
 * Parameter(s):
 * market (required): a string literal for the market (ex: BTC-LTC)
 * count (optional): a number between 1-100 for the number of entries to return (default = 20)
 *
 *     {
 * 	"success" : true,
 * 	"message" : "",
 * 	"result" : [{
 * 			"OrderId" : "12323",
 * 			"TimeStamp" : "2014-02-25T07:40:08.68",
 * 			"Quantity" : 185.06100000,
 * 			"Price" : 0.00000174,
 * 			"Total" : 0.00032200
 * 		}, {
 * 			"OrderUuid" : "12322",
 * 			"TimeStamp" : "2014-02-25T07:39:18.603",
 * 			"Quantity" : 10.74500000,
 * 			"Price" : 0.00000172,
 * 			"Total" : 0.00001848
 * 		}, {
 * 			"OrderUuid" : "12321",
 * 			"TimeStamp" : "2014-02-25T07:39:18.6",
 * 			"Quantity" : 5.62100000,
 * 			"Price" : 0.00000172,
 * 			"Total" : 0.00000966
 * 		}, {
 * 			"OrderUuid" : "12319",
 * 			"TimeStamp" : "2014-02-25T07:39:18.6",
 * 			"Quantity" : 76.23000000,
 * 			"Price" : 0.00000173,
 * 			"Total" : 0.00013187
 * 		}, {
 * 			"OrderUuid" : "12317",
 * 			"TimeStamp" : "2014-02-25T07:39:18.6",
 * 			"Quantity" : 52.47500000,
 * 			"Price" : 0.00000174,
 * 			"Total" : 0.00009130
 * 		}
 * 	]
 * }
 *************************************************************************************/
void PoolBrowser::bleutradeTrades(QNetworkReply* response)
{
    int z = 0;
    double high, low = 100000;

    ui->tblBleutradeTrades->clear();
    ui->tblBleutradeTrades->setColumnWidth(0, 60);
    ui->tblBleutradeTrades->setColumnWidth(1, 110);
    ui->tblBleutradeTrades->setColumnWidth(2, 110);
    ui->tblBleutradeTrades->setColumnWidth(3, 100);
    ui->tblBleutradeTrades->setColumnWidth(4, 160);
    ui->tblBleutradeTrades->setSortingEnabled(false);

    QString apiResponse = response->readAll();

    apiResponse = apiResponse.replace("{\"success\":true,\"message\":\"\",\"result\":[", "").replace("]}","").replace("},{", "}{");

    QStringList qslApiResponse = apiResponse.split("{", QString::SkipEmptyParts);

    int tradeCount = qslApiResponse.count();
    QVector<double> xAxis(tradeCount), yAxis(tradeCount);

    for(int i = 0; i < tradeCount; i++){
        mValue jsonResponse = new mValue();

        //Fix missing leading brace caused by split string, otherwise it will not be recognized an an mObject
        qslApiResponse[i].replace("\"Id", "{\"Id");

        //json_spirit does not handle null so make it "null"
        qslApiResponse[i].replace("null", "\"null\"");

        //Make sure the response is valid
        if(read_string(qslApiResponse[i].toStdString(), jsonResponse)) {
            mObject jsonObject = jsonResponse.get_obj();

            try
            {
                _bleutradeTrades->setId(getPairValue(jsonObject, "Id").get_real());
                //_bleutradesTrades->setTimeStamp(getPairValue(jsonObject, "TimeStamp").get_str());
                _bleutradeTrades->setQuantity(getPairValue(jsonObject, "Quantity").get_real());
                _bleutradeTrades->setPrice(getPairValue(jsonObject, "Price").get_real());
                _bleutradeTrades->setTotal(getPairValue(jsonObject, "Total").get_real());
                _bleutradeTrades->setFillType(getPairValue(jsonObject, "FillType").get_str());
                _bleutradeTrades->setOrderType(getPairValue(jsonObject, "OrderType").get_str());
            }
            catch (std::exception) {} //API did not return all needed data so skip this trade

            QTreeWidgetItem * qtTrades = new QTreeWidgetItem();

            qtTrades->setText(0, _bleutradeTrades->getOrderType());
            qtTrades->setText(1, _bleutradeTrades->getPrice(QString()));
            qtTrades->setText(2, _bleutradeTrades->getQuantity(QString()));
            qtTrades->setText(3, _bleutradeTrades->getTotal(QString()));
            qtTrades->setText(4, _bleutradeTrades->getTimeStamp());

            ui->tblBleutradeTrades->addTopLevelItem(qtTrades);

            xAxis[z] = tradeCount - z;
            yAxis[z] = _bleutradeTrades->getPrice(double()) * 100000000;

            high = _bleutradeTrades->getPrice(double()) > high ? _bleutradeTrades->getPrice(double()) : high;
            low = _bleutradeTrades->getPrice(double()) < low ? _bleutradeTrades->getPrice(double()) : low;

            z++;
        }
    }

    high *=  100000000;
    low *=  100000000;

    ui->qCustomPlotBleutradeTrades->graph(0)->setData(xAxis, yAxis);
    ui->qCustomPlotBleutradeTrades->graph(0)->setPen(QPen(QColor(34, 177, 76)));
    ui->qCustomPlotBleutradeTrades->graph(0)->setBrush(QBrush(QColor(34, 177, 76, 20)));

    ui->qCustomPlotBleutradeTrades->xAxis->setRange(1, tradeCount);
    ui->qCustomPlotBleutradeTrades->yAxis->setRange(low, high);

    ui->qCustomPlotBleutradeTrades->replot();

}
/*************************************************************************************
 * Method: PoolBrowser::bleutradeOrders
 * Parameter(s): QNetworkReply* response
 *
 * Used to get retrieve the orderbook for a given market
 *
 * Parameters:
 * market	(required)	a string literal for the market (ex: BTC-LTC)
 * type	(required)	buy, sell or both to identify the type of orderbook to return.
 * depth	(optional)	defaults to 20 - how deep of an order book to retrieve. Max is 100
 *
 * Response
 *     {
 * 	"success" : true,
 * 	"message" : "",
 * 	"result" : {
 * 		"buy" : [{
 * 				"Quantity" : 12.37000000,
 * 				"Rate" : 0.02525000
 * 			}
 * 		],
 * 		"sell" : [{
 * 				"Quantity" : 32.55412402,
 * 				"Rate" : 0.02540000
 * 			}, {
 * 				"Quantity" : 60.00000000,
 * 				"Rate" : 0.02550000
 * 			}, {
 * 				"Quantity" : 60.00000000,
 * 				"Rate" : 0.02575000
 * 			}, {
 * 				"Quantity" : 84.00000000,
 * 				"Rate" : 0.02600000
 * 			}
 * 		]
 * 	}
 * }
 ************************************************************************************/
void PoolBrowser::bleutradeOrders(QNetworkReply* response)
{
    int z = 0;
    double high = 0;
    double low = 100000;
    double sumBuys = 0;
    double sumSells = 0;
    double sumHighest = 0;

    ui->qTreeWidgetBleutradeBuy->clear();
    ui->qTreeWidgetBleutradeBuy->sortByColumn(0, Qt::DescendingOrder);
    ui->qTreeWidgetBleutradeBuy->setSortingEnabled(true);

    ui->qTreeWidgetBleutradeSell->clear();
    ui->qTreeWidgetBleutradeSell->sortByColumn(0, Qt::AscendingOrder);
    ui->qTreeWidgetBleutradeSell->setSortingEnabled(true);

    QString apiResponse = response->readAll();

    apiResponse = apiResponse.replace("{\"success\":true,\"message\":\"\",\"result\":{\"buy\":[", "");
    QStringList qslApiResponse = apiResponse.split("],\"sell\":[");

    QStringList qslApiResponseBuys = qslApiResponse[0].replace("},{", "}{").split("{", QString::SkipEmptyParts);
    QStringList qslApiResponseSells = qslApiResponse[1].replace("]}}","").replace("},{", "}{").split("{", QString::SkipEmptyParts);

    //Use shorest depth as limit and use buy length if they are the same
    int depth = qslApiResponseBuys.length() > qslApiResponseSells.length()
            ? qslApiResponseSells.length() : qslApiResponseSells.length() > qslApiResponseBuys.length()
            ? qslApiResponseBuys.length() : qslApiResponseBuys.length();

    //Prevent overflow by limiting depth to 50
    //Also check for odd number of orders and drop the last one
    //To avoid an overflow when there are less than 50 orders
    depth = depth > 50
            ? 50 : depth % 2 == 1
            ? depth - 1 : depth;

    QVector<double> xAxisBuys(depth), yAxisBuys(depth);
    QVector<double> xAxisSells(depth), yAxisSells(depth);

    for(int i = 0; i < depth; i++){
        mValue jsonResponse = new mValue();

        //Fix missing leading brace caused by split string, otherwise it will not be recognized an an mObject
        qslApiResponseBuys[i].replace("\"Quantity", "{\"Quantity");
        qslApiResponseSells[i].replace("\"Quantity", "{\"Quantity");

        //json_spirit does not handle null so make it "null"
        qslApiResponseBuys[i].replace("null", "\"null\"");
        qslApiResponseSells[i].replace("null", "\"null\"");

        //Make sure the response is valid
        if(read_string(qslApiResponseBuys[i].toStdString(), jsonResponse)) {
            mObject jsonObjectBuys = jsonResponse.get_obj();

            try
            {
                _bleutradeOrders->setQuantity(getPairValue(jsonObjectBuys, "Quantity").get_real());
                _bleutradeOrders->setPrice(getPairValue(jsonObjectBuys, "Rate").get_real());
                _bleutradeOrders->setOrderType("Buy");
            }
            catch (std::exception) {} //API did not return all needed data so skip this order

            QTreeWidgetItem * qtBuys = new QTreeWidgetItem();

            qtBuys->setText(0, _bleutradeOrders->getPrice(QString()));
            qtBuys->setText(1, _bleutradeOrders->getQuantity(QString()));

            ui->qTreeWidgetBleutradeBuy->addTopLevelItem(qtBuys);

            sumBuys += _bleutradeOrders->getQuantity(double());
            xAxisBuys[z] = _bleutradeOrders->getPrice(double()) * 100000000;
            yAxisBuys[z] = sumBuys;
        }

        high = _bleutradeOrders->getPrice(double()) > high ? _bleutradeOrders->getPrice(double()) : high;
        low = _bleutradeOrders->getPrice(double()) < low ? _bleutradeOrders->getPrice(double()) : low;

        //Make sure the response is valid
        if(read_string(qslApiResponseSells[i].toStdString(), jsonResponse)) {
            mObject jsonObjectSells = jsonResponse.get_obj();

            try
            {
                _bleutradeOrders->setQuantity(getPairValue(jsonObjectSells, "Quantity").get_real());
                _bleutradeOrders->setPrice(getPairValue(jsonObjectSells, "Rate").get_real());
                _bleutradeOrders->setOrderType("Sell");
            }
            catch (std::exception) {} //API did not return all needed data so skip this order

            QTreeWidgetItem * qtSells = new QTreeWidgetItem();

            qtSells->setText(0, _bleutradeOrders->getPrice(QString()));
            qtSells->setText(1, _bleutradeOrders->getQuantity(QString()));

            ui->qTreeWidgetBleutradeSell->addTopLevelItem(qtSells);

            sumSells += _bleutradeOrders->getQuantity(double());
            xAxisSells[z] = _bleutradeOrders->getPrice(double()) * 100000000;
            yAxisSells[z] = sumSells;
        }

        high = _bleutradeOrders->getPrice(double()) > high ? _bleutradeOrders->getPrice(double()) : high;
        low = _bleutradeOrders->getPrice(double()) < low ? _bleutradeOrders->getPrice(double()) : low;

        z++;
    }

    high *=  100000000;
    low *=  100000000;

    sumHighest = sumBuys > sumSells ? sumBuys : sumBuys < sumSells ? sumSells : sumBuys;

    ui->qCustomPlotBleutradeOrderDepth->graph(0)->setData(xAxisBuys, yAxisBuys);
    ui->qCustomPlotBleutradeOrderDepth->graph(1)->setData(xAxisSells, yAxisSells);

    ui->qCustomPlotBleutradeOrderDepth->graph(0)->setPen(QPen(QColor(34, 177, 76)));
    ui->qCustomPlotBleutradeOrderDepth->graph(0)->setBrush(QBrush(QColor(34, 177, 76, 20)));
    ui->qCustomPlotBleutradeOrderDepth->graph(1)->setPen(QPen(QColor(237, 24, 35)));
    ui->qCustomPlotBleutradeOrderDepth->graph(1)->setBrush(QBrush(QColor(237, 24, 35, 20)));

    ui->qCustomPlotBleutradeOrderDepth->xAxis->setRange(low, high);
    ui->qCustomPlotBleutradeOrderDepth->yAxis->setRange(low, sumHighest);

    ui->qCustomPlotBleutradeOrderDepth->replot();
}

const mValue& PoolBrowser::getPairValue(const mObject& obj, const std::string& name)
{
    mObject::const_iterator iter = obj.find(name);

    assert(iter != obj.end());
    assert(iter->first == name);

    return iter->second;
}

void PoolBrowser::updateLabel(QLabel* qLabel, double d1, double d2, QString prefix, int decimalPlaces)
{
    qLabel->setText("");

    if (d1 > d2) {
        qLabel->setText(prefix + "<font color=\"green\"><b>" + QString::number(d1, 'f', decimalPlaces) + "</b></font>");
    }
    else if (d1 < d2) {
        qLabel->setText(prefix + "<font color=\"red\"><b>" + QString::number(d1, 'f', decimalPlaces) + "</b></font>");
    }
    else {
        qLabel->setText(prefix + QString::number(d1, 'f', decimalPlaces));
    }
}
void PoolBrowser::updateLabel(QLabel* qLabel, double d1, double d2, QString prefix, QString suffix, int decimalPlaces)
{
    qLabel->setText("");

    if (d1 > d2) {
        qLabel->setText(prefix + "<font color=\"green\"><b>" + QString::number(d1, 'f', decimalPlaces) + suffix + "</b></font>");
    }
    else if (d1 < d2) {
        qLabel->setText(prefix + "<font color=\"red\"><b>" + QString::number(d1, 'f', decimalPlaces) + suffix + "</b></font>");
    }
    else {
        qLabel->setText(prefix + QString::number(d1, 'f', decimalPlaces) + suffix);
    }
}

void PoolBrowser::setModel(ClientModel *model)
{
    this->model = model;
}

PoolBrowser::~PoolBrowser()
{
    delete ui;
}
