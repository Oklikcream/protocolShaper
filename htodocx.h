#ifndef HTODOCX_H
#define HTODOCX_H

#include <QAxObject>

struct HField {
    QString name;                       // Имя
    QString dimension;                  // Размерность
    QString type;                       // Тип переменной
    int lengthBits;                     // Длина бит
    double lsb;                         // ЦМР
    QMap<QString, quint64> enumValues;  // Возможные значения
    QString endian;                     // Направление байт
    QString note;                       // Примечание
};

struct HGroup {
    int lengthBytes;                    // Длина байт
    bool isUnion;                       // Объединение полей
    QString unionName;                  // Имя объедиения
    QString unionType;                  // Тип объединения
    QList<HField> fields;               // Поля
};

struct HPacket {
    QString name;                       // Имя пакета
    QList<HGroup> groups;               // Группы
};

struct HUnionInfo {
    HGroup grp;                         // Группа
    int paramNumber;                    // Номер параметра
};

int BytesFromCType(const QString &cType);
bool ParseComment(const QString &comment, HField &f);
QList<HPacket> ParseH(const QString &inFilePath);
void SetCellText(QAxObject* table, int row, int col, const QString& text, int fontSize);
void MergeCellsVertical(QAxObject* table, int startRow, int endRow, int col);
void CreateUnionTable(QAxObject* selection, const HGroup &grp, int paramNumber);
bool ConvertHtoDOCX(QAxObject* word, const QString &inFilePath, const QString &outFilePath);

#endif // HTODOCX_H
