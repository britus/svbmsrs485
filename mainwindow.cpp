#include "ui_mainwindow.h"
#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QScrollBar>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QStandardPaths>
#include <QTextCursor>
#include <mainwindow.h>

Q_DECLARE_METATYPE(QSerialPortInfo)
Q_DECLARE_METATYPE(QSerialPort::BaudRate)
Q_DECLARE_METATYPE(QSerialPort::DataBits)
Q_DECLARE_METATYPE(QSerialPort::StopBits)
Q_DECLARE_METATYPE(QSerialPort::Parity)

static inline const QString configFile()
{
    return QStringLiteral("%1%2%3") //
       .arg(
          QStandardPaths::writableLocation( //
             QStandardPaths::AppConfigLocation),
          QDir::separator(),
          "svbmstester.conf");
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_settings(configFile(), QSettings::IniFormat, this)
    , m_config()
    , m_bms(this)
{
    ui->setupUi(this);
    initPortConfig();
    uiFillControls();
    onDisconnected();

    connect(&m_bms, &CSSuperVoltBmsDevice::connected, this, &MainWindow::onConnected);
    connect(&m_bms, &CSSuperVoltBmsDevice::disconnected, this, &MainWindow::onDisconnected);
    connect(&m_bms, &CSSuperVoltBmsDevice::errorOccured, this, &MainWindow::onErrorOccured);
    connect(&m_bms, &CSSuperVoltBmsDevice::message, this, &MainWindow::onMessage);

    m_config.options |= CSSuperVoltBmsDevice::OPT_SOI_BYTE_3E;
    m_config.options |= CSSuperVoltBmsDevice::OPT_ASCII_CHKSUM;
    m_config.options |= CSSuperVoltBmsDevice::OPT_ASCII_LENGTH;
    m_config.address = 1;
}

MainWindow::~MainWindow()
{
    disconnect(&m_bms);
    savePortConfig();
    delete ui;
}

template <typename T>
inline T MainWindow::cv(const QString& key, const uint def)
{
    return m_settings.value(key, def).value<T>();
}

inline void MainWindow::initPortConfig()
{
    m_settings.beginGroup("SERIAL-PORT");
    m_config.portName = m_settings.value("port", "").toString();
    m_config.baudRate = cv<QSerialPort::BaudRate>("baudRate", QSerialPort::Baud19200);
    m_config.dataBits = cv<QSerialPort::DataBits>("dataBits", QSerialPort::Data8);
    m_config.stopBits = cv<QSerialPort::StopBits>("stopBits", QSerialPort::OneStop);
    m_config.parity = cv<QSerialPort::Parity>("parity", QSerialPort::NoParity);
    m_config.flowCtrl = cv<QSerialPort::FlowControl>("flowCtrl", QSerialPort::NoFlowControl);
    m_settings.endGroup();
}

inline void MainWindow::savePortConfig()
{
    m_settings.beginGroup("SERIAL-PORT");
    m_settings.setValue("port", m_config.portName);
    m_settings.setValue("baudRate", m_config.baudRate);
    m_settings.setValue("dataBits", m_config.dataBits);
    m_settings.setValue("stopBits", m_config.stopBits);
    m_settings.setValue("parity", m_config.parity);
    m_settings.setValue("flowCtrl", m_config.flowCtrl);
    m_settings.setValue("flags", m_config.traceFlags);
    m_settings.endGroup();
    m_settings.sync();
}

inline void MainWindow::uiFillControls()
{
    const QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
    int selection;

    selection = -1;
    ui->cbxSerialPort->clear();
    for (int i = 0; i < ports.count(); i++) {
        ui->cbxSerialPort->addItem(ports[i].portName(), QVariant::fromValue(ports[i]));
        if (ports[i].portName() == m_config.portName) {
            selection = i;
        }
    }
    if (selection > -1) {
        ui->cbxSerialPort->setCurrentIndex(selection);
    }

    typedef struct {
        QString name;
        QSerialPort::BaudRate baud;
    } TBaudRates;

    TBaudRates baudRates[8] = {
       {tr("1200"), QSerialPort::Baud1200},
       {tr("2400"), QSerialPort::Baud2400},
       {tr("4800"), QSerialPort::Baud4800},
       {tr("9600"), QSerialPort::Baud9600},
       {tr("19200"), QSerialPort::Baud19200},
       {tr("38400"), QSerialPort::Baud38400},
       {tr("57600"), QSerialPort::Baud57600},
       {tr("115200"), QSerialPort::Baud115200},
    };

    selection = -1;
    ui->cbxBaudRate->clear();
    for (int i = 0; i < 8; i++) {
        ui->cbxBaudRate->addItem(baudRates[i].name, QVariant::fromValue(baudRates[i].baud));
        if (baudRates[i].baud == m_config.baudRate) {
            selection = i;
        }
    }
    if (selection > -1) {
        ui->cbxBaudRate->setCurrentIndex(selection);
    }

    typedef struct {
        QString name;
        QSerialPort::DataBits bits;
    } TDataBits;

    TDataBits dataBits[4] = {
       {tr("5 Bits"), QSerialPort::Data5},
       {tr("6 Bits"), QSerialPort::Data6},
       {tr("7 Bits"), QSerialPort::Data7},
       {tr("8 Bits"), QSerialPort::Data8},
    };

    selection = -1;
    ui->cbxDataBits->clear();
    for (int i = 0; i < 4; i++) {
        ui->cbxDataBits->addItem(dataBits[i].name, QVariant::fromValue(dataBits[i].bits));
        if (dataBits[i].bits == m_config.dataBits) {
            selection = i;
        }
    }
    if (selection > -1) {
        ui->cbxDataBits->setCurrentIndex(selection);
    }

    typedef struct {
        QString name;
        QSerialPort::StopBits bits;
    } TStopBits;

    TStopBits stopBits[3] = {
       {tr("One Stop"), QSerialPort::OneStop},
       {tr("One and Half"), QSerialPort::OneAndHalfStop},
       {tr("Two Stop"), QSerialPort::TwoStop},
    };

    selection = -1;
    ui->cbxStopBits->clear();
    for (int i = 0; i < 3; i++) {
        ui->cbxStopBits->addItem(stopBits[i].name, QVariant::fromValue(stopBits[i].bits));
        if (stopBits[i].bits == m_config.stopBits) {
            selection = i;
        }
    }
    if (selection > -1) {
        ui->cbxStopBits->setCurrentIndex(selection);
    }

    typedef struct {
        QString name;
        QSerialPort::Parity parity;
    } TParity;

    TParity parity[5] = {
       {tr("No Parity"), QSerialPort::NoParity},
       {tr("Event Parity"), QSerialPort::EvenParity},
       {tr("Odd Parity"), QSerialPort::OddParity},
       {tr("Space Parity"), QSerialPort::SpaceParity},
       {tr("Mark Parity"), QSerialPort::MarkParity},
    };

    selection = -1;
    ui->cbxParity->clear();
    for (int i = 0; i < 5; i++) {
        ui->cbxParity->addItem(parity[i].name, QVariant::fromValue(parity[i].parity));
        if (parity[i].parity == m_config.parity) {
            selection = i;
        }
    }
    if (selection > -1) {
        ui->cbxParity->setCurrentIndex(selection);
    }
}

inline void MainWindow::writeLog(const QString& message, bool reset)
{
    QTextDocument* doc;
    if ((doc = ui->textBrowser->document())) {
        if (reset) {
            doc->setPlainText(message + "\n");
        }
        else {
            doc->setPlainText(doc->toPlainText() + message + "\n");
        }
        ui->textBrowser->moveCursor(QTextCursor::Down);
    }
}

void MainWindow::onConnected()
{
    writeLog(QStringLiteral("BMS connected."), true);

    ui->cbxSerialPort->setEnabled(false);
    ui->cbxBaudRate->setEnabled(false);
    ui->cbxDataBits->setEnabled(false);
    ui->cbxStopBits->setEnabled(false);
    ui->cbxParity->setEnabled(false);
    ui->gbBmsFunc->setEnabled(true);
    ui->btnClose->setEnabled(true);
    ui->btnOpen->setEnabled(false);
}

void MainWindow::onDisconnected()
{
    writeLog(QStringLiteral("BMS disconnected."));
    ui->cbxSerialPort->setEnabled(true);
    ui->cbxBaudRate->setEnabled(true);
    ui->cbxDataBits->setEnabled(true);
    ui->cbxStopBits->setEnabled(true);
    ui->cbxParity->setEnabled(true);
    ui->gbBmsFunc->setEnabled(false);
    ui->btnClose->setEnabled(false);
    ui->btnOpen->setEnabled(true);
}

void MainWindow::onErrorOccured(CSSuperVoltBmsDevice::BmsError error)
{
    writeLog(tr("BMS error #%1 occured.").arg(error));
}

void MainWindow::onMessage(const QString& message)
{
    writeLog(message);
}

void MainWindow::on_cbxSerialPort_activated(int index)
{
    QSerialPortInfo spi;
    QVariant v = ui->cbxSerialPort->itemData(index);
    if (v.isNull() || !v.isValid()) {
        return;
    }
    spi = v.value<QSerialPortInfo>();
    m_config.portName = spi.portName();
}

void MainWindow::on_cbxBaudRate_activated(int index)
{
    m_config.baudRate = ui->cbxBaudRate->itemData(index).value<QSerialPort::BaudRate>();
}

void MainWindow::on_cbxDataBits_activated(int index)
{
    m_config.dataBits = ui->cbxDataBits->itemData(index).value<QSerialPort::DataBits>();
}

void MainWindow::on_cbxStopBits_activated(int index)
{
    m_config.stopBits = ui->cbxStopBits->itemData(index).value<QSerialPort::StopBits>();
}

void MainWindow::on_cbxParity_activated(int index)
{
    m_config.parity = ui->cbxParity->itemData(index).value<QSerialPort::Parity>();
}

void MainWindow::on_btnOpen_clicked()
{
    if (!m_bms.isOpen()) {
        m_bms.setConfig(m_config);
        m_bms.open();
    }
}

void MainWindow::on_btnClose_clicked()
{
    if (m_bms.isOpen()) {
        m_bms.close();
    }
}

void MainWindow::on_edAddress_valueChanged(int value)
{
    m_config.address = value;
}

void MainWindow::on_rbSoi3E_clicked(bool checked)
{
    if (checked) {
        m_config.options &= ~CSSuperVoltBmsDevice::OPT_SOI_BYTE_7E;
        m_config.options |= CSSuperVoltBmsDevice::OPT_SOI_BYTE_3E;
    }
    else {
        m_config.options &= ~CSSuperVoltBmsDevice::OPT_SOI_BYTE_3E;
        m_config.options |= CSSuperVoltBmsDevice::OPT_SOI_BYTE_7E;
    }
}

void MainWindow::on_rbSoi7E_clicked(bool checked)
{
    on_rbSoi3E_clicked(!checked);
}

void MainWindow::on_cbAsciiChksum_clicked(bool checked)
{
    if (checked) {
        m_config.options |= CSSuperVoltBmsDevice::OPT_ASCII_CHKSUM;
    }
    else {
        m_config.options &= ~CSSuperVoltBmsDevice::OPT_ASCII_CHKSUM;
    }
}

void MainWindow::on_cbAsciiLength_clicked(bool checked)
{
    if (checked) {
        m_config.options |= CSSuperVoltBmsDevice::OPT_ASCII_LENGTH;
    }
    else {
        m_config.options &= ~CSSuperVoltBmsDevice::OPT_ASCII_LENGTH;
    }
}

void MainWindow::on_cbxFuncions_activated(int)
{
    //
}

void MainWindow::on_btnExecFunc_clicked()
{
    if (!m_bms.isOpen()) {
        onErrorOccured(CSSuperVoltBmsDevice::NotOpenError);
        return;
    }

    m_bms.setOptions(m_config.options);
    m_bms.setAddress(m_config.address);

    switch (ui->cbxFuncions->currentIndex()) {
        case 0: {
            m_bms.fetchTime();
            break;
        }
        case 1: {
            m_bms.fetchProtocolVersion();
            break;
        }
        case 2: {
            m_bms.fetchDeviceAddress();
            break;
        }
        case 3: {
            m_bms.fetchManufacturer();
            break;
        }
        case 4: {
            m_bms.fetchAnalogData(false);
            break;
        }
        case 5: {
            m_bms.fetchAnalogData(true);
            break;
        }
        case 6: {
            break;
        }
        case 7: {
            break;
        }
    }
}
