#pragma once
#include <QMainWindow>
#include <QSerialPort>
#include <QSettings>
#include <cssupervoltbmsdevice.h>

QT_BEGIN_NAMESPACE

namespace Ui {
class MainWindow;
}

QT_END_NAMESPACE

class MainWindow: public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void onConnected();
    void onDisconnected();
    void onErrorOccured(CSSuperVoltBmsDevice::BmsError);
    void onMessage(const QString& message);

    void on_btnOpen_clicked();
    void on_btnClose_clicked();
    void on_cbxFuncions_activated(int);
    void on_btnExecFunc_clicked();
    void on_cbxSerialPort_activated(int);
    void on_cbxBaudRate_activated(int);
    void on_cbxDataBits_activated(int);
    void on_cbxStopBits_activated(int);
    void on_cbxParity_activated(int);
    void on_edAddress_valueChanged(int);

    void on_rbSoi3E_clicked(bool checked);

    void on_rbSoi7E_clicked(bool checked);

    void on_cbAsciiChksum_clicked(bool checked);

    void on_cbAsciiLength_clicked(bool checked);

private:
    Ui::MainWindow* ui;
    QSettings m_settings;
    CSSuperVoltBmsDevice::TPortConfig m_config;
    CSSuperVoltBmsDevice m_bms;

private:
    inline void uiFillControls();
    inline void initPortConfig();
    inline void savePortConfig();

    template <typename T>
    inline T cv(const QString& key, const uint def);

    inline void writeLog(const QString& message, bool reset = false);
};
