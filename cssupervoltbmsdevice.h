#pragma once
#include <QObject>
#include <QSerialPort>
#include <piplatesio/csiodevice.h>

class CSSuperVoltBmsDevice: public QObject, public CSIoDevice
{
    Q_OBJECT

public:
    Q_INTERFACES(CSIoDevice)

    enum BmsError {
        NoError = 0x00,
        InvalidVersion = 0x01,
        InvalidChecksum = 0x02,
        InvalidLChecksum = 0x03,
        InvalidCid2 = 0x04,
        InvalidFormat = 0x05,
        InvalidData = 0x06,
        UserErrorFirst = 0x80, /* ~ 0xfe */
        UserErrorLast = 0xfe,

        /* our serial port error codes */
        DeviceNotFoundError = UserErrorLast + QSerialPort::DeviceNotFoundError,
        PermissionError = UserErrorLast + QSerialPort::PermissionError,
        OpenError = UserErrorLast + QSerialPort::OpenError,
        ParityError = UserErrorLast + QSerialPort::ParityError,
        FramingError = UserErrorLast + QSerialPort::FramingError,
        BreakConditionError = UserErrorLast + QSerialPort::BreakConditionError,
        WriteError = UserErrorLast + QSerialPort::WriteError,
        ReadError = UserErrorLast + QSerialPort::ReadError,
        ResourceError = UserErrorLast + QSerialPort::ResourceError,
        UnsupportedOperationError = UserErrorLast + QSerialPort::UnsupportedOperationError,
        UnknownError = UserErrorLast + QSerialPort::UnknownError,
        TimeoutError = UserErrorLast + QSerialPort::TimeoutError,
        NotOpenError = UserErrorLast + QSerialPort::NotOpenError,
    };
    Q_ENUM(BmsError)

    typedef struct {
        QString portName;
        QSerialPort::BaudRate baudRate;
        QSerialPort::DataBits dataBits;
        QSerialPort::StopBits stopBits;
        QSerialPort::Parity parity;
        QSerialPort::FlowControl flowCtrl;
        quint8 address;
        uint options;
        uint traceFlags;
    } TPortConfig;

    static const quint8 OPT_SOI_BYTE_3E = 0x01;
    static const quint8 OPT_SOI_BYTE_7E = 0x02;
    static const quint8 OPT_ASCII_CHKSUM = 0x04;
    static const quint8 OPT_ASCII_LENGTH = 0x08;

    explicit CSSuperVoltBmsDevice(QObject* parent = nullptr);

    ~CSSuperVoltBmsDevice();

    bool open() override Q_OVERRIDE(CSIoDevice);
    void close() override Q_OVERRIDE(CSIoDevice);
    bool isOpen() const override Q_OVERRIDE(CSIoDevice);

    const TPortConfig& config() const;
    void setConfig(const TPortConfig& newConfig);
    void setOptions(uint options);
    void setAddress(uint address);
    void fetchAnalogData(bool fixed = false);
    void fetchManufacturer();
    void fetchDeviceAddress();
    void fetchProtocolVersion();
    void fetchTime();

signals:
    void connected();
    void disconnected();
    void errorOccured(CSSuperVoltBmsDevice::BmsError);
    void message(const QString&);

private slots:
    void onPortError(QSerialPort::SerialPortError);
    void onAboutToClose();
    void onReadyRead();
    void onBytesWritten(qint64);

private:
    /* fixed codes */
    static const quint8 BMS_PROTO_SOI_3E = 0x3e;
    static const quint8 BMS_PROTO_SOI_7E = 0x7e;
    static const quint8 BMS_PROTO_EOI = 0x0d;

    /* protocol version */
    static const quint8 BMS_PROTO_VER = 0x22;

    /* SX150 Lithium Iron Phosphate Battery Management System  */
    static const quint8 BMS_CID1_LIFEPO4 = 0x4a;

    /* function codes */
    static const quint8 BMS_CID2_FETCH_ANALOG_DATA = 0x41;
    static const quint8 BMS_CID2_FETCH_MANUFACTURER = 0x51;
    static const quint8 BMS_CID2_FETCH_DEVICE_ADDR = 0x50;
    static const quint8 BMS_CID2_FETCH_PROTO_VER = 0x4f;
    static const quint8 BMS_CID2_FETCH_TIME = 0x4d;

    QSerialPort m_port;
    TPortConfig m_config;
    QByteArray m_inputBuffer;

private:
    inline QString resolveSymLink(const QString& portName);
    inline bool setupSerialPort(const QString& portName, QSerialPort* port);
    inline void response(const QByteArray& buffer);
    inline void toAsciiHex8Bit(const quint8 value, QByteArray& result);
    inline void toAsciiHex16Bit(const quint16 value, QByteArray& result);
    inline void appendStart(QByteArray& packet);
    inline void appendEnd(QByteArray& packet);
    inline void appendHeader(QByteArray& packet, quint8 cid2);
    inline bool appendLength(QByteArray& packet, quint16 length);
    inline void appendInfo(QByteArray& packet, quint16 info);
    inline bool appendChecksum(QByteArray& packet);
    inline bool transmit(const QByteArray& packet);
};
Q_DECLARE_METATYPE(CSSuperVoltBmsDevice::BmsError)
Q_DECLARE_METATYPE(CSSuperVoltBmsDevice::TPortConfig)
