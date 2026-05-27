#ifndef XMLTOH_H
#define XMLTOH_H

#include <QTextStream>
#include <QMap>
#include <QList>

struct Field {
    QString name;                       // Имя переменной
    QString dimension;                  // Размерность
    QString type;                       // Тип переменной
    int lengthBits;                     // Длина байт
    double lsb;                         // ЦМР
    bool constant;                      // Константность
    quint64 constValue;                 // Значение константы
    QMap<QString, quint64> enumValues;  // Возможные значения
    QString script;                     // Скрипт
};

struct Group {
    int lengthBytes;                    // Длина байт
    QList<Field> fields;                // Поля
};

struct Packet {
    QString name;                       // Имя пакета
    QString direction;                  // Направление пакета
    QString endian;                     // Направленеи байт
    QList<Group> groups;                // Группы
};

QString makeFieldComment(const Field &f, const QString &endian);
void writePacket(const Packet &pkt, int packetIndex, QTextStream &out);
bool ConverteXMLtoH(QString inFilePatch, QString outFilePatch);

#endif // XMLTOH_H
