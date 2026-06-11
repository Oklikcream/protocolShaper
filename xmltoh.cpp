#include "xmltoh.h"
#include <QFile>
#include <QXmlStreamReader>
#include <QDebug>
#include <QFileInfo>

// Разбор XML файла
QList<XmlPacket> ParseXML(const QString &inFilePath) {
    QFile xmlFile(inFilePath);
    if (!xmlFile.open(QIODevice::ReadOnly)) {
        throw std::runtime_error(
            QString("Не удалось открыть файл %1").arg(inFilePath).toStdString()
        );
    }

    QXmlStreamReader xml(&xmlFile);
    QList<XmlPacket> packets;
    QString currentDirection;

    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();
        if (token != QXmlStreamReader::StartElement) {
            continue;
        }

        if (xml.name() == "Type") {
            currentDirection = xml.attributes().value("name").toString();
        } else if (xml.name() == "Packet") {
            XmlPacket pkt;
            pkt.direction = currentDirection;
            pkt.name = xml.attributes().value("name").toString();
            pkt.endian = xml.attributes().value("endian").toString();

            // Выделяем пакеты
            while (!(xml.tokenType() == QXmlStreamReader::EndElement && xml.name() == "Packet")) {
                xml.readNext();
                // Группы
                if (xml.tokenType() == QXmlStreamReader::StartElement && xml.name() == "Group") {
                    XmlGroup grp;
                    grp.lengthBytes = xml.attributes().value("length").toInt();
                    while (!(xml.tokenType() == QXmlStreamReader::EndElement && xml.name() == "Group")) {
                        xml.readNext();
                        // Поля групп
                        if (xml.tokenType() == QXmlStreamReader::StartElement && xml.name() == "Field") {
                            XmlField f;
                            QStringList fullName = xml.attributes().value("name").toString().split(", ");
                            f.name = fullName.value(0);
                            if (fullName.size() == 2)
                                f.dimension = fullName.at(1);

                            QString typeStr = xml.attributes().value("type").toString();
                            if (typeStr == "unsigned" || typeStr == "signed" || typeStr == "float" || typeStr == "double")
                                f.type = typeStr;
                            else
                                throw std::runtime_error(
                                    QString("Неизвестный тип данных: ").arg(typeStr).toStdString()
                                );

                            f.lengthBits = xml.attributes().value("length").toInt();
                            f.lsb = xml.attributes().value("lsb").toDouble();
                            f.constant = (xml.attributes().value("constant") == "true");
                            f.constValue = xml.attributes().value("value").toULongLong();
                            f.script = xml.attributes().value("script").toString();
                            while (xml.readNextStartElement()) {
                                if (xml.name() == "Values") {
                                    QString valName = xml.attributes().value("name").toString();
                                    quint64 val = xml.attributes().value("value").toULongLong();
                                    f.enumValues[valName] = val;
                                }
                                xml.skipCurrentElement();
                            }
                            grp.fields.append(f);
                        }
                    }
                    pkt.groups.append(grp);
                }
            }
            packets.append(pkt);
        }
    }

    if (xml.hasError()) {
        throw std::runtime_error(
            QString("Ошибка XML: %1").arg(xml.errorString()).toStdString()
        );
    }

    xmlFile.close();
    return packets;
}

// Запись пакета в файл
void WritePacket(const XmlPacket &pkt, int packetIndex, QTextStream &out) {
    QString structName = (pkt.direction == "Прием" ? "Rx_" : "Tx_") + QString("Packet%1").arg(packetIndex);
    out << "#pragma pack(1)\n";
    out << "typedef struct {\n";

    int fieldCounter = 0;
    int groupCounter = 0;
    int groupFieldCounter = 0;

    for (const XmlGroup &grp : pkt.groups) {
        bool isBitfieldGroup = true;
        int totalBits = 0;

        for (const XmlField &f : grp.fields) {
            if (f.type == "float" || f.type == "double") {
                isBitfieldGroup = false;
                break;
            }
            totalBits += f.lengthBits;
        }

        // Запись объединения
        if (isBitfieldGroup && grp.fields.size() > 1 && totalBits <= 64) {
            QString groupType;
            if (totalBits <= 8) groupType = "quint8";
            else if (totalBits <= 16) groupType = "quint16";
            else if (totalBits <= 32) groupType = "quint32";
            else groupType = "quint64";

            out << "    typedef union {\n";
            out << "        " << groupType << " raw;\n";
            out << "        struct {\n";

            // Запись битового поля объединения
            for (const XmlField &f : grp.fields) {
                out << "            " << groupType << " groupField"
                    << ++groupFieldCounter << " : " << f.lengthBits << ";"
                    << " " << MakeFieldComment(f, pkt.endian) << "\n";
            }
            out << "        } fields;\n";
            out << "    } group" << ++groupCounter << ";\n";
            out << "    group" << groupCounter << " field" << ++fieldCounter << ";\n";
            groupFieldCounter = 0;
        }
        // Запись обычного поля
        else {
            for (const XmlField &f : grp.fields) {
                QString cType;
                if (f.type == "unsigned") {
                    if (f.lengthBits <= 8) cType = "quint8";
                    else if (f.lengthBits <= 16) cType = "quint16";
                    else if (f.lengthBits <= 32) cType = "quint32";
                    else cType = "quint64";
                } else if (f.type == "signed") {
                    if (f.lengthBits <= 8) cType = "qint8";
                    else if (f.lengthBits <= 16) cType = "qint16";
                    else if (f.lengthBits <= 32) cType = "qint32";
                    else cType = "qint64";
                } else if (f.type == "float") {
                    cType = "float";
                } else if (f.type == "double") {
                    cType = "double";
                } else {
                    cType = "quint" + QString::number(f.lengthBits);
                }
                out << "    " << cType << " field" << ++fieldCounter << "; ";
                out << MakeFieldComment(f, pkt.endian) << "\n";
            }
        }
    }

    out << "} " << structName << ";\n";
    out << "#pragma pack()\n\n";
}

// Создание комментария
QString MakeFieldComment(const XmlField &f, const QString &endian) {
    // Имя, размерность, цмр
    QString comment = QString("// %1; %2; %3; ").arg(f.name, f.dimension).arg(f.lsb);

    // Значения
    if (!f.enumValues.isEmpty()) {
        QStringList items;
        for (auto it = f.enumValues.begin(); it != f.enumValues.cend(); ++it) {
            items << QString("%1=%2").arg(it.key()).arg(it.value());
        }
        comment += items.join(", ");
    }

    // Направление байт
    comment += QString("; %1; ").arg(endian);

    // Примечание
    if (f.constant)
        comment += QString("константа=%1").arg(f.constValue);
    if (!f.script.isEmpty())
    {
        if (f.constant)
            comment += ", ";
        comment += "script=" + f.script;
    }
    comment += + ";";

    return comment;
}

bool ConvertXMLtoH(const QString &inFilePath,const QString &outFilePath) {

    QList<XmlPacket> packets;
    try {
        packets = ParseXML(inFilePath);
    } catch (const std::exception &e) {
        qCritical() << e.what();
        return false;
    }

    QFile outFile(outFilePath);
    if (!outFile.open(QIODevice::WriteOnly)) {
        qCritical() << "Не удалось записать в файл " << outFilePath;
        return false;
    }

    QFileInfo fileInfo(inFilePath);
    QString baseName = fileInfo.baseName().toUpper().replace(' ', '_');

    QTextStream out(&outFile);
    out << QString("#ifndef %1_H\n").arg(baseName);
    out << QString("#define %1_H\n\n").arg(baseName);
    out << "#include \"qglobal.h\"\n";
    out << "#include <QByteArray>\n\n";

    int pktIndex = 0;
    for (const XmlPacket &pkt : packets) {
        WritePacket(pkt, ++pktIndex, out);
    }

    out << QString("#endif // %1_H\n").arg(baseName);
    out.flush();
    outFile.close();

    return true;
}

