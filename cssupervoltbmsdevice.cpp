#include <QDebug>
#include <QDir>
#include <QFileInfoList>
#include <QSerialPortInfo>
#include <QThread>
#include <cssupervoltbmsdevice.h>

CSSuperVoltBmsDevice::CSSuperVoltBmsDevice(QObject* parent)
    : QObject(parent)
    , m_port(this)
    , m_config()
    , m_inputBuffer()
{
    connect(&m_port, &QSerialPort::errorOccurred, this, &CSSuperVoltBmsDevice::onPortError);
    connect(&m_port, &QSerialPort::aboutToClose, this, &CSSuperVoltBmsDevice::onAboutToClose);
    connect(&m_port, &QSerialPort::readyRead, this, &CSSuperVoltBmsDevice::onReadyRead);
    connect(&m_port, &QSerialPort::bytesWritten, this, &CSSuperVoltBmsDevice::onBytesWritten);

    m_config.options = OPT_SOI_BYTE_3E;
    m_config.address = 1;
}

CSSuperVoltBmsDevice::~CSSuperVoltBmsDevice()
{
    qDebug() << Q_FUNC_INFO;

    disconnect(&m_port);

    if (m_port.isOpen()) {
        m_port.flush();
        m_port.close();
    }
}

void CSSuperVoltBmsDevice::onPortError(QSerialPort::SerialPortError error)
{
    if (error != QSerialPort::NoError) {
        emit errorOccured(static_cast<BmsError>(UserErrorLast + error));
    }
}

void CSSuperVoltBmsDevice::onAboutToClose()
{
    emit disconnected();
}

void CSSuperVoltBmsDevice::onBytesWritten(qint64 size)
{
    qDebug() << "BMSDEV:" << size << "bytes written to device" << m_port.portName();
}

void CSSuperVoltBmsDevice::onReadyRead()
{
    while (m_port.bytesAvailable() > 0) {
        m_inputBuffer.append(m_port.readAll());
        QThread::yieldCurrentThread();
    }

    emit message(tr("RCV> [%1] %2") //
                    .arg(m_inputBuffer.size())
                    .arg(QString(m_inputBuffer.toHex(' '))));

    if (m_inputBuffer.size() < 3) {
        emit errorOccured(InvalidFormat);
        return;
    }

    qint64 size = m_inputBuffer.size();

    if (
       (m_inputBuffer.at(0) != BMS_PROTO_SOI_3E) && //
       (m_inputBuffer.at(0) != BMS_PROTO_SOI_7E))   //
    {
        emit errorOccured(InvalidFormat);
        return;
    }

    if (m_inputBuffer.at(1) != BMS_PROTO_VER) {
        emit errorOccured(InvalidVersion);
        return;
    }

    if (m_inputBuffer.at(size - 1) != BMS_PROTO_EOI) {
        emit errorOccured(InvalidFormat);
        return;
    }

    /* handle BMS response */
    response(m_inputBuffer);

    /* reset input buffer */
    m_inputBuffer.clear();
}

inline QString CSSuperVoltBmsDevice::resolveSymLink(const QString& portName)
{
    QString result = portName;

    /* QT doesn't support symlinks to TTY serial port devices.
     * Try to resolve udev symlinks /dev/ttyXXXnn*/
    QDir devPath("/dev");
    QFileInfoList fil = devPath.entryInfoList(QStringList() << "ttyBMS*");
    if (!fil.isEmpty()) {
        qDebug() << "BMSDEV: Found ttyBMSnn symlinks to serial devices.";
        foreach (auto fi, fil) {
            if (fi.isSymLink()) {
                QString sym = fi.absoluteFilePath();
                QString tgt = fi.symLinkTarget();
                qDebug() << "BMSDEV: " << sym << " -> " << tgt;
                if (sym.contains(portName)) {
                    result = tgt.replace(devPath.absolutePath(), "").replace("/", "");
                    qDebug() << "BMSDEV: Using device" << result << "for" << portName;
                    break;
                }
            }
        }
    }

    return result;
}

inline bool CSSuperVoltBmsDevice::setupSerialPort(const QString& portName, QSerialPort* port)
{
    QSerialPortInfo spi;
    QString name = resolveSymLink(portName);
    foreach (auto p, spi.availablePorts()) {
        if (p.portName().contains(name)) {
            port->setPort(p);
            port->setBaudRate(m_config.baudRate);
            port->setStopBits(m_config.stopBits);
            port->setFlowControl(m_config.flowCtrl);
            port->setParity(m_config.parity);
            port->clearError();
            return true;
        }
    }
    return false;
}

/* convert byte to ASCII Hex representation of high and low nibble (split) */
inline void CSSuperVoltBmsDevice::toAsciiHex8Bit(const quint8 value, QByteArray& result)
{
    char hex[3] = {};
    /* add ASCII hex code in uppercase */
    for (quint8 i = 0; i < 2; i++) {
        quint8 nib = (i == 0 ? (value >> 4) & 0x0f : value & 0x0f);
        snprintf(hex, 2, "%X", nib);
        snprintf(hex, 3, "%02X", hex[0]);
        result.append(hex);
    }
}

inline void CSSuperVoltBmsDevice::toAsciiHex16Bit(const quint16 value, QByteArray& result)
{
    /* high and low byte */
    toAsciiHex8Bit((value >> 8) & 0x00ff, result);
    toAsciiHex8Bit(value & 0x00ff, result);
}

inline void CSSuperVoltBmsDevice::appendHeader(QByteArray& packet, quint8 cid2)
{
    /* ASCIIhex Protocol Version */
    toAsciiHex8Bit(BMS_PROTO_VER, packet);

    /* ASCIIhex Device address */
    toAsciiHex8Bit(m_config.address, packet);

    /* ASCIIhex CID1 -> Device identification code */
    toAsciiHex8Bit(BMS_CID1_LIFEPO4, packet);

    /* ASCIIhex CID2 -> Control Identification Code */
    toAsciiHex8Bit(cid2, packet);
}

/* SOI as byte */
inline void CSSuperVoltBmsDevice::appendStart(QByteArray& packet)
{
    if (m_config.options & OPT_SOI_BYTE_3E) {
        packet.append(BMS_PROTO_SOI_3E);
        return;
    }

    packet.append(BMS_PROTO_SOI_7E);
}

/* EOI as byte */
inline void CSSuperVoltBmsDevice::appendEnd(QByteArray& packet)
{
    packet.append(BMS_PROTO_EOI);
}

/* LENGTH consists of 2 bytes, composed of LENID and LCHKSUM.
 * LENID represents the number of ASCII bytes in the INFO item.
 * When LENID=0, INFO is empty, meaning there is no such item.
 * LENGTH is split into four ASCII bytes, with the high byte first
 * and the low byte last. The calculation of the
 * checksum is: D11D10D9D8 + D7D6D5D4 + D3D2D1D0, after summing, take
 * the modulo 16, negate it, and add 1.
 * The ASCII byte count of the INFO field is 18 (in decimal), which
 * is equivalent to LENID=000000010010B.
 * D11D10D9D8 + D7D6D5D4 + D3D2D1D0 = 0000B + 0001B + 0010B = 0011B.
 * The remainder modulo 16 is 0011B. The complement of 0011B plus 1
 * is 1101B, which is LCHKSUM. Therefore, LENGTH is 1101000000010010B,
 * which is equivalent to D012H.
 *
 * NOTE: Length CANNOT exceed 12bit !!!!!
 * NOTE: 4 bit reserved for LCHKSUM
 */
inline bool CSSuperVoltBmsDevice::appendLength(QByteArray& packet, quint16 length)
{
    qint16 chksum;
    char hex[5];

#if 0
    /* test length: LCHKSUM: 0xD  LENID=0x012 -> LENGTH=0xD012 */
    length = 18;
#endif

    /* check value bit size. */
    if ((length & 0xf000) > 0) {
        emit errorOccured(InvalidFormat);
        return false;
    }

    /* no INFO field */
    if (length == 0) {
#if 0
        length = packet.size() / 2;
        length += 5; /* LENGTH(2) + CHKSUM(2) + EOI(1) */
#endif
        /* LENGTH field is zero */
        if (m_config.options & OPT_ASCII_LENGTH) {
            toAsciiHex16Bit(0, packet);
        }
        else {
            snprintf(hex, 5, "%04X", 0);
            packet.append(hex);
        }
        return true;
    }

    /* 0 = D11 D10 D9 D8 | 1 = D7 D6 D5 D4 | 2 = D3 D2 D1 D0 */
    quint8 bits[3] = {
       static_cast<quint8>((length >> 8) & 0x0f),
       static_cast<quint8>((length >> 4) & 0x0f),
       static_cast<quint8>(length & 0x000f),
    };

    chksum = bits[0] + bits[1] + bits[2];
    chksum = (~(chksum % 16)) + 1;

    if (m_config.options & OPT_ASCII_LENGTH) {
        /* LCHKSUM 4 bit LSB */
        snprintf(hex, 2, "%X", (chksum & 0x000f));
        snprintf(hex, 3, "%02X", hex[0]);
        packet.append(hex);
        /* LENGTH 12 bit value */
        snprintf(hex, 2, "%X", ((length >> 8) & 0x000f));
        snprintf(hex, 3, "%02X", hex[0]);
        packet.append(hex);
        snprintf(hex, 2, "%X", ((length >> 4) & 0x000f));
        snprintf(hex, 3, "%02X", hex[0]);
        packet.append(hex);
        snprintf(hex, 2, "%X", (length & 0x000f));
        snprintf(hex, 3, "%02X", hex[0]);
        packet.append(hex);
    }
    else {
        /* LCHKSUM 4 bit LSB */
        snprintf(hex, 2, "%X", (chksum & 0x000f));
        packet.append(hex);
        /* LENGTH 12 bit value */
        snprintf(hex, 4, "%03X", length & 0x0fff);
        packet.append(hex);
    }

    return true;
}

/* Add the INFO field. LENID must not be 0x00 ! */
inline void CSSuperVoltBmsDevice::appendInfo(QByteArray& packet, quint16 info)
{
    char hex[5];
    if (m_config.options & OPT_ASCII_LENGTH) {
        toAsciiHex16Bit(info, packet);
    }
    else {
        snprintf(hex, 5, "%04X", info);
        packet.append(hex);
    }
}

/* The calculation of CHKSUM is to add up the characters other
 * than SOI, EOI, and CHKSUM according to their ASCII values,
 * and then take the modulo 65536 of the sum, invert it, and add 1.
 * CHKSUM is transmitted as four separate ASCII codes, first the
 * high byte and then the low byte. */
inline bool CSSuperVoltBmsDevice::appendChecksum(QByteArray& packet)
{
    int size, chksum = 0;

#if 0
    /* test chksum: 0x02c5 ~+ 1 -> 0xFD3B */
    packet = QStringLiteral("20014043E00200").toLocal8Bit();
#endif

    if (packet.isEmpty()) {
        emit errorOccured(InvalidData);
        return false;
    }

    /* skip SOI byte */
    size = packet.size();
    for (int i = 1; i < size; i++) {
        chksum += static_cast<quint8>(packet[i++]);
    }
    if (!chksum) {
        emit errorOccured(InvalidChecksum);
        return false;
    }
    chksum = (~(chksum % 65536)) + 1;

    /* high and low byte */
    if (m_config.options & OPT_ASCII_CHKSUM) {
        toAsciiHex8Bit(((chksum >> 8) & 0x00ff), packet);
        toAsciiHex8Bit((chksum & 0x00ff), packet);
    }
    else {
        char hex[3];
        snprintf(hex, 3, "%02X", (chksum >> 8) & 0x00ff);
        packet.append(hex);
        snprintf(hex, 3, "%02X", (chksum & 0x00ff));
        packet.append(hex);
    }

    return true;
}

static inline QString toMessage(const QByteArray& buffer)
{
    QString result = {};
    for (int i = 0; i < buffer.size(); i++) {
        if (buffer[i] == 0x0a) {
            result.append(QStringLiteral("\\n"));
        }
        else if (buffer[i] == 0x0d) {
            result.append(QStringLiteral("\\r"));
        }
        else if (buffer[i] < 0x20 || buffer[i] > 0x7E) {
            result.append(QStringLiteral("<%1>").arg(buffer[i], 2, 16, QChar('0')));
        }
        else {
            result.append(buffer[i]);
        }
    }
    return result;
}

inline void CSSuperVoltBmsDevice::response(const QByteArray& buffer)
{
    qDebug() << "BMSDEV:RSP>" << buffer;

    emit message(tr("RESP> [%1:%2] %3") //
                    .arg(m_config.address)
                    .arg(buffer.size())
                    .arg(toMessage(buffer)));
}

inline bool CSSuperVoltBmsDevice::transmit(const QByteArray& packet)
{
    qDebug() << "BMSDEV:SND>" << packet;

    emit message(tr("SND> [%1:%2] %3") //
                    .arg(m_config.address)
                    .arg(packet.size())
                    .arg(toMessage(packet)));

    return m_port.write(packet) == packet.size();
}

/* ----------------------------------------------------------
 *  API
 * ---------------------------------------------------------- */

const CSSuperVoltBmsDevice::TPortConfig& CSSuperVoltBmsDevice::config() const
{
    return m_config;
}

void CSSuperVoltBmsDevice::setConfig(const TPortConfig& newConfig)
{
    m_config = newConfig;
}

void CSSuperVoltBmsDevice::setOptions(uint options)
{
    m_config.options = options;
}

void CSSuperVoltBmsDevice::setAddress(uint address)
{
    m_config.address = address;
}

bool CSSuperVoltBmsDevice::open()
{
    if (m_port.isOpen()) {
        return true;
    }

    if (!setupSerialPort(m_config.portName, &m_port)) {
        emit errorOccured(DeviceNotFoundError);
        return false;
    }

    if (!m_port.open(QSerialPort::ReadWrite)) {
        emit errorOccured(OpenError);
        return false;
    }

    /* clear buffers */
    m_port.flush();

    emit connected();
    return true;
}

void CSSuperVoltBmsDevice::close()
{
    if (m_port.isOpen()) {
        m_port.flush();
        m_port.close();
    }
}

bool CSSuperVoltBmsDevice::isOpen() const
{
    return m_port.isOpen();
}

/* Fetch current date / time from BMS */
void CSSuperVoltBmsDevice::fetchTime()
{
    QByteArray packet = {};

    appendStart(packet);
    appendHeader(packet, BMS_CID2_FETCH_TIME);
    /* no INFO field LENID = 0x00 */
    if (!appendLength(packet, 0)) {
        return;
    }
    if (!appendChecksum(packet)) {
        return;
    }
    appendEnd(packet);
    transmit(packet);
}

void CSSuperVoltBmsDevice::fetchProtocolVersion()
{
    QByteArray packet = {};

    appendStart(packet);
    appendHeader(packet, BMS_CID2_FETCH_PROTO_VER);
    /* no INFO field LENID = 0x00 */
    if (!appendLength(packet, 0)) {
        return;
    }
    if (!appendChecksum(packet)) {
        return;
    }
    appendEnd(packet);
    transmit(packet);
}

void CSSuperVoltBmsDevice::fetchDeviceAddress()
{
    QByteArray packet = {};

    appendStart(packet);
    appendHeader(packet, BMS_CID2_FETCH_DEVICE_ADDR);
    /* no INFO field LENID = 0x00 */
    if (!appendLength(packet, 0)) {
        return;
    }
    if (!appendChecksum(packet)) {
        return;
    }
    appendEnd(packet);
    transmit(packet);
}

void CSSuperVoltBmsDevice::fetchManufacturer()
{
    QByteArray packet = {};

    appendStart(packet);
    appendHeader(packet, BMS_CID2_FETCH_MANUFACTURER);
    /* no INFO field LENID = 0x00 */
    if (!appendLength(packet, 0)) {
        return;
    }
    if (!appendChecksum(packet)) {
        return;
    }
    appendEnd(packet);
    transmit(packet);
}

void CSSuperVoltBmsDevice::fetchAnalogData(bool fixed)
{
    QByteArray packet = {};

    appendStart(packet);
    appendHeader(packet, BMS_CID2_FETCH_ANALOG_DATA + (fixed ? 1 : 0));
    /* INFO field exist: LENGTH(2) + INFO(2) + CHKSUM(2) + EOI(1) */
    if (!appendLength(packet, 2)) {
        return;
    }
    appendInfo(packet, 0x00ff);
    if (!appendChecksum(packet)) {
        return;
    }
    appendEnd(packet);
    transmit(packet);
}
