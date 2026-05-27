#include "xmltoh.h"
#include <QFile>
#include <QXmlStreamReader>
#include <QTextStream>
#include <QDebug>
#include <QMap>
#include <QList>

QString makeFieldComment(const Field &f, const QString &endian) {

    QString comment = "// " + f.name + "; ";
    comment += f.dimension + "; ";
    comment += QString::number(f.lsb) + "; ";

    if (!f.enumValues.isEmpty()) {
        for (auto values = f.enumValues.begin(); values != f.enumValues.end(); ++values) {
            comment += values.key() + "=" + QString::number(values.value()) + " ";
        }
        comment += "; ";
    } else {
        comment += "; ";
    }

    comment += endian + "; ";

    if (f.constant)
        comment += "константа = " + QString::number(f.constValue);
    if (!f.script.isEmpty())
    {
        if (f.constant)
            comment += ", ";
        comment += "CRC script: " + f.script;
    }
    comment += + "; ";

    return comment;
}

void writePacket(const Packet &pkt, int packetIndex, QTextStream &out) {
    QString structName = (pkt.direction == "Прием" ? "Rx_" : "Tx_") + QString("Packet%1").arg(packetIndex);
    out << "#pragma pack(1)\n";
    out << "typedef struct {\n";

    int fieldCounter = 0;
    int groupCounter = 0;
    for (int i = 0; i < pkt.groups.size(); ++i) {
        const Group &grp = pkt.groups[i];
        bool isBitfieldGroup = true;
        int totalBits = 0;
        for (const Field &f : grp.fields) {
            if (f.type == "float" || f.type == "double") {
                isBitfieldGroup = false;
                break;
            }
            totalBits += f.lengthBits;
        }

        if (isBitfieldGroup && grp.fields.size() > 1 && totalBits <= 64) {
            QString groupStructName = structName + "_group" + QString::number(i+1);
            out << "    typedef union {\n";
            out << "        " << (totalBits <= 32 ? "quint32" : "quint64") << " raw;\n";
            out << "        struct {\n";
            int bitOffset = 0;
            for (const Field &f : grp.fields) {
                QString baseType = (f.type == "signed") ? "qint" : "quint";
                baseType += QString::number(f.lengthBits);
                out << "            " << baseType << " field" << ++fieldCounter << " : " << f.lengthBits << ";";
                out << " " << makeFieldComment(f, pkt.endian) << "\n";
                bitOffset += f.lengthBits;
            }
            out << "        } fields;\n";
            out << "    } group" << ++groupCounter << ";\n";
            out << "    group" << groupCounter << " group" << i+1 << ";\n";
        } else {
            for (const Field &f : grp.fields) {
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
                out << "    " << cType << " field" << ++fieldCounter << ";";
                out << " " << makeFieldComment(f, pkt.endian) << "\n";
            }
        }
    }
    out << "} " << structName << ";\n";
    out << "#pragma pack()\n\n";
}

bool ConverteXMLtoH(QString inFilePatch, QString outFilePatch) {
    QFile xmlFile(inFilePatch);
    if (!xmlFile.open(QIODevice::ReadOnly)) {
        qCritical() << "Не удалось открыть файл " << inFilePatch;
        return 0;
    }

    QXmlStreamReader xml(&xmlFile);
    QList<Packet> packets;
    QString currentDirection;

    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();
        if (token == QXmlStreamReader::StartElement) {
            if (xml.name() == "Type") {
                currentDirection = xml.attributes().value("name").toString();
            } else if (xml.name() == "Packet") {
                Packet pkt;
                pkt.direction = currentDirection;
                pkt.name = xml.attributes().value("name").toString();
                pkt.endian = xml.attributes().value("endian").toString();
                while (!(xml.tokenType() == QXmlStreamReader::EndElement && xml.name() == "Packet")) {
                    xml.readNext();
                    if (xml.tokenType() == QXmlStreamReader::StartElement && xml.name() == "Group") {
                        Group grp;
                        grp.lengthBytes = xml.attributes().value("length").toInt();
                        while (!(xml.tokenType() == QXmlStreamReader::EndElement && xml.name() == "Group")) {
                            xml.readNext();
                            if (xml.tokenType() == QXmlStreamReader::StartElement && xml.name() == "Field") {
                                Field f;
                                QStringList fullName = xml.attributes().value("name").toString().split(", ");
                                f.name = fullName[0];
                                if (fullName.size() == 2)
                                    f.dimension = fullName[1];
                                QString typeStr = xml.attributes().value("type").toString();
                                if (typeStr == "unsigned")
                                    f.type = "unsigned";
                                else if (typeStr == "signed")
                                    f.type = "signed";
                                else if (typeStr == "float")
                                    f.type = "float";
                                else if (typeStr == "double")
                                    f.type = "double";
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
    }

    if (xml.hasError()) {
        qCritical() << "Ошибка XML:" << xml.errorString();
        return 0;
    }
    xmlFile.close();

    QFile outFile(outFilePatch);
    if (!outFile.open(QIODevice::WriteOnly)) {
        qCritical() << "Не удалось записать в файл " << outFilePatch;
        return 0;
    }
    QTextStream out(&outFile);
    out << "#ifndef PROTOCOL_GENERATED_H\n";
    out << "#define PROTOCOL_GENERATED_H\n\n";


    out << "#include \"qglobal.h\"\n";
    out << "#include <QByteArray>\n\n";

    int pktIndex = 0;
    for (const Packet &pkt : packets) {
        writePacket(pkt, ++pktIndex, out);
    }

    out << "#endif // PROTOCOL_GENERATED_H\n";
    out.flush();
    outFile.close();

    return 1;
}
