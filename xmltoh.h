#ifndef XMLTOH_H
#define XMLTOH_H

#include <QTextStream>
#include <QMap>
#include <QList>

struct XmlField {
    QString name;                       // Имя переменной
    QString dimension;                  // Размерность
    QString type;                       // Тип переменной
    int lengthBits;                     // Длина бит
    double lsb;                         // ЦМР
    bool constant;                      // Константность
    quint64 constValue;                 // Значение константы
    QMap<QString, quint64> enumValues;  // Возможные значения
    QString script;                     // Скрипт
};

struct XmlGroup {
    int lengthBytes;                    // Длина байт
    QList<XmlField> fields;                // Поля
};

struct XmlPacket {
    QString name;                       // Имя пакета
    QString direction;                  // Направление пакета
    QString endian;                     // Направление байт
    QList<XmlGroup> groups;                // Группы
};

QList<XmlPacket> ParseXML(const QString &inFilePatch);
void WritePacket(const XmlPacket &pkt, int packetIndex, QTextStream &out);
QString MakeFieldComment(const XmlField &f, const QString &endian);
bool ConvertXMLtoH(const QString &inFilePatch, const QString &outFilePatch);



#endif // XMLTOH_H
